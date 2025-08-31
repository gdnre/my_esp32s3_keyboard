#include "my_gpio_keys.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "stdlib.h"
#include "string.h"

// 存储按键信息的结构体，不暴露给其它文件
typedef struct
{
    my_input_gpio_keys_config_t gpio_config; // 初始化按键时使用的配置
    my_key_info_t *gpio_keys;                // 实际按键信息
} _my_gpio_keys_handle_t;

static const char *TAG = "my_gpio_keys";

static uint8_t gpio_key_init_flag = 0;

// 实际存储按键信息的变量，不为null
static _my_gpio_keys_handle_t _gpio_keys_handle = {
    .gpio_config = {.key_num = 0,
                    .active_level = 0,
                    .pull_def = 1,
                    .key_pins = NULL},
    .gpio_keys = NULL,
};

static void IRAM_ATTR my_gpio_keys_isr_handler(void *arg)
{
    if (my_input_task_handle)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        // xTaskNotifyFromISR(my_input_task_handle, arg, eSetBits, NULL);
        vTaskNotifyGiveFromISR(my_input_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
};

int8_t my_gpio_keys_config_init(my_input_gpio_keys_config_t *gpio_conf,
                                my_gpio_keys_handle_t *handle)
{
    if (gpio_key_init_flag || *handle)
    {
        ESP_LOGW(TAG, "my_gpio_key_init already init");
        return -1;
    }

    // 复制config中内容
    _gpio_keys_handle.gpio_config.key_num = gpio_conf->key_num;
    _gpio_keys_handle.gpio_config.active_level = gpio_conf->active_level;
    _gpio_keys_handle.gpio_config.pull_def = gpio_conf->pull_def;
    _gpio_keys_handle.gpio_config.key_pins =
        (uint8_t *)calloc(gpio_conf->key_num, sizeof(uint8_t));
    if (_gpio_keys_handle.gpio_config.key_pins == NULL)
    {
        ESP_LOGE(TAG, "my_gpio_key_init malloc gpio_config.key_pins failed");
        return 0;
    }
    memcpy(_gpio_keys_handle.gpio_config.key_pins, gpio_conf->key_pins,
           gpio_conf->key_num * sizeof(uint8_t));

    _gpio_keys_handle.gpio_keys =
        (my_key_info_t *)calloc(gpio_conf->key_num, sizeof(my_key_info_t));
    if (_gpio_keys_handle.gpio_keys == NULL)
    {
        ESP_LOGE(TAG, "my_gpio_key_init malloc gpio_keys failed");
        free(_gpio_keys_handle.gpio_config.key_pins);
        _gpio_keys_handle.gpio_config.key_pins = NULL;
        _gpio_keys_handle.gpio_config.key_num = 0;
        _gpio_keys_handle.gpio_config.active_level = 0;
        _gpio_keys_handle.gpio_config.pull_def = 1;
        return 0;
    }
    for (int i = 0; i < _gpio_keys_handle.gpio_config.key_num; i++)
    {
        _gpio_keys_handle.gpio_keys[i] = (my_key_info_t){.id = i,
                                                         .type = MY_INPUT_GPIO_KEY,
                                                         .active_level = gpio_conf->active_level,
                                                         .pull_def = gpio_conf->pull_def,
                                                         .state = MY_KEY_IDLE,
                                                         .pressed_timer = 0,
                                                         .released_timer = 0};
    }
    *handle = &_gpio_keys_handle;
    gpio_key_init_flag = 1;
    return 1;
}

// 在调用该函数之前可以单独修改特定引脚的配置
void my_gpio_keys_io_init(my_gpio_keys_handle_t handle)
{
    if (!handle)
    {
        return;
    }
    _my_gpio_keys_handle_t *h = (_my_gpio_keys_handle_t *)handle;
    my_input_gpio_keys_config_t *key_conf = &h->gpio_config;
    my_key_info_t *keys = h->gpio_keys;

    // 使用默认配置批量初始化引脚
    gpio_config_t io_conf = {.pin_bit_mask = 0,
                             .mode = GPIO_MODE_INPUT,
                             .intr_type = GPIO_INTR_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .pull_up_en = GPIO_PULLUP_DISABLE};

    // 添加要配置的引脚
    for (size_t i = 0; i < h->gpio_config.key_num; i++)
    {
        io_conf.pin_bit_mask |= 1ULL << key_conf->key_pins[i];
    }
    gpio_config(&io_conf);

    // 为每个引脚单独设置中断
    for (size_t i = 0; i < h->gpio_config.key_num; i++)
    {
        switch (keys[i].active_level)
        {
        case 0:
            if (key_conf->pull_def == 1)
                gpio_pullup_en(key_conf->key_pins[i]);
            gpio_set_intr_type(key_conf->key_pins[i], GPIO_INTR_NEGEDGE);
            break;
        case 1:
            if (key_conf->pull_def == 1)
                gpio_pulldown_en(key_conf->key_pins[i]);
            gpio_set_intr_type(key_conf->key_pins[i], GPIO_INTR_POSEDGE);
        default:
            break;
        }
    }

    for (size_t i = 0; i < h->gpio_config.key_num; i++)
    {
        gpio_isr_handler_add(key_conf->key_pins[i], my_gpio_keys_isr_handler,
                             &keys[i]);
        gpio_intr_disable(key_conf->key_pins[i]);
    }
    // return 1;
}

uint8_t check_gpio_keys(my_gpio_keys_handle_t handle)
{
    if (!handle)
    {
        return 0;
    }
    _my_gpio_keys_handle_t *h = (_my_gpio_keys_handle_t *)handle;
    uint8_t any_active = 0;
    int _level = -1;
    my_key_info_t *_key_info = NULL;
    for (size_t i = 0; i < h->gpio_config.key_num; i++)
    {
        _level = gpio_get_level(h->gpio_config.key_pins[i]);
        _key_info = &h->gpio_keys[i];
        any_active += update_key_info(_key_info, _level);
    }
    return any_active;
}

uint8_t my_gpio_key_register_event_cb(int16_t id, my_input_key_event_t event, my_input_key_event_cb_t cb)
{
    if (!gpio_key_init_flag)
    {
        ESP_LOGE(TAG, "gpio key %d register event cb fail ,gpio keys handle is null", id);
        return 0;
    }

    if (id < 0 || id >= _gpio_keys_handle.gpio_config.key_num)
    {
        ESP_LOGW(TAG, "gpio key %d register event cb fail ,id is out of range", id);
        return 0;
    }
    if (event < 0 || event >= MY_KEY_EVENT_NUM)
    {
        ESP_LOGW(TAG, "gpio key %d register event cb fail ,event is out of range", id);
        return 0;
    }
    _gpio_keys_handle.gpio_keys[id].event_cbs[event] = cb;
    return 1;
}

void my_gpio_keys_intr_dis(my_gpio_keys_handle_t handle)
{
    if (!handle)
    {
        ESP_LOGD(__func__, "gpio keys handle is null");
        return;
    }
    _my_gpio_keys_handle_t *h = (_my_gpio_keys_handle_t *)handle;
    for (size_t i = 0; i < h->gpio_config.key_num; i++)
    {
        gpio_intr_disable(h->gpio_config.key_pins[i]);
    };
}

void my_gpio_keys_intr_en(my_gpio_keys_handle_t handle)
{
    if (!handle)
    {
        ESP_LOGD(__func__, "gpio keys handle is null");
        return;
    }
    _my_gpio_keys_handle_t *h = (_my_gpio_keys_handle_t *)handle;
    for (size_t i = 0; i < h->gpio_config.key_num; i++)
    {
        gpio_intr_enable(h->gpio_config.key_pins[i]);
    }
}
#include "my_encoder_keys.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "stdlib.h"
#include "string.h"

#define MY_DEBUG_MODE 1
#if MY_DEBUG_MODE
#define MY_LOGI(format, ...) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#define MY_LOGW(format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#define MY_LOGE(format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#else
#define MY_LOGI(format, ...)
#define MY_LOGW(format, ...)
#define MY_LOGE(format, ...)
#endif

typedef struct
{
    uint8_t r_round;      // 右方向，顺时针的按键序号，默认应该为1
    uint8_t l_round;      // 左方向，逆时针的按键序号，默认应该为0
    uint8_t last_A_level; // 存储上一次a引脚电平
    my_input_encoder_keys_config_t encoder_config;
    my_key_info_t encoder_keys[2];
} _my_encoder_keys_handle_t;

static const char *TAG = "my_encoder_keys";
static uint8_t encoder_key_init_flag = 0;

static _my_encoder_keys_handle_t _encoder_keys_handle = {
    .r_round = 1,
    .l_round = 0,
    .last_A_level = 0};

static void IRAM_ATTR my_encoder_keys_isr_handler(void *arg)
{
    if (my_input_task_handle) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        // xTaskNotifyFromISR(my_input_task_handle, arg, eSetBits, NULL);
        vTaskNotifyGiveFromISR(my_input_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
};

int8_t my_encoder_keys_config_init(my_input_encoder_keys_config_t *encoder_conf, my_encoder_keys_handle_t *handle)
{
    if (encoder_key_init_flag || *handle) {
        ESP_LOGW(TAG, "my_encoder_key_init already init");
        return -1;
    }
    if (!encoder_conf || !handle) {
        return 0;
    }
    my_input_encoder_keys_config_t *_cfg = &_encoder_keys_handle.encoder_config;
    memcpy(_cfg, encoder_conf, sizeof(my_input_encoder_keys_config_t));

    _encoder_keys_handle.last_A_level = !_cfg->active_level;
    // 默认认为数组中第二个是顺时针键
    _encoder_keys_handle.r_round = 1;
    if (_cfg->change_direction) // 如果设置了切换，则第一个是顺时针
        _encoder_keys_handle.r_round = 0;
    _encoder_keys_handle.l_round = !_encoder_keys_handle.r_round; // 因为只有两个键，另一个直接用!运算

    for (size_t i = 0; i < 2; i++) {
        _encoder_keys_handle.encoder_keys[i].active_level = _cfg->active_level;
        _encoder_keys_handle.encoder_keys[i].id = i;
        _encoder_keys_handle.encoder_keys[i].pull_def = _cfg->pull_def;
        _encoder_keys_handle.encoder_keys[i].state = MY_KEY_IDLE;
        _encoder_keys_handle.encoder_keys[i].type = MY_INPUT_ENCODER;
        _encoder_keys_handle.encoder_keys[i].pressed_timer = 0;
        _encoder_keys_handle.encoder_keys[i].released_timer = 0;
    }
    *handle = &_encoder_keys_handle;
    encoder_key_init_flag = 1;
    return 1;
}

void my_encoder_keys_io_init(my_encoder_keys_handle_t handle)
{
    if (!handle) {
        return;
    }

    _my_encoder_keys_handle_t *h = (_my_encoder_keys_handle_t *)handle;
    my_input_encoder_keys_config_t *cfg = &h->encoder_config;

    uint64_t encoder_mask = (1ULL << cfg->a_pin) | (1ULL << cfg->b_pin);
    gpio_config_t io_conf = {
        .pin_bit_mask = encoder_mask,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    gpio_config(&io_conf);

    switch (cfg->active_level) {
        case 0:
            if (cfg->pull_def) {
                gpio_pullup_en(cfg->a_pin);
                gpio_pullup_en(cfg->b_pin);
            }
            gpio_set_intr_type(cfg->a_pin, GPIO_INTR_NEGEDGE);
            // gpio_set_intr_type(cfg->b_pin, GPIO_INTR_NEGEDGE);
            break;
        case 1:
            if (cfg->pull_def) {
                gpio_pulldown_en(cfg->a_pin);
                gpio_pulldown_en(cfg->b_pin);
            }
            gpio_set_intr_type(cfg->a_pin, GPIO_INTR_POSEDGE);
            // gpio_set_intr_type(cfg->b_pin, GPIO_INTR_POSEDGE);
            break;
        default:
            break;
    }
    gpio_isr_handler_add(cfg->a_pin, my_encoder_keys_isr_handler, &cfg->a_pin);
    gpio_intr_disable(cfg->a_pin);
}

uint8_t check_encoder_keys(my_encoder_keys_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    _my_encoder_keys_handle_t *h = (_my_encoder_keys_handle_t *)handle;
    my_input_encoder_keys_config_t *cfg = &h->encoder_config;

    uint8_t any_active = 0;
    int a_level = gpio_get_level(cfg->a_pin);
    // MY_LOGE("enter, a_level:%d", a_level);
    if (a_level == h->last_A_level) // 如果当前a引脚电平和之前记录电平无变化，说明并非上升沿或下降沿，a引脚电平等于触发电平时说明按键在活动，要等待下降沿，不能终止按键任务
    {
        any_active = (a_level == cfg->active_level);
        // MY_LOGE("a_level no change ,any_active:%d", any_active);
        if (!any_active) // 如果a引脚电平不等于触发电平，检查两个按键状态，强制变为释放状态
        {
            if (h->encoder_keys[0].state != MY_KEY_IDLE) {
                update_key_info(&h->encoder_keys[0], !cfg->active_level);
                // MY_LOGE("key 0 change");
            }
            if (h->encoder_keys[1].state != MY_KEY_IDLE) {
                update_key_info(&h->encoder_keys[1], !cfg->active_level);
                // MY_LOGE("key 1 change");
            }
        }
        // MY_LOGE("func return");
        return any_active;
    }

    h->last_A_level = a_level;                // a引脚电平和之前记录电平不同时，更新之前记录的电平
    int b_level = gpio_get_level(cfg->b_pin); // 获取b引脚电平
    // MY_LOGE("a change, b_level:%d", b_level);
    if (a_level == cfg->active_level) // 上升沿，触发按键
    {
        // MY_LOGE("a rising");
        if (b_level == cfg->active_level) {                                   // b引脚电平等于触发电平，假设此时是顺时针
            update_key_info(&h->encoder_keys[h->r_round], cfg->active_level); // 始终告诉按键处理程序按键已经按下
            // MY_LOGE("right");
        }
        else { // b引脚电平不等于触发电平，假设此时是逆时针
            update_key_info(&h->encoder_keys[h->l_round], cfg->active_level);
            // MY_LOGE("left");
        }
    }
    else // if (a_level != cfg->active_level) // 下降沿，释放按键
    {
        // MY_LOGE("a falling");
        if (b_level != cfg->active_level) {
            update_key_info(&h->encoder_keys[h->r_round], !cfg->active_level);
            // MY_LOGE("release right");
        }
        else {
            update_key_info(&h->encoder_keys[h->l_round], !cfg->active_level);
            // MY_LOGE("release left");
        }
    }
    // MY_LOGE("func end");
    return 1;
}

uint8_t my_encoder_key_register_event_cb(int16_t id, my_input_key_event_t event, my_input_key_event_cb_t cb)
{
    if (!encoder_key_init_flag) {
        ESP_LOGE(TAG, "encoder key %d register event cb fail ,encoder keys handle is null", id);
        return 0;
    }

    if (id < 0 || id >= 2) {
        ESP_LOGW(TAG, "encoder key %d register event cb fail ,id must be 0 or 1", id);
        return 0;
    }
    if (event < 0 || event >= MY_KEY_EVENT_NUM) {
        ESP_LOGW(TAG, "encoder key %d register event cb fail ,event is out of range", id);
        return 0;
    }
    _encoder_keys_handle.encoder_keys[id].event_cbs[event] = cb;
    return 1;
}

void my_encoder_keys_intr_dis(my_encoder_keys_handle_t handle)
{
    if (!handle) {
        ESP_LOGD(__func__, "encoder keys handle is null");
        return;
    }
    _my_encoder_keys_handle_t *h = (_my_encoder_keys_handle_t *)handle;
    gpio_intr_disable(h->encoder_config.a_pin);
}

void my_encoder_keys_intr_en(my_encoder_keys_handle_t handle)
{
    if (!handle) {
        ESP_LOGD(__func__, "encoder keys handle is null");
        return;
    }
    _my_encoder_keys_handle_t *h = (_my_encoder_keys_handle_t *)handle;
    gpio_intr_enable(h->encoder_config.a_pin);
}
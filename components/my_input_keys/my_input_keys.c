#include "my_input_keys.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "my_encoder_keys.h"
#include "my_input_base.h"
#include "stdlib.h"
#include "string.h"
// #include "esp_intr_alloc.h"

#define MY_INTR_FLAG_DEFAULT 0

static const char *TAG = "my_input_keys";

#define my_input_base_evt_cb_exec(cb_index) \
    if (my_input_base_event_cbs[cb_index]) { my_input_base_event_cbs[cb_index](cb_index); }

#define my_keys_intr_en_with_handle(gpio, matrix, encoder) \
    do {                                                   \
        my_gpio_keys_intr_en(gpio);                        \
        my_matrix_in_keys_intr_en(matrix);                 \
        my_encoder_keys_intr_en(encoder);                  \
    } while (0)

#define my_keys_intr_dis_with_handle(gpio, matrix, encoder) \
    do {                                                    \
        my_encoder_keys_intr_dis(gpio);                     \
        my_matrix_in_keys_intr_dis(matrix);                 \
        my_encoder_keys_intr_dis(encoder);                  \
    } while (0)

#define my_keys_intr_en() my_keys_intr_en_with_handle(gpio_handle, matrix_handle, encoder_handle)
#define my_keys_intr_dis() my_keys_intr_dis_with_handle(gpio_handle, matrix_handle, encoder_handle)

my_input_task_info_t my_input_task_info = {
    .continue_cycle_ptr = NULL,
    .active_cycle_count_ptr = NULL,
    .active_key_num_ptr = NULL,
};

int64_t input_task_enter_time_us = 0;

static my_matrix_keys_handle_t matrix_handle = NULL;
static my_gpio_keys_handle_t gpio_handle = NULL;
static my_encoder_keys_handle_t encoder_handle = NULL;
int32_t long_pressed_time_us = 500000;

my_input_base_event_cb_t my_input_base_event_cbs[MY_INPUT_EVENT_NUM] = {NULL};

int64_t last_active_time = 0;
uint8_t my_input_task_waiting = 0;

TaskHandle_t my_input_task_handle = NULL;

int64_t my_input_get_last_active_time_us()
{
    if (my_input_task_waiting) {
        return last_active_time;
    }
    else {
        return esp_timer_get_time();
    }
};

uint8_t my_input_not_active_over(int64_t sleep_time_us)
{
    if (!my_input_task_waiting) {
        return 0;
    }
    int64_t now = esp_timer_get_time();
    if (now > last_active_time) {
        return (now - last_active_time) > sleep_time_us;
    }
    else {
        return (INT64_MAX - last_active_time + now) > sleep_time_us;
    }
};

int8_t my_input_key_config_init(my_input_matrix_config_t *matrix_conf, my_input_gpio_keys_config_t *gpio_conf, my_input_encoder_keys_config_t *encoder_conf)
{
    if (gpio_conf && gpio_conf->key_num && gpio_conf->key_pins)
        my_gpio_keys_config_init(gpio_conf, &gpio_handle);
    else
        ESP_LOGD(TAG, "my_input_key_config_init gpio_conf invalid");

    if (matrix_conf && matrix_conf->row_num && matrix_conf->col_num && matrix_conf->row_pins && matrix_conf->col_pins)
        my_matrix_keys_config_init(matrix_conf, &matrix_handle);
    else
        ESP_LOGD(TAG, "my_input_key_config_init matrix_conf invalid");
    if (encoder_conf && (encoder_conf->a_pin != encoder_conf->b_pin))
        my_encoder_keys_config_init(encoder_conf, &encoder_handle);
    else
        ESP_LOGD(TAG, "my_input_key_config_init encoder_conf invali");
    return 1;
}

int8_t my_input_key_io_init()
{
    // 安装中断服务
    gpio_install_isr_service(MY_INTR_FLAG_DEFAULT);
    if (gpio_handle) {
        my_gpio_keys_io_init(gpio_handle);
    }
    if (matrix_handle) {
        my_matrix_keys_io_init(matrix_handle);
    }
    if (encoder_handle) {
        my_encoder_keys_io_init(encoder_handle);
    }

    return 1;
}

uint8_t update_key_info(my_key_info_t *key, int8_t level)
{
    int64_t now = esp_timer_get_time();

    if (now - key->pressed_timer < MY_KEY_DEBOUNCE_US || now - key->released_timer < MY_KEY_DEBOUNCE_US) {// 按键消抖，之后考虑根据不同输入类型，设置不同的消抖时间
        *my_input_task_info.continue_cycle_ptr = 1;
        return 0;
    }

    uint8_t key_active = 0;
    static uint8_t first_enter = 1;
    if (key->active_level == level) { // 按键按下
        key_active = 1;
        if (key->state == MY_KEY_IDLE) { // 按键之前是释放状态，需要改为按下状态，并记录按下时间，执行按下事件
            key->state = MY_KEY_HOLD;
            // 执行按下事件
            if (key->event_cbs[MY_KEY_PRESSED_EVENT])
                key->event_cbs[MY_KEY_PRESSED_EVENT](MY_KEY_PRESSED_EVENT, key);
            key->pressed_timer = now; // 执行事件后再更新时间
            ESP_LOGD(TAG, "key %d pressed", key->id);
        }
        else if (key->state == MY_KEY_HOLD) {
            // 按键之前是按下状态，检查按键是否要变为长按状态
            if (now - key->pressed_timer > long_pressed_time_us) { // 按键长按，改为长按状态
                key->state = MY_KEY_LONGPRESSED;
                // 执行长按事件
                if (key->event_cbs[MY_KEY_LONGPRESSED_START_EVENT])
                    key->event_cbs[MY_KEY_LONGPRESSED_START_EVENT](
                        MY_KEY_LONGPRESSED_START_EVENT, key);
                ESP_LOGD(TAG, "key %d long pressed", key->id);
            }
        }
        else if (key->state == MY_KEY_LONGPRESSED) {
            if (key->event_cbs[MY_KEY_LONGPRESSED_HOLD_EVENT])
                key->event_cbs[MY_KEY_LONGPRESSED_HOLD_EVENT](
                    MY_KEY_LONGPRESSED_HOLD_EVENT, key);
        }
    }
    else // if (key->active_level != level)
    {    // 按键释放
        key_active = 0;
        if (key->state == MY_KEY_HOLD) {
            key->state = MY_KEY_IDLE;
            // 执行释放事件
            if (key->event_cbs[MY_KEY_RELEASED_EVENT])
                key->event_cbs[MY_KEY_RELEASED_EVENT](MY_KEY_RELEASED_EVENT, key);
            key->released_timer = now;
            ESP_LOGD(TAG, "key %d released", key->id);
        }
        else if (key->state == MY_KEY_LONGPRESSED) {
            key->state = MY_KEY_IDLE;
            // 执行长按释放事件，如果没有长按事件回调，则默认执行释放事件
            if (key->event_cbs[MY_KEY_LONGPRESSED_RELEASED_EVENT])
                key->event_cbs[MY_KEY_LONGPRESSED_RELEASED_EVENT](
                    MY_KEY_LONGPRESSED_RELEASED_EVENT, key);
            else if (key->event_cbs[MY_KEY_RELEASED_EVENT])
                key->event_cbs[MY_KEY_RELEASED_EVENT](MY_KEY_RELEASED_EVENT, key);
            key->released_timer = now;
            ESP_LOGD(TAG, "key %d long released", key->id);
        }
    }
    return key_active;
}

int8_t my_input_register_event_cb(uint8_t type, int16_t id, my_input_key_event_t event, my_input_key_event_cb_t cb)
{
    int8_t ret = 0;
    switch (type) {
        case MY_INPUT_GPIO_KEY:
            ret = my_gpio_key_register_event_cb(id, event, cb);
            break;
        case MY_INPUT_KEY_MATRIX:
            ret = my_matrix_key_register_event_cb(id, event, cb);
            break;
        case MY_INPUT_ENCODER:
            ret = my_encoder_key_register_event_cb(id, event, cb);
            break;
        default:
            break;
    }
    return ret;
};

int8_t my_input_register_base_event_cb(uint8_t type, my_input_base_event_t event, my_input_base_event_cb_t cb)
{
    if (type != MY_INPUT_KEY_TYPE_BASE_AND_NUM || event >= MY_INPUT_EVENT_NUM)
        return 0;
    my_input_base_event_cbs[event] = cb;
    return 1;
}

// void my_input_key_begin()
// {
//     uint8_t gpio_pins[] = {0};
//     my_input_gpio_keys_config_t gpio_config = {
//         .key_num = 1,
//         .active_level = 0,
//         .key_pins = gpio_pins,
//     };
//     uint8_t col_pins[] = {15, 16, 17, 18};
//     uint8_t row_pins[] = {7, 6, 5, 4};
//     my_input_matrix_config_t matrix_config = {
//         .active_level = 1,
//         .col_num = 4,
//         .row_num = 4,
//         .col_pins = col_pins,
//         .row_pins = row_pins,
//         .switch_input_pins = 0,
//         .pull_def = 1,
//     };
//     my_input_encoder_keys_config_t encoder_conf = {
//         .a_pin = 10,
//         .b_pin = 11,
//         .active_level = 1,
//         .change_direction = 1,
//         .pull_def = 1};
//     gpio_config.key_num = sizeof(gpio_pins) / sizeof(gpio_pins[0]);
//     my_input_key_config_init(&matrix_config, NULL, NULL);
//     // ESP_LOGI(TAG, "my_input_key_config_init finish");
//     my_input_key_io_init();
//     // ESP_LOGI(TAG, "my_input_key_io_init finish");
//     // 任务栈空间要大于2k，2k会爆栈
//     xTaskCreatePinnedToCore(my_input_key_task, "my_input_task", 8192, NULL, 2, &my_input_task_handle, 1);
//     // ESP_LOGI(TAG, "my_input_key_task finish");
// }

void my_input_key_task(void *arg)
{
    my_matrix_scan_one_line(matrix_handle, 0);
    input_task_enter_time_us = esp_timer_get_time();
    uint16_t active_key_num = 0;
    uint16_t active_cycle_count = 0;
    uint16_t continue_cycle = 0;
    my_input_task_info.active_key_num_ptr = &active_key_num;
    my_input_task_info.continue_cycle_ptr = &continue_cycle;
    my_input_task_info.active_cycle_count_ptr = &active_cycle_count;
    ESP_LOGD(__func__, "task high water mask %d", uxTaskGetStackHighWaterMark(NULL));
    for (;;) {
        active_cycle_count++;
        active_key_num = 0;
        // 按键扫描周期开始
        my_input_base_evt_cb_exec(MY_INPUT_CYCLE_START_EVENT);

        // 编码器按键检查开始
        my_input_base_evt_cb_exec(MY_INPUT_ENCODER_CHECK_START_EVENT);
        active_key_num += check_encoder_keys(encoder_handle);
        my_input_base_evt_cb_exec(MY_INPUT_ENCODER_CHECK_END_EVENT);
        // 编码器检查结束

        // gpio按键检查开始
        my_input_base_evt_cb_exec(MY_INPUT_GPIO_CHECK_START_EVENT);
        active_key_num += check_gpio_keys(gpio_handle);
        my_input_base_evt_cb_exec(MY_INPUT_GPIO_CHECK_END_EVENT);
        // gpio按键检查结束

        // 矩阵按键检查开始
        my_input_base_evt_cb_exec(MY_INPUT_MATRIX_CHECK_START_EVENT);
        // key_active |= check_matrix_keys(matrix_handle);
        active_key_num += check_matrix_keys_faster(matrix_handle);
        my_input_base_evt_cb_exec(MY_INPUT_MATRIX_CHECK_END_EVENT);
        // 矩阵按键检查结束

        my_input_base_evt_cb_exec(MY_INPUT_CYCLE_END_EVENT);
        // 按键扫描周期结束
        continue_cycle += active_key_num;
        if (continue_cycle) {
            continue_cycle = 0;
            vTaskDelay(pdMS_TO_TICKS(MY_KEY_SCAN_INTERVAL_MS)); // 每次检测间隔1ms，也有消抖作用，抖动小于1ms的不需要额外消抖
        }
        else {                                            // key_active==0
            my_matrix_set_out_to_level(matrix_handle, 1); // 将输出端电平全部设置为触发电平
            my_keys_intr_en();

            last_active_time = esp_timer_get_time();
            my_input_task_waiting = 1;
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            my_input_task_waiting = 0;
            last_active_time = esp_timer_get_time();

            my_keys_intr_dis();
            my_matrix_scan_one_line(matrix_handle, 0); // 任务唤醒后，先将out端的一列设置为扫描状态，这样键盘扫描任务可以直接读取输入端电平
            active_cycle_count = 0;
            // ESP_LOGI(TAG, "task wakeup");
        }
    }
    my_input_task_info.active_cycle_count_ptr = NULL;
    my_input_task_info.continue_cycle_ptr = NULL;
    my_input_task_info.active_key_num_ptr = NULL;
    vTaskSuspend(NULL);
}
#include "class/hid/hid.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "my_ble_hid.h"
#include "my_config.h"
#include "my_espnow.h"
#include "my_input_keys.h"
#include "my_keyboard.h"
#include "my_usb.h"
#include "my_usb_hid.h"
#include <stdio.h>

typedef struct __attribute__((packed)) {
    union {
        struct
        {
            uint8_t report_type : 1; // 0表示键盘报告，1表示consumer报告
            uint8_t can_free : 1;
            uint8_t need_free : 1;
            uint8_t send_to_usb : 1;
            uint8_t send_to_ble : 1;
            uint8_t send_to_espnow : 1;
            uint8_t data_need_process : 1;
            uint8_t reserve : 1;
        };
        uint8_t raw;
    } flag;
    int64_t send_time_us;
    uint16_t valid_time_ms;
    uint16_t report_data_size;
    union {
        my_keyboard_report_t kb_report;
        my_consumer_report_t cons_report;
    };
    // void *report; // 弃用，因为很难判断释放内存的时机
} my_kb_queue_data_t;

uint16_t my_hid_report_valid_time_ms = 300; // 记得在启动一定时间后，在循环开始的回调中将它调小

uint8_t my_hid_report_change = 0;
uint8_t _current_layer = MY_ORIGINAL_LAYER;
uint8_t _swap_fn = 0; // 开启切换fn时，如果按下fn，触发原始层，默认为fn层，保存到rtc内存中，当深睡唤醒时保存状态，重启时恢复默认

QueueHandle_t my_send_report_queue = NULL;
// QueueHandle_t my_send_code_queue = NULL;

// 将按键添加到hid report
void my_kb_hid_report_add_key(my_kb_key_config_t *kb_cfg)
{
    uint8_t ret = 0xFF;
    switch (kb_cfg->type) {
        case MY_KEYBOARD_CODE:
            ret = my_hid_add_keycode_raw(kb_cfg->code8);
            if (ret == 1) {
                my_hid_report_change |= MY_KEYBOARD_REPORT_CHANGE;
            }
            break;
        case MY_KEYBOARD_CHAR:
            if (kb_cfg->code8s[1]) {
                ret |= my_hid_add_keycode_raw(HID_KEY_SHIFT_LEFT);
            }
            ret &= my_hid_add_keycode_raw(kb_cfg->code8s[0]);
            if (ret == 1) {
                my_hid_report_change |= MY_KEYBOARD_REPORT_CHANGE;
            }
            break;
        case MY_CONSUMER_CODE:
            ret = my_hid_add_consumer_code16(kb_cfg->code16);
            if (ret == 1) {
                my_hid_report_change |= MY_CONSUMER_REPORT_CHANGE;
            }
            break;
        case MY_EMPTY_KEY:
        case MY_KEYCODE_NONE:
        default:
            break;
    }
}

// 将按键从hid report移除
void my_kb_hid_report_remove_key(my_kb_key_config_t *kb_cfg)
{
    uint8_t ret = 0xFF;
    switch (kb_cfg->type) {
        case MY_KEYBOARD_CODE:
            ret = my_hid_remove_keycode_raw(kb_cfg->code8);
            if (ret == 1) {
                my_hid_report_change |= MY_KEYBOARD_REPORT_CHANGE;
            }
            break;
        case MY_KEYBOARD_CHAR:
            if (kb_cfg->code8s[1]) {
                ret |= my_hid_remove_keycode_raw(HID_KEY_SHIFT_LEFT);
            }
            ret &= my_hid_remove_keycode_raw(kb_cfg->code8s[0]);
            if (ret == 1) {
                my_hid_report_change |= MY_KEYBOARD_REPORT_CHANGE;
            }
            break;
        case MY_CONSUMER_CODE:
            ret = my_hid_remove_consumer_code();
            if (ret == 1) {
                my_hid_report_change |= MY_CONSUMER_REPORT_CHANGE;
            }
            break;
        case MY_EMPTY_KEY:
        case MY_KEYCODE_NONE:
            break;
        default:
            break;
    }
}

/*
swap_fn
    fn按下 fn松开/fn2松开
0     1      0
1     0      1
*/
void my_fn_key_handler(my_kb_key_config_t *kb_cfg, uint8_t key_pressed)
{
    switch (kb_cfg->type) {
        case MY_FN_KEY:
            if (key_pressed) {
                _current_layer = MY_FN_LAYER ^ _swap_fn;
            }
            else {
                _current_layer = MY_ORIGINAL_LAYER ^ _swap_fn;
            }
            break;
        case MY_FN2_KEY:
            if (key_pressed) {
                _current_layer = MY_FN2_LAYER;
            }
            else {

                _current_layer = MY_ORIGINAL_LAYER ^ _swap_fn;
            }
            break;
        case MY_FN_SWITCH_KEY:
            if (key_pressed) {
                _swap_fn = !_swap_fn;
                my_cfg_fn_sw_state = _swap_fn;
                if (_current_layer == MY_ORIGINAL_LAYER) {
                    _current_layer = MY_FN_LAYER;
                }
                else if (_current_layer == MY_FN_LAYER) {
                    _current_layer = MY_ORIGINAL_LAYER;
                }
            }
            break;
        default:
            break;
    }
    if (my_lvgl_is_running()) {
        my_lv_buttonm_send_event(LV_EVENT_MY_SET_FN_LAYER, (void *)_current_layer);
    }
}

// MY_ESP_CTRL_KEY类型按键处理程序，会向my config的事件池发布事件
void my_ctrl_key_handler(my_kb_key_config_t *kb_cfg, uint8_t key_pressed)
{
    if (!key_pressed) {
        return;
    }
    my_cfg_event_post(kb_cfg->code16, NULL, 0, 0);
}

void my_keyboard_key_handler(my_key_info_t *key_info, uint8_t key_pressed)
{
    uint8_t target_layer = _current_layer;
    if (key_pressed == 1) // 如果是按下按键，要记录当前按下的层
    {
        key_info->user_data.layer_mask = target_layer;
    }
    else if (key_pressed == 0) // 如果是释放按键，要根据记录的layer_mask设置目标层
    {
        target_layer = key_info->user_data.layer_mask;
    }
    else {
        return;
    }
    my_kb_key_config_t *_cfg = &my_kb_keys_config[target_layer][key_info->type][key_info->id];
    if (_cfg->type == MY_KEYCODE_NONE) {
        _cfg = &my_kb_keys_config[MY_ORIGINAL_LAYER][key_info->type][key_info->id];
    }
    if (_cfg->type <= MY_EMPTY_KEY) {
        if (key_pressed)
            my_kb_hid_report_add_key(_cfg);
        else
            my_kb_hid_report_remove_key(_cfg);
    }
    else if (_cfg->type <= MY_FN_SWITCH_KEY) {
        my_fn_key_handler(_cfg, key_pressed);
    }
    else if (_cfg->type == MY_ESP_CTRL_KEY) {
        my_ctrl_key_handler(_cfg, key_pressed);
    }
}

void my_input_cycle_start_cb(my_input_base_event_t event)
{
    if (my_get_time() < 300000) {
        // if (my_input_task_info.continue_cycle_ptr) {
        //     *my_input_task_info.continue_cycle_ptr += 1;
        // }
    }
    else {
        // 超过300ms后，清除该回调
        my_hid_report_valid_time_ms = 100;
        my_input_register_base_event_cb(MY_INPUT_KEY_TYPE_BASE_AND_NUM, event, NULL);
    }
}

void my_input_cycle_end_cb(my_input_base_event_t event)
{
    // 将当前的hid报告发送到队列，让另一个任务处理
    // todo: 使用一个my_send_code_queue队列，接收单个code来支持发送一系列报告

    if (!my_send_report_queue || !my_hid_report_change) {
        return;
    }
    my_kb_queue_data_t queue_data = {0};

    if (my_cfg_usb.data.u8) { queue_data.flag.send_to_usb = 1; }
    if (my_cfg_ble.data.u8) { queue_data.flag.send_to_ble = 1; }
    if (my_cfg_wifi_mode.data.u8 & MY_WIFI_MODE_ESPNOW) { queue_data.flag.send_to_espnow = 1; }
    queue_data.send_time_us = my_get_time();
    queue_data.valid_time_ms = my_hid_report_valid_time_ms;
    queue_data.flag.can_free = 0;
    queue_data.flag.need_free = 0;

    if (my_hid_report_change & MY_KEYBOARD_REPORT_CHANGE) {
        queue_data.flag.report_type = 0;
        queue_data.report_data_size = sizeof(my_keyboard_report_t);
        memcpy(&queue_data.kb_report, &my_keyboard_report, queue_data.report_data_size);
        if (xQueueSend(my_send_report_queue, &queue_data, 0) != pdTRUE) {
            my_kb_queue_data_t temp;
            xQueueReceive(my_send_report_queue, &temp, 0);
            xQueueSend(my_send_report_queue, &queue_data, 1);
        }
    }
    if (my_hid_report_change & MY_CONSUMER_REPORT_CHANGE) {
        queue_data.flag.report_type = 1;
        queue_data.report_data_size = sizeof(my_consumer_report_t);
        memcpy(&queue_data.cons_report, &my_consumer_report, queue_data.report_data_size);
        if (xQueueSend(my_send_report_queue, &queue_data, 0) != pdTRUE) {
            my_kb_queue_data_t temp;
            xQueueReceive(my_send_report_queue, &temp, 0);
            xQueueSend(my_send_report_queue, &queue_data, 1);
        }
    }
    my_hid_report_change = 0;
}

void my_input_cycle_end_cb_backup(my_input_base_event_t event)
{
    if (my_cfg_usb.data.u8) {
        if (my_hid_report_change & MY_USB_KEYBOARD_REPORT_CHANGE) {
            if (my_hid_send_keyboard_report()) {
                my_hid_report_change &= ~MY_USB_KEYBOARD_REPORT_CHANGE;
            }
        }
        if (my_hid_report_change & MY_USB_CONSUMER_REPORT_CHANGE) {
            if (my_hid_send_consumer_report()) {
                my_hid_report_change &= ~MY_USB_CONSUMER_REPORT_CHANGE;
            }
        }
    }
    if (my_cfg_ble.data.u8) {
        if (my_hid_report_change & MY_BLE_KEYBOARD_REPORT_CHANGE) {
            if (my_ble_hidd_send_report(my_keyboard_report.report_id, &my_keyboard_report.modifier, my_keyboard_report_size) == ESP_OK) {
                my_hid_report_change &= ~MY_BLE_KEYBOARD_REPORT_CHANGE;
            }
        }
        if (my_hid_report_change & MY_BLE_CONSUMER_REPORT_CHANGE) {
            if (my_ble_hidd_send_report(my_consumer_report.report_id, (void *)&my_consumer_report.consumer_code16, my_consumer_report_size) == ESP_OK) {
                my_hid_report_change &= ~MY_BLE_CONSUMER_REPORT_CHANGE;
            }
        }
    }
    if (my_cfg_wifi_mode.data.u8 & MY_WIFI_MODE_ESPNOW) {
        if (my_hid_report_change & MY_ESPNOW_KEYBOARD_REPORT_CHANGE) {
            if (my_espnow_send_hid_input_report((uint8_t *)&my_keyboard_report, my_keyboard_report_size + 1) == ESP_OK) {
                my_hid_report_change &= ~MY_ESPNOW_KEYBOARD_REPORT_CHANGE;
            }
        }
        if (my_hid_report_change & MY_ESPNOW_CONSUMER_REPORT_CHANGE) {
            if (my_espnow_send_hid_input_report((uint8_t *)&my_consumer_report, my_consumer_report_size + 1) == ESP_OK) {
                my_hid_report_change &= ~MY_ESPNOW_CONSUMER_REPORT_CHANGE;
            }
        }
    }
}

void my_lvgl_key_state_event_handler(my_key_info_t *key_info, uint8_t key_pressed)
{
    if (my_lvgl_is_running()) {
        uint32_t lv_event_param = 0; // 后16位指示按键类型
        switch (key_info->type) {
            case MY_INPUT_KEY_MATRIX: // 后16位为1时表示按键矩阵，前16位的前8位是列号，后8位是行号
                uint8_t col = 0;
                uint8_t row = 0;
                if (my_id_to_matrix(key_info->id, 0, &row, &col) != 0)
                    return;
                lv_event_param = (col | (row << 8)) & 0xFFFF;
                lv_event_param |= 1 << 16;
                break;
            case MY_INPUT_ENCODER: // 后16位为2时表示编码器按键，id为按键id
                lv_event_param = key_info->id;
                lv_event_param |= 2 << 16;
                break;
            default:
                return;
                break;
        }
        // ESP_LOGI(__func__, "key_info.id=%d, pressed:%d", key_info->id, key_pressed);
        if (key_pressed)
            my_lv_buttonm_send_event(LV_EVENT_MY_KEY_STATE_PRESS, lv_event_param);
        else
            my_lv_buttonm_send_event(LV_EVENT_MY_KEY_STATE_RELEASE, lv_event_param);
    }
}

void my_espnow_reciver_recv_input_report_cb(uint8_t *data, uint8_t data_len)
{
    if (!data_len || !data)
        return;
    if (data[0] == my_keyboard_report.report_id && data_len == (my_keyboard_report_size + 1)) {
        memcpy(&my_keyboard_report, data, data_len);
        my_hid_send_keyboard_report();
    }
    else if (data[0] == my_consumer_report.report_id && data_len == (my_consumer_report_size + 1)) {
        memcpy(&my_consumer_report, data, data_len);
        my_hid_send_consumer_report();
    }
    else if (data_len == my_keyboard_report_size) {
        memcpy(&my_keyboard_report.modifier, data, data_len);
        my_hid_send_keyboard_report();
    }
    else if (data_len == my_consumer_report_size) {
        memcpy(&my_consumer_report.consumer_code16, data, data_len);
        my_hid_send_consumer_report();
    }
};

void my_espnow_reciver_recv_get_output_cb()
{
    if (my_usbd_ready()) {
        my_espnow_send_hid_output_report((uint8_t *)&my_kb_led_report, my_kb_led_report_size);
    }
    else {
        my_espnow_send_hid_output_report((uint8_t *)&my_kb_led_report, 0);
    };
    return;
};

void my_espnow_device_recv_output_report_cb(uint8_t *data, uint8_t data_len)
{
    if (!data)
        return;
    if (data_len == 0 && !(my_cfg_ble_state & MY_FEATURE_CONNECTED) && !my_usbd_ready()) {
        my_kb_led_report.raw = 0;
        esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, 1, 0);
    }
    if ((my_cfg_ble_state & MY_FEATURE_CONNECTED) && my_usbd_ready()) {
        return;
    }

    if (data[0] == my_kb_led_report.report_id && data_len == my_kb_led_report_size) {
        my_kb_led_report.raw = data[1];
        esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, 1, 0);
    }
    else if (data_len == 1) {
        my_kb_led_report.raw = data[0];
        esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, 1, 0);
    }
};

// 当当前时间传入0或一个较小值时，时间的检查将无效
// 如果check_time_only为1，则只返回时间检查的结果
static bool my_kb_queue_data_is_valid(int64_t cur_time, my_kb_queue_data_t *data, uint8_t check_time_only)
{
    if (data) {
        if (cur_time >= data->send_time_us + data->valid_time_ms * 1000) {
            return false;
        }
        else if (check_time_only) {
            return true;
        }
        if (data->flag.report_type == 0) {
            return (data->report_data_size == sizeof(my_keyboard_report_t));
        }
        else {
            return (data->report_data_size == sizeof(my_consumer_report_t));
        }
    }
    return false;
}

// 从键盘报告队列中获取一个合法数据，直到队列中没有数据或重试超过一定次数
// 获取到数据返回pdTRUE，返回时队列长度为0返回pdFALSE
static BaseType_t my_kb_queue_pop_until_valid(QueueHandle_t handle, int64_t cur_time, my_kb_queue_data_t *data_buf, TickType_t ticks_to_wait)
{
    if (!data_buf || !handle) {
        return -1;
    }
    BaseType_t ret;
    UBaseType_t queue_len;
    TickType_t wait_time = 0;
    uint8_t fail_count = 0;
    do {
        // 第一次循环，先不阻塞尝试获取数据
        ret = xQueueReceive(handle, data_buf, wait_time);
        if (ret == pdTRUE) {
            // 如果获取到数据，检查数据
            if (my_kb_queue_data_is_valid(cur_time, data_buf, 0)) {
                // 数据有效可以直接返回
                return pdTRUE;
            }
            wait_time = 0; // 因为获取数据成功，下次获取不阻塞
            fail_count = 0;
        }
        else {
            // 如果没有获取到数据，可能是有竞争或者队列为空，设置一个等待时间
            wait_time = ticks_to_wait;
            fail_count++;
        }
        // 获取当前队列长度，如果为0则退出循环
        queue_len = uxQueueMessagesWaiting(handle);
    } while (queue_len > 0 && fail_count < 3);
    if (queue_len != 0)
        return -1;
    return pdFALSE;
}

void my_kb_send_report_task(void *arg)
{
    if (!my_send_report_queue)
        my_send_report_queue = xQueueCreate(10, sizeof(my_kb_queue_data_t));
    assert(my_send_report_queue);
    QueueHandle_t usb_retry_queue = xQueueCreate(6, sizeof(my_kb_queue_data_t));
    assert(usb_retry_queue);
    QueueHandle_t ble_retry_queue = xQueueCreate(6, sizeof(my_kb_queue_data_t));
    assert(ble_retry_queue);
    QueueHandle_t espnow_retry_queue = xQueueCreate(6, sizeof(my_kb_queue_data_t));
    assert(espnow_retry_queue);

    my_kb_queue_data_t send_queue_data;
    my_kb_queue_data_t usb_data = {0};
    my_kb_queue_data_t ble_data = {0};
    my_kb_queue_data_t espnow_data = {0};
    TickType_t queue_wait_time = portMAX_DELAY;
    BaseType_t p_ret;
    int64_t cycle_start_time_us;
    uint8_t any_report_send = 0;
    for (;;) {
        p_ret = xQueueReceive(my_send_report_queue, &send_queue_data, queue_wait_time);
        cycle_start_time_us = my_get_time();
        // 检查是否获取到数据和数据的合法性，并根据需要发送到usb、espnow和ble队列中
        // todo 循环获取直到数据合法或队列为空
        if (p_ret == pdTRUE) {
            if (my_kb_queue_data_is_valid(cycle_start_time_us, &send_queue_data, 0)) {
                if (my_cfg_usb.data.u8 && send_queue_data.flag.send_to_usb) {
                    // 将数据发送到usb队列，如果发送失败说明队列已满，取出一个数据
                    if (xQueueSend(usb_retry_queue, &send_queue_data, 0) != pdTRUE) {
                        if (my_kb_queue_pop_until_valid(usb_retry_queue, cycle_start_time_us, &usb_data, 0) == pdTRUE) {
                            // 如果成功取出数据，设置对应标志位，之后先处理该数据
                            usb_data.flag.data_need_process = 1;
                        }
                        // 不管有没有成功取出数据，都尝试再次入队，还失败就不管了
                        xQueueSend(usb_retry_queue, &send_queue_data, 0);
                    }
                }
                if (my_cfg_ble.data.u8 && send_queue_data.flag.send_to_ble) {
                    if (xQueueSend(ble_retry_queue, &send_queue_data, 0) != pdTRUE) {
                        if (my_kb_queue_pop_until_valid(ble_retry_queue, cycle_start_time_us, &ble_data, 0) == pdTRUE) {
                            ble_data.flag.data_need_process = 1;
                        }
                        xQueueSend(ble_retry_queue, &send_queue_data, 0);
                    }
                }
                if ((my_cfg_wifi_mode.data.u8 & MY_WIFI_MODE_ESPNOW) && send_queue_data.flag.send_to_espnow) {
                    if (xQueueSend(espnow_retry_queue, &send_queue_data, 0) != pdTRUE) {
                        if (my_kb_queue_pop_until_valid(espnow_retry_queue, cycle_start_time_us, &espnow_data, 0) == pdTRUE) {
                            espnow_data.flag.data_need_process = 1;
                        }
                        xQueueSend(espnow_retry_queue, &send_queue_data, 0);
                    }
                }
            };
        }

        // 依次发送usb、ble、espnow数据
        cycle_start_time_us = my_get_time();
        if (my_cfg_usb.data.u8) {
            if (usb_data.flag.data_need_process && // 上面可能因为队列已满，将数据取出了，这里再检查下数据是否过期
                !my_kb_queue_data_is_valid(cycle_start_time_us, &usb_data, 1)) {
                usb_data.flag.data_need_process = 0;
            }
            if (!usb_data.flag.data_need_process && // 如果没取出或刚才检查到数据已经过期，这里尝试取出数据
                my_kb_queue_pop_until_valid(usb_retry_queue, cycle_start_time_us, &usb_data, 0) == pdTRUE) {
                usb_data.flag.data_need_process = 1;
            }
            if (usb_data.flag.data_need_process) {
                usb_data.flag.data_need_process = 0;
                any_report_send++;
                bool send_ok = 0;
                if (usb_data.flag.report_type == 0) {
                    send_ok = my_hid_send_keyboard_report_raw(&usb_data.kb_report);
                }
                else if (usb_data.flag.report_type == 1) {
                    send_ok = my_hid_send_consumer_report_raw(&usb_data.cons_report);
                }
                if (!send_ok) { // 如果报告没有成功发送，将数据重新放回队头
                    xQueueSendToFront(usb_retry_queue, &usb_data, 0);
                }
            }
        }
        if (my_cfg_ble.data.u8) {
            if (ble_data.flag.data_need_process &&
                !my_kb_queue_data_is_valid(cycle_start_time_us, &ble_data, 1)) {
                ble_data.flag.data_need_process = 0;
            }
            if (!ble_data.flag.data_need_process &&
                my_kb_queue_pop_until_valid(ble_retry_queue, cycle_start_time_us, &ble_data, 0) == pdTRUE) {
                ble_data.flag.data_need_process = 1;
            }
            if (ble_data.flag.data_need_process) {
                ble_data.flag.data_need_process = 0;
                any_report_send++;
                bool send_ok = 0;
                if (ble_data.flag.report_type == 0) {
                    send_ok = (my_ble_hidd_send_report(ble_data.kb_report.report_id, &ble_data.kb_report.modifier, my_keyboard_report_size) == ESP_OK);
                }
                else if (ble_data.flag.report_type == 1) {
                    send_ok = (my_ble_hidd_send_report(ble_data.cons_report.report_id, &ble_data.cons_report.consumer_code16, my_consumer_report_size) == ESP_OK);
                }
                if (!send_ok) {
                    xQueueSendToFront(ble_retry_queue, &ble_data, 0);
                }
            }
        }
        if (my_cfg_wifi_mode.data.u8 & MY_WIFI_MODE_ESPNOW) {
            if (espnow_data.flag.data_need_process &&
                !my_kb_queue_data_is_valid(cycle_start_time_us, &espnow_data, 1)) {
                espnow_data.flag.data_need_process = 0;
            }
            if (!espnow_data.flag.data_need_process &&
                my_kb_queue_pop_until_valid(espnow_retry_queue, cycle_start_time_us, &espnow_data, 0) == pdTRUE) {
                espnow_data.flag.data_need_process = 1;
            }
            if (espnow_data.flag.data_need_process) {
                espnow_data.flag.data_need_process = 0;
                any_report_send++;
                bool send_ok = 0;
                if (ble_data.flag.report_type == 0) {
                    send_ok = (my_espnow_send_hid_input_report((uint8_t *)(&espnow_data.kb_report), espnow_data.report_data_size) == ESP_OK);
                }
                else if (ble_data.flag.report_type == 1) {
                    send_ok = (my_espnow_send_hid_input_report((uint8_t *)(&espnow_data.cons_report), espnow_data.report_data_size) == ESP_OK);
                }
                if (!send_ok) {
                    xQueueSendToFront(espnow_retry_queue, &espnow_data, 0);
                }
            }
        }
        // 处理完后，所有需要发送的数据都应该放回队列里
        if (any_report_send) {
            vTaskDelay(pdMS_TO_TICKS(1));
            any_report_send = 0;
        }
        if (
            uxQueueMessagesWaiting(espnow_retry_queue) > 0 ||
            uxQueueMessagesWaiting(ble_retry_queue) > 0 ||
            uxQueueMessagesWaiting(usb_retry_queue) > 0) {
            queue_wait_time = pdMS_TO_TICKS(1);
        }
        else {
            queue_wait_time = portMAX_DELAY;
        }
    }

    if (my_send_report_queue) {
        vQueueDelete(my_send_report_queue);
    }
    vQueueDelete(usb_retry_queue);
    vQueueDelete(ble_retry_queue);
    vQueueDelete(espnow_retry_queue);
    vTaskDelete(NULL);
}

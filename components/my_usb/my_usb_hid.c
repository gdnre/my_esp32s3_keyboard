// todo：实现write报告方法，最好由keyboard组件去实现，让ble hid也能实现功能
// 利用键盘报告队列和按键更改掩码，掩码控制是否可操作改位
// 添加或删除按键时，始终从队尾报告开始查找，如果该位没有锁定，查找之前的报告直到队头，修改最新没有锁定的报告中的该按键，并将位设置为锁定，如果该位锁定，检查该位的值是否和待修改值相同，相同则不操作，不同则在队列中复制一份队尾报告，修改该位，并将新报告的该位锁定。
// 发送报告时，始终发送队头的报告，在报告发送完成回调中检查队列中队头队尾位置是否相同，不同则队头后移1，并发送队头报告

#include "class/hid/hid_device.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "my_usb.h"
#include "my_usbd_descriptor.h"
#include "sdkconfig.h"
#include "tinyusb.h"
#include "tusb_config.h"
#include <stdlib.h>

#include "my_usb_hid.h"

static const char *TAG = "my usb hid";

my_keyboard_report_t my_keyboard_report = {
    .report_id = MY_REPORTID_KEYBOARD,
};
uint16_t my_keyboard_report_size = sizeof(my_keyboard_report) - 1; // 实际收到的数据就是结构体大小，但传给usb hid的数据只要id以外的部分

my_consumer_report_t my_consumer_report = {
    .report_id = MY_REPORTID_CONSUMER,
};
uint16_t my_consumer_report_size = sizeof(my_consumer_report) - 1;

my_kb_led_report_t my_kb_led_report = {
    .report_id = MY_REPORTID_KEYBOARD,
    .raw = 0};
uint16_t my_kb_led_report_size = sizeof(my_kb_led_report); // 这是一个output报告，报告大小就是实际收到的数据大小

my_mouse_report_t my_mouse_report = {
    .report_id = MY_REPORTID_MOUSE,
};
uint16_t my_mouse_report_size = sizeof(my_mouse_report) - 1;

my_absmouse_report_t my_absmouse_report = {
    .report_id = MY_REPORTID_ABSMOUSE,
};
uint16_t my_absmouse_report_size = sizeof(my_absmouse_report) - 1;

uint8_t my_hid_instance = MY_HID_INSTANCE_INVALID;

void my_usb_hid_init()
{
    my_hid_instance = 0;
    // tud_hid_ready();
    if (tud_connected()) {
        my_usb_connected = 1;
    }
}

void my_usb_hid_deinit()
{
    my_hid_instance = MY_HID_INSTANCE_INVALID;
    my_usb_connected = 0;
}

bool my_usb_hid_send_report(uint8_t report_id, const void *report_data, uint16_t data_len)
{
    if (!my_usb_connected) {
        return false;
    }
    bool ret = tud_hid_n_report(my_hid_instance, report_id, report_data, data_len);
    return ret;
}

bool my_hid_send_keyboard_report_raw(my_keyboard_report_t *report)
{
    return my_usb_hid_send_report(report->report_id, &report->modifier, my_keyboard_report_size);
}

bool my_hid_send_consumer_report_raw(my_consumer_report_t *report)
{
    return my_usb_hid_send_report(report->report_id, &report->consumer_code16, my_consumer_report_size);
}

uint8_t my_hid_add_keycode_raw(uint8_t keycode)
{
    uint8_t ret;
    if (keycode >= HID_KEY_CONTROL_LEFT && keycode <= HID_KEY_GUI_RIGHT) {
        uint8_t mask = 1 << (keycode % 8);
        ret = my_keyboard_report.modifier & mask;
        my_keyboard_report.modifier |= mask;
    }
    else if (keycode <= 135) {
        uint8_t arr_num = keycode / 8;
        uint8_t _bit = keycode % 8;
        uint8_t mask = 1 << _bit;
        ret = my_keyboard_report.keycodes[arr_num] & mask;
        my_keyboard_report.keycodes[arr_num] |= mask;
    }
    else
        return 0;

    if (ret)
        return MY_HID_NO_NEED_OPERATE;
    else
        return 1;
}

uint8_t my_hid_remove_keycode_raw(uint8_t keycode)
{
    uint8_t ret;
    if (keycode >= HID_KEY_CONTROL_LEFT && keycode <= HID_KEY_GUI_RIGHT) {
        uint8_t mask = ~(1 << (keycode % 8));
        ret = my_keyboard_report.modifier | mask;
        my_keyboard_report.modifier &= mask;
    }
    else if (keycode <= 135) {
        uint8_t arr_num = keycode / 8;
        uint8_t _bit = keycode % 8;
        uint8_t mask = ~(1 << _bit);
        ret = my_keyboard_report.keycodes[arr_num] | mask; // 如果对应按键为按下状态，则ret会为0xFF
        my_keyboard_report.keycodes[arr_num] &= mask;
    }
    else
        return 0;

    if (ret != 0xFF)
        return MY_HID_NO_NEED_OPERATE;
    else
        return 1;
}

uint8_t my_hid_remove_keycode_all(uint8_t return_num, uint8_t simple_return)
{
    if (!return_num) {
        my_keyboard_report.modifier = 0;
        memset(my_keyboard_report.keycodes, 0, 17);
        return 1;
    }
    else if (simple_return) {
        uint8_t ret = 0;
        if (my_keyboard_report.modifier > 0) {
            ret++;
            my_keyboard_report.modifier = 0;
        }
        for (int i = 0; i < 17; i++) {
            if (my_keyboard_report.keycodes[i] > 0) {
                ret++;
                my_keyboard_report.keycodes[i] = 0;
            }
        }
        return ret;
    }
    else {
        uint8_t ret = 0;
        uint8_t temp;
        temp = my_keyboard_report.modifier;
        while (temp) {
            temp &= (temp - 1);
            ret++;
        }
        my_keyboard_report.modifier = 0;
        for (int i = 0; i < 17; i++) {
            temp = my_keyboard_report.keycodes[i];
            while (temp) {
                temp &= (temp - 1);
                ret++;
            }
            my_keyboard_report.keycodes[i] = 0;
        }
        return ret;
    }
}

uint8_t my_hid_add_consumer_code16(uint16_t consumer_code16)
{
    if (consumer_code16 > 0x03FF)
        return 0;
    if (consumer_code16 == my_consumer_report.consumer_code16) {
        return MY_HID_NO_NEED_OPERATE;
    }
    my_consumer_report.consumer_code16 = consumer_code16;
    return 1;
}

uint8_t my_hid_remove_consumer_code()
{
    if (my_consumer_report.consumer_code16 == 0) {
        return MY_HID_NO_NEED_OPERATE;
    }
    my_consumer_report.consumer_code16 = 0;
    return 1;
}

uint8_t my_hid_report_add_mouse_button(uint8_t code8)
{
    uint8_t ret;
    if (code8 >= MY_MOUSE_BUTTON_HID_CODE_MIN && code8 < MY_MOUSE_BUTTON_HID_CODE_NUM) {
        uint8_t mask = 1 << (code8 - MY_MOUSE_BUTTON_HID_CODE_MIN);
        ret = my_mouse_report.button_raw_value & mask;
        my_mouse_report.button_raw_value |= mask;
    }
    else {
        return 0;
    }

    if (ret)
        return MY_HID_NO_NEED_OPERATE;
    else
        return 1;
}

uint8_t my_hid_report_remove_mouse_button(uint8_t code8)
{
    uint8_t ret;
    if (code8 >= MY_MOUSE_BUTTON_HID_CODE_MIN && code8 < MY_MOUSE_BUTTON_HID_CODE_NUM) {
        uint8_t mask = ~(1 << (code8 - MY_MOUSE_BUTTON_HID_CODE_MIN));
        ret = my_mouse_report.button_raw_value | mask;
        my_mouse_report.button_raw_value &= mask;
    }
    else {
        return 0;
    }

    if (ret != 0xFF)
        return MY_HID_NO_NEED_OPERATE;
    else
        return 1;
}

uint8_t my_hid_report_remove_mouse_button_all(uint8_t return_num)
{
    if (my_mouse_report.button_raw_value == 0) {
        return MY_HID_NO_NEED_OPERATE;
    }
    my_mouse_report.button_raw_value = 0;
    return 1;
}

uint8_t my_hid_report_set_mouse_pointer_value8(my_mouse_value_type_t target, int8_t value)
{
    if (value > 127 || value < -127) {
        return 0;
    }
    int8_t *target_ptr = NULL;
    switch (target) {
        case MY_MOUSE_VALUE_X:
            target_ptr = &my_mouse_report.x;
            break;
        case MY_MOUSE_VALUE_Y:
            target_ptr = &my_mouse_report.y;
            break;
        case MY_MOUSE_VALUE_WHEEL_V:
            target_ptr = &my_mouse_report.wheel_vertical;
            break;
        case MY_MOUSE_VALUE_WHEEL_H:
            target_ptr = &my_mouse_report.wheel_horizontal;
            break;
        default:
            return 0;
            break;
    }
    if (*target_ptr == value) {
        return MY_HID_NO_NEED_OPERATE;
    }
    *target_ptr = value;
    return 1;
};

uint8_t my_hid_report_set_mouse_pointer_value16(my_mouse_value_type_t target, uint16_t value)
{
    if (value > 0x7FFF) {
        return 0;
    }
    uint16_t *target_ptr = NULL;
    switch (target) {
        case MY_MOUSE_VALUE_ABS_X:
            target_ptr = &my_absmouse_report.x;
            break;
        case MY_MOUSE_VALUE_ABS_Y:
            target_ptr = &my_absmouse_report.y;
            break;
        default:
            return 0;
            break;
    }
    if (*target_ptr == value) {
        return MY_HID_NO_NEED_OPERATE;
    }
    *target_ptr = value;
    return 1;
}

uint8_t my_hid_report_remove_mouse_pointer_value_all(uint8_t return_num)
{
    uint8_t ret = 0;
    if (!return_num) {
        my_mouse_report.x = 0;
        my_mouse_report.y = 0;
        my_mouse_report.wheel_vertical = 0;
        my_mouse_report.wheel_horizontal = 0;
        return 1;
    }
    else {
        if (my_mouse_report.x != 0) {
            my_mouse_report.x = 0;
            ret++;
        }
        if (my_mouse_report.y != 0) {
            my_mouse_report.y = 0;
            ret++;
        }
        if (my_mouse_report.wheel_vertical != 0) {
            my_mouse_report.wheel_vertical = 0;
            ret++;
        }
        if (my_mouse_report.wheel_horizontal != 0) {
            my_mouse_report.wheel_horizontal = 0;
            ret++;
        }
    }
    return ret;
}

uint8_t my_hid_report_sync_abs_mouse_report()
{
    my_absmouse_report.button_raw_value = my_mouse_report.button_raw_value;
    my_absmouse_report.wheel_vertical = my_mouse_report.wheel_vertical;
    my_absmouse_report.wheel_horizontal = my_mouse_report.wheel_horizontal;
    return 1;
}

#pragma region tinyusb回调函数
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    ESP_LOGD(__func__, "instance: %d, report_id: %d, report_type: %d, reqlen: %d", instance, report_id, report_type, reqlen);
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    if (instance == my_hid_instance && report_type == HID_REPORT_TYPE_OUTPUT) {
        if (bufsize >= 2 && buffer[0] == my_kb_led_report.report_id) {
            my_kb_led_report.raw = buffer[1];
            ESP_LOGD(TAG, "raw: %2x, num lock: %d, caps lock: %d, scroll lock: %d", my_kb_led_report.raw, my_kb_led_report.num_lock, my_kb_led_report.caps_lock, my_kb_led_report.scroll_lock);
            esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, 1, 0);
        }
    }
}

// Invoked when received SET_IDLE request. return false will stall the request
// - Idle Rate = 0 : only send report if there is changes, i.e. skip duplication
// - Idle Rate > 0 : skip duplication, but send at least 1 report every idle rate (in unit of 4 ms).
bool tud_hid_set_idle_cb(uint8_t instance, uint8_t idle_rate)
{
    return false;
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len)
{
    ESP_LOGD(__func__, "instance: %d, len: %d", instance, len);
    return;
}

void tud_hid_report_failed_cb(uint8_t instance, hid_report_type_t report_type, uint8_t const *report, uint16_t xferred_bytes)
{
    ESP_LOGD(__func__, "instance: %d, report_type: %d, xferred_bytes: %d", instance, report_type, xferred_bytes);
}

#pragma endregion
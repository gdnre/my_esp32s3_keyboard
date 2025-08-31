// todo：实现write报告方法，最好由keyboard组件去实现，让ble hid也能实现功能
// 利用键盘报告队列和按键更改掩码，掩码控制是否可操作改位
// 添加或删除按键时，始终从队尾报告开始查找，如果该位没有锁定，查找之前的报告直到队头，修改最新没有锁定的报告中的该按键，并将位设置为锁定，如果该位锁定，检查该位的值是否和待修改值相同，相同则不操作，不同则在队列中复制一份队尾报告，修改该位，并将新报告的该位锁定。
// 发送报告时，始终发送队头的报告，在报告发送完成回调中检查队列中队头队尾位置是否相同，不同则队头后移1，并发送队头报告

#include "my_usb_hid.h"
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

static const char *TAG = "my usb hid";

my_keyboard_report_t my_keyboard_report = {.report_id = MY_REPORTID_KEYBOARD};
uint16_t my_keyboard_report_size = sizeof(my_keyboard_report) - 1;

my_consumer_report_t my_consumer_report = {.report_id = MY_REPORTID_CONSUMER};
uint16_t my_consumer_report_size = sizeof(my_consumer_report) - 1;

my_kb_led_report_t my_kb_led_report = {
    .report_id = MY_REPORTID_KEYBOARD,
    .raw = 0};
uint16_t my_kb_led_report_size = sizeof(my_kb_led_report);


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

bool my_hid_send_keyboard_report_raw(my_keyboard_report_t *report)
{
    if (!my_usb_connected) {
        return false;
    }
    static bool ret;
    ret = tud_hid_n_report(my_hid_instance, report->report_id, &report->modifier, my_keyboard_report_size);
    return ret;
}

bool my_hid_send_consumer_report_raw(my_consumer_report_t *report)
{
    if (!my_usb_connected) {
        return false;
    }
    return tud_hid_n_report(my_hid_instance, report->report_id, &report->consumer_code16, my_consumer_report_size);
}

bool my_hid_send_keyboard_report()
{
    if (!my_usb_connected) {
        return false;
    }
    static bool ret;
    ret = tud_hid_n_report(my_hid_instance, MY_REPORTID_KEYBOARD, &my_keyboard_report.modifier, my_keyboard_report_size);
    return ret;
}

bool my_hid_send_consumer_report()
{
    if (!my_usb_connected) {
        return false;
    }
    return tud_hid_n_report(my_hid_instance, MY_REPORTID_CONSUMER, &my_consumer_report.consumer_code16, my_consumer_report_size);
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
        ret = my_keyboard_report.keycodes[arr_num] | mask;
        my_keyboard_report.keycodes[arr_num] &= mask;
    }
    else
        return 0;

    if (ret != 0xFF)
        return MY_HID_NO_NEED_OPERATE;
    else
        return 1;
}
// void my_hid_remove_key_bit(uint8_t bit_num)
// {
//     uint8_t arr_num = bit_num / 8;
//     if (arr_num >= 17)
//         return 0;
//     uint8_t _bit = bit_num % 8;
//     _bit = ~(1 << _bit);
//     my_keyboard_report.keycodes[arr_num] &= _bit;
// }
// void my_hid_add_key_bit_2_arr(uint8_t arr_num, uint8_t bit_offset)
// {
//     if (arr_num >= 17)
//         return 0;
//     my_keyboard_report.keycodes[arr_num] |= (1 << bit_offset);
// }
// void my_hid_remove_key_bit_2_arr(uint8_t arr_num, uint8_t bit_offset)
// {
//     if (arr_num >= 17)
//         return 0;
//     uint8_t mask = ~(1 << bit_offset);
//     my_keyboard_report.keycodes[arr_num] &= mask;
// }
// void my_hid_add_keys_2_arr(uint8_t arr_num, uint8_t keycode)
// {
//     if (arr_num >= 17)
//         return 0;
//     my_keyboard_report.keycodes[arr_num] |= keycode;
// }
// void my_hid_remove_keys_2_arr(uint8_t arr_num, uint8_t keycode)
// {
//     if (arr_num >= 17)
//         return 0;
//     uint8_t mask = ~keycode;
//     my_keyboard_report.keycodes[arr_num] &= mask;
// }

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

uint8_t my_hid_add_consumer_code(uint8_t consumer_code[2])
{
    uint16_t consumer_code16 = consumer_code[0] | (consumer_code[1] << 8);
    return my_hid_add_consumer_code16(consumer_code16);
}

uint8_t my_hid_remove_consumer_code()
{
    if (my_consumer_report.consumer_code16 == 0) {
        return MY_HID_NO_NEED_OPERATE;
    }
    my_consumer_report.consumer_code16 = 0;
    return 1;
}

uint8_t my_usb_keyboard_pressed(uint8_t keycode)
{
    uint8_t ret = my_hid_add_keycode_raw(keycode);
    my_hid_send_keyboard_report();
    return ret;
}

uint8_t my_usb_keyboard_released(uint8_t keycode)
{
    uint8_t ret = my_hid_remove_keycode_raw(keycode);
    my_hid_send_keyboard_report();
    return ret;
}

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

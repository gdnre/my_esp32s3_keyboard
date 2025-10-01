#pragma once
#include "my_usbd_descriptor.h"

#define MY_HID_INSTANCE_INVALID 0xFF
#define MY_HID_NO_NEED_OPERATE 0xFF

typedef enum {
    MY_MOUSE_VALUE_X,
    MY_MOUSE_VALUE_Y,
    MY_MOUSE_VALUE_WHEEL_V,
    MY_MOUSE_VALUE_WHEEL_H,
    MY_MOUSE_VALUE_ABS_X,
    MY_MOUSE_VALUE_ABS_Y,
} my_mouse_value_type_t;

extern my_keyboard_report_t my_keyboard_report;
extern uint16_t my_keyboard_report_size;

extern my_consumer_report_t my_consumer_report;
extern uint16_t my_consumer_report_size;

extern my_kb_led_report_t my_kb_led_report;
extern uint16_t my_kb_led_report_size;

extern my_mouse_report_t my_mouse_report;
extern uint16_t my_mouse_report_size;

extern my_absmouse_report_t my_absmouse_report;
extern uint16_t my_absmouse_report_size;

// 实际上只在内部使用，且作用只是指示设备已被初始化，不执行有意义的初始化代码
// 但其它函数依赖其设置的值
void my_usb_hid_init();
void my_usb_hid_deinit();

// 发送usb hid报告，报告数据为id以外的部分
bool my_usb_hid_send_report(uint8_t report_id, const void *report_data, uint16_t data_len);
// esp_err_t my_usb_hid_send_report_with_reason(uint8_t report_id, const void *report_data, uint16_t data_len);

bool my_hid_send_keyboard_report_raw(my_keyboard_report_t *report);
bool my_hid_send_consumer_report_raw(my_consumer_report_t *report);

// 发送usb hid报告，报告内容为默认的my_keyboard_report中内容
#define my_hid_send_keyboard_report() my_usb_hid_send_report(my_keyboard_report.report_id, &my_keyboard_report.modifier, my_keyboard_report_size)
// 发送usb hid报告，报告内容为默认的my_consumer_report中内容
#define my_hid_send_consumer_report() my_usb_hid_send_report(my_consumer_report.report_id, &my_consumer_report.consumer_code16, my_consumer_report_size)

/// @brief 将按键加入键盘报文
/// @param keycode 单个按键的键码
/// @return 如果按键非法返回0，如果按键之前不存在返回1，如果按键已存在返回MY_HID_NO_NEED_OPERATE
uint8_t my_hid_add_keycode_raw(uint8_t keycode);

uint8_t my_hid_remove_keycode_raw(uint8_t keycode);

/// @brief 将键盘报文清除为0，即释放所有按键
/// @param return_num 是否返回清除的按键数量，如果设为1，则会循环检查有多少按键之前被按下
/// @param simple_return 只检查之前是否有按键被按下，不检查具体数量，返回按键数组中有几个数组成员不为0，包括modifer按键
/// @return 如果return_num为0，始终返回1，如果return_num为1，simple_return为0，则返回之前按下的按键数量，如果simple_return为0，则返回按键数组中有几个数组成员不为0
uint8_t my_hid_remove_keycode_all(uint8_t return_num, uint8_t simple_return);

uint8_t my_hid_add_consumer_code16(uint16_t consumer_code16);

uint8_t my_hid_remove_consumer_code();

// 将对应的鼠标按键设置为1，即按下状态
uint8_t my_hid_report_add_mouse_button(uint8_t code8);
// 释放对应的鼠标按键
uint8_t my_hid_report_remove_mouse_button(uint8_t code8);
// 释放所有鼠标按键
// 如果之前有按键被按下返回1，否则返回MY_HID_NO_NEED_OPERATE
// 参数为保留参数，暂未使用
uint8_t my_hid_report_remove_mouse_button_all(uint8_t return_num);

// 设置鼠标移动量和滚轮数值这类范围在-127到127的数值
uint8_t my_hid_report_set_mouse_pointer_value8(my_mouse_value_type_t target, int8_t value);
// 设置绝对值鼠标移动量这类范围在0到0x7fff的数值
uint8_t my_hid_report_set_mouse_pointer_value16(my_mouse_value_type_t target, uint16_t value);
// 清除my_hid_report_set_mouse_pointer_value8会设置的值，不清除绝对值鼠标
uint8_t my_hid_report_remove_mouse_pointer_value_all(uint8_t return_num);
// 将鼠标报文x、y以外的值同步到绝对值鼠标报文
uint8_t my_hid_report_sync_abs_mouse_report();

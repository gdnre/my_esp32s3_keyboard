#pragma once
#include "my_usbd_descriptor.h"

#define MY_HID_INSTANCE_INVALID 0xFF
#define MY_HID_NO_NEED_OPERATE 0xFF

extern my_keyboard_report_t my_keyboard_report;
extern uint16_t my_keyboard_report_size;

extern my_consumer_report_t my_consumer_report;
extern uint16_t my_consumer_report_size;

extern my_kb_led_report_t my_kb_led_report;
extern uint16_t my_kb_led_report_size;

// 实际上只在内部使用，且作用只是指示设备已被初始化，不执行有意义的初始化代码
// 但其它函数依赖其设置的值
void my_usb_hid_init();

void my_usb_hid_deinit();

bool my_hid_send_keyboard_report_raw(my_keyboard_report_t *report);

bool my_hid_send_consumer_report_raw(my_consumer_report_t *report);

// 发送usb hid报告，报告内容为默认的my_keyboard_report中内容
bool my_hid_send_keyboard_report();

// 发送usb hid报告，报告内容为默认的my_consumer_report中内容
bool my_hid_send_consumer_report();

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

uint8_t my_usb_keyboard_pressed(uint8_t keycode);

uint8_t my_usb_keyboard_released(uint8_t keycode);

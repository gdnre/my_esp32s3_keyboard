#pragma once
#include "esp_system.h"

RTC_DATA_ATTR extern uint8_t my_chip_mac[6];
RTC_DATA_ATTR extern uint8_t my_chip_mac_size;
RTC_DATA_ATTR extern uint16_t my_chip_mac16;
RTC_DATA_ATTR extern char my_chip_macstr_short[5];

// 初始化nvs存储，不会重复调用，失败终止运行
void my_nvs_init(void);

// 初始化默认事件循环，不会重复调用，失败终止运行
void my_esp_default_event_loop_init(void);

// 获取芯片mac信息，用于拼接成不重复的设备名
// 注意从深睡启动时该函数可能不会被重复执行
void my_get_chip_mac(void);

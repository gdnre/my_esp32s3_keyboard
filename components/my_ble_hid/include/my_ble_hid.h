#pragma once
#include "esp_err.h"

void my_ble_state_change_cb(uint8_t is_connected);
/// @brief 检查ble hid是否可用
/// @return 可用返回1，否则返回0，返回my_ble_hid_usable的值
uint8_t my_ble_hidd_usable_check();

/// @brief 设置电池电量，使用bluedroid时实际没生效
/// @param level 电池电量，0-100
/// @return
esp_err_t my_ble_hidd_batttery_level_set(uint8_t level);

/// @brief 发送ble hid报告
/// @param report_id 报告id
/// @param data 报告数据
/// @param length 报告长度
/// @return
esp_err_t my_ble_hidd_send_report(size_t report_id, uint8_t *data, size_t length);

/// @brief 启动ble和ble hid服务
/// @param
/// @return
esp_err_t my_ble_hid_start(void);

/// @brief 请使用my_ble_stop来停用ble，仅停止ble hid服务，不停止ble，实际上停止后ble也不可用
/// @param
/// @return
esp_err_t my_ble_hid_stop(void);

/// @brief 停止ble服务，如果失败，会直接报错重启
/// @param
/// @return
esp_err_t my_ble_stop(void);

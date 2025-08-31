#pragma once
#include "my_encoder_keys.h"
#include "my_gpio_keys.h"
#include "my_input_base.h"
#include "my_matrix_keys.h"

#define MY_KEY_SCAN_INTERVAL_MS 1 // 一次按键扫描结束到下次扫描开始的时间间隔

extern my_input_base_event_cb_t my_input_base_event_cbs[MY_INPUT_EVENT_NUM];

extern int64_t last_active_time;      // 请使用my_input_get_last_active_time_us()获取按键最后有活动的时间
extern uint8_t my_input_task_waiting; // 任务是否处于等待状态，结合last_active_time检查任务进入等待的时间，每次进入和退出等待之前，会更新一次last_active_time
extern int64_t input_task_enter_time_us;

// // 参考配置，调用后注册按键回调函数即可使用，注意freertos的tick频率可能需要设置为1000hz以上，要防止出现vtaskdelay(0)
// void my_input_key_begin();

// 注意，这个任务只要没有进入等待，my_input_not_active_over永远都会返回true
void my_input_key_task(void *arg);

int8_t my_input_key_config_init(my_input_matrix_config_t *matrix_conf, my_input_gpio_keys_config_t *gpio_conf, my_input_encoder_keys_config_t *encoder_conf);

// 如果要为特定gpio按键设置不同的active_level，需要在这个函数之前修改
int8_t my_input_key_io_init();

/// @brief 为按键事件注册回调函数
/// @param type 按键是gpio还是matrix， my_input_key_type_t类型
/// @param id 在type内的按键id
/// @param event 要注册的按键事件
/// @param cb 回调函数
/// @return
int8_t my_input_register_event_cb(uint8_t type, int16_t id, my_input_key_event_t event, my_input_key_event_cb_t cb);

/// @brief 为按键处理任务的特定时期注册回调，注意回调只能有一个，重复注册会覆盖之前的回调
/// @param type 始终应该为MY_INPUT_KEY_TYPE_BASE_AND_NUM
/// @param event
/// @param cb 要注册的回调，可以为null，为null时清除回调
/// @return
int8_t my_input_register_base_event_cb(uint8_t type, my_input_base_event_t event, my_input_base_event_cb_t cb);

// 返回最后按键有活动的时间
int64_t my_input_get_last_active_time_us();

/// @brief 检查没有按键活动是否超过sleep_time_us，不包括等于
/// @param sleep_time_us 传入的比较时间
/// @return
uint8_t my_input_not_active_over(int64_t sleep_time_us);

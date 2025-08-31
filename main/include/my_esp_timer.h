#pragma once
#include "esp_timer.h"
typedef struct
{
    esp_timer_handle_t handle;
    esp_timer_create_args_t *timer_args;
    uint8_t running;
    uint8_t is_periodic;
    uint64_t timeout_us;
} my_timer_handle_t;

typedef enum
{
    MY_FUNC_STOP = 0,
    MY_FUNC_START,
    MY_FUNC_RESTART,
    MY_FUNC_DELETE,
} my_func_run_type_t;

// 初始化电压检测adc引脚相关配置,同时创建一个定时器定时发送事件
// 每隔2s向my config事件池的默认event base发送MY_CFG_EVENT_TIMER_CHECK_VOLTAGE事件
// 也可作为一个通用的2s定时器使用
esp_err_t my_device_voltage_check_timer_run();

// 在MY_CFG_EVENT_TIMER_CHECK_VOLTAGE事件中(或其它时候,非线程安全),可以调用该函数获取电压
// 如果要在其它情况使用, 注意调用前必须保证已调用my_device_voltage_check_timer_run函数,本函数预期在对应事件中被调用,能发送事件即意味着已初始化,因此没有进行初始化检查
void my_device_voltage_check(int *out_bat_voltage, int *out_usb_voltage);

// esp_err_t my_usb_check_timer_run(my_func_run_type_t run_type, esp_timer_cb_t cb, void *arg);

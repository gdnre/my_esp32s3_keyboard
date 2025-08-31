#pragma once
#include "my_input_base.h"

typedef struct
{
    uint8_t key_num;
    int8_t active_level;
    uint8_t *key_pins;
    uint8_t pull_def;
} my_input_gpio_keys_config_t;


// _my_gpio_keys_handle_t类型的指针
typedef void* my_gpio_keys_handle_t;

/**
 * @brief 初始化GPIO按键配置，并非线程安全
 * 
 * 该函数用于初始化指定的GPIO按键配置，并创建一个GPIO按键处理句柄。
 * 
 * @param gpio_conf 指向GPIO按键配置的指针，包含按键相关的GPIO信息和配置，内容会被复制，用户不用保留配置信息，需要确保引脚数组长度等于引脚数
 * @param handle 指向GPIO按键处理句柄的指针，函数将通过此参数返回创建的句柄
 * @return int8_t 返回执行结果
 */
int8_t my_gpio_keys_config_init(my_input_gpio_keys_config_t *gpio_conf, my_gpio_keys_handle_t *handle);

/**
 * @brief 初始化GPIO按键输入输出，需要先执行gpio_install_isr_service()
 * 
 * 该函数用于初始化与按键相关的GPIO配置，以确保按键输入功能正常工作
 * 
 * @param handle 按键GPIO的句柄，用于操作特定的GPIO设备
 */
void my_gpio_keys_io_init(my_gpio_keys_handle_t handle);


/// @brief 依次检查并更新按键状态，执行回调
/// @param handle _my_gpio_keys_handle_t类型的指针
/// @return 0：没有按键被按下，1：有按键被按下
uint8_t check_gpio_keys(my_gpio_keys_handle_t handle);

/**
 * 注册gpio按键事件回调函数。需要在init函数之后调用
 * 
 * 本函数用于向系统注册一个gpio按键事件回调函数，当指定的按键事件发生时，系统将调用该回调函数。回调函数只能存在一个，后注册的会覆盖之前的回调，传入NULL时可以取消注册该事件。
 * 
 * @param id 按键的标识符，用于指定需要监控的按键。
 * @param event 按键事件类型，表明了按键的状态，如按下、松开等。
 * @param cb 回调函数指针，当指定的按键事件发生时，该函数将被调用。回调函数的参数1为触发的事件，参数2为my_key_info_t类型数据。
 * 
 * @return 函数执行结果的错误代码
 */
uint8_t my_gpio_key_register_event_cb(int16_t id, my_input_key_event_t event, my_input_key_event_cb_t cb);


void my_gpio_keys_intr_dis(my_gpio_keys_handle_t handle);
void my_gpio_keys_intr_en(my_gpio_keys_handle_t handle);
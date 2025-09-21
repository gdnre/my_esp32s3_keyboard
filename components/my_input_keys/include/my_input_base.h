#pragma once
#include "freertos/FreeRTOS.h"
#include "stdio.h"

#define MY_MAX_IN_PINS_NUM 12 // 应该大于等于输入引脚的数量

/**
 * @brief 定义输入设备的类型
 *
 * 此枚举类型用于区分不同的输入设备种类，包括GPIO按键、矩阵键盘和编码器。
 * 这些设备类型在初始化和配置输入设备时会被使用，以确定如何正确处理和读取输入。
 */
typedef enum {
    MY_INPUT_GPIO_KEY = 0,          // 每个io控制一个按键
    MY_INPUT_KEY_MATRIX,            // 矩阵键盘输入
    MY_INPUT_ENCODER,               // 编码器输入
    MY_INPUT_KEY_TYPE_BASE_AND_NUM, // 按键输入类型数量，也用于注册my_input_base_event_t回调
} my_input_key_type_t;

// 枚举类型，用于表示按键的状态
typedef enum {
    MY_KEY_IDLE = 0,       // 0000 按键处于空闲状态，包括释放瞬间和未按下状态
    MY_KEY_HOLD = 1,       // 0001 按键处于按下状态，包括按下瞬间和持续按下状态
    MY_KEY_RELEASED = 2,   // 0010 按键被释放瞬间，需要根据事件判断
    MY_KEY_PRESSED = 3,    // 0011 按键被按下瞬间，需要根据事件判断
    MY_KEY_LONGPRESSED = 5 // 0101 按键被长时间按下
} my_input_key_state_t;

/**
 * @brief 定义按键事件的枚举类型
 *
 * 这个枚举类型用于定义和区分不同的按键事件，包括按键释放、按键按下和长按开始事件。
 * 它主要用于事件处理和事件回调数组的索引。
 */
typedef enum {
    MY_KEY_RELEASED_EVENT = 0,         // 按键释放事件，按下并放开时触发
    MY_KEY_PRESSED_EVENT,              // 按键按下事件，按下瞬间触发
    MY_KEY_LONGPRESSED_START_EVENT,    // 长按开始事件
    MY_KEY_LONGPRESSED_HOLD_EVENT,     // 在长按时，每轮扫描时都会触发，不要连续执行复杂任务
    MY_KEY_LONGPRESSED_RELEASED_EVENT, // 从长按释放事件，如果这个事件没有注册回调，会默认执行按键释放回调，如果有长按释放回调，从长按释放时不会执行短按释放回调
    MY_KEY_EVENT_NUM                   // 事件数量，确保事件添加在这个之前，并且按顺序，不要随意修改，根据这最后一个枚举值建立事件回调数组
} my_input_key_event_t;

// 在按键检测之前之后调用的事件，事件的执行顺序按照如下枚举顺序
typedef enum {
    MY_INPUT_CYCLE_START_EVENT = 0, // 检测周期开始
    MY_INPUT_ENCODER_CHECK_START_EVENT,
    MY_INPUT_ENCODER_CHECK_END_EVENT,
    MY_INPUT_GPIO_CHECK_START_EVENT,
    MY_INPUT_GPIO_CHECK_END_EVENT,
    MY_INPUT_MATRIX_CHECK_START_EVENT,
    MY_INPUT_MATRIX_CHECK_END_EVENT,
    MY_INPUT_CYCLE_END_EVENT, // 检测周期结束
    MY_INPUT_EVENT_NUM
} my_input_base_event_t;

// 按键事件回调函数，对于my_input_key_event_t，arg为my_key_info_t类型指针
typedef void (*my_input_key_event_cb_t)(my_input_key_event_t event, void *arg);
// typedef void my_input(void *arg);

typedef void (*my_input_base_event_cb_t)(my_input_base_event_t event);

typedef struct {
    uint16_t *continue_cycle_ptr; // 用于控制是否强制继续任务的按键检测循环
    uint16_t *active_cycle_count_ptr;
    uint16_t *active_key_num_ptr;
} my_input_task_info_t;
extern my_input_task_info_t my_input_task_info;

typedef struct // 用于记录一个物理按键的信息
{
    int16_t id;             // 按键id，根据传入按键顺序依次分配，不同的按键type之间id独立
    uint8_t type;           // 按键类型my_input_key_type_t
    int8_t active_level;    // 按键按下时的电平
    uint8_t pull_def;       // 是否默认上下拉，根据按下时电平初始化成按键空闲状态电平
    uint8_t state;          // 按键状态my_input_key_state_t
    int64_t pressed_timer;  // 最后一次按下的时间点，如果当前为按下瞬间，则是上一次按下的时间点
    int64_t released_timer; // 最后一次释放的时间点，如果当前为释放瞬间，则是上一次释放的时间点
    struct
    {
        uint8_t layer_mask;    // 用于my_keyboard的数据，对于需要释放的按键，比如标准键盘按键，按下后应该锁定当前层，确保释放时fn层正确，按下时，将层设置为_current_layer，释放时设置为layer_mask
        uint8_t trigger_count; // 可以用于长按按住时，触发了多少次
    } user_data;
    my_input_key_event_cb_t event_cbs[MY_KEY_EVENT_NUM]; // 按键回调数组
} my_key_info_t;

extern TaskHandle_t my_input_task_handle;
extern int32_t long_pressed_time_us;

/// @brief 更新按键信息，切换按键状态，执行回调等
/// @param key 按键信息指针，init函数获得的handle中包含按键信息
/// @param level 当前按键状态，会与按键信息中的active_level进行比较
/// @return 返回当前按键是否被按下
uint8_t update_key_info(my_key_info_t *key, int8_t level);

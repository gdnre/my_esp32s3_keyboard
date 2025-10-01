#pragma once
#include "esp_err.h"
#include "my_input_base.h"

// todo：改为通过一个报告结构体管理，目前的实现不需要usb、蓝牙等连接类型报告改变的标识
typedef enum {
    MY_USB_KEYBOARD_REPORT_CHANGE = (1 << 0),
    MY_BLE_KEYBOARD_REPORT_CHANGE = (1 << 1),
    MY_ESPNOW_KEYBOARD_REPORT_CHANGE = (1 << 2),
    MY_KEYBOARD_REPORT_CHANGE = MY_USB_KEYBOARD_REPORT_CHANGE | MY_BLE_KEYBOARD_REPORT_CHANGE | MY_ESPNOW_KEYBOARD_REPORT_CHANGE,
    MY_USB_CONSUMER_REPORT_CHANGE = (1 << 3),
    MY_BLE_CONSUMER_REPORT_CHANGE = (1 << 4),
    MY_ESPNOW_CONSUMER_REPORT_CHANGE = (1 << 5),
    MY_CONSUMER_REPORT_CHANGE = MY_USB_CONSUMER_REPORT_CHANGE | MY_BLE_CONSUMER_REPORT_CHANGE | MY_ESPNOW_CONSUMER_REPORT_CHANGE,
    MY_USB_MOUSE_REPORT_CHANGE = (1 << 6),
    MY_BLE_MOUSE_REPORT_CHANGE = (1 << 7),
    MY_ESPNOW_MOUSE_REPORT_CHANGE = (1 << 8),
    MY_MOUSE_REPORT_CHANGE = MY_USB_MOUSE_REPORT_CHANGE | MY_BLE_MOUSE_REPORT_CHANGE | MY_ESPNOW_MOUSE_REPORT_CHANGE,
    MY_USB_ABSMOUSE_REPORT_CHANGE = (1 << 9),
    MY_BLE_ABSMOUSE_REPORT_CHANGE = (1 << 10),
    MY_ESPNOW_ABSMOUSE_REPORT_CHANGE = (1 << 11),
    MY_ABSMOUSE_REPORT_CHANGE = MY_USB_ABSMOUSE_REPORT_CHANGE | MY_BLE_ABSMOUSE_REPORT_CHANGE | MY_ESPNOW_ABSMOUSE_REPORT_CHANGE,
} my_hid_report_change_flag_t;

typedef enum {
    MY_KEYCODE_NONE,          // 未初始化，使用原始层的值
    MY_KEYBOARD_CODE,         // 按键值为原始键盘码
    MY_KEYBOARD_CHAR,         // 按键值为字符码
    MY_CONSUMER_CODE,         // 按键值为CONSUMER码
    MY_EMPTY_KEY,             // 按键值为空，按下后不进行任何操作，注意和NONE区分
    MY_FN_KEY,                // 按键为fn键
    MY_FN2_KEY,               // 按键为fn2键
    MY_FN_SWITCH_KEY,         // 按键为fn切换键
    MY_ESP_CTRL_KEY,          // 按键为一些控制键
    MY_KEYCODE_MOUSE_BUTTON,  // 鼠标上的按键，按键值使用hid_mouse_button_bm_t中的值
    MY_KEYCODE_MOUSE_X,       // 鼠标指针x方向相对移动，值为-127-127
    MY_KEYCODE_MOUSE_Y,       // y方向相对移动
    MY_KEYCODE_MOUSE_WHEEL_V, // 鼠标滚轮上下，值为-127-127
    MY_KEYCODE_MOUSE_WHEEL_H, // 鼠标滚轮左右
    MY_KEYCODE_MOUSE_ABS_X,   // 鼠标指针x方向绝对位置，值为0-0x7fff，会按比例映射到屏幕
    MY_KEYCODE_MOUSE_ABS_Y,   // y方向绝对位置
    MY_KEYCODE_COMBINE,       // 组合键，会同时按下，同时释放
    MY_KEYCODE_TYPE_NUM
} my_keycode_type_t; // 如果添加了新的按键类型，记得在下面的数组中补充名称字符串

#define MY_KEYCODE_TYPE_STR_ARR {       \
    "non", /*MY_KEYCODE_NONE*/          \
    " ",   /*MY_KEYBOARD_CODE*/         \
    " ",   /*MY_KEYBOARD_CHAR*/         \
    " ",   /*MY_CONSUMER_CODE*/         \
    " ",   /*MY_EMPTY_KEY*/             \
    "fn",  /*MY_FN_KEY*/                \
    "fn2", /*MY_FN2_KEY*/               \
    "fns", /*MY_FN_SWITCH_KEY*/         \
    " ",   /*MY_ESP_CTRL_KEY*/          \
    " ",   /*MY_KEYCODE_MOUSE_BUTTON*/  \
    "mpx", /*MY_KEYCODE_MOUSE_X*/       \
    "mpy", /*MY_KEYCODE_MOUSE_Y*/       \
    "whv", /*MY_KEYCODE_MOUSE_WHEEL_V*/ \
    "whh", /*MY_KEYCODE_MOUSE_WHEEL_H*/ \
    "amx", /*MY_KEYCODE_MOUSE_ABS_X*/   \
    "amy", /*MY_KEYCODE_MOUSE_ABS_Y*/   \
    "cmb", /*MY_KEYCODE_COMBINE*/       \
}

typedef enum {
    MY_ORIGINAL_LAYER = 0,
    MY_FN_LAYER,
    MY_FN2_LAYER,
    MY_TOTAL_LAYER
} my_fn_layer_t;

#define MY_COMBINE_KEYS_MAX_NUM 8
#define MY_COMBINE_KEYS_NAME_MAX_LEN 16

typedef struct
{
    uint8_t code_num;
    union {                                           // todo:统一其它类似的联合体中的成员名，以u作为无符号成员前缀
        uint8_t ucode8s[2 * MY_COMBINE_KEYS_MAX_NUM]; // 只支持hid按键码，保留兼容其它按键码的内存
        int16_t code16s[MY_COMBINE_KEYS_MAX_NUM];
    };
    char name[MY_COMBINE_KEYS_NAME_MAX_LEN];
} my_combine_key_t;

typedef struct
{
    uint8_t key_num;
    my_combine_key_t *key_arr_ptr;
} my_combine_keys_t;

extern my_combine_keys_t my_combine_keys;

typedef struct {
    uint8_t type;
    union {
        uint16_t code16;
        uint8_t code8;
        uint8_t code8s[2];
        int8_t s_code8;
        int16_t s_code16;
    };
} __packed my_kb_key_config_t;

extern uint8_t const my_ascii2keycode_arr[128][2];

extern uint16_t my_hid_report_change;
RTC_DATA_ATTR extern uint8_t _current_layer;
// 开启切换fn时，如果按下fn，触发原始层，默认为fn层，保存到rtc内存中，当深睡唤醒时保存状态，重启时恢复默认
RTC_DATA_ATTR extern uint8_t _swap_fn;
RTC_DATA_ATTR extern my_kb_key_config_t *my_kb_keys_config[MY_TOTAL_LAYER][MY_INPUT_KEY_TYPE_BASE_AND_NUM];
extern uint16_t my_kb_gpio_keys_num;
extern uint16_t my_kb_encoder_keys_num;
extern uint16_t my_kb_matrix_keys_num;
extern uint16_t *my_kb_keys_num_arr[MY_INPUT_KEY_TYPE_BASE_AND_NUM];
// 初始化按键io，为按键注册键盘事件等，应该始终运行
esp_err_t my_keyboard_init(void);

// 按键处理程序，根据传入的按键信息和按下状态处理按键，my_key_info_t在my_input组件定义，它应该是描述每个物理按键的结构体，每个类型的物理按键应该有唯一id，也为数组索引，根据该索引获得按键配置
void my_keyboard_key_handler(my_key_info_t *key_info, uint8_t key_pressed);

// 按键循环开始时，如果当前开机事件小于300，则直接将按键活动状态设置为1，让任务继续循环
// 现在按键通过其它任务发送，不依赖按键输入任务
void my_input_cycle_start_cb(my_input_base_event_t event);
// 每次按键循环结束时，如果有按键变化，发送键盘报告
void my_input_cycle_end_cb(my_input_base_event_t event);

// 按键状态改变（事件触发时），将按下的按键序号处理后发送给lvgl
void my_lvgl_key_state_event_handler(my_key_info_t *key_info, uint8_t key_pressed);

// 仅限接收器使用的按键初始化程序
esp_err_t my_receiver_key_init(void);

// extern uint8_t my_kb_send_report_task_waiting;
void my_kb_send_report_task(void *arg);

/// @brief 检查键盘发送任务是否正在运行，且按键活动是否超过sleep_time_us，不包括等于
/// @param sleep_time_us 传入的比较时间
/// @return 返回0表示不能睡眠，1表示可以睡眠
uint8_t my_keyboard_not_active_over(int64_t sleep_time_us);

#include "my_keyboard.h"
#include "class/hid/hid.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "my_ble_hid.h"
#include "my_config.h"
#include "my_input_keys.h"
#include "my_keyboard_define.h"
#include "my_usb.h"
#include "my_usb_hid.h"
#include "sdkconfig.h"
#include <stdio.h>

// #include "esp_lvgl_port.h"

static const char *TAG = "my_keyboard";
#pragma region lvgl用变量
char const *my_keycode_E0toE7_to_str_arr[8] = {
    "ctr",
    "shi",
    "alt",
    "gui",
    "ctr",
    "shi",
    "alt",
    "gui",
};

char const *my_keycode_28to2c_to_str_arr[5] = {
    "\xEF\xA2\xA2",
    "esc",
    "\xEF\x95\x9A",
    "tab",
    "spa",
};

char const *my_keycode_to_str_arr[] = {MY_HID_KEYCODE_TO_STRING};
uint16_t my_keycode_to_str_arr_len = sizeof(my_keycode_to_str_arr) / sizeof(char *);

// 用于lvgl显示按键的字符串map，每层的长度应该为按键数+行数
char *_matrix_str_map_0[MY_MATRIX_KEY_NUM + MY_ROW_NUM] = {NULL};
char *_matrix_str_map_1[MY_MATRIX_KEY_NUM + MY_ROW_NUM] = {NULL};
char *_matrix_str_map_2[MY_MATRIX_KEY_NUM + MY_ROW_NUM] = {NULL};

char *_encoder_str_map_0[3] = {NULL};
char *_encoder_str_map_1[3] = {NULL};
char *_encoder_str_map_2[3] = {NULL};

char **matrix_key_str_map[MY_TOTAL_LAYER] = {_matrix_str_map_0, _matrix_str_map_1, _matrix_str_map_2};
char **encoder_key_str_map[MY_TOTAL_LAYER] = {_encoder_str_map_0, _encoder_str_map_1, _encoder_str_map_2};
uint8_t my_fn_layer_num = MY_TOTAL_LAYER;
#pragma endregion lvgl用变量

#pragma region 按键配置快捷宏
uint8_t const my_ascii2keycode_arr[128][2] = {HID_ASCII_TO_KEYCODE};
uint8_t const my_keycode2ascii_arr[128][2] = {HID_KEYCODE_TO_ASCII};
#define ASCII_TO_HID_KEYCODE(x) (my_ascii2keycode_arr[(uint8_t)(x)][1])
#define ASCII_TO_HID_MODIFIER(x) (my_ascii2keycode_arr[(uint8_t)(x)][0])
#define MY_CHAR_CFG(x)                         \
    {                                          \
        .type = MY_KEYBOARD_CHAR,              \
        .code8s = { ASCII_TO_HID_KEYCODE(x),   \
                    ASCII_TO_HID_MODIFIER(x) } \
    }

#define MY_KEYCODE_CFG(x) {.type = MY_KEYBOARD_CODE, .code8 = x}
#define MY_CONSUMER_CFG(x) {.type = MY_CONSUMER_CODE, .code16 = x}
#define MY_FN_CFG {.type = MY_FN_KEY}
#define MY_FN2_CFG {.type = MY_FN2_KEY}
#define MY_FN_SW_CFG {.type = MY_FN_SWITCH_KEY}
#define MY_CTRL_CFG(x) {.type = MY_ESP_CTRL_KEY, .code16 = x}
#define MY_EMPTY_CFG {.type = MY_EMPTY_KEY}
#define MY_MOUSE_BUTTON_CFG(x) {.type = MY_KEYCODE_MOUSE_BUTTON, .code8 = x}
#define MY_MOUSE_X_CFG(x) {.type = MY_KEYCODE_MOUSE_X, .s_code8 = x}
#define MY_MOUSE_Y_CFG(x) {.type = MY_KEYCODE_MOUSE_Y, .s_code8 = x}
#define MY_MOUSE_WHEEL_V_CFG(x) {.type = MY_KEYCODE_MOUSE_WHEEL_V, .s_code8 = x}
#define MY_MOUSE_WHEEL_H_CFG(x) {.type = MY_KEYCODE_MOUSE_WHEEL_H, .s_code8 = x}

#define MY_RESTART_CFG MY_CTRL_CFG(MY_CFG_EVENT_RESTART_DEVICE)
#define MY_DEEP_SLEEP_START_CFG MY_CTRL_CFG(MY_CFG_EVENT_DEEP_SLEEP_START)
#define MY_SW_AP_CFG MY_CTRL_CFG(MY_CFG_EVENT_SWITCH_AP)
#define MY_SW_STA_CFG MY_CTRL_CFG(MY_CFG_EVENT_SWITCH_STA)
#define MY_SW_ESPNOW_CFG MY_CTRL_CFG(MY_CFG_EVENT_SWITCH_ESPNOW)
#define MY_SW_USB_CFG MY_CTRL_CFG(MY_CFG_EVENT_SWITCH_USB)
#define MY_SW_BLE_CFG MY_CTRL_CFG(MY_CFG_EVENT_SWITCH_BLE)
#define MY_USE_MSC_CFG MY_CTRL_CFG(MY_CFG_EVENT_SET_BOOT_MODE_MSC)
#define MY_SW_SCREEN_CFG MY_CTRL_CFG(MY_CFG_EVENT_SWITCH_SCREEN)
#define MY_ESPNOW_PAIRING_CFG MY_CTRL_CFG(MY_CFG_EVENT_SET_ESPNOW_PAIRING)
#define MY_INCREASE_BRIGHTNESS_CFG MY_CTRL_CFG(MY_CFG_EVENT_INCREASE_BRIGHTNESS)
#define MY_SW_DEEP_SLEEP_EN_CFG MY_CTRL_CFG(MY_CFG_EVENT_SWITCH_SLEEP_EN)
#define MY_LOG_LEVEL_CFG MY_CTRL_CFG(MY_CFG_EVENT_SWITCH_LOG_LEVEL)
#define MY_ERASE_NVS_CFG MY_CTRL_CFG(MY_CFG_EVENT_ERASE_NVS_CONFIGS)
#define MY_SW_LV_SCR_CFG MY_CTRL_CFG(MY_CFG_EVENT_SWITCH_LVGL_SCREEN)
#define MY_SW_LED_MODE_CFG MY_CTRL_CFG(MY_CFG_EVENT_SWITCH_LED_MODE)
#define MY_INCREASE_LED_BRI_CFG MY_CTRL_CFG(MY_CFG_EVENT_SWITCH_LED_BRIGHTNESS)

#define MY_MOUSE_BTN_LEFT_CFG MY_MOUSE_BUTTON_CFG(MY_MOUSE_BUTTON_HID_CODE_LEFT)
#define MY_MOUSE_BTN_RIGHT_CFG MY_MOUSE_BUTTON_CFG(MY_MOUSE_BUTTON_HID_CODE_RIGHT)
#define MY_MOUSE_BTN_MIDDLE_CFG MY_MOUSE_BUTTON_CFG(MY_MOUSE_BUTTON_HID_CODE_MIDDLE)
#define MY_MOUSE_BTN_BACKWARD_CFG MY_MOUSE_BUTTON_CFG(MY_MOUSE_BUTTON_HID_CODE_BACKWARD)
#define MY_MOUSE_BTN_FORWARD_CFG MY_MOUSE_BUTTON_CFG(MY_MOUSE_BUTTON_HID_CODE_FORWARD)
#pragma endregion 按键配置快捷宏

#pragma region 组合键和宏按键配置
my_combine_key_t my_default_combine_key_arr[] = {
    {.code_num = 2, .ucode8s = {HID_KEY_CONTROL_LEFT, HID_KEY_C}, .name = "ctrl+c"},
    {.code_num = 2, .ucode8s = {HID_KEY_CONTROL_LEFT, HID_KEY_V}, .name = "ctrl+v"},
    {.code_num = 2, .ucode8s = {HID_KEY_CONTROL_LEFT, HID_KEY_X}, .name = "ctrl+x"},
    {.code_num = 2, .ucode8s = {HID_KEY_CONTROL_LEFT, HID_KEY_Z}, .name = "ctrl+z"},
    {.code_num = 2, .ucode8s = {HID_KEY_CONTROL_LEFT, HID_KEY_S}, .name = "ctrl+s"},
    {.code_num = 2, .ucode8s = {HID_KEY_CONTROL_LEFT, HID_KEY_A}, .name = "ctrl+a"},
    {.code_num = 2, .ucode8s = {HID_KEY_ALT_LEFT, HID_KEY_TAB}, .name = "alt+tab"},
    {.code_num = 2, .ucode8s = {HID_KEY_ALT_LEFT, HID_KEY_F4}, .name = "alt+f4"},
    {.code_num = 2, .ucode8s = {HID_KEY_GUI_LEFT, HID_KEY_D}, .name = "win+d"},
    {.code_num = 2, .ucode8s = {HID_KEY_GUI_LEFT, HID_KEY_L}, .name = "win+l"},
};
my_combine_keys_t my_combine_keys = {
    .key_num = sizeof(my_default_combine_key_arr) / sizeof(my_default_combine_key_arr[0]),
    .key_arr_ptr = my_default_combine_key_arr,
};
#pragma endregion

#pragma region 键盘按键配置
RTC_DATA_ATTR static my_kb_key_config_t _config_gpio_keys_0[1];
RTC_DATA_ATTR static my_kb_key_config_t _config_gpio_keys_1[1];
RTC_DATA_ATTR static my_kb_key_config_t _config_gpio_keys_2[1];

RTC_DATA_ATTR static my_kb_key_config_t _config_matrix_keys_0[MY_MATRIX_KEY_NUM] = {
    MY_KEYCODE_CFG(HID_KEY_DELETE), MY_EMPTY_CFG, MY_EMPTY_CFG, MY_CONSUMER_CFG(HID_USAGE_CONSUMER_PLAY_PAUSE), MY_EMPTY_CFG,
    MY_KEYCODE_CFG(HID_KEY_BACKSPACE), MY_KEYCODE_CFG(HID_KEY_KEYPAD_DIVIDE), MY_KEYCODE_CFG(HID_KEY_KEYPAD_MULTIPLY), MY_KEYCODE_CFG(HID_KEY_KEYPAD_SUBTRACT), MY_EMPTY_CFG,
    MY_KEYCODE_CFG(HID_KEY_KEYPAD_7), MY_KEYCODE_CFG(HID_KEY_KEYPAD_8), MY_KEYCODE_CFG(HID_KEY_KEYPAD_9), MY_FN2_CFG, MY_EMPTY_CFG,
    MY_KEYCODE_CFG(HID_KEY_KEYPAD_4), MY_KEYCODE_CFG(HID_KEY_KEYPAD_5), MY_KEYCODE_CFG(HID_KEY_KEYPAD_6), MY_KEYCODE_CFG(HID_KEY_KEYPAD_ADD), MY_EMPTY_CFG,
    MY_KEYCODE_CFG(HID_KEY_KEYPAD_1), MY_KEYCODE_CFG(HID_KEY_KEYPAD_2), MY_KEYCODE_CFG(HID_KEY_KEYPAD_3), MY_KEYCODE_CFG(HID_KEY_KEYPAD_ENTER), MY_EMPTY_CFG,
    MY_FN_CFG, MY_KEYCODE_CFG(HID_KEY_KEYPAD_0), MY_KEYCODE_CFG(HID_KEY_KEYPAD_DECIMAL), MY_KEYCODE_CFG(HID_KEY_KEYPAD_ENTER), MY_RESTART_CFG};
RTC_DATA_ATTR static my_kb_key_config_t _config_matrix_keys_1[MY_MATRIX_KEY_NUM] = {
    MY_KEYCODE_CFG(HID_KEY_DELETE), MY_EMPTY_CFG, MY_EMPTY_CFG, MY_CONSUMER_CFG(HID_USAGE_CONSUMER_MUTE), MY_EMPTY_CFG,
    MY_KEYCODE_CFG(HID_KEY_BACKSPACE), MY_KEYCODE_CFG(HID_KEY_KEYPAD_DIVIDE), MY_KEYCODE_CFG(HID_KEY_KEYPAD_MULTIPLY), MY_KEYCODE_CFG(HID_KEY_KEYPAD_SUBTRACT), MY_EMPTY_CFG,
    MY_MOUSE_BTN_BACKWARD_CFG, MY_MOUSE_Y_CFG(-5), MY_MOUSE_BTN_FORWARD_CFG, MY_FN2_CFG, MY_EMPTY_CFG,
    MY_MOUSE_X_CFG(-5), MY_MOUSE_Y_CFG(5), MY_MOUSE_X_CFG(5), MY_MOUSE_WHEEL_V_CFG(1), MY_EMPTY_CFG,
    MY_MOUSE_BTN_LEFT_CFG, MY_MOUSE_BTN_RIGHT_CFG, MY_MOUSE_BTN_MIDDLE_CFG, MY_MOUSE_WHEEL_V_CFG(-1), MY_EMPTY_CFG,
    MY_FN_CFG, MY_MOUSE_WHEEL_H_CFG(-1), MY_MOUSE_WHEEL_H_CFG(1), MY_KEYCODE_CFG(HID_KEY_KEYPAD_ENTER), MY_KEYCODE_CFG(HID_KEY_NUM_LOCK)};
RTC_DATA_ATTR static my_kb_key_config_t _config_matrix_keys_2[MY_MATRIX_KEY_NUM] = {
    MY_KEYCODE_CFG(HID_KEY_DELETE), MY_EMPTY_CFG, MY_EMPTY_CFG, MY_CONSUMER_CFG(HID_USAGE_CONSUMER_PLAY_PAUSE), MY_EMPTY_CFG,
    MY_SW_USB_CFG, MY_SW_BLE_CFG, MY_SW_ESPNOW_CFG, MY_FN_SW_CFG, MY_EMPTY_CFG,
    MY_SW_SCREEN_CFG, MY_INCREASE_BRIGHTNESS_CFG, MY_USE_MSC_CFG, MY_FN2_CFG, MY_EMPTY_CFG,
    MY_SW_AP_CFG, MY_SW_STA_CFG, MY_SW_DEEP_SLEEP_EN_CFG, MY_DEEP_SLEEP_START_CFG, MY_EMPTY_CFG,
    MY_ESPNOW_PAIRING_CFG, MY_LOG_LEVEL_CFG, MY_ERASE_NVS_CFG, MY_SW_LV_SCR_CFG, MY_EMPTY_CFG,
    MY_FN_CFG, MY_SW_LED_MODE_CFG, MY_INCREASE_LED_BRI_CFG, MY_KEYCODE_CFG(HID_KEY_KEYPAD_ENTER), MY_RESTART_CFG};

RTC_DATA_ATTR static my_kb_key_config_t _config_encoder_keys_0[2] = {
    MY_CONSUMER_CFG(HID_USAGE_CONSUMER_VOLUME_DECREMENT),
    MY_CONSUMER_CFG(HID_USAGE_CONSUMER_VOLUME_INCREMENT)};
RTC_DATA_ATTR static my_kb_key_config_t _config_encoder_keys_1[2] = {
    MY_CONSUMER_CFG(HID_USAGE_CONSUMER_VOLUME_DECREMENT),
    MY_CONSUMER_CFG(HID_USAGE_CONSUMER_VOLUME_INCREMENT)};
RTC_DATA_ATTR static my_kb_key_config_t _config_encoder_keys_2[2] = {
    MY_CONSUMER_CFG(HID_USAGE_CONSUMER_SCAN_PREVIOUS),
    MY_CONSUMER_CFG(HID_USAGE_CONSUMER_SCAN_NEXT)};

// 存储按键配置的数组的数组的指针数组，共有my_fn_layer_t中成员个fn层，每个fn层中每种my_input_key_type_t类型对应数组的一个成员，也对应一种按键类型
my_kb_key_config_t *my_kb_keys_config[MY_TOTAL_LAYER][MY_INPUT_KEY_TYPE_BASE_AND_NUM] = {
    {_config_gpio_keys_0, _config_matrix_keys_0, _config_encoder_keys_0},
    {_config_gpio_keys_1, _config_matrix_keys_1, _config_encoder_keys_1},
    {_config_gpio_keys_2, _config_matrix_keys_2, _config_encoder_keys_2}};

uint16_t my_kb_gpio_keys_num = sizeof(_config_gpio_keys_0) / sizeof(_config_gpio_keys_0[0]);
uint16_t my_kb_matrix_keys_num = sizeof(_config_matrix_keys_0) / sizeof(_config_matrix_keys_0[0]);
uint16_t my_kb_encoder_keys_num = sizeof(_config_encoder_keys_0) / sizeof(_config_encoder_keys_0[0]);
uint16_t *my_kb_keys_num_arr[MY_INPUT_KEY_TYPE_BASE_AND_NUM] = {&my_kb_gpio_keys_num, &my_kb_matrix_keys_num, &my_kb_encoder_keys_num};

#pragma endregion 键盘按键配置

void my_key_pressed_cb(my_input_key_event_t event, void *arg)
{
    if (!arg) {
        return;
    }
    my_key_info_t *my_key = (my_key_info_t *)arg;
    my_keyboard_key_handler(my_key, 1);
    my_lvgl_key_state_event_handler(my_key, 1);
    return;
}

void my_key_released_cb(my_input_key_event_t event, void *arg)
{
    if (!arg) {
        return;
    }
    my_key_info_t *my_key = (my_key_info_t *)arg;
    my_keyboard_key_handler(my_key, 0);
    my_lvgl_key_state_event_handler(my_key, 0);
}

// uint8_t gpio_pins[] = {0};
// my_input_gpio_keys_config_t gpio_config = {
//     .key_num = 1,
//     .active_level = 0,
//     .key_pins = gpio_pins,
//     .key_num = sizeof(gpio_pins) / sizeof(gpio_pins[0])
// };

static uint8_t col_pins[] = {MY_COLS};
static uint8_t row_pins[] = {MY_ROWS};

static my_input_matrix_config_t matrix_config = {
    .active_level = MY_MATRIX_ACTIVE_LEVEL,
    .col_num = MY_COL_NUM,
    .row_num = MY_ROW_NUM,
    .col_pins = col_pins,
    .row_pins = row_pins,
    .switch_input_pins = MY_MATRIX_SWITCH_INPUT_PINS,
    .pull_def = MY_MATRIX_PULL_DEFAULT,
};
static my_input_encoder_keys_config_t encoder_conf = {
    .a_pin = MY_ENCODER_A_PIN,
    .b_pin = MY_ENCODER_B_PIN,
    .active_level = MY_ENCODER_ACTIVE_LEVEL,
    .change_direction = MY_ENCODER_CHANGE_DIRECTION,
    .pull_def = MY_ENCODER_PULL_DEFAULT};
TaskHandle_t my_kb_send_task_handle = NULL;
esp_err_t my_keyboard_init(void)
{
    my_input_key_config_init(&matrix_config, NULL, &encoder_conf);
    my_input_key_io_init();
    xTaskCreatePinnedToCore(my_kb_send_report_task, "kb send task", 4096, NULL, 4, &my_kb_send_task_handle, 1);
    my_input_register_base_event_cb(MY_INPUT_KEY_TYPE_BASE_AND_NUM, MY_INPUT_CYCLE_END_EVENT, my_input_cycle_end_cb);
    my_input_register_base_event_cb(MY_INPUT_KEY_TYPE_BASE_AND_NUM, MY_INPUT_CYCLE_START_EVENT, my_input_cycle_start_cb);
    for (size_t i = 0; i < 2; i++) {
        my_input_register_event_cb(MY_INPUT_ENCODER, i, MY_KEY_PRESSED_EVENT, my_key_pressed_cb);
        my_input_register_event_cb(MY_INPUT_ENCODER, i, MY_KEY_RELEASED_EVENT, my_key_released_cb);
    }
    for (size_t i = 0; i < MY_MATRIX_KEY_NUM; i++) {
        my_input_register_event_cb(MY_INPUT_KEY_MATRIX, i, MY_KEY_PRESSED_EVENT, my_key_pressed_cb);
        my_input_register_event_cb(MY_INPUT_KEY_MATRIX, i, MY_KEY_RELEASED_EVENT, my_key_released_cb);
    }
    // 任务栈空间要大于2k，2k可能会爆栈
    xTaskCreatePinnedToCore(my_input_key_task, "my_input_task", 4096, NULL, 3, &my_input_task_handle, 1);
    return ESP_OK;
}

#pragma region // 接收器用函数
// #if CONFIG_MY_DEVICE_IS_RECEIVER
void my_receiver_key_released_cb(my_input_key_event_t event, void *arg)
{
    if (!arg) {
        return;
    }
    // my_key_info_t *my_key = (my_key_info_t *)arg;
}

void my_receiver_key_pressed_cb(my_input_key_event_t event, void *arg)
{
    if (!arg) {
        return;
    }
    my_key_info_t *my_key = (my_key_info_t *)arg;
    int64_t now = esp_timer_get_time();
    // 计算这次按键按下的时间和上次按下的时间间隔
    int64_t time_gap = now - my_key->pressed_timer;
    // 如果时间间隔很短，可能是抖动，如果间隔很长，说明并非双击或以上
    if (time_gap >= 10000 && time_gap < 500000) {
        my_key->user_data.trigger_count++;
        if (my_key->user_data.trigger_count == 1) {
            // 如果是双击，则切换usb开关
            my_cfg_event_post(MY_CFG_EVENT_SWITCH_USB, NULL, 0, 0);
        }
        else if (my_key->user_data.trigger_count == 2) {
            // 如果是三击，将下次启动模式切换为msc模式，注意三击前一定会触发双击
            my_cfg_event_post(MY_CFG_EVENT_SET_BOOT_MODE_MSC, NULL, 0, 0);
        }
    }
    else {
        my_key->user_data.trigger_count = 0;
    }
}

void my_receiver_key_longpressed_cb(my_input_key_event_t event, void *arg)
{
    if (!arg) {
        return;
    }
    my_key_info_t *my_key = (my_key_info_t *)arg;
    switch (event) {
        case MY_KEY_LONGPRESSED_HOLD_EVENT:
            int64_t now = esp_timer_get_time();
            if (now - my_key->pressed_timer > 2000000 + 2000000 * my_key->user_data.trigger_count) {
                my_key->user_data.trigger_count++;
                if (my_key->user_data.trigger_count == 1) {
                    // 第一次触发长按，设置重新配对
                    my_cfg_event_post(MY_CFG_EVENT_SET_ESPNOW_PAIRING, NULL, 0, 0);
                }
            }
            break;
        default:
            break;
    }
}

esp_err_t my_receiver_key_init(void)
{
    uint8_t gpio_pins[] = {0};
    my_input_gpio_keys_config_t gpio_config = {
        .key_num = 1,
        .active_level = 0,
        .key_pins = gpio_pins,
        .pull_def = 1,
    };
    gpio_config.key_num = sizeof(gpio_pins) / sizeof(gpio_pins[0]);

    my_input_key_config_init(NULL, &gpio_config, NULL);
    my_input_key_io_init();
    // 任务栈空间要大于2k，2k可能会爆栈

    for (size_t i = 0; i < gpio_config.key_num; i++) {
        my_input_register_event_cb(MY_INPUT_GPIO_KEY, i, MY_KEY_PRESSED_EVENT, my_receiver_key_pressed_cb);
        my_input_register_event_cb(MY_INPUT_GPIO_KEY, i, MY_KEY_RELEASED_EVENT, my_receiver_key_released_cb);
        my_input_register_event_cb(MY_INPUT_GPIO_KEY, i, MY_KEY_LONGPRESSED_HOLD_EVENT, my_receiver_key_longpressed_cb);
    }
    xTaskCreatePinnedToCore(my_input_key_task, "input task", 4096, NULL, 1, &my_input_task_handle, 1);
    return ESP_OK;
}
// #endif // CONFIG_MY_DEVICE_IS_RECEIVER
#pragma endregion 接收器用函数

#pragma region lvgl显示相关

char *my_keycode_type_str_arr[] = MY_KEYCODE_TYPE_STR_ARR;
char *my_cfg_event_str_arr[] = MY_CFG_EVENT_STR_ARR;

// 根据key_cfg生成lvgl buttonmatrix组件使用的map
static esp_err_t my_key_cfg_to_str_handler(my_kb_key_config_t *key_cfg, char *out_str)
{
    if (!out_str || !key_cfg) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (key_cfg->type) {
        case MY_KEYBOARD_CODE: {
            if (key_cfg->code8 < my_keycode_to_str_arr_len) {
                snprintf(out_str, MY_KEY_STRING_MAP_MAX_LENGTH, "%s\n", my_keycode_to_str_arr[key_cfg->code8]);
            }
            else if (key_cfg->code8 >= HID_KEY_CONTROL_LEFT && key_cfg->code8 <= HID_KEY_GUI_RIGHT) {
                snprintf(out_str, MY_KEY_STRING_MAP_MAX_LENGTH, "%s\n", my_keycode_E0toE7_to_str_arr[key_cfg->code8 - HID_KEY_CONTROL_LEFT]);
            }
            else {
                snprintf(out_str, MY_KEY_STRING_MAP_MAX_LENGTH, " \n");
            }
        } break;
        case MY_KEYBOARD_CHAR: {
            snprintf(out_str, MY_KEY_STRING_MAP_MAX_LENGTH, "%c\n", key_cfg->code8 == 0 ? ' ' : key_cfg->code8);
        } break;
        case MY_CONSUMER_CODE: {
            const char *_str;
            switch (key_cfg->code16) {
                case HID_USAGE_CONSUMER_PLAY_PAUSE:
                    _str = "\xEF\x81\x8C";
                    break;
                case HID_USAGE_CONSUMER_SCAN_NEXT:
                    _str = "\xEF\x81\x91";
                    break;
                case HID_USAGE_CONSUMER_SCAN_PREVIOUS:
                    _str = "\xEF\x81\x88";
                    break;
                case HID_USAGE_CONSUMER_STOP:
                    _str = "\xEF\x81\x8D";
                    break;
                case HID_USAGE_CONSUMER_MUTE:
                    _str = "\xEF\x80\xA6x";
                    break;
                case HID_USAGE_CONSUMER_VOLUME_INCREMENT:
                    _str = "\xEF\x80\xA6+";
                    break;
                case HID_USAGE_CONSUMER_VOLUME_DECREMENT:
                    _str = "\xEF\x80\xA6-";
                    break;
                default:
                    _str = " ";
                    break;
            }
            snprintf(out_str, MY_KEY_STRING_MAP_MAX_LENGTH, "%s\n", _str);
        } break;
        case MY_ESP_CTRL_KEY: {
            if (key_cfg->code8 < MY_CFG_EVENT_SWITCH_MAX_NUM) {
                snprintf(out_str, MY_KEY_STRING_MAP_MAX_LENGTH, "%s\n", my_cfg_event_str_arr[key_cfg->code8]);
            }
            else {
                snprintf(out_str, MY_KEY_STRING_MAP_MAX_LENGTH, " \n");
            }
        } break;
        case MY_KEYCODE_NONE: { // 这里要检查对应位置原始层的配置
            snprintf(out_str, MY_KEY_STRING_MAP_MAX_LENGTH, " \n");
            return ESP_ERR_NOT_SUPPORTED;
        } break;
        case MY_KEYCODE_MOUSE_BUTTON: {
            snprintf(out_str, MY_KEY_STRING_MAP_MAX_LENGTH, "mo%d\n", key_cfg->code8);
        } break;
        case MY_FN_KEY:
        case MY_FN2_KEY:
        case MY_FN_SWITCH_KEY:
        default:
            if (key_cfg->type < MY_KEYCODE_TYPE_NUM) {
                snprintf(out_str, MY_KEY_STRING_MAP_MAX_LENGTH, "%s\n", my_keycode_type_str_arr[key_cfg->type]);
            }
            else {
                snprintf(out_str, MY_KEY_STRING_MAP_MAX_LENGTH, " \n");
            }
            break;
    }
    return ESP_OK;
}

esp_err_t my_keyboard_lv_str_map_init(void)
{
    static uint8_t inited = 0;
    if (inited) {
        return ESP_ERR_INVALID_STATE;
    }
    inited = 1;
    esp_err_t ret = ESP_OK;

    static char *end_of_line = "\n";
    static char *end_of_map = "";

    for (size_t l = 0; l < MY_TOTAL_LAYER; l++) {
        uint8_t current_col = 0;
        uint8_t current_row = 0;
        int16_t cfg_id = -1;
        for (size_t n = 0; n < MY_MATRIX_KEY_NUM + MY_ROW_NUM; n++) {
            current_row = n / (MY_COL_NUM + 1);
            current_col = n % (MY_COL_NUM + 1);

            if (current_col == MY_COL_NUM) {
                if (current_row == (MY_ROW_NUM - 1) || current_row == 0)
                    matrix_key_str_map[l][n] = end_of_map;
                else
                    matrix_key_str_map[l][n] = end_of_line;
            }
            else {
                matrix_key_str_map[l][n] = malloc(MY_KEY_STRING_MAP_MAX_LENGTH * sizeof(char));
                if (!matrix_key_str_map[l][n]) {
                    ESP_LOGE(TAG, "malloc lv_str_map failed");
                    return ESP_ERR_NOT_ALLOWED;
                }
                cfg_id = my_matrix_to_id(current_row, current_col, 0);
                if (cfg_id < 0) {
                    ESP_LOGE(TAG, "can't find matrix config from [row: %d,col: %d]", current_row, current_col);
                    free(matrix_key_str_map[l][n]);
                    return ESP_ERR_INVALID_ARG;
                }
                ret = my_key_cfg_to_str_handler(&my_kb_keys_config[l][MY_INPUT_KEY_MATRIX][cfg_id], matrix_key_str_map[l][n]);
                if (ret == ESP_ERR_NOT_SUPPORTED) {
                    my_key_cfg_to_str_handler(&my_kb_keys_config[0][MY_INPUT_KEY_MATRIX][cfg_id], matrix_key_str_map[l][n]);
                }
            }
        }
        for (size_t n = 0; n < 3; n++) {
            if (n >= 2) {
                encoder_key_str_map[l][n] = end_of_map;
            }
            else {
                encoder_key_str_map[l][n] = malloc(MY_KEY_STRING_MAP_MAX_LENGTH * sizeof(char));
                if (!encoder_key_str_map[l][n]) {
                    ESP_LOGE(TAG, "malloc lv_str_map failed");
                    return ESP_ERR_NO_MEM;
                }
                ret = my_key_cfg_to_str_handler(&my_kb_keys_config[l][MY_INPUT_ENCODER][n], encoder_key_str_map[l][n]);
                if (ret == ESP_ERR_NOT_SUPPORTED) {
                    my_key_cfg_to_str_handler(&my_kb_keys_config[0][MY_INPUT_ENCODER][n], encoder_key_str_map[l][n]);
                }
            }
        }
    }
    return ESP_OK;
}

#pragma endregion

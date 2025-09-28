#pragma once
#include "esp_event.h"
#include "esp_log.h"
// #include "esp_timer.h"
#include "cJSON.h"
#include "nvs.h"

#define MY_DEBUG_MODE 1
#if MY_DEBUG_MODE
#define MY_LOGI(format, ...) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#define MY_LOGW(format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#define MY_LOGE(format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#else
#define MY_LOGI(format, ...)
#define MY_LOGW(format, ...)
#define MY_LOGE(format, ...)
#endif

#define MY_BINARY_FILE_DECLARE(filename)                                        \
    extern const uint8_t filename##_start[] asm("_binary_" #filename "_start"); \
    extern const uint8_t filename##_end[] asm("_binary_" #filename "_end");     \
    const size_t filename##_size = (filename##_end - filename##_start);

// extern int64_t _my_t1, _my_t2;
// #define MY_EXECTIME_RECORD_START _my_t1 = esp_timer_get_time()
// #define MY_EXECTIME_RECORD_END _my_t2 = esp_timer_get_time()
// #define MY_EXECTIME_RECORD_LOGI(str) ESP_LOGI("EXECTIME", "%s: %lld us", str, _my_t2 - _my_t1)
/*
// #define MY_EXECTIME_LOGI(func)     \
//     _my_t1 = esp_timer_get_time(); \
//     func;                          \
//     _my_t2 = esp_timer_get_time(); \
//     ESP_LOGI("EXECTIME", "%s: %lld us", #func, _my_t2 - _my_t1)
*/

// 电池电量检测
#define MY_BAT_CHECK_EN_LEVEL 1
#define MY_BAT_CHECK_EN_PIN 46
#define MY_BAT_CHECK_PIN 5
// USB电压检测
#define MY_USB_VBUS_CHECK_PIN 4
#define MY_VOL_CHECK_PULLUP_RESISTOR 100
#define MY_VOL_CHECK_PULLDOWN_RESISTOR 20
#define MY_VOL_MEASURE_TO_ACTUAL_RATE (MY_VOL_CHECK_PULLUP_RESISTOR + MY_VOL_CHECK_PULLDOWN_RESISTOR) / MY_VOL_CHECK_PULLDOWN_RESISTOR

/*********************
 *      引脚定义
 *********************/
// 键盘矩阵
/*布局示意图            out
 左上角  col0   col1    col2    col3    col4
    row0                                空
    row1                                空
    row2                                空
in  row3                                空
    row4                                空
    row5                               exkey
*/
#define MY_COL0 40
#define MY_COL1 39
#define MY_COL2 38
#define MY_COL3 3
#define MY_COL4 0
#define MY_COLS MY_COL0, MY_COL1, MY_COL2, MY_COL3, MY_COL4
#define MY_COL_NUM 5

#define MY_ROW0 21
#define MY_ROW1 14
#define MY_ROW2 13
#define MY_ROW3 10
#define MY_ROW4 11
#define MY_ROW5 12
#define MY_ROWS MY_ROW0, MY_ROW1, MY_ROW2, MY_ROW3, MY_ROW4, MY_ROW5
#define MY_ROW_NUM 6

#define MY_MATRIX_KEY_NUM (MY_COL_NUM * MY_ROW_NUM)
#define MY_KEY_STRING_MAP_MAX_LENGTH 8 // 在lvgl中显示按键提示文字的最大长度，8假设其中最长有一个symbol，3个字符，一个换行符和一个字符串结束符

#define MY_MATRIX_ACTIVE_LEVEL 1
#define MY_MATRIX_SWITCH_INPUT_PINS 0
#define MY_MATRIX_PULL_DEFAULT 1

#define MY_ENCODER_A_PIN 1
#define MY_ENCODER_B_PIN 2
#define MY_ENCODER_ACTIVE_LEVEL 1
#define MY_ENCODER_PULL_DEFAULT 1
#define MY_ENCODER_CHANGE_DIRECTION 0

// 如果想在睡眠模式时通过vbus检测引脚唤醒设备，需要修改vbus检测引脚的分压电阻，使分压后大于电压大于阈值，此时还需要调整adc的衰减配置，以支持更大的量程（默认无衰减时，最大量程约小于1.1V）
//
#if MY_MATRIX_ACTIVE_LEVEL == MY_ENCODER_ACTIVE_LEVEL && MY_MATRIX_ACTIVE_LEVEL > 0
#define MY_SLEEP_WAKE_UP_IO_MASK ((1 << MY_USB_VBUS_CHECK_PIN) | (1 << MY_ROW0) | (1 << MY_ROW1) | (1 << MY_ROW2) | (1 << MY_ROW3) | (1 << MY_ROW4) | (1 << MY_ROW5) | (1 << MY_ENCODER_A_PIN) | (1 << MY_ENCODER_B_PIN))
#elif MY_MATRIX_ACTIVE_LEVEL == MY_ENCODER_ACTIVE_LEVEL
#define MY_SLEEP_WAKE_UP_IO_MASK ((1 << MY_ROW0) | (1 << MY_ROW1) | (1 << MY_ROW2) | (1 << MY_ROW3) | (1 << MY_ROW4) | (1 << MY_ROW5) | (1 << MY_ENCODER_A_PIN) | (1 << MY_ENCODER_B_PIN))
#else
#define MY_SLEEP_WAKE_UP_IO_MASK ((1 << MY_ROW0) | (1 << MY_ROW1) | (1 << MY_ROW2) | (1 << MY_ROW3) | (1 << MY_ROW4) | (1 << MY_ROW5))
#endif

#define MY_INA226_SDA 16
#define MY_INA226_SCL 15
#define MY_INA226_ADDR 0x40
// 采样电阻10mΩ
#define MY_INA226_SHUNT_RESISTANCE 0.01f
// 最大电流约等于80mv/采样电阻
#define MY_INA226_MAX_CURRENT 8.0f
// 226制造商id寄存器地址
#define MY_INA226_REG_MANUFACTURERID (0xFE)
// 226芯片id寄存器地址
#define MY_INA226_REG_DIEID (0xFF)

// LED引脚
// LED电源(控制亮度)，如果用ao3401，可能无法完全关闭，因为Vgsth为-0.4到-1.3
#define MY_LED_POWER 48
#define MY_LED_ON_LEVEL 0
// LED数据输入
#define MY_LED_DIN 47
#define MY_LED_COUNT 24

ESP_EVENT_DECLARE_BASE(MY_CONFIG_EVENTS);
ESP_EVENT_DECLARE_BASE(MY_STATE_CHANGE_EVENTS);

typedef enum {
    MY_WIFI_MODE_NULL = 0b0000,
    MY_WIFI_MODE_STA = 0b0001,
    MY_WIFI_MODE_AP = 0b0010,
    MY_WIFI_MODE_APSTA = 0b0011,
    MY_WIFI_MoDE_NAN = 0b0100,
    MY_ESP_WIFI_MODE_MAX,
    MY_WIFI_MODE_ESPNOW = 0b1000,
    MY_WIFI_MODE_APESPNOW = 0b1010,
    MY_WIFI_MODE_MAX,
} my_wifi_mode_t;

typedef enum {
    MY_FEATURE_USE = 0x01,       // 使用该功能
    MY_FEATURE_INITED = 0x02,    // 功能已被初始化
    MY_FEATURE_RUNNING = 0x04,   // 功能任务正在运行
    MY_FEATURE_CONNECTED = 0x08, // 功能已连接
} my_feature_state_t;

typedef struct
{
    float f;
    int raw;
} my_adc_value_t;

typedef struct
{
    float bus_vol;
    float shunt_vol;
    float current;
    float power;
} my_ina226_result_t;

#define MY_NUMLOCK_BIT (1 << 0)
#define MY_CAPSLOCK_BIT (1 << 1)
typedef struct
{
    my_feature_state_t usb_hid_state;
    my_feature_state_t ble_state;
    my_feature_state_t ap_state;
    my_feature_state_t sta_state;
    my_feature_state_t espnow_state;
    uint8_t fn_sw;
    uint8_t model_to_show;
    my_adc_value_t vbus_vol;
    my_adc_value_t bat_vol;
    my_feature_state_t ina226_state;
    my_ina226_result_t ina226_result;
    union {
        uint8_t raw;
        struct
        {
            uint8_t num_lock : 1;
            uint8_t caps_lock : 1;
            uint8_t scroll_lock : 1;
            uint8_t compose : 1;
            uint8_t kana : 1;
            uint8_t shift : 1;
            uint8_t unused : 2;
        };
    } kb_led;
} my_lvgl_widgets_info_t;

extern uint8_t my_app_main_is_return;

extern my_feature_state_t my_cfg_usb_hid_state;
extern my_feature_state_t my_cfg_ble_state;
extern my_feature_state_t my_cfg_ap_state;
extern my_feature_state_t my_cfg_sta_state;
extern my_feature_state_t my_cfg_display_state;
extern my_feature_state_t my_cfg_adc_state;
extern my_feature_state_t my_cfg_ina226_state;
extern my_feature_state_t my_cfg_espnow_state;
extern my_lvgl_widgets_info_t my_lv_info;

extern int my_vbus_vol;
extern int my_bat_vol;
extern my_ina226_result_t my_ina226_data;

extern my_feature_state_t my_cfg_fn_sw_state;
extern uint8_t my_cfg_kb_led_raw_state;

typedef enum {
    MY_CFG_EVENT_RESTART_DEVICE,
    MY_CFG_EVENT_DEEP_SLEEP_START,
    MY_CFG_EVENT_SET_BOOT_MODE_MSC,
    MY_CFG_EVENT_SWITCH_USB,
    MY_CFG_EVENT_SWITCH_BLE,
    MY_CFG_EVENT_SWITCH_STA,
    MY_CFG_EVENT_SWITCH_AP,
    MY_CFG_EVENT_SWITCH_ESPNOW,
    MY_CFG_EVENT_SWITCH_SCREEN, // 打开或关闭屏幕
    MY_CFG_EVENT_INCREASE_BRIGHTNESS,
    MY_CFG_EVENT_SWITCH_SLEEP_EN,
    MY_CFG_EVENT_SET_ESPNOW_PAIRING,
    MY_CFG_EVENT_SWITCH_LOG_LEVEL,
    MY_CFG_EVENT_ERASE_NVS_CONFIGS,
    MY_CFG_EVENT_SWITCH_LVGL_SCREEN,    // 相当于切换lvgl屏幕上显示的桌面
    MY_CFG_EVENT_SWITCH_LED_MODE,       // 切换led灯模式
    MY_CFG_EVENT_SWITCH_LED_BRIGHTNESS, // 循环增加led灯亮度，仅在单色模式下生效
    MY_CFG_EVENT_SWITCH_MAX_NUM,
    // 下面的事件不该被键盘使用
    MY_CFG_EVENT_TIMER_CHECK_VOLTAGE,
    MY_CFG_EVENT_TIMER_MAX_NUM,
} my_cfg_event_t;

#define MY_CFG_EVENT_STR_ARR {                      \
    "RST", /* MY_CFG_EVENT_RESTART_DEVICE */        \
    "DS",  /* MY_CFG_EVENT_DEEP_SLEEP_START */      \
    "MSC", /* MY_CFG_EVENT_SET_BOOT_MODE_MSC */     \
    "USB", /* MY_CFG_EVENT_SWITCH_USB */            \
    "BLE", /* MY_CFG_EVENT_SWITCH_BLE */            \
    "STA", /* MY_CFG_EVENT_SWITCH_STA */            \
    "AP",  /* MY_CFG_EVENT_SWITCH_AP */             \
    "NOW", /* MY_CFG_EVENT_SWITCH_ESPNOW */         \
    "SCR", /* MY_CFG_EVENT_SWITCH_SCREEN */         \
    "BRI", /* MY_CFG_EVENT_INCREASE_BRIGHTNESS */   \
    "SEN", /* MY_CFG_EVENT_SWITCH_SLEEP_EN */       \
    "PAI", /* MY_CFG_EVENT_SET_ESPNOW_PAIRING */    \
    "LOG", /* MY_CFG_EVENT_SWITCH_LOG_LEVEL */      \
    "ERA", /* MY_CFG_EVENT_ERASE_NVS_CONFIGS */     \
    "LVS", /* MY_CFG_EVENT_SWITCH_LVGL_SCREEN */    \
    "LED", /* MY_CFG_EVENT_SWITCH_LED_MODE */       \
    "LBR", /* MY_CFG_EVENT_SWITCH_LED_BRIGHTNESS */ \
    "ERR", /* MY_CFG_EVENT_SWITCH_MAX_NUM */        \
}

typedef enum {
    MY_BOOT_MODE_DEFAULT,
    MY_BOOT_MODE_MSC,
} my_cfg_boot_mode_t;

// 注意数据大小不能超过255B
typedef struct
{
    uint8_t size;
    uint8_t is_ptr;
    const char *name;
    union {
        void *ptr;
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
    } data;
} my_nvs_cfg_t;

typedef struct {
    uint8_t ioType;
    uint8_t layer;
    uint16_t size; // 包括头的总数据大小
    uint16_t crc;
    uint8_t data[0];
} __packed my_key_config_bin_data_t;

RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_boot_mode;      // 程序运行开始时设置为nvs中my_cfg_next_boot_mode的值，理论上不会存储进nvs
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_next_boot_mode; // 设置下次的启动模式
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_check_update;

RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_out_def_config; // 在启动时，输出默认配置到fat json

RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_usb;         // 启动时检查是否开启usb hid功能，要存储在nvs中
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_ble;         // 启动时检查是否开启ble hid功能，要存储在nvs中
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_use_display; // 启动时检查是否开启显示屏，要存储在nvs中，如果默认不开启，之后开启时可能需要时间加载
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_brightness;  // 屏幕亮度
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_wifi_mode;   // wifi功能，ap,sta,apsta,都是为了开启http修改配置，需要用到fat分区

RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_sleep_time;   // 无操作时，进入休眠的时间，单位为秒
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_sleep_enable; // 是否进入休眠
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_log_level;    // 日志等级

RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_lvScrIndex; // 当前要显示的屏幕桌面

RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_led_brightness;  // led的亮度,0-100%的百分比
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_led_mode;        // led的模式，要关闭则设置为关闭模式
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_led_color;       // led的颜色，仅在单色模式下有效，值为0xRRGGBB转换为10进制，注意rgb的值也影响亮度
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_led_temperature; // led的色温，0-1000000
RTC_DATA_ATTR extern my_nvs_cfg_t my_cfg_led_calibrate;   // led颜色的校准值

RTC_DATA_ATTR extern char my_bkImage0[256];
RTC_DATA_ATTR extern char my_bkImage1[256];

extern my_nvs_cfg_t *my_nvs_cfg_list_to_get[];
extern uint8_t my_nvs_cfg_list_to_get_size;
extern my_nvs_cfg_t *my_nvs_cfg_list_for_json[];
extern uint8_t my_nvs_cfg_list_for_json_size;

esp_err_t my_cfg_get_boot_mode(void);
esp_err_t my_cfg_get_nvs_configs(void);
esp_err_t my_cfg_erase_nvs_configs(void);
esp_err_t my_cfg_save_config_to_nvs(my_nvs_cfg_t *cfg);

/// @brief 从nvs中获取字符串，获取的字符串会用malloc分配内存，注意分配的内存大小会为out_len+1
/// @param key nvs的键
/// @param out_str 字符串指针的指针
/// @param out_len 字符串长度，包括\0
/// @return
/// @note 如果out_str为NULL，则不会分配内存，只返回字符串长度，out_len也可以为null，但不要两者都为null，会产生无谓的函数调用
esp_err_t my_cfg_get_nvs_str(const char *key, char **out_str, size_t *out_len);

/// @brief 从nvs中获取字符串，获取的字符串会通过out_buf返回
/// @param key nvs的键
/// @param out_buf 接收数据的缓冲区，缓冲区大小应该大于等于out_len+1
/// @param out_len 返回获取到的字符串长度，包括\0，可以为null
/// @param buf_size 缓冲区大小
/// @return
/// @note 如果out_buf为NULL且buf_size为0，则可以只返回字符串长度，out_len也可以为null，但不要两者都为null
esp_err_t my_cfg_get_nvs_str_static(const char *key, char *out_buf, size_t *out_len, size_t buf_size);

// 传入配置的cjson对象，自动配置nvs和按键配置，配置时会将有改变的nvs配置写入nvs，将按键配置以bin格式保存
esp_err_t my_json_cfgs_parse(cJSON *root, int *out_change_count, uint8_t *out_config_type);
// 从configIndex文件中读取json配置所在路径并进行配置
// 需要进行如下步骤：
// 1. 读取configIndex文件，从中获取配置所在路径
// 2. 从上面获取路径中读取json配置文件，配置读取后，还会保存至nvs或保存为bin文件
esp_err_t my_cfg_get_json_configs();

// 将当前配置转换为json字符串，两个参数分别指示是否转换nvs和键盘按键配置
// 0表示不转换，1/其它表示转换
// 注意返回的字符串使用后要用free删除
char *my_cfg_to_json_str(uint8_t convert_nvs, uint8_t convert_key);

// 将当前对应配置保存到对应路径下，传入null表示不保存对应配置，如果配置已存在，会被覆盖
esp_err_t my_cfg_save_cfgs_to_fat(const char *nvs_cfg_path, const char *key_cfg_path, const char *total_cfg_path);
// 将默认配置输出为json模板，会先检测my_cfg_out_def_config的值是否为1，再检测configTemplate.json、defCfgs.json文件是否存在，只会输出不存在的文件
esp_err_t my_cfg_save_configs_template_to_fat();
// 将当前的按键配置以bin格式保存到fatfs中，只应该在json配置读取后且有修改时被调用
esp_err_t my_key_configs_save_to_bin(uint16_t *out_write_bytes);
esp_err_t my_key_configs_load_from_bin();
// 创建config用的事件循环
void my_config_event_loop_init(void);

void my_config_event_loop_deinit(void);

/// @brief 向config事件循环发送事件
/// @param event_id
/// @param event_data
/// @param event_data_size
/// @param ticks_to_wait 如果事件队列已满，最多等待多久
/// @return
esp_err_t my_cfg_event_post(int32_t event_id, const void *event_data, size_t event_data_size, TickType_t ticks_to_wait);
esp_err_t my_cfg_event_handler_instance_register(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *instance);
esp_err_t my_cfg_event_handler_register(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);
esp_err_t my_cfg_event_handler_instance_unregister(int32_t event_id, esp_event_handler_instance_t instance);
esp_err_t my_cfg_event_handler_unregister(int32_t event_id, esp_event_handler_t event_handler);

/*键盘lvgl显示相关配置*/
typedef enum {
    MY_LV_WIDGET_KB_1ST_ROW = 0,
    MY_LV_WIDGET_KB_2ND_ROW,
    MY_LV_WIDGET_BLE,
    MY_LV_WIDGET_USB,
    MY_LV_WIDGET_STA,
    MY_LV_WIDGET_AP,
    MY_LV_WIDGET_ESP_NOW,
    MY_LV_WIDGET_LED_NUM,
    MY_LV_WIDGET_LED_CAP,
    MY_LV_WIDGET_FN_SW,
    MY_LV_WIDGET_SCR,
    MY_LV_WIDGET_VBUS_VOL,
    MY_LV_WIDGET_BAT_VOL,
    MY_LV_WIDGET_INA226,
    MY_LV_WIDGET_SCR1,
    MY_LV_WIDGET_TOTAL_NUM
} my_lv_widget_index_t;

extern char **matrix_key_str_map[3];  // 指针数组数量应该等于键盘中的MY_TOTAL_LAYER
extern char **encoder_key_str_map[3]; // 指针数组数量应该等于键盘中的MY_TOTAL_LAYER
extern uint8_t my_fn_layer_num;
esp_err_t my_keyboard_lv_str_map_init(void);

extern uint32_t LV_EVENT_MY_SET_FN_LAYER;
extern uint32_t LV_EVENT_MY_KEY_STATE_PRESS;
extern uint32_t LV_EVENT_MY_KEY_STATE_RELEASE;
extern uint32_t LV_EVENT_MY_WIDGET_STATE_CHANGE;
extern uint32_t LV_EVENT_MY_POP_MESSAGE;
uint8_t my_lv_buttonm_send_event(uint32_t event_code, void *param);
uint8_t my_lv_widget_state_change_send_event(uint8_t widget_index);
// 字符串必须为静态字符串
uint8_t my_lv_send_pop_message(char *pop_text);
uint8_t my_lvgl_is_running(void);
extern const char my_cfg_base_path[];
extern const char my_cfg_dataDir_path[];
extern const char configIndex_path[];
extern const char configTemplate_path[];
extern const char nvsCfgs_path[];
extern const char keyCfgs_path[];
extern const char totalCfgs_path[];
extern const char defCfgs_path[];
extern const char keyConfigsBin_path[];

RTC_DATA_ATTR extern uint32_t wake_up_count;
int64_t my_get_time();

// #define ROWPINS_BITMASK 0x207C00    //自己算
// #define RAPINS_BITMASK 0x02         //自己算

// // 屏幕引脚
// // 屏幕插在左侧时用注释掉的这个配置
// // #define LCD_SCL 9
// // #define LCD_SDA 8

// #define LCD_SCL 42
// #define LCD_SDA 41
// #define LCD_RES 18
// #define LCD_DC 7
// #define LCD_CS 17
// #define LCD_BL 6
// #define LCD_PINS {LCD_SCL, LCD_SDA, LCD_RES, LCD_DC, LCD_CS, LCD_BL}

// // 数字键和大小写锁定指示
// #define SIGN_LED_DIN 45
// #define SIGN_LED_COUNT 2

// // 其它扩展引脚
// // 串口
// // #define MY_RX0 44
// // #define MY_TX0 43
// #define MY_RX0 35
// #define MY_TX0 36

// // usb引脚,未引出
// // #define D正 20 //全速设备d+上拉
// // #define D负 19

// // R2模组多3个引脚，可以使用多出的引脚和226通信
// // #define INA226_SDA 36
// // #define INA226_SCL 35

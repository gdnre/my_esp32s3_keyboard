#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "soc/soc.h"
#include "soc/usb_reg.h"
#include "string.h"
#include "tinyusb.h"
// USB_GOTGCTL_REG //usb状态寄存器，如果tud库没法正确检测到设备断开，考虑检查这个寄存器
#include "driver/gpio.h"
#include "soc/gpio_reg.h"

#include "my_ble_hid.h"
#include "my_config.h"
#include "my_display.h"
#include "my_esp_timer.h"
#include "my_espnow.h"
#include "my_fat_mount.h"
#include "my_ina226.h"
#include "my_init.h"
#include "my_input_keys.h"
#include "my_keyboard.h"
#include "my_led_strips.h"
#include "my_ota.h"
#include "my_usb.h"
#include "my_usb_hid.h"
#include "my_wifi_start.h"

static const char *TAG = "main";
static int64_t my_timer = 0;
void my_wifi_switch_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void my_config_switch_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void my_hid_output_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

uint32_t wake_up_count = 0;
uint8_t my_app_main_is_return = 0;
int my_vbus_vol = 0;
int my_bat_vol = 0;
my_ina226_result_t my_ina226_data = {
    .bus_vol = 0,
    .current = 0,
    .power = 0,
    .shunt_vol = 0};

inline static void my_set_log_level(esp_log_level_t level)
{
    esp_log_set_level_master(level);
    esp_log_level_set("*", level);
}
void app_main(void)
{
    my_timer = esp_timer_get_time();
    wake_up_count++;
    wake_up_count = wake_up_count == 0 ? 2 : wake_up_count; // 如果溢出，设置为2
    esp_reset_reason_t reset_reason = esp_reset_reason();
    ESP_LOGD(TAG, "start reason: %d", reset_reason);
    uint8_t read_nvs_config = 1;

    if (reset_reason == ESP_RST_PANIC) {
        ESP_LOGE(TAG, "restart because of ESP_RST_PANIC");
        read_nvs_config = 0;
    }
    if (wake_up_count <= 1) {   // 如果不是从睡眠中启动，获取nvs中的配置
        my_cfg_get_boot_mode(); // 读取nvs中设置的启动模式

        if (my_cfg_boot_mode.data.u8 == MY_BOOT_MODE_MSC) {
            // MSC模式
            my_get_chip_mac();
            my_esp_default_event_loop_init();
            my_led_set_mode(BLINK_DOUBLE_RED);
            my_usb_start(MY_USBD_MSC);
            return;
        }

        // 默认模式
        ESP_ERROR_CHECK(my_mount_fat(NULL, NULL));
        if (my_cfg_check_update.data.u8) {
            // 检查更新
            my_backlight_set_level(0);
            if (my_fat_ota_start() == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(2000)); // 延迟2秒，没实际意义
                esp_restart();
            }
        }
        // 正式开始进入程序，先读取nvs配置
        if (read_nvs_config) {
            my_cfg_get_nvs_configs();
            my_set_log_level(my_cfg_log_level.data.u8);
            my_cfg_save_configs_template_to_fat();
            my_cfg_get_json_configs();
            my_key_configs_load_from_bin();
        }
        my_led_start(0);
    }
    else { // 从深睡启动
        // my_set_log_level(my_cfg_log_level.data.u8);
        my_set_log_level(ESP_LOG_ERROR); // 为了加快启动速度，深睡唤醒时先默认设置为ESP_LOG_ERROR
        ESP_ERROR_CHECK(my_mount_fat(NULL, NULL));
    }

    my_esp_default_event_loop_init();
    my_get_chip_mac();
    my_config_event_loop_init();
    my_cfg_fn_sw_state = _swap_fn;
#if CONFIG_MY_DEVICE_IS_RECEIVER
    my_set_log_level(ESP_LOG_ERROR);
    my_cfg_ble.data.u8 = 0;
    my_cfg_wifi_mode.data.u8 = MY_WIFI_MODE_ESPNOW;
    my_cfg_use_display.data.u8 = 0;
    my_cfg_usb.data.u8 = 1;
    my_receiver_key_init();
#else
    my_keyboard_init(); // 启动按键处理程序
#endif

    if (my_cfg_usb.data.u8) { // 启动usb
        if (my_usb_start(MY_USBD_HID) == ESP_OK) { my_cfg_usb_hid_state = MY_FEATURE_USE | MY_FEATURE_INITED; }
    }
    if (my_cfg_ble.data.u8) { // 启动ble
        if (my_ble_hid_start() == ESP_OK) { my_cfg_ble_state = MY_FEATURE_USE | MY_FEATURE_INITED; }
    }

    if (my_cfg_wifi_mode.data.u8) { // 启动wifi
        my_wifi_set_features(my_cfg_wifi_mode.data.u8);
        if (my_cfg_wifi_mode.data.u8 & MY_WIFI_MODE_ESPNOW) {
            my_cfg_espnow_state |= MY_FEATURE_USE;
            if (my_cfg_wifi_mode.data.u8 != MY_WIFI_MODE_ESPNOW) {
                my_cfg_wifi_mode.data.u8 = MY_WIFI_MODE_ESPNOW;
                my_cfg_save_config_to_nvs(&my_cfg_wifi_mode);
            }
        }
        else {
            if (my_cfg_wifi_mode.data.u8 & MY_WIFI_MODE_STA) { my_cfg_sta_state |= MY_FEATURE_USE; }
            if (my_cfg_wifi_mode.data.u8 & MY_WIFI_MODE_AP) { my_cfg_ap_state |= MY_FEATURE_USE; }
        }
    }

    if (my_cfg_use_display.data.u8) { // 启动lvgl
        my_display_start();
    }
    else {
        my_backlight_set_level(0);
    }

    my_cfg_event_handler_register(ESP_EVENT_ANY_ID, my_config_switch_event_handler, NULL);
#if !CONFIG_MY_DEVICE_IS_RECEIVER
    my_cfg_event_handler_register(MY_CFG_EVENT_SWITCH_AP, my_wifi_switch_event_handler, NULL);
    my_cfg_event_handler_register(MY_CFG_EVENT_SWITCH_STA, my_wifi_switch_event_handler, NULL);
    my_cfg_event_handler_register(MY_CFG_EVENT_SWITCH_ESPNOW, my_wifi_switch_event_handler, NULL);
#endif
    esp_event_handler_register(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, my_hid_output_event_handle, NULL);
    if (my_kb_led_report.raw) // 对于键盘，可能在lvgl启动前就收到了键盘指示灯报告，这种情况下需要手动发送一次，对于接收器没影响，可发可不发
        esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, sizeof(my_kb_led_report.raw), 0);
#if !CONFIG_MY_DEVICE_IS_RECEIVER
    if (my_cfg_usb_hid_state) {
        my_lv_widget_state_change_send_event(MY_LV_WIDGET_USB);
    }
    my_device_voltage_check_timer_run();
    my_ina226_task_start(3072, 1, 1);
#endif

    if (reset_reason == ESP_RST_DEEPSLEEP) {
        // 从深睡模式启动时，某些函数放到后面执行
        my_set_log_level(my_cfg_log_level.data.u8);
        my_led_start(0);
    }
    my_app_main_is_return = 1;

    vTaskDelay(pdMS_TO_TICKS(60000));
    esp_ota_mark_app_valid_cancel_rollback(); // 等待一分钟后，如果没有发生异常，说明该app可用于回滚（大概，至少能启动）
    return;
}

void my_deep_sleep_start()
{
    my_led_set_mode(BLINK_MAX);
    // 如果是自动触发的，没有任何影响，如果是手动触发的，检查下输出引脚是否会在深睡保持低电平
    esp_sleep_enable_ext1_wakeup_io(MY_SLEEP_WAKE_UP_IO_MASK, MY_MATRIX_ACTIVE_LEVEL > 0 ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
}

void my_wifi_switch_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    uint8_t target_mode = my_cfg_wifi_mode.data.u8;
    uint8_t lv_widget_index = 0xff;
    switch (event_id) {
        case MY_CFG_EVENT_SWITCH_STA:
            if (target_mode & MY_WIFI_MODE_ESPNOW) // 使用espnow时，不允许操作sta
            {
                my_lv_send_pop_message("please stop espnow");
                return;
            }
            else {
                if (target_mode & MY_WIFI_MODE_STA) // 如果当前启用了sta模式
                {
                    target_mode &= (~MY_WIFI_MODE_STA);
                    my_cfg_sta_state &= (~(MY_FEATURE_USE | MY_FEATURE_CONNECTED));
                }
                else // 如果当前没有启用sta模式
                {
                    target_mode |= MY_WIFI_MODE_STA;
                    my_cfg_sta_state |= MY_FEATURE_USE;
                }
                lv_widget_index = MY_LV_WIDGET_STA;
            }
            break;
        case MY_CFG_EVENT_SWITCH_AP:
            if (target_mode & MY_WIFI_MODE_ESPNOW) // 使用espnow时，不允许操作ap
            {
                my_lv_send_pop_message("please stop espnow");
                return;
            }
            else {
                if (target_mode & MY_WIFI_MODE_AP) {
                    target_mode &= (~MY_WIFI_MODE_AP);
                    my_cfg_ap_state &= (~(MY_FEATURE_USE | MY_FEATURE_CONNECTED));
                }
                else {
                    target_mode |= MY_WIFI_MODE_AP;
                    my_cfg_ap_state |= MY_FEATURE_USE;
                }
                lv_widget_index = MY_LV_WIDGET_AP;
            }
            break;
        case MY_CFG_EVENT_SWITCH_ESPNOW:
            if (target_mode & MY_WIFI_MODE_ESPNOW) // 如果之前已经启用了espnow
            {
                target_mode &= (~MY_WIFI_MODE_ESPNOW); // 关闭espnow模式
                my_cfg_espnow_state &= (~(MY_FEATURE_USE | MY_FEATURE_CONNECTED));
                if (!(my_cfg_usb_hid_state & MY_FEATURE_CONNECTED) && !(my_cfg_ble_state & MY_FEATURE_CONNECTED)) {
                    my_kb_led_report.raw = 0;
                    esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, 1, 0);
                }
            }
            else // 如果之前没启用espnow
            {
                target_mode = MY_WIFI_MODE_ESPNOW; // 打开espnow
                my_cfg_espnow_state |= (MY_FEATURE_USE);
                my_cfg_ap_state &= (~(MY_FEATURE_USE | MY_FEATURE_CONNECTED));
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_AP);
                my_cfg_sta_state &= (~(MY_FEATURE_USE | MY_FEATURE_CONNECTED));
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_STA);
            }
            lv_widget_index = MY_LV_WIDGET_ESP_NOW;
            break;
        default:
            break;
    }
    my_cfg_wifi_mode.data.u8 = target_mode;
    my_wifi_set_features(target_mode);
    my_cfg_save_config_to_nvs(&my_cfg_wifi_mode);
    my_lv_widget_state_change_send_event(lv_widget_index);
}

void my_config_switch_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id <= MY_CFG_EVENT_SWITCH_MAX_NUM) {
        switch (event_id) {
            case MY_CFG_EVENT_RESTART_DEVICE:
                if (my_cfg_next_boot_mode.data.u8 == MY_BOOT_MODE_MSC) {
                    my_lv_send_pop_message("ENTER MSC MODE");
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                esp_restart();
                break;
            case MY_CFG_EVENT_DEEP_SLEEP_START:
                my_lv_send_pop_message("deepsleep after 2s");
                vTaskDelay(pdMS_TO_TICKS(2000)); // 延迟2秒后进入深睡，要及时松开按键
                my_deep_sleep_start();
                break;
            case MY_CFG_EVENT_SWITCH_USB:
                my_cfg_usb.data.u8 = !my_cfg_usb.data.u8;
                if (my_cfg_usb.data.u8) {
                    my_usb_start(MY_USBD_HID);
                    my_cfg_usb_hid_state |= MY_FEATURE_USE;
                }
                else {
                    my_usb_start(MY_USBD_NULL);
                    my_cfg_usb_hid_state &= (~(MY_FEATURE_USE | MY_FEATURE_CONNECTED));
                    my_kb_led_report.raw = 0;
                    esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, 1, 0);
                }
                my_cfg_save_config_to_nvs(&my_cfg_usb);
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_USB);
                break;
            case MY_CFG_EVENT_SWITCH_BLE:
                my_cfg_ble.data.u8 = !my_cfg_ble.data.u8;
                if (my_cfg_ble.data.u8) {
                    my_ble_hid_start();
                    my_cfg_ble_state |= MY_FEATURE_USE;
                }
                else {
                    my_ble_stop();
                    my_cfg_ble_state &= (~(MY_FEATURE_USE | MY_FEATURE_CONNECTED));
                    if (!(my_cfg_usb_hid_state & MY_FEATURE_CONNECTED)) {
                        my_kb_led_report.raw = 0;
                        esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, 1, 0);
                    }
                }
                my_cfg_save_config_to_nvs(&my_cfg_ble);
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_BLE);
                break;
            case MY_CFG_EVENT_SET_BOOT_MODE_MSC:
                my_cfg_next_boot_mode.data.u8 = MY_BOOT_MODE_MSC;
                my_cfg_save_config_to_nvs(&my_cfg_next_boot_mode);
                my_lv_send_pop_message("restart to enter MSC mode");
                break;
            case MY_CFG_EVENT_INCREASE_BRIGHTNESS:
                if (my_cfg_use_display.data.u8) {
                    uint8_t duty = ((my_cfg_brightness.data.u8 / 10 + 1) * 10) % 110;
                    my_cfg_brightness.data.u8 = duty == 0 ? 10 : duty;
                    my_pwm_set_duty(my_cfg_brightness.data.u8);
                    my_cfg_brightness.data.u8 = my_pwm_get_duty();
                    my_cfg_save_config_to_nvs(&my_cfg_brightness);
                    static char bri_pop_text[20] = {0};
                    snprintf(bri_pop_text, sizeof(bri_pop_text), "Bri: %d", my_cfg_brightness.data.u8);
                    my_lv_send_pop_message(bri_pop_text);
                }
                break;
            case MY_CFG_EVENT_SWITCH_SCREEN:
                my_cfg_use_display.data.u8 = !my_cfg_use_display.data.u8;
                if (my_cfg_use_display.data.u8) {
                    my_display_start();
                }
                else {
                    my_display_stop();
                }
                my_cfg_save_config_to_nvs(&my_cfg_use_display);
                break;
            case MY_CFG_EVENT_SWITCH_LVGL_SCREEN:
                if (my_lv_switch_screen() == ESP_OK) {
                    my_cfg_save_config_to_nvs(&my_cfg_lvScrIndex);
                }
                break;
            case MY_CFG_EVENT_SWITCH_SLEEP_EN:
                my_cfg_sleep_enable.data.u8 = !my_cfg_sleep_enable.data.u8; // 不记录是否允许睡眠
                static char sleep_pop_text[20] = {0};
                snprintf(sleep_pop_text, sizeof(sleep_pop_text), "sleep en: %d", my_cfg_sleep_enable.data.u8);
                my_lv_send_pop_message(sleep_pop_text);
                break;
            case MY_CFG_EVENT_SET_ESPNOW_PAIRING:
                esp_err_t ret = my_espnow_re_pairing();
                if (ret == ESP_ERR_NOT_ALLOWED) {
                    my_lv_send_pop_message("espnow is stop");
                }
                else if (ret == ESP_ERR_INVALID_STATE) {
                    my_lv_send_pop_message("already set");
                }
                break;
            case MY_CFG_EVENT_SWITCH_LED_MODE:
                my_led_start(1);
                break;
            case MY_CFG_EVENT_SWITCH_LED_BRIGHTNESS:
                uint8_t led_bri = my_cfg_led_brightness.data.u8;
                led_bri = (led_bri + 20) % 120;
                my_led_set_brightness(led_bri);
                my_cfg_led_brightness.data.u8 = led_bri;
                my_cfg_save_config_to_nvs(&my_cfg_led_brightness);
                break;
            case MY_CFG_EVENT_SWITCH_LOG_LEVEL:
                uint8_t _log_level = (my_cfg_log_level.data.u8 + 1) % (CONFIG_LOG_MAXIMUM_LEVEL + 1);
                // _log_level = _log_level == 0 ? 1 : _log_level;
                my_cfg_log_level.data.u8 = _log_level;
                my_set_log_level(my_cfg_log_level.data.u8);
                my_cfg_save_config_to_nvs(&my_cfg_log_level);
                char *log_pop_text = NULL;
                switch (my_cfg_log_level.data.u8) {
                    case ESP_LOG_NONE:
                        log_pop_text = "log level: none";
                        break;
                    case ESP_LOG_ERROR:
                        log_pop_text = "log level: error";
                        ESP_LOGE(TAG, "%s", log_pop_text);
                        break;
                    case ESP_LOG_WARN:
                        log_pop_text = "log level: warn";
                        ESP_LOGW(TAG, "%s", log_pop_text);
                        break;
                    case ESP_LOG_INFO:
                        log_pop_text = "log level: info";
                        ESP_LOGI(TAG, "%s", log_pop_text);
                        break;
                    case ESP_LOG_DEBUG:
                        log_pop_text = "log level: debug";
                        ESP_LOGD(TAG, "%s", log_pop_text);
                        break;
                    case ESP_LOG_VERBOSE:
                        log_pop_text = "log level: verbose";
                        ESP_LOGV(TAG, "%s", log_pop_text);
                        break;
                    default:
                        break;
                }
                if (log_pop_text)
                    my_lv_send_pop_message(log_pop_text);
                break;

            case MY_CFG_EVENT_ERASE_NVS_CONFIGS:
                // todo
                static uint8_t trigger_count = 0;
                static char erase_pop_text[25] = {0};
                trigger_count++;
                if (trigger_count >= 5) {
                    trigger_count = 0;
                    my_cfg_erase_nvs_configs();
                    snprintf(erase_pop_text, sizeof(erase_pop_text), "erase nvs configs");
                    ESP_LOGI(TAG, "%s", erase_pop_text);
                    my_lv_send_pop_message(erase_pop_text);
                }
                else {
                    snprintf(erase_pop_text, sizeof(erase_pop_text), "enter %d times more", 5 - trigger_count);
                    ESP_LOGI(TAG, "%s", erase_pop_text);
                    my_lv_send_pop_message(erase_pop_text);
                }
                break;
            default:
                break;
        }
    }
    else {
        switch (event_id) {
            case MY_CFG_EVENT_TIMER_CHECK_VOLTAGE:
                my_device_voltage_check(&my_bat_vol, &my_vbus_vol);
                if (my_ina226_get_result(&my_ina226_data.bus_vol, &my_ina226_data.current, &my_ina226_data.power, &my_ina226_data.shunt_vol) == ESP_OK)
                    my_cfg_ina226_state |= (MY_FEATURE_CONNECTED | MY_FEATURE_USE);
                else
                    my_cfg_ina226_state = 0;

                my_lv_widget_state_change_send_event(MY_LV_WIDGET_VBUS_VOL);

                if (my_cfg_sleep_enable.data.u8 && my_vbus_vol < 3300) {
                    uint64_t sleep_time = my_cfg_sleep_time.data.u16 * 1000000;
                    if (my_input_not_active_over(sleep_time)) {
                        my_deep_sleep_start();
                    }
                }
                break;
            default:
                break;
        }
    }

    // ESP_LOGW(TAG, "free heap size: %ld", esp_get_free_heap_size());
}

void my_hid_output_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == MY_HID_OUTPUT_GET_REPORT_EVENT && event_data) {
        uint8_t led_raw = *((uint8_t *)event_data);
        my_cfg_kb_led_raw_state = led_raw;
        my_lv_widget_state_change_send_event(MY_LV_WIDGET_LED_NUM);
#if MY_ESPNOW_IS_RECEIVER
        my_espnow_send_hid_output_report((uint8_t *)&my_kb_led_report, my_kb_led_report_size);
#endif
    }
}

void my_ble_state_change_cb(uint8_t is_connected)
{
    switch (is_connected) {
        case 0:                                          // u断开
            if (my_cfg_ble_state & MY_FEATURE_CONNECTED) // 如果此时记录的状态还是连接，去除连接标志，发送事件
            {
                my_cfg_ble_state = my_cfg_ble_state & (~MY_FEATURE_CONNECTED); // 修改状态，如果之后事件发送失败也不管，其它应用自行记录连接状态和该状态对比
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_BLE);
                if (!(my_cfg_usb_hid_state & MY_FEATURE_CONNECTED)) {
                    my_kb_led_report.raw = 0;
                    esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, 1, 0);
                }
            }
            break;
        case 1:                                             // 连接
            if (!(my_cfg_ble_state & MY_FEATURE_CONNECTED)) // 如果此时状态不是连接
            {
                my_cfg_ble_state |= MY_FEATURE_CONNECTED;
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_BLE);
            }
            break;
        default:
            break;
    }
};

void my_wifi_state_change_cb(uint8_t wifi_mode, int8_t value)
{
    switch (wifi_mode) {
        case WIFI_MODE_STA:
            if (value > 0 && !(my_cfg_sta_state & MY_FEATURE_CONNECTED)) // 如果WiFi已连接，但状态为未连接
            {
                my_cfg_sta_state |= MY_FEATURE_CONNECTED;
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_STA);
            }
            else if (value == 0 && (my_cfg_sta_state & MY_FEATURE_CONNECTED)) // 如果wifi断开，但状态为已连接
            {
                my_cfg_sta_state &= (~MY_FEATURE_CONNECTED);
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_STA);
            }
            break;
        case WIFI_MODE_AP:
            if (value > 0 && !(my_cfg_ap_state & MY_FEATURE_CONNECTED)) {
                my_cfg_ap_state |= MY_FEATURE_CONNECTED;
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_AP);
            }
            else if (value <= 0 && (my_cfg_ap_state & MY_FEATURE_CONNECTED)) {
                my_cfg_ap_state &= (~MY_FEATURE_CONNECTED);
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_AP);
            }
            break;
        case MY_WIFI_MODE_ESPNOW:
            if (value > 0 && !(my_cfg_espnow_state & MY_FEATURE_CONNECTED)) {
                my_cfg_espnow_state |= MY_FEATURE_CONNECTED;
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_ESP_NOW);
#if !MY_ESPNOW_IS_RECEIVER
                my_espnow_send_data_default(MY_ESPNOW_DATA_TYPE_GET_HID_OUTPUT, 1, NULL, 0);
#endif
            }
            else if (value <= 0 && (my_cfg_espnow_state & MY_FEATURE_CONNECTED)) {
                my_cfg_espnow_state &= (~MY_FEATURE_CONNECTED);
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_ESP_NOW);
                if (!(my_cfg_usb_hid_state & MY_FEATURE_CONNECTED) && !(my_cfg_ble_state & MY_FEATURE_CONNECTED)) {
                    my_kb_led_report.raw = 0;
                    esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, 1, 0);
                }
            }
            break;
        default:
            break;
    }
}

// 将在tinyusb任务中被调用，最好不要阻塞
void my_usb_state_change_cb(uint8_t is_mount)
{
    switch (is_mount) {
        case 0:                                              // usb断开
            if (my_cfg_usb_hid_state & MY_FEATURE_CONNECTED) // 如果此时记录的状态还是连接，去除连接标志，发送事件
            {
                my_cfg_usb_hid_state &= (~MY_FEATURE_CONNECTED); // 修改状态，如果之后事件发送失败也不管，其它应用自行记录连接状态和该状态对比
                my_kb_led_report.raw = 0;
                esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, 1, 0);
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_USB);
            }
            break;
        case 1:                                                 // usb连接
            if (!(my_cfg_usb_hid_state & MY_FEATURE_CONNECTED)) // 如果此时状态不是连接
            {
                my_cfg_usb_hid_state |= MY_FEATURE_CONNECTED;
                my_lv_widget_state_change_send_event(MY_LV_WIDGET_USB);
            }
            break;
        default:
            break;
    }
}
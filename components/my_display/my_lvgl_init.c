#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "string.h"

#include "my_config.h"
#include "my_display.h"
#include "my_lvgl_private.h"

void my_lv_change_key_state_cb(lv_event_t *e);
void my_lv_set_fn_layer_cb(lv_event_t *e);
static const char *TAG = "my_lv_init";

uint32_t LV_EVENT_MY_SET_FN_LAYER = 0;
uint32_t LV_EVENT_MY_KEY_STATE_PRESS = 0;
uint32_t LV_EVENT_MY_KEY_STATE_RELEASE = 0;
uint32_t LV_EVENT_MY_WIDGET_STATE_CHANGE = 0;
uint32_t LV_EVENT_MY_POP_MESSAGE = 0;

lv_obj_t *my_lv_widgets[MY_LV_WIDGET_TOTAL_NUM] = {NULL};

#pragma region 按钮矩阵初始化

lv_obj_t *buttonm[2] = {NULL, NULL};
char **matrix_key_map[2] = {NULL, NULL};
uint8_t kb_2nd_line_offset = MY_COL_NUM + 1; // 键盘第二行的按键偏移量，等于列数+1，即map中实际的列数

void my_lv_create_kb_buttonm(lv_obj_t *scr)
{
    // lv_theme_t *theme = lv_theme_default_init(my_lv_display, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), LV_THEME_DEFAULT_DARK, LV_FONT_DEFAULT);
    my_keyboard_lv_str_map_init();

    lv_style_t *style_buttonm_main = my_lv_style_get_buttonm_main();
    lv_style_t *style_buttonm_item = my_lv_style_get_buttonm_item();

    matrix_key_map[0] = matrix_key_str_map[my_cfg_fn_sw_state];
    matrix_key_map[1] = matrix_key_str_map[my_cfg_fn_sw_state] + kb_2nd_line_offset;
    // matrix_key_map[0] = matrix_key_str_map[0];
    // matrix_key_map[1] = matrix_key_str_map[0] + kb_2nd_line_offset;

    LV_EVENT_MY_SET_FN_LAYER = lv_event_register_id();
    LV_EVENT_MY_KEY_STATE_PRESS = lv_event_register_id();
    LV_EVENT_MY_KEY_STATE_RELEASE = lv_event_register_id();

    for (size_t i = 0; i < 2; i++) {
        buttonm[i] = lv_buttonmatrix_create(scr);
        lv_obj_add_style(buttonm[i], style_buttonm_main, 0);
        lv_obj_add_style(buttonm[i], style_buttonm_item, LV_PART_ITEMS);

        // 设置按键状态为checked时的样式，用于模拟按键被按下状态
        lv_obj_remove_style(buttonm[i], NULL, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_color_filter_dsc(buttonm[i], &lv_color_filter_shade, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_color_filter_opa(buttonm[i], LV_OPA_60, LV_PART_ITEMS | LV_STATE_CHECKED);

        lv_buttonmatrix_set_map(buttonm[i], matrix_key_map[i]);
    }
    // 隐藏除了最后一行外所有行的最后列
    lv_buttonmatrix_set_button_ctrl(buttonm[0], MY_COL_NUM - 1, LV_BUTTONMATRIX_CTRL_HIDDEN);
    for (size_t i = 0; i < MY_ROW_NUM - 2; i++) {
        lv_buttonmatrix_set_button_ctrl(buttonm[1], i * MY_COL_NUM + MY_COL_NUM - 1, LV_BUTTONMATRIX_CTRL_HIDDEN);
    }

    my_lv_widgets[MY_LV_WIDGET_KB_1ST_ROW] = buttonm[0];
    my_lv_widgets[MY_LV_WIDGET_KB_2ND_ROW] = buttonm[1];

    // 设置键盘最上面一排的大小
    lv_obj_set_height(buttonm[0], item_obj_height);
    lv_obj_set_width(buttonm[0], main_obj_width);
    // 设置键盘其它排的大小
    lv_obj_set_height(buttonm[1], main_obj_height);
    lv_obj_set_width(buttonm[1], main_obj_width);

    lv_obj_align(buttonm[1], LV_ALIGN_BOTTOM_MID, align_center_offset_x, align_center_offset_y);
    lv_obj_align_to(buttonm[0], buttonm[1], LV_ALIGN_OUT_TOP_MID, 0, key_gap_align_offset_y);

    lv_obj_add_event_cb(buttonm[0], my_lv_set_fn_layer_cb, LV_EVENT_MY_SET_FN_LAYER, &buttonm);
    lv_obj_add_event_cb(buttonm[0], my_lv_change_key_state_cb, LV_EVENT_MY_KEY_STATE_PRESS, &buttonm);
    lv_obj_add_event_cb(buttonm[0], my_lv_change_key_state_cb, LV_EVENT_MY_KEY_STATE_RELEASE, &buttonm);
}

void my_lv_set_fn_layer_cb(lv_event_t *e)
{
    lvgl_port_lock(0);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_MY_SET_FN_LAYER) {
        uint32_t fn_layer = lv_event_get_param(e);
        if (fn_layer >= my_fn_layer_num) {
            lvgl_port_unlock();
            return;
        }
        if (matrix_key_map[0] != matrix_key_str_map[fn_layer]) {
            matrix_key_map[0] = matrix_key_str_map[fn_layer];
            lv_buttonmatrix_set_map(buttonm[0], matrix_key_map[0]);
        }
        if (matrix_key_map[1] != matrix_key_str_map[fn_layer] + kb_2nd_line_offset) {
            matrix_key_map[1] = matrix_key_str_map[fn_layer] + kb_2nd_line_offset;
            lv_buttonmatrix_set_map(buttonm[1], matrix_key_map[1]);
        }
        if (my_lv_info.fn_sw != my_cfg_fn_sw_state) {
            my_lv_info.fn_sw = my_cfg_fn_sw_state;
            if (my_lv_info.fn_sw) {
                lv_obj_remove_flag(my_lv_widgets[MY_LV_WIDGET_FN_SW], LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_add_flag(my_lv_widgets[MY_LV_WIDGET_FN_SW], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    lvgl_port_unlock();
}

uint8_t my_lv_set_buttonm_key_state(uint32_t id, uint8_t is_pressed)
{
    // 解析id，后16位为按键类型，1为矩阵按键，2为编码器按键，0为清除所有按键
    uint16_t key_type = (id >> 16);
    uint16_t key_id;
    uint8_t target_obj = 0;

    switch (key_type) {
        case 0:
            lv_buttonmatrix_clear_button_ctrl_all(buttonm[0], LV_BUTTONMATRIX_CTRL_CHECKED);
            lv_buttonmatrix_clear_button_ctrl_all(buttonm[1], LV_BUTTONMATRIX_CTRL_CHECKED);
            return 0;
            break;
        case 1: // 如果是矩阵按键，前16位的前8位是列号，后8位是行号
            uint8_t col = id & 0xFF;
            uint8_t row = (id >> 8) & 0xFF;
            if (row == 0) {
                key_id = col;
                target_obj = 0;
            }
            else if (row < MY_ROW_NUM) {
                key_id = col + (row - 1) * MY_COL_NUM;
                target_obj = 1;
            }
            else
                return 1;
            break;
        case 2:
            return 0;
            key_id = (id & 0xFFFF);
            target_obj = 2;
            break;
        default:
            return 1;
            break;
    }

    if (is_pressed) {
        lv_buttonmatrix_set_button_ctrl(buttonm[target_obj], key_id, LV_BUTTONMATRIX_CTRL_CHECKED);
        lv_buttonmatrix_set_selected_button(buttonm[target_obj], key_id);
    }
    else {
        lv_buttonmatrix_clear_button_ctrl(buttonm[target_obj], key_id, LV_BUTTONMATRIX_CTRL_CHECKED);
    }

    return 0;
}

// LV_EVENT_MY_KEY_STATE_PRESS
// LV_EVENT_MY_KEY_STATE_RELEASE
void my_lv_change_key_state_cb(lv_event_t *e)
{
    lvgl_port_lock(0);
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key_id = (uint32_t)lv_event_get_param(e);
    if (code == LV_EVENT_MY_KEY_STATE_PRESS) {
        my_lv_set_buttonm_key_state(key_id, 1);
    }
    else {
        my_lv_set_buttonm_key_state(key_id, 0);
    }
    lvgl_port_unlock();
}

uint8_t my_lv_buttonm_send_event(uint32_t event_code, void *param)
{
    if (!my_lvgl_is_running() || !buttonm[0] || !matrix_key_map[0])
        return 1;

    if (event_code == LV_EVENT_MY_KEY_STATE_PRESS ||
        event_code == LV_EVENT_MY_KEY_STATE_RELEASE ||
        event_code == LV_EVENT_MY_SET_FN_LAYER) {
        lvgl_port_lock(0);
        uint8_t ret = lv_obj_send_event(buttonm[0], event_code, param);
        lvgl_port_unlock();
        if (ret)
            return 0;
        else
            return 1;
    }
    return 1;
}
#pragma endregion

#pragma region 信息显示初始化

lv_obj_t *hardware_info_flex = NULL;
lv_obj_t *left_text_flex = NULL;
lv_obj_t *right_text_flex = NULL;
lv_obj_t *signal_led_flex = NULL;
#define flex_height 100
#define flex_width 22
#define MY_FEATURE_ENABLE_LV_STATE LV_STATE_USER_1
uint8_t my_lv_widget_state_change_send_event(uint8_t widget_index)
{
    if (!my_lvgl_is_running() || widget_index >= MY_LV_WIDGET_TOTAL_NUM) {
        return 1;
    }

    uint32_t param = widget_index;
    uint8_t ret = LV_RESULT_INVALID;
    if (lvgl_port_lock(100)) {
        ret = lv_obj_send_event(my_lv_widgets[MY_LV_WIDGET_SCR], LV_EVENT_MY_WIDGET_STATE_CHANGE, param);
        lvgl_port_unlock();
    }
    if (ret == LV_RESULT_OK)
        return ESP_OK;
    else
        return ESP_FAIL;
}

void my_lv_widget_state_change_cb(lv_event_t *e)
{
    lvgl_port_lock(0);
    uint32_t code = lv_event_get_code(e);
    if (code == LV_EVENT_MY_WIDGET_STATE_CHANGE) {
        uint8_t index = (uint8_t)(lv_event_get_param(e)) & 0xff;
        int16_t data = -1;
        if (index >= MY_LV_WIDGET_TOTAL_NUM || !my_lv_widgets[index]) {
            lvgl_port_unlock();
            return;
        }

        switch (index) {
            case MY_LV_WIDGET_BLE:
                if (my_lv_info.ble_state != my_cfg_ble_state) {
                    my_lv_info.ble_state = my_cfg_ble_state;
                    data = my_lv_info.ble_state;
                }
                break;
            case MY_LV_WIDGET_USB:
                if (my_lv_info.usb_hid_state != my_cfg_usb_hid_state) {
                    my_lv_info.usb_hid_state = my_cfg_usb_hid_state;
                    data = my_lv_info.usb_hid_state;
                }
                break;
            case MY_LV_WIDGET_ESP_NOW:
                if (my_lv_info.espnow_state != my_cfg_espnow_state) {
                    my_lv_info.espnow_state = my_cfg_espnow_state;
                    data = my_lv_info.espnow_state;
                }
                break;
            case MY_LV_WIDGET_STA:
                if (my_lv_info.sta_state != my_cfg_sta_state) {
                    my_lv_info.sta_state = my_cfg_sta_state;
                    data = my_lv_info.sta_state;
                }
                break;
            case MY_LV_WIDGET_AP:
                if (my_lv_info.ap_state != my_cfg_ap_state) {
                    my_lv_info.ap_state = my_cfg_ap_state;
                    data = my_lv_info.ap_state;
                }
                break;
            case MY_LV_WIDGET_LED_NUM:
            case MY_LV_WIDGET_LED_CAP:
                if (my_lv_info.kb_led.raw != my_cfg_kb_led_raw_state) {
                    my_lv_info.kb_led.raw = my_cfg_kb_led_raw_state;
                    if (my_lv_widgets[MY_LV_WIDGET_LED_NUM]) {
                        if (my_lv_info.kb_led.num_lock)
                            lv_obj_set_state(my_lv_widgets[MY_LV_WIDGET_LED_NUM], MY_FEATURE_ENABLE_LV_STATE, 1);
                        else
                            lv_obj_set_state(my_lv_widgets[MY_LV_WIDGET_LED_NUM], MY_FEATURE_ENABLE_LV_STATE, 0);
                    }
                    if (my_lv_widgets[MY_LV_WIDGET_LED_CAP]) {
                        if (my_lv_info.kb_led.caps_lock)
                            lv_obj_set_state(my_lv_widgets[MY_LV_WIDGET_LED_CAP], MY_FEATURE_ENABLE_LV_STATE, 1);
                        else
                            lv_obj_set_state(my_lv_widgets[MY_LV_WIDGET_LED_CAP], MY_FEATURE_ENABLE_LV_STATE, 0);
                    }
                }
                break;
            case MY_LV_WIDGET_FN_SW:
                if (my_lv_info.fn_sw != my_cfg_fn_sw_state) {
                    my_lv_info.fn_sw = my_cfg_fn_sw_state;
                    if (my_lv_info.fn_sw)
                        lv_obj_remove_flag(my_lv_widgets[index], LV_OBJ_FLAG_HIDDEN);
                    else
                        lv_obj_add_flag(my_lv_widgets[index], LV_OBJ_FLAG_HIDDEN);
                }
                break;
            case MY_LV_WIDGET_VBUS_VOL:
            case MY_LV_WIDGET_BAT_VOL:
            case MY_LV_WIDGET_INA226:
                if (my_lv_info.vbus_vol.raw != my_vbus_vol) {
                    my_lv_info.vbus_vol.raw = my_vbus_vol;
                    my_lv_info.vbus_vol.f = my_lv_info.vbus_vol.raw / 1000.0f;
                    lv_label_set_text_fmt(my_lv_widgets[MY_LV_WIDGET_VBUS_VOL], "vbus:\n%0.3fV", my_lv_info.vbus_vol.f);
                }
                if (my_lv_info.bat_vol.raw != my_bat_vol) {
                    my_lv_info.bat_vol.raw = my_bat_vol;
                    my_lv_info.bat_vol.f = my_lv_info.bat_vol.raw / 1000.0f;
                    lv_label_set_text_fmt(my_lv_widgets[MY_LV_WIDGET_BAT_VOL], "bat:\n%0.3fV", my_lv_info.bat_vol.f);
                }
                if (my_lv_info.ina226_state != my_cfg_ina226_state) {
                    my_lv_info.ina226_state = my_cfg_ina226_state;
                    lv_obj_set_flag(my_lv_widgets[MY_LV_WIDGET_INA226], LV_OBJ_FLAG_HIDDEN, (my_lv_info.ina226_state & (MY_FEATURE_CONNECTED | MY_FEATURE_USE)) ? 0 : 1);
                }
                if (my_lv_info.ina226_state & (MY_FEATURE_CONNECTED | MY_FEATURE_USE)) {
                    memcpy(&my_lv_info.ina226_result, &my_ina226_data, sizeof(my_ina226_data));
                    if (my_lv_info.ina226_result.current < 0 && my_lv_info.ina226_result.current > -0.0004f)
                        my_lv_info.ina226_result.current = 0.0f;
                    lv_label_set_text_fmt(my_lv_widgets[MY_LV_WIDGET_INA226], "INA: \n%0.3fV\n%0.3fA\n%0.3fW", my_lv_info.ina226_result.bus_vol, my_lv_info.ina226_result.current, my_lv_info.ina226_result.power);
                }
                break;
            default:
                break;
        }

        if (data != -1) {
            if (data & MY_FEATURE_CONNECTED)
                lv_obj_set_state(my_lv_widgets[index], MY_FEATURE_ENABLE_LV_STATE, 1);
            else
                lv_obj_set_state(my_lv_widgets[index], MY_FEATURE_ENABLE_LV_STATE, 0);

            if (data & MY_FEATURE_USE)
                lv_obj_remove_flag(my_lv_widgets[index], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(my_lv_widgets[index], LV_OBJ_FLAG_HIDDEN);
        }
        lvgl_port_unlock();
    }
}

void my_lv_create_hardware_info_display(lv_obj_t *scr)
{
    LV_EVENT_MY_WIDGET_STATE_CHANGE = lv_event_register_id();

    hardware_info_flex = lv_obj_create(scr);
    lv_obj_set_layout(hardware_info_flex, LV_LAYOUT_FLEX);
    lv_obj_align(hardware_info_flex, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_remove_style(hardware_info_flex, NULL, 0);
    lv_obj_set_flex_flow(hardware_info_flex, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(hardware_info_flex, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(hardware_info_flex, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(hardware_info_flex, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(hardware_info_flex, flex_width, flex_height);
    lv_obj_set_style_pad_all(hardware_info_flex, 3, 0);
    lv_obj_set_style_pad_gap(hardware_info_flex, 5, 0);

    lv_style_t *label_style = my_lv_style_get_label_main();
    lv_style_t *label_enabled_style = my_lv_style_get_label_enabled();
    for (size_t i = MY_LV_WIDGET_BLE; i <= MY_LV_WIDGET_ESP_NOW; i++) {
        my_lv_widgets[i] = lv_label_create(hardware_info_flex);
        lv_obj_add_style(my_lv_widgets[i], label_style, 0);
        lv_obj_add_style(my_lv_widgets[i], label_enabled_style, MY_FEATURE_ENABLE_LV_STATE);
    }
    lv_label_set_text(my_lv_widgets[MY_LV_WIDGET_BLE], LV_SYMBOL_BLUETOOTH);
    lv_label_set_text(my_lv_widgets[MY_LV_WIDGET_USB], LV_SYMBOL_USB);
    lv_label_set_text(my_lv_widgets[MY_LV_WIDGET_STA], LV_SYMBOL_WIFI);
    lv_label_set_text(my_lv_widgets[MY_LV_WIDGET_AP], "ap");
    lv_label_set_text(my_lv_widgets[MY_LV_WIDGET_ESP_NOW], "ES");

    lv_obj_set_style_bg_color(my_lv_widgets[MY_LV_WIDGET_ESP_NOW], lv_color_hex(0xe43e39), MY_FEATURE_ENABLE_LV_STATE);
    lv_obj_set_style_outline_color(my_lv_widgets[MY_LV_WIDGET_ESP_NOW], lv_color_hex(0xe43e39), MY_FEATURE_ENABLE_LV_STATE);
    // lv_obj_set_size(my_lv_widgets[MY_LV_WIDGET_ESP_NOW], 15, 13);

    signal_led_flex = lv_obj_create(scr);
    lv_obj_remove_style(signal_led_flex, NULL, 0);
    lv_obj_set_flex_flow(signal_led_flex, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(signal_led_flex, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(signal_led_flex, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(signal_led_flex, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(signal_led_flex, 26, 14);
    lv_obj_set_layout(signal_led_flex, LV_LAYOUT_FLEX);
    lv_obj_align(signal_led_flex, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_pad_all(signal_led_flex, 3, 0);
    lv_obj_set_style_pad_gap(signal_led_flex, 3, 0);

    my_lv_widgets[MY_LV_WIDGET_LED_NUM] = lv_obj_create(signal_led_flex);
    my_lv_widgets[MY_LV_WIDGET_LED_CAP] = lv_obj_create(signal_led_flex);

    lv_style_t *led_style = my_lv_style_get_led_main();
    lv_obj_remove_style_all(my_lv_widgets[MY_LV_WIDGET_LED_NUM]);
    lv_obj_remove_style_all(my_lv_widgets[MY_LV_WIDGET_LED_CAP]);
    lv_obj_add_style(my_lv_widgets[MY_LV_WIDGET_LED_NUM], led_style, 0);
    lv_obj_add_style(my_lv_widgets[MY_LV_WIDGET_LED_CAP], led_style, 0);

    lv_obj_set_style_bg_color(my_lv_widgets[MY_LV_WIDGET_LED_NUM], lv_palette_main(LV_PALETTE_LIGHT_GREEN), MY_FEATURE_ENABLE_LV_STATE);
    lv_obj_set_style_bg_color(my_lv_widgets[MY_LV_WIDGET_LED_CAP], lv_color_hex(0Xc2e3fa), MY_FEATURE_ENABLE_LV_STATE);

    my_lv_widgets[MY_LV_WIDGET_FN_SW] = lv_label_create(scr);
    lv_obj_set_style_text_font(my_lv_widgets[MY_LV_WIDGET_FN_SW], &lv_font_montserrat_10, 0);
    lv_label_set_text(my_lv_widgets[MY_LV_WIDGET_FN_SW], "Fn");
    lv_obj_set_style_bg_color(my_lv_widgets[MY_LV_WIDGET_FN_SW], lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_style_bg_opa(my_lv_widgets[MY_LV_WIDGET_FN_SW], LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(my_lv_widgets[MY_LV_WIDGET_FN_SW], 1, 0);
    lv_obj_set_style_shadow_spread(my_lv_widgets[MY_LV_WIDGET_FN_SW], 0, 0);
    lv_obj_set_style_shadow_offset_y(my_lv_widgets[MY_LV_WIDGET_FN_SW], 1, 0);
    lv_obj_set_style_shadow_offset_x(my_lv_widgets[MY_LV_WIDGET_FN_SW], 1, 0);
    lv_obj_align_to(my_lv_widgets[MY_LV_WIDGET_FN_SW], signal_led_flex, LV_ALIGN_OUT_BOTTOM_RIGHT, -2, 2);

    lv_obj_add_event_cb(my_lv_widgets[MY_LV_WIDGET_SCR], my_lv_widget_state_change_cb, LV_EVENT_MY_WIDGET_STATE_CHANGE, &my_lv_widgets);
}

void my_lv_create_power_supply_labels(lv_obj_t *scr)
{
    my_lv_widgets[MY_LV_WIDGET_VBUS_VOL] = lv_label_create(scr);
    my_lv_widgets[MY_LV_WIDGET_BAT_VOL] = lv_label_create(scr);

    my_lv_info.bat_vol.f = 0;
    my_lv_info.vbus_vol.f = 0;
    lv_label_set_text_fmt(my_lv_widgets[MY_LV_WIDGET_VBUS_VOL], "vbus:\n%0.3fV", my_lv_info.vbus_vol.f);
    lv_label_set_text_fmt(my_lv_widgets[MY_LV_WIDGET_BAT_VOL], "bat:\n%0.3fV", my_lv_info.bat_vol.f);

    lv_obj_set_style_text_line_space(my_lv_widgets[MY_LV_WIDGET_VBUS_VOL], -2, 0);
    lv_obj_set_style_text_line_space(my_lv_widgets[MY_LV_WIDGET_BAT_VOL], -2, 0);

    lv_obj_set_style_text_font(my_lv_widgets[MY_LV_WIDGET_VBUS_VOL], &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_font(my_lv_widgets[MY_LV_WIDGET_BAT_VOL], &lv_font_montserrat_10, 0);

    lv_obj_align(my_lv_widgets[MY_LV_WIDGET_BAT_VOL], LV_ALIGN_BOTTOM_LEFT, 1, 0);
    lv_obj_align_to(my_lv_widgets[MY_LV_WIDGET_VBUS_VOL], my_lv_widgets[MY_LV_WIDGET_BAT_VOL], LV_ALIGN_OUT_TOP_LEFT, 0, 1);

    my_lv_widgets[MY_LV_WIDGET_INA226] = lv_label_create(scr);
    lv_label_set_text_fmt(my_lv_widgets[MY_LV_WIDGET_INA226], "INA:\n%0.3fV\n%0.3fA\n%0.3fW", my_lv_info.ina226_result.bus_vol, my_lv_info.ina226_result.current, my_lv_info.ina226_result.power);
    lv_obj_set_style_text_font(my_lv_widgets[MY_LV_WIDGET_INA226], &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_line_space(my_lv_widgets[MY_LV_WIDGET_INA226], -2, 0);
    lv_obj_set_style_text_align(my_lv_widgets[MY_LV_WIDGET_INA226], LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(my_lv_widgets[MY_LV_WIDGET_INA226], LV_ALIGN_RIGHT_MID, 0, 0);
}

static esp_err_t my_lv_set_obj_state(lv_obj_t *lv_obj, uint8_t is_hidden, uint8_t is_enable)
{
    if (!lv_obj) {
        return ESP_FAIL;
    }
    lv_obj_set_flag(lv_obj, LV_OBJ_FLAG_HIDDEN, is_hidden);
    lv_obj_set_state(lv_obj, MY_FEATURE_ENABLE_LV_STATE, is_enable);
    return ESP_OK;
}

void my_lv_widget_check_all(uint8_t force_check)
{
    if (!lvgl_port_lock(5000)) {
        return;
    }
    if ((my_lv_info.ble_state != my_cfg_ble_state) || force_check) {
        if (ESP_OK == my_lv_set_obj_state(my_lv_widgets[MY_LV_WIDGET_BLE], !(my_cfg_ble_state & MY_FEATURE_USE), my_cfg_ble_state & MY_FEATURE_CONNECTED)) {
            my_lv_info.ble_state = my_cfg_ble_state;
        }
    }
    if ((my_lv_info.usb_hid_state != my_cfg_usb_hid_state) || force_check) {
        if (ESP_OK == my_lv_set_obj_state(my_lv_widgets[MY_LV_WIDGET_USB], !(my_cfg_usb_hid_state & MY_FEATURE_USE), my_cfg_usb_hid_state & MY_FEATURE_CONNECTED)) {
            my_lv_info.usb_hid_state = my_cfg_usb_hid_state;
        }
    }
    if ((my_lv_info.sta_state != my_cfg_sta_state) || force_check) {
        if (ESP_OK == my_lv_set_obj_state(my_lv_widgets[MY_LV_WIDGET_STA], !(my_cfg_sta_state & MY_FEATURE_USE), my_cfg_sta_state & MY_FEATURE_CONNECTED)) {
            my_lv_info.sta_state = my_cfg_sta_state;
        }
    }
    if ((my_lv_info.ap_state != my_cfg_ap_state) || force_check) {
        if (ESP_OK == my_lv_set_obj_state(my_lv_widgets[MY_LV_WIDGET_AP], !(my_cfg_ap_state & MY_FEATURE_USE), my_cfg_ap_state & MY_FEATURE_CONNECTED)) {
            my_lv_info.ap_state = my_cfg_ap_state;
        }
    }
    if ((my_lv_info.espnow_state != my_cfg_espnow_state) || force_check) {
        if (ESP_OK == my_lv_set_obj_state(my_lv_widgets[MY_LV_WIDGET_ESP_NOW], !(my_cfg_espnow_state & MY_FEATURE_USE), my_cfg_espnow_state & MY_FEATURE_CONNECTED)) {
            my_lv_info.espnow_state = my_cfg_espnow_state;
        }
    }

    if ((my_lv_info.kb_led.raw != my_cfg_kb_led_raw_state) || force_check) {
        if (ESP_OK == my_lv_set_obj_state(my_lv_widgets[MY_LV_WIDGET_LED_NUM], 0, my_cfg_kb_led_raw_state & MY_NUMLOCK_BIT)) {
            my_lv_info.kb_led.num_lock = (my_cfg_kb_led_raw_state & MY_NUMLOCK_BIT) > 0;
        }
        if (ESP_OK == my_lv_set_obj_state(my_lv_widgets[MY_LV_WIDGET_LED_CAP], 0, my_cfg_kb_led_raw_state & MY_CAPSLOCK_BIT)) {
            my_lv_info.kb_led.caps_lock = (my_cfg_kb_led_raw_state & MY_CAPSLOCK_BIT) > 0;
        }
    }

    if ((my_lv_info.fn_sw != my_cfg_fn_sw_state) || force_check) {
        if (ESP_OK == my_lv_set_obj_state(my_lv_widgets[MY_LV_WIDGET_FN_SW], !my_cfg_fn_sw_state, 0)) {
            my_lv_info.fn_sw = my_cfg_fn_sw_state;
        }
    }

    if (my_lv_info.vbus_vol.raw != my_vbus_vol || force_check) {
        my_lv_info.vbus_vol.raw = my_vbus_vol;
        my_lv_info.vbus_vol.f = my_lv_info.vbus_vol.raw / 1000.0f;
        lv_label_set_text_fmt(my_lv_widgets[MY_LV_WIDGET_VBUS_VOL], "vbus:\n%0.3fV", my_lv_info.vbus_vol.f);
    }
    if (my_lv_info.bat_vol.raw != my_bat_vol || force_check) {
        my_lv_info.bat_vol.raw = my_bat_vol;
        my_lv_info.bat_vol.f = my_lv_info.bat_vol.raw / 1000.0f;
        lv_label_set_text_fmt(my_lv_widgets[MY_LV_WIDGET_BAT_VOL], "bat:\n%0.3fV", my_lv_info.bat_vol.f);
    }
    if (my_lv_info.ina226_state != my_cfg_ina226_state || force_check) {
        my_lv_info.ina226_state = my_cfg_ina226_state;
        if (my_lv_info.ina226_state & (MY_FEATURE_CONNECTED | MY_FEATURE_USE)) {
            lv_obj_remove_flag(my_lv_widgets[MY_LV_WIDGET_INA226], LV_OBJ_FLAG_HIDDEN);
            memcpy(&my_lv_info.ina226_result, &my_ina226_data, sizeof(my_ina226_data));
            lv_label_set_text_fmt(my_lv_widgets[MY_LV_WIDGET_INA226], "INA:\n%0.3fV\n%0.3fA\n%0.3fW", my_lv_info.ina226_result.bus_vol, my_lv_info.ina226_result.current, my_lv_info.ina226_result.power);
        }
        else {
            lv_obj_add_flag(my_lv_widgets[MY_LV_WIDGET_INA226], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lvgl_port_unlock();
}
#pragma endregion

void my_lv_create_pop_window(const char *pop_text, uint32_t delete_delay_ms)
{
    static int64_t _last_pop_time = 0;
    static int64_t _now;
    _now = esp_timer_get_time();
    if (_now - _last_pop_time < 500000) // 500ms内只显示一次
        return;
    lv_obj_t *pop_win = lv_label_create(lv_layer_top());
    lv_style_t *pop_style = my_lv_style_get_pop_window();
    lv_obj_add_style(pop_win, pop_style, 0);
    // lv_label_set_text(pop_win, pop_text);
    lv_label_set_text_static(pop_win, pop_text);
    lv_obj_center(pop_win);
    lv_obj_delete_delayed(pop_win, delete_delay_ms);
    _last_pop_time = _now;
};

void my_lv_pop_message_cb(lv_event_t *e)
{
    uint32_t code = lv_event_get_code(e);
    if (code == LV_EVENT_MY_POP_MESSAGE) {
        lvgl_port_lock(0);
        char *pop_text = lv_event_get_param(e);
        if (pop_text)
            my_lv_create_pop_window(pop_text, 500);
        lvgl_port_unlock();
    };
}

uint8_t my_lv_send_pop_message(char *pop_text)
{
    if (my_lvgl_is_running() && pop_text) {
        lvgl_port_lock(0);
        lv_obj_send_event(lv_layer_top(), LV_EVENT_MY_POP_MESSAGE, pop_text);
        lvgl_port_unlock();
        return 0;
    }
    return 1;
}
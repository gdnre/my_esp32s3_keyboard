#pragma once
#include "esp_log.h"
#include "esp_err.h"
#include <inttypes.h>
#include <stdio.h>

/**
 * @brief Define blinking type and priority.
 *
 */
enum {
    BLINK_DOUBLE_RED = 0,
    BLINK_TRIPLE_GREEN,
    BLINK_WHITE_BREATHE_SLOW,
    BLINK_WHITE_BREATHE_FAST,
    BLINK_BLUE_BREATH,
    BLINK_COLOR_HSV_RING,
    BLINK_COLOR_RGB_RING,
    BLINK_FLOWING,
    BLINK_MAX,                  // 关闭led
    MY_LED_STRIPS_SINGLE_COLOR, // 单色led
    MY_LED_MODE_MAX,
};

// 设置led的亮度，参数为亮度百分比，大于100时为100，为0时亮度会强制设置1，要关闭灯请直接调节模式
esp_err_t my_led_set_brightness(uint8_t bri_percentage);

// 直接设置当前的led模式，如果没有初始化，会先初始化
esp_err_t my_led_set_mode(int led_mode);

// 如果switch_mode为0，以my_cfg_led_mode的值执行my_led_set_mode，否则以my_cfg_led_mode+1（自动在合法模式之间循环）执行my_led_set_mode，并且会自动设置和保存my_cfg_led_mode
esp_err_t my_led_start(bool switch_mode);

esp_err_t my_led_full_clean();
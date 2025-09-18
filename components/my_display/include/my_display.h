#pragma once
#include "esp_err.h"
#include "stdint.h"

void my_display_start(void);

void my_display_stop(void);

esp_err_t my_pwm_set_duty(uint8_t duty_percentage);

uint8_t my_pwm_get_duty();

// 简单设置背光打开或关闭，如果lvgl已经启动过，请使用my_pwm_set_duty函数
esp_err_t my_backlight_set_level(uint8_t level);

// 切换到特定屏幕
esp_err_t my_lv_switch_screen_2(uint8_t index);

// 根据my_cfg_lvScrIndex切换屏幕，成功返回ESP_OK，应该更新my_cfg_lvScrIndex
esp_err_t my_lv_switch_screen();
#pragma once
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "my_config.h"
#include "my_display.h"
/* LCD size */
#define MY_LCD_H_RES (240)
#define MY_LCD_V_RES (135)

/* LCD settings */
#define MY_LCD_SPI_NUM (SPI3_HOST)
#define MY_LCD_PIXEL_CLK_HZ (40 * 1000 * 1000)
#define MY_LCD_CMD_BITS (8)
#define MY_LCD_PARAM_BITS (8)
#define MY_LCD_COLOR_SPACE (ESP_LCD_COLOR_SPACE_RGB)
#define MY_LCD_BITS_PER_PIXEL (16)
#define MY_LCD_DRAW_BUFF_DOUBLE (1)
#define MY_LCD_DRAW_BUFF_HEIGHT (20)
#define MY_LCD_BL_ON_LEVEL (0)
#define MY_LCD_INVERT_COLOR (1)

/* LCD pins */
#define MY_LCD_GPIO_SCLK (42)
#define MY_LCD_GPIO_MOSI (41)
#define MY_LCD_GPIO_RST (18)
#define MY_LCD_GPIO_DC (7)
#define MY_LCD_GPIO_CS (17)
#define MY_LCD_GPIO_BL (6)

/*pwm控制背光*/
#define MY_PWM_DUTY_RESOLUTION LEDC_TIMER_10_BIT
#define MY_PWM_DUTY_MAX (1 << MY_PWM_DUTY_RESOLUTION)
#define MY_PWM_FREQUENCY 5000
#define MY_PWM_TIMER LEDC_TIMER_0
#define MY_SCREEN_DEFAULT_BRIGHTNESS 100

// 因为小屏幕像素不一定是方形，需要进行缩放
#define MY_LV_IMG_SIZE_OFFSET_X 1.06
#define MY_LV_IMG_SIZE_MAX_X 226 // MY_LCD_H_RES/MY_LV_IMG_SIZE_OFFSET_X
#define MY_LV_IMG_SIZE_MAX_Y MY_LCD_V_RES

// lvgl屏幕的数量，并非硬件显示器数量，可以理解成不同的桌面
#define MY_LV_SCREEN_NUM 2
extern uint8_t my_lv_screen_actually_num;

extern uint8_t my_display_inited;
extern uint32_t s_current_brightness;
extern ledc_channel_config_t s_ledc_channel_cfg;
/* LCD IO and panel */
extern esp_lcd_panel_io_handle_t my_lcd_io_handle;
extern esp_lcd_panel_handle_t my_lcd_panel_handle;

/* LVGL display and touch */
extern lv_display_t *my_lv_display;

extern lv_obj_t *my_lv_widgets[MY_LV_WIDGET_TOTAL_NUM];

extern lv_obj_t *my_lv_screens[MY_LV_SCREEN_NUM];

void my_lv_create_kb_buttonm(lv_obj_t *scr);
uint8_t my_lv_set_buttonm_key_state(uint32_t id, uint8_t is_pressed);
void my_lv_create_hardware_info_display(lv_obj_t *scr);
void my_lv_create_power_supply_labels(lv_obj_t *scr);
void my_lvgl_fs_init(void);
void my_lv_widget_check_all(uint8_t force_check);

#define item_border_width 2
#define item_shadow_width 3
#define item_shadow_spread 2
#define item_outline_width 1
#define item_outline_pad 2
#define item_text_line_space -13 // 字体行间距，本应用中用来调节字体在按键图标中的上下位置

#define item_obj_height 21
#define item_obj_width 35
#define item_col MY_COL_NUM
#define item_row MY_ROW_NUM

#define key_gap_align_offset_y -3

#define main_obj_height (item_row - 1) * item_obj_height
#define main_obj_width item_col *item_obj_width

#define align_center_offset_x item_obj_width / 2
#define align_center_offset_y -(MY_LCD_V_RES - item_obj_height - main_obj_height + key_gap_align_offset_y) / 2

#define main_pad_all item_outline_width + item_outline_pad
#define main_pad_gap item_outline_width + item_outline_pad * 2

#define MY_COLOR_GREY 0xCCCCCC

lv_style_t *my_lv_style_get_buttonm_item();
lv_style_t *my_lv_style_get_buttonm_main();
lv_style_t *my_lv_style_get_label_main();
lv_style_t *my_lv_style_get_label_enabled();
lv_style_t *my_lv_style_get_led_main();
lv_style_t *my_lv_style_get_pop_window();

void my_lv_pop_message_cb(lv_event_t *e);
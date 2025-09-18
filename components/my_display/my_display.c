// todo 很烦，lvgl发送事件函数并非线程安全，考虑创建一个事件池用来接管外部控制lvgl的一切指令
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
#include <string.h>

#include "my_config.h"
#include "my_display.h"
#include "my_fat_mount.h"
#include "my_lvgl_private.h"

static const char *TAG = "my display";
uint8_t my_display_inited = 0;
uint8_t my_lvgl_running = 0;

lv_obj_t *my_lv_screens[MY_LV_SCREEN_NUM];
uint8_t my_lv_screen_actually_num = 1;

char my_bkImage0[256] = {0};
char my_bkImage1[256] = {0};

/* LCD IO and panel */
esp_lcd_panel_io_handle_t my_lcd_io_handle = NULL;
esp_lcd_panel_handle_t my_lcd_panel_handle = NULL;

/* LVGL display and touch */
lv_display_t *my_lv_display = NULL;

// 初始化时会被my_cfg_display_brightness的值覆盖
uint32_t s_current_brightness = MY_SCREEN_DEFAULT_BRIGHTNESS;
ledc_channel_config_t s_ledc_channel_cfg = {
    .gpio_num = MY_LCD_GPIO_BL,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = MY_PWM_TIMER,
    .duty = 0,
    .hpoint = 0,
    .flags.output_invert = !MY_LCD_BL_ON_LEVEL};

uint32_t my_pwm_percentage_to_duty(uint8_t percentage)
{
    if (percentage) {
        if (percentage > 100)
            percentage = 100;
        return (percentage * MY_PWM_DUTY_MAX) / 100;
    }
    return 0;
}

esp_err_t my_pwm_set_duty(uint8_t duty_percentage)
{
    if (!my_display_inited) {
        return ESP_ERR_NOT_ALLOWED;
    }
    esp_err_t ret = ESP_OK;
    uint32_t duty = my_pwm_percentage_to_duty(duty_percentage);
    ledc_set_duty(s_ledc_channel_cfg.speed_mode, s_ledc_channel_cfg.channel, duty);
    ret = ledc_update_duty(s_ledc_channel_cfg.speed_mode, s_ledc_channel_cfg.channel);
    if (ret == ESP_OK) {
        s_current_brightness = (duty_percentage > 100 ? 100 : duty_percentage);
    }
    return ret;
}

uint8_t my_pwm_get_duty()
{
    return s_current_brightness;
}

esp_err_t my_backlight_set_level(uint8_t level)
{
    // gpio_reset_pin(MY_LCD_GPIO_BL);
    return level ? gpio_set_level(MY_LCD_GPIO_BL, MY_LCD_BL_ON_LEVEL) : gpio_reset_pin(MY_LCD_GPIO_BL);
    // return gpio_set_level(MY_LCD_GPIO_BL, level ? MY_LCD_BL_ON_LEVEL : !MY_LCD_BL_ON_LEVEL);
}

// 初始化PWM定时器（只需初始化一次）
static esp_err_t s_my_pwm_timer_init()
{
    static bool timer_initialized = false;
    if (!timer_initialized) {
        ledc_timer_config_t timer_cfg = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = MY_PWM_DUTY_RESOLUTION,
            .timer_num = MY_PWM_TIMER,
            .freq_hz = MY_PWM_FREQUENCY,
            .clk_cfg = LEDC_AUTO_CLK};
        esp_err_t ret = ledc_timer_config(&timer_cfg);
        if (ret == ESP_OK) {
            timer_initialized = true;
        }
        return ret;
    }
    else {
        return ESP_OK;
    }
};

static esp_err_t app_lcd_init(void)
{
    esp_err_t ret = ESP_OK;

    // /* LCD backlight */
    s_my_pwm_timer_init();
    s_current_brightness = my_cfg_brightness.data.u8;
    s_ledc_channel_cfg.duty = my_pwm_percentage_to_duty(s_current_brightness);
    ledc_channel_config(&s_ledc_channel_cfg);

    /* LCD initialization */
    const spi_bus_config_t buscfg = {
        .sclk_io_num = MY_LCD_GPIO_SCLK,
        .mosi_io_num = MY_LCD_GPIO_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = MY_LCD_H_RES * MY_LCD_DRAW_BUFF_HEIGHT * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(MY_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = MY_LCD_GPIO_DC,
        .cs_gpio_num = MY_LCD_GPIO_CS,
        .pclk_hz = MY_LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = MY_LCD_CMD_BITS,
        .lcd_param_bits = MY_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)MY_LCD_SPI_NUM, &io_config, &my_lcd_io_handle), err, TAG, "New panel IO failed");

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = MY_LCD_GPIO_RST,
        .color_space = MY_LCD_COLOR_SPACE,
        .bits_per_pixel = MY_LCD_BITS_PER_PIXEL,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(my_lcd_io_handle, &panel_config, &my_lcd_panel_handle), err, TAG, "New panel failed");

    ESP_ERROR_CHECK(esp_lcd_panel_reset(my_lcd_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(my_lcd_panel_handle));
    // esp_lcd_panel_mirror(my_lcd_panel_handle, false, true);
    // esp_lcd_panel_swap_xy(my_lcd_panel_handle, true);
    esp_lcd_panel_set_gap(my_lcd_panel_handle, 40, 53);
    esp_lcd_panel_invert_color(my_lcd_panel_handle, MY_LCD_INVERT_COLOR);
    esp_lcd_panel_disp_on_off(my_lcd_panel_handle, true);

    return ret;

err:
    if (my_lcd_panel_handle) {
        esp_lcd_panel_del(my_lcd_panel_handle);
    }
    if (my_lcd_io_handle) {
        esp_lcd_panel_io_del(my_lcd_io_handle);
    }
    spi_bus_free(MY_LCD_SPI_NUM);
    return ret;
}

static esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 1,        /* LVGL task priority */
        .task_stack = 8192,        /* LVGL task stack size */
        .task_affinity = -1,       /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 1000, /* Maximum sleep in LVGL task */
        .timer_period_ms = 10      /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = my_lcd_io_handle,
        .panel_handle = my_lcd_panel_handle,
        .buffer_size = MY_LCD_H_RES * MY_LCD_DRAW_BUFF_HEIGHT,
        .double_buffer = MY_LCD_DRAW_BUFF_DOUBLE,
        .hres = MY_LCD_H_RES,
        .vres = MY_LCD_V_RES,
        .monochrome = false,
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif
        .rotation = {
            .swap_xy = true,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = true,
#endif
        }};
    my_lv_display = lvgl_port_add_disp(&disp_cfg);

    return ESP_OK;
}

static void app_main_display(void)
{
    /* Task lock */
    lvgl_port_lock(0);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_palette_main(LV_PALETTE_BLUE), 0);

    char my_lv_fs_letter[2] = {0, 0};
    lv_fs_get_letters(my_lv_fs_letter);
    my_lv_fs_letter[1] = '\0';

    // // 动态解析文件系统中的图片消耗，有信息显示的屏幕先禁止设置背景图片
    // if (my_bkImage0[0] != '\0' && my_file_exist(my_bkImage0)) {
    //     char *img_path = malloc(strlen(my_bkImage0) + 3);
    //     if (img_path) {
    //         sprintf(img_path, "%c:%s", my_lv_fs_letter[0], my_bkImage0);
    //         lv_obj_t *img = lv_image_create(scr);
    //         if (img) {
    //             lv_image_set_src(img, img_path);
    //             lv_obj_center(img);
    //             lv_image_set_scale_x(img, (uint32_t)(MY_LV_IMG_SIZE_OFFSET_X * 256));
    //         }
    //         free(img_path);
    //     }
    // }

    my_lv_widgets[MY_LV_WIDGET_SCR] = scr;
    my_lv_screens[0] = scr;
    my_lv_create_kb_buttonm(my_lv_widgets[MY_LV_WIDGET_SCR]);
    my_lv_create_hardware_info_display(my_lv_widgets[MY_LV_WIDGET_SCR]);
    my_lv_create_power_supply_labels(my_lv_widgets[MY_LV_WIDGET_SCR]);
    LV_EVENT_MY_POP_MESSAGE = lv_event_register_id();
    lv_obj_add_event_cb(lv_layer_top(), my_lv_pop_message_cb, LV_EVENT_MY_POP_MESSAGE, lv_layer_top());

    my_lv_widget_check_all(1);

    if (MY_LV_SCREEN_NUM > 1) {
        my_lv_widgets[MY_LV_WIDGET_SCR1] = lv_obj_create(NULL);
        if (my_lv_widgets[MY_LV_WIDGET_SCR1]) {
            my_lv_screen_actually_num = 2;
            my_lv_screens[1] = my_lv_widgets[MY_LV_WIDGET_SCR1];
            lv_obj_set_scrollbar_mode(my_lv_screens[1], LV_SCROLLBAR_MODE_OFF);
            lv_obj_remove_flag(my_lv_screens[1], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(my_lv_screens[1], lv_palette_main(LV_PALETTE_GREY), 0);

            if (my_bkImage1[0] != '\0' && my_file_exist(my_bkImage1)) {
                char *img_path = malloc(strlen(my_bkImage1) + 3);
                if (img_path) {
                    sprintf(img_path, "%c:%s", my_lv_fs_letter[0], my_bkImage1);
                    lv_obj_t *img = lv_image_create(my_lv_screens[1]);
                    if (img) {
                        lv_image_set_src(img, img_path);
                        lv_obj_center(img);
                        lv_image_set_scale_x(img, (uint32_t)(MY_LV_IMG_SIZE_OFFSET_X * 256));
                    }
                    free(img_path);
                }
            }
        }

        for (size_t i = 2; i < MY_LV_SCREEN_NUM; i++) {
            my_lv_screens[i] = lv_obj_create(NULL);
            if (my_lv_screens[i]) {
                my_lv_screen_actually_num = i + 1;
                lv_obj_set_scrollbar_mode(my_lv_screens[i], LV_SCROLLBAR_MODE_OFF);
                lv_obj_remove_flag(my_lv_screens[i], LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_set_style_bg_color(my_lv_screens[i], lv_palette_main(LV_PALETTE_GREY), 0);
            }
            else {
                break;
            }
        }
    }

    // 根据nvs中的设置加载默认屏幕，如果设置有问题，重置为0
    if (my_cfg_lvScrIndex.data.u8 < my_lv_screen_actually_num) {
        if (my_cfg_lvScrIndex.data.u8 != 0) {
            lv_screen_load(my_lv_screens[my_cfg_lvScrIndex.data.u8]);
        }
    }
    else {
        my_cfg_lvScrIndex.data.u8 = 0;
        my_cfg_save_config_to_nvs(&my_cfg_lvScrIndex);
    }

    /* Task unlock */
    lvgl_port_unlock();
}

void my_display_start(void)
{
    if (my_display_inited) {
        esp_lcd_panel_disp_sleep(my_lcd_panel_handle, false);
        esp_lcd_panel_disp_on_off(my_lcd_panel_handle, true);
        lvgl_port_resume();
        my_lvgl_running = 1;
        my_lv_widget_check_all(0);
        my_pwm_set_duty(my_cfg_brightness.data.u8);
    }
    else {
        /* LCD HW initialization */
        ESP_ERROR_CHECK(app_lcd_init());

        /* LVGL initialization */
        ESP_ERROR_CHECK(app_lvgl_init());

        my_lvgl_fs_init();

        /* Show LVGL objects */
        app_main_display();

        my_display_inited = 1;
        my_lvgl_running = 1;
    }
    ESP_LOGI(TAG, "Display started");
    return;
}

void my_display_stop(void)
{
    if (my_display_inited) {
        my_pwm_set_duty(0);
        my_lvgl_running = 0;
        lvgl_port_stop();
        esp_lcd_panel_disp_on_off(my_lcd_panel_handle, false);
        esp_lcd_panel_disp_sleep(my_lcd_panel_handle, true);
    }
}

uint8_t my_lvgl_is_running(void)
{
    return my_lvgl_running;
}

esp_err_t my_lv_switch_screen_2(uint8_t index)
{
    if (index < my_lv_screen_actually_num) {
        lvgl_port_lock(0);
        lv_screen_load(my_lv_screens[index]);
        lvgl_port_unlock();
        return ESP_OK;
    }
    else {
        return ESP_FAIL;
    }
}

esp_err_t my_lv_switch_screen()
{
    uint8_t index = my_cfg_lvScrIndex.data.u8;
    index = (index + 1) % my_lv_screen_actually_num;
    if (index != my_cfg_lvScrIndex.data.u8) {
        lvgl_port_lock(0);
        lv_screen_load(my_lv_screens[index]);
        my_cfg_lvScrIndex.data.u8 = index;
        lvgl_port_unlock();
    }
    else {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}
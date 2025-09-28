
#include "driver/gpio.h"
#include "led_indicator_strips.h"

#include "my_config.h"
#include "my_led_strips.h"
#define WS2812_GPIO_NUM MY_LED_DIN
#define WS2812_STRIPS_NUM MY_LED_COUNT

#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

static const char *TAG = "my led strips";
static led_indicator_handle_t led_handle = NULL;
static int s_current_mode = -1;

/**
 * @brief Blinking twice times in red has a priority level of 0 (highest).
 *
 */
static const blink_step_t double_red_blink[] = {
    /*!< Set color to red by R:255 G:0 B:0 */
    {LED_BLINK_RGB, SET_RGB(255, 0, 0), 0},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_STOP, 0, 0},
};

/**
 * @brief Blinking three times in green with a priority level of 1.
 *
 */
static const blink_step_t triple_green_blink[] = {
    /*!< Set color to green by R:0 G:255 B:0 */
    {LED_BLINK_RGB, SET_RGB(0, 255, 0), 0},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_STOP, 0, 0},
};

/**
 * @brief Slow breathing in white with a priority level of 2.
 *
 */
static const blink_step_t breath_white_slow_blink[] = {
    /*!< Set Color to white and brightness to zero by H:0 S:0 V:0 */
    {LED_BLINK_HSV, SET_HSV(0, 0, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_ON, 1000},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 1000},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Fast breathing in white with a priority level of 3.
 *
 */
static const blink_step_t breath_white_fast_blink[] = {
    /*!< Set Color to white and brightness to zero by H:0 S:0 V:0 */
    {LED_BLINK_HSV, SET_HSV(0, 0, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_ON, 500},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 500},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Breathing in green with a priority level of 4.
 *
 */
static const blink_step_t breath_blue_blink[] = {
    /*!< Set Color to blue and brightness to zero by H:240 S:255 V:0 */
    {LED_BLINK_HSV, SET_HSV(240, 255, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_ON, 1000},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 1000},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Color gradient by HSV with a priority level of 5.
 *
 */
static const blink_step_t color_hsv_ring_blink[] = {
    /*!< Set Color to RED */
    {LED_BLINK_HSV, SET_HSV(0, MAX_SATURATION, MAX_BRIGHTNESS), 0},
    {LED_BLINK_HSV_RING, SET_HSV(240, MAX_SATURATION, 127), 2000},
    {LED_BLINK_HSV_RING, SET_HSV(0, MAX_SATURATION, MAX_BRIGHTNESS), 2000},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Color gradient by RGB with a priority level of 5.
 *
 */
static const blink_step_t color_rgb_ring_blink[] = {
    /*!< Set Color to Green */
    {LED_BLINK_RGB, SET_RGB(0, 255, 0), 0},
    {LED_BLINK_RGB_RING, SET_RGB(255, 0, 255), 2000},
    {LED_BLINK_RGB_RING, SET_RGB(0, 255, 0), 2000},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Flowing lights with a priority level of 6(lowest).
 *        Insert the index:MAX_INDEX to control all the strips
 *
 */
static const blink_step_t flowing_blink[] = {
    {LED_BLINK_HSV, SET_IHSV(MAX_INDEX, 0, MAX_SATURATION, MAX_BRIGHTNESS), 0},
    {LED_BLINK_HSV_RING, SET_IHSV(MAX_INDEX, MAX_HUE, MAX_SATURATION, MAX_BRIGHTNESS), 2000},
    {LED_BLINK_LOOP, 0, 0},
};

blink_step_t const *led_mode[] = {
    [BLINK_DOUBLE_RED] = double_red_blink,
    [BLINK_TRIPLE_GREEN] = triple_green_blink,
    [BLINK_WHITE_BREATHE_SLOW] = breath_white_slow_blink,
    [BLINK_WHITE_BREATHE_FAST] = breath_white_fast_blink,
    [BLINK_BLUE_BREATH] = breath_blue_blink,
    [BLINK_COLOR_HSV_RING] = color_hsv_ring_blink,
    [BLINK_COLOR_RGB_RING] = color_rgb_ring_blink,
    [BLINK_FLOWING] = flowing_blink,
    [BLINK_MAX] = NULL,
};

esp_err_t my_led_strips_init()
{
    if (led_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_GPIO_NUM,                           // The GPIO that connected to the LED strip's data line
        .max_leds = WS2812_STRIPS_NUM,                               // The number of LEDs in the strip,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,                               // LED strip model
        .flags.invert_out = false,                                   // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = true,                // DMA feature is available on ESP target like ESP32-S3
    };

    led_indicator_strips_config_t strips_config = {
        .led_strip_cfg = strip_config,
        .led_strip_driver = LED_STRIP_RMT,
        .led_strip_rmt_cfg = rmt_config,
    };

    const led_indicator_config_t config = {
        .blink_lists = led_mode,
        .blink_list_num = BLINK_MAX,
    };

    gpio_set_direction(MY_LED_POWER, GPIO_MODE_OUTPUT);
    gpio_set_level(MY_LED_POWER, MY_LED_ON_LEVEL);
    esp_err_t ret = led_indicator_new_strips_device(&config, &strips_config, &led_handle);
    if (ret != ESP_OK) {
        assert(led_handle == NULL);
        led_handle = NULL;
    }
    else {
        assert(led_handle != NULL);
    }
    return ret;
}

esp_err_t my_led_strip_delete()
{
    if (!led_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = led_indicator_delete(led_handle);
    if (ret == ESP_OK) {
        led_handle = NULL;
        s_current_mode = -1;
    }
    return ret;
}

static esp_err_t my_led_strip_start_blink_internal(int blink_type)
{
    if (led_handle) {
        return led_indicator_start(led_handle, blink_type);
    }
    else {
        return ESP_ERR_INVALID_STATE;
    }
}

static esp_err_t my_led_strip_stop_blink_internal(int blink_type)
{
    if (led_handle) {
        return led_indicator_stop(led_handle, blink_type);
    }
    else {
        return ESP_ERR_INVALID_STATE;
    }
}

static esp_err_t my_led_strip_start_blink_preempt_internal(int blink_type)
{
    if (led_handle) {
        return led_indicator_preempt_start(led_handle, blink_type);
    }
    else {
        return ESP_ERR_INVALID_STATE;
    }
}

static esp_err_t my_led_strip_stop_blink_preempt_internal(int blink_type)
{
    if (led_handle) {
        return led_indicator_preempt_stop(led_handle, blink_type);
    }
    else {
        return ESP_ERR_INVALID_STATE;
    }
}

static esp_err_t my_led_strip_set_on_off_internal(bool on_off)
{
    if (led_handle) {
        return led_indicator_set_on_off(led_handle, on_off);
    }
    else {
        return ESP_ERR_INVALID_STATE;
    }
}

#define INSERT_RGB_INDEX(index, rgb) ((((index) & 0x7F) << 25) | ((rgb) & 0xFFFFFF))
static esp_err_t my_led_strip_set_brightness_internal(uint32_t brightness, uint32_t cur_rgb)
{
    if (led_handle) {
        uint32_t target_rgb = INSERT_RGB_INDEX(MAX_INDEX, cur_rgb);

        uint8_t r = GET_RED(target_rgb);
        uint8_t g = GET_GREEN(target_rgb);
        uint8_t b = GET_BLUE(target_rgb);

        if (my_cfg_led_calibrate.data.u32) {
            uint8_t r_c = GET_RED(my_cfg_led_calibrate.data.u32);
            uint8_t g_c = GET_GREEN(my_cfg_led_calibrate.data.u32);
            uint8_t b_c = GET_BLUE(my_cfg_led_calibrate.data.u32);

            r = (r * r_c / 255);
            g = (g * g_c / 255);
            b = (b * b_c / 255);
        }

        r = (r * brightness / 255);
        g = (g * brightness / 255);
        b = (b * brightness / 255);
        MY_LOGE("r:%d, g:%d, b:%d", r, g, b);
        // target_rgb = SET_IRGB(GET_INDEX(cur_rgb), r, g, b);
        target_rgb = SET_IRGB(MAX_INDEX, r, g, b);
        esp_err_t ret = led_indicator_set_rgb(led_handle, target_rgb);

        // if (my_cfg_led_temperature.data.u32 <= 0xffffff) {// 调整色温，实际也是通过调整rgb值来实现，要调整就直接改校准值
        //     uint32_t temp = my_cfg_led_temperature.data.u32 & 0xffffff;
        //     // temp = INSERT_RGB_INDEX(GET_INDEX(target_rgb), temp);
        //     temp = INSERT_RGB_INDEX(MAX_INDEX, temp);
        //     led_indicator_set_color_temperature(led_handle, temp);
        // }

        // led_indicator_set_brightness(led_handle, INSERT_INDEX(GET_INDEX(target_rgb), brightness));
        // led_indicator_set_brightness(led_handle, INSERT_INDEX(MAX_INDEX, brightness)); // 没法控制所有灯，不知道为啥，改成手动缩放rgb值
        return ret;
    }
    else {
        return ESP_ERR_INVALID_STATE;
    }
}

static esp_err_t my_led_strip_set_rgb_internal(uint32_t rgb_value)
{
    if (led_handle) {
        return led_indicator_set_rgb(led_handle, rgb_value);
    }
    else {
        return ESP_ERR_INVALID_STATE;
    }
}

esp_err_t my_led_set_brightness(uint8_t bri_percentage)
{
    if (s_current_mode != MY_LED_STRIPS_SINGLE_COLOR) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t target_brightness = 255;
    if (bri_percentage < 100) {
        if (bri_percentage <= 10) {
            // 如果亮度设置为0，则等于关闭，没有意义，所以强制设置为1
            target_brightness = 25;
        }
        else {
            target_brightness = bri_percentage * 255 / 100;
        }
    }

    return my_led_strip_set_brightness_internal(target_brightness, my_cfg_led_color.data.u32);
}

esp_err_t my_led_set_mode(int led_mode)
{
    if (WS2812_STRIPS_NUM == 0) {
        // 没有灯时
        return ESP_ERR_NOT_ALLOWED;
    }
    if (led_mode < 0 || led_mode >= MY_LED_MODE_MAX) {
        // 参数不对时
        return ESP_ERR_INVALID_ARG;
    }

    // 因为电源开关引脚可能无效，就算启动时led未使用，也要先初始化相关功能来关闭led
    led_indicator_handle_t _h = led_handle;
    if (!led_handle && my_led_strips_init() != ESP_OK) {
        s_current_mode = -1;
        return ESP_FAIL;
    }

    if (s_current_mode >= 0) {
        if (s_current_mode < BLINK_MAX) { // 如果记录的当前模式是闪烁模式，先停止
            my_led_strip_stop_blink_internal(s_current_mode);
        }
        led_indicator_set_rgb(led_handle, INSERT_RGB_INDEX(MAX_INDEX, 0)); // 关闭所有led
    }

    s_current_mode = led_mode;
    if (led_mode == BLINK_MAX) { // 当模式等于BLINK_MAX时，关闭led
        // led_indicator_set_rgb(led_handle, INSERT_RGB_INDEX(MAX_INDEX, 0)); // 这里关闭的上一个模式是控制所有led，所以不会有问题，如果不是控制所有led，不确定是否需要额外设置
        esp_err_t ret = led_indicator_set_on_off(led_handle, 0);
        if (_h) {
            gpio_set_level(MY_LED_POWER, !MY_LED_ON_LEVEL);
        }
        else { // 如果之前没有进行初始化，则关闭后释放资源
            my_led_full_clean();
        }
        return ret;
    }
    else { // 当模式为任意led开启模式时，要将控制引脚设置为开
        gpio_set_level(MY_LED_POWER, MY_LED_ON_LEVEL);
        if (led_mode == MY_LED_STRIPS_SINGLE_COLOR) {
            return my_led_set_brightness(my_cfg_led_brightness.data.u8);
        }
        else if (led_mode < BLINK_MAX) {
            return my_led_strip_start_blink_internal(led_mode);
        }
    }
    return ESP_FAIL;
}

esp_err_t my_led_start(bool switch_mode)
{
    if (!switch_mode) {
        if (my_cfg_led_mode.data.u8 >= MY_LED_MODE_MAX) {
            my_cfg_led_mode.data.u8 = BLINK_MAX;
            my_cfg_save_config_to_nvs(&my_cfg_led_mode);
        }
        return my_led_set_mode(my_cfg_led_mode.data.u8);
    }
    else {
        int target_mode = my_cfg_led_mode.data.u8 >= MY_LED_MODE_MAX ? MY_LED_STRIPS_SINGLE_COLOR : my_cfg_led_mode.data.u8 + 1;
        target_mode = target_mode % MY_LED_MODE_MAX;
        if (target_mode != my_cfg_led_mode.data.u8) {
            my_cfg_led_mode.data.u8 = target_mode;
            my_cfg_save_config_to_nvs(&my_cfg_led_mode);
        }
        return my_led_set_mode(target_mode);
    }
}

esp_err_t my_led_full_clean()
{
    if (WS2812_STRIPS_NUM == 0) {
        return ESP_OK;
    }
    esp_err_t ret = my_led_strip_delete();
    if (ret == ESP_OK) {
        gpio_reset_pin(WS2812_GPIO_NUM);
        gpio_reset_pin(MY_LED_POWER);
    }
    return ret;
}
#include "my_esp_timer.h"
#include "esp_timer.h"
#include "stdio.h"
#include "my_config.h"
#include "string.h"
#include "driver/gpio.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"
#ifdef MY_BAT_CHECK_EN_LEVEL
#if MY_BAT_CHECK_EN_LEVEL
#define MY_BAT_CHECK_EN_INPUT_MODE GPIO_PULLUP_ONLY
#else
#define MY_BAT_CHECK_EN_INPUT_MODE GPIO_PULLDOWN_ONLY
#endif
#endif

static const char *TAG = "my timer";

typedef struct
{
    adc_oneshot_unit_handle_t adc_handle;
    adc_unit_t adc_unit;
    adc_channel_t channel;
    adc_cali_handle_t cali_handle;
    int raw_value;
    int voltage;
} my_oneshot_adc_handle_t;
my_oneshot_adc_handle_t my_bat_adc_handle = {
    .adc_handle = NULL,
    .cali_handle = NULL,
    .raw_value = 0,
    .voltage = 0};
my_oneshot_adc_handle_t my_vbus_adc_handle = {
    .adc_handle = NULL,
    .cali_handle = NULL,
    .raw_value = 0,
    .voltage = 0,
};

esp_err_t my_timer_run(my_timer_handle_t *my_handle, my_func_run_type_t run_type, esp_timer_cb_t cb, void *arg, const char *name, bool skip_unhandled_events)
{
    esp_err_t ret = ESP_FAIL;
    if (!my_handle)
        return ret;

    if (run_type == MY_FUNC_START) // 启动类型为start时
    {
        if (!my_handle->handle) // 如果没有句柄，说明未初始化，先初始化
        {
            if (!cb || my_handle->timer_args) // 如果没有回调或存在定时器参数，直接返回
                return ESP_ERR_INVALID_ARG;

            my_handle->timer_args = malloc(sizeof(esp_timer_create_args_t));
            if (!my_handle->timer_args)
            {
                return ESP_ERR_NO_MEM;
            }
            else
            {
                my_handle->timer_args->callback = cb;
                my_handle->timer_args->arg = arg;
                my_handle->timer_args->dispatch_method = ESP_TIMER_TASK;
                my_handle->timer_args->name = name;
                my_handle->timer_args->skip_unhandled_events = skip_unhandled_events;
            }
            ret = esp_timer_create(my_handle->timer_args, &my_handle->handle);
            if (ret != ESP_OK)
            {
                free(my_handle->timer_args);
                return ret;
            }
        }

        my_handle->running = esp_timer_is_active(my_handle->handle);
        if (my_handle->running) // 如果正在运行，直接返回ok
            return ESP_OK;

        if (my_handle->is_periodic) // 周期性运行或运行一次
            ret = esp_timer_start_periodic(my_handle->handle, my_handle->timeout_us);
        else
            ret = esp_timer_start_once(my_handle->handle, my_handle->timeout_us);

        if (ret == ESP_OK)
            my_handle->running = 1;
    }
    else if (run_type == MY_FUNC_STOP)
    {
        if (!my_handle->handle) // 如果没有句柄，说明为初始化，返回错误
            return ESP_ERR_INVALID_ARG;
        my_handle->running = esp_timer_is_active(my_handle->handle); // 检查是否正在运行
        if (!my_handle->running)                                     // 如果已经停止，则返回成功
            return ESP_OK;
        ret = esp_timer_stop(my_handle->handle);
        if (ret == ESP_OK)
            my_handle->running = 0;
    }
    else if (run_type == MY_FUNC_DELETE)
    {
        if (!my_handle->handle) // 如果没有句柄，说明为初始化，返回错误
            return ESP_ERR_INVALID_STATE;
        my_handle->running = esp_timer_is_active(my_handle->handle); // 检查定时器是否正在运行
        if (my_handle->running)                                      // 如果定时器正在运行，先停止
        {
            ret = esp_timer_stop(my_handle->handle);
            if (ret != ESP_OK)
                return ret;
        }
        ret = esp_timer_delete(my_handle->handle);
        if (ret == ESP_OK)
        {
            my_handle->handle = NULL;
            free(my_handle->timer_args);
            my_handle->timer_args = NULL;
        }
    }
    else if (run_type == MY_FUNC_RESTART)
    {
        if (!my_handle->handle)
            return ESP_ERR_INVALID_STATE;
        my_handle->running = esp_timer_is_active(my_handle->handle);
        if (!my_handle->running) // 如果定时器处于停止状态，使用start启动
        {
            if (my_handle->is_periodic)
            {
                ret = esp_timer_start_periodic(my_handle->handle, my_handle->timeout_us);
            }
            else
            {
                ret = esp_timer_start_once(my_handle->handle, my_handle->timeout_us);
            }

            if (ret == ESP_OK)
                my_handle->running = 1;
        }
        else // 定时器正在运行，重启
        {
            ret = esp_timer_restart(my_handle->handle, my_handle->timeout_us);
        }
    }
    return ret;
}

void my_device_voltage_check(int *out_bat_voltage, int *out_usb_voltage)
{
    // 控制引脚，不打开时，无法检测电池电压
    // gpio_set_direction(MY_BAT_CHECK_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(MY_BAT_CHECK_EN_PIN, MY_BAT_CHECK_EN_LEVEL);
    adc_oneshot_read(my_vbus_adc_handle.adc_handle, my_vbus_adc_handle.channel, &my_vbus_adc_handle.raw_value);
    adc_oneshot_read(my_bat_adc_handle.adc_handle, my_bat_adc_handle.channel, &my_bat_adc_handle.raw_value);

    adc_cali_raw_to_voltage(my_vbus_adc_handle.cali_handle, my_vbus_adc_handle.raw_value, &my_vbus_adc_handle.voltage);
    adc_cali_raw_to_voltage(my_bat_adc_handle.cali_handle, my_bat_adc_handle.raw_value, &my_bat_adc_handle.voltage);
    gpio_set_level(MY_BAT_CHECK_EN_PIN, !MY_BAT_CHECK_EN_LEVEL);
    // gpio_reset_pin(MY_BAT_CHECK_EN_PIN);

    if (out_bat_voltage)
    {
        *out_bat_voltage = my_bat_adc_handle.voltage * MY_VOL_MEASURE_TO_ACTUAL_RATE;
    }
    if (out_usb_voltage)
    {
        *out_usb_voltage = my_vbus_adc_handle.voltage * MY_VOL_MEASURE_TO_ACTUAL_RATE;
    }
};

void my_device_voltage_check_timer_cb(void *arg)
{
    my_cfg_event_post(MY_CFG_EVENT_TIMER_CHECK_VOLTAGE, NULL, 0, 0);
};

esp_err_t my_device_voltage_check_timer_run()
{
    static my_timer_handle_t my_timer_handle = {
        .handle = NULL,
        .timer_args = NULL,
        .is_periodic = 1,
        .timeout_us = 2000 * 1000,
    };
    static const char *name = "vol check";
    if (!my_timer_handle.handle)
    {
        // 初始化引脚和启动定时器，两个adc引脚和一个控制引脚
        ESP_ERROR_CHECK(adc_continuous_io_to_channel(MY_USB_VBUS_CHECK_PIN, &my_vbus_adc_handle.adc_unit, &my_vbus_adc_handle.channel));
        ESP_ERROR_CHECK(adc_continuous_io_to_channel(MY_BAT_CHECK_PIN, &my_bat_adc_handle.adc_unit, &my_bat_adc_handle.channel));
        gpio_set_direction(MY_BAT_CHECK_EN_PIN, GPIO_MODE_OUTPUT);

        adc_oneshot_unit_init_cfg_t adc_init_config = {
            .unit_id = my_vbus_adc_handle.adc_unit,
        };

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = my_vbus_adc_handle.adc_unit,
            .chan = my_vbus_adc_handle.channel,
            .atten = ADC_ATTEN_DB_0,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };

        ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_config, &my_vbus_adc_handle.adc_handle));
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &my_vbus_adc_handle.cali_handle));
        if (my_vbus_adc_handle.adc_unit == my_bat_adc_handle.adc_unit)
        {
            my_bat_adc_handle.adc_handle = my_vbus_adc_handle.adc_handle;
            my_bat_adc_handle.cali_handle = my_vbus_adc_handle.cali_handle;
        }
        else
        {
            adc_init_config.unit_id = my_bat_adc_handle.adc_unit;
            ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_config, &my_bat_adc_handle.adc_handle));
            cali_config.unit_id = my_bat_adc_handle.adc_unit;
            cali_config.chan = my_bat_adc_handle.channel;
            ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &my_bat_adc_handle.cali_handle));
        }
        // 删除 adc_oneshot_del_unit(adc_handle);

        adc_oneshot_chan_cfg_t adc_config = {
            .atten = ADC_ATTEN_DB_0,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };

        ESP_ERROR_CHECK(adc_oneshot_config_channel(my_vbus_adc_handle.adc_handle, my_vbus_adc_handle.channel, &adc_config));
        ESP_ERROR_CHECK(adc_oneshot_config_channel(my_bat_adc_handle.adc_handle, my_bat_adc_handle.channel, &adc_config));
        return my_timer_run(&my_timer_handle, MY_FUNC_START, my_device_voltage_check_timer_cb, NULL, name, 1);
    }
    return ESP_OK;
    // todo 关闭定时器
}

// esp_timer_handle_t my_usb_check_timer_handle = NULL;
// 在usb连接事件时，激活定时器，每秒检查一次usb状态
// esp_err_t my_usb_check_timer_run(my_func_run_type_t run_type, esp_timer_cb_t cb, void *arg)
// {
//     static my_timer_handle_t my_usb_timer_handle = {
//         .handle = NULL,
//         .timer_args = NULL,
//         .is_periodic = 1,
//         .timeout_us = 1000 * 1000,
//     };
//     static const char *name = "usb_check";
//     return my_timer_run(&my_usb_timer_handle, run_type, cb, arg, name, 1);
// }

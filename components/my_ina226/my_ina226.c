// todo 如果有多设备需求，把函数改为接受一个my_ina226_handle_t参数，操作和返回值都通过这个句柄
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "ina226.h"
#include <stdio.h>

#include "my_config.h"
#include "my_ina226.h"

static const char *TAG = "my ina226";

#define MY_I2C_MASTER_SDA_IO MY_INA226_SDA
#define MY_I2C_MASTER_SCL_IO MY_INA226_SCL
#define MY_I2C_INA_ADDRESS MY_INA226_ADDR

typedef struct
{
    uint8_t addr;
    void *dev; // ina226_handle_t类型，可用于判断设备是否已初始化
    uint8_t is_connected;
    uint16_t retry_count;
    my_ina226_result_t result;
    struct
    {
        float shunt_resistance;
        float max_current;
        uint8_t averaging_mode;
        uint8_t shunt_voltage_conv_time;
        uint8_t bus_voltage_conv_time;
        uint8_t operating_mode;
    } setting;
} my_ina226_handle_t;

my_ina226_handle_t my_226_handle = {
    .addr = MY_I2C_INA_ADDRESS,
    .dev = NULL,
    .is_connected = 0,
    .result = {
        .bus_vol = 0,
        .shunt_vol = 0,
        .power = 0,
        .current = 0,
    },
    .setting = {
        .shunt_resistance = MY_INA226_SHUNT_RESISTANCE,
        .max_current = MY_INA226_MAX_CURRENT,
        .averaging_mode = INA226_AVG_MODE_4,
        .shunt_voltage_conv_time = INA226_VOLT_CONV_TIME_1_1MS,
        .bus_voltage_conv_time = INA226_VOLT_CONV_TIME_1_1MS,
        .operating_mode = INA226_OP_MODE_CONT_SHUNT_BUS,
    },
};

static i2c_master_bus_handle_t s_master_handle = NULL;
static TaskHandle_t s_ina226_task_handle = NULL;
void my_ina226_task(void *pvParameters);

uint8_t my_ina226_is_connected()
{
    return my_226_handle.is_connected;
};

esp_err_t my_ina226_get_result(float *bus_vol, float *current, float *power, float *shunt_vol)
{
    if (!my_226_handle.is_connected) {
        return ESP_FAIL;
    }

    if (bus_vol)
        *bus_vol = my_226_handle.result.bus_vol;
    if (current)
        *current = my_226_handle.result.current;
    if (power)
        *power = my_226_handle.result.power;
    if (shunt_vol)
        *shunt_vol = my_226_handle.result.shunt_vol;
    return ESP_OK;
};

void my_ina_i2c_master_bus_init(void)
{
    if (s_master_handle != NULL) {
        return;
    }
    i2c_master_bus_config_t master_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = 0,
        .scl_io_num = MY_I2C_MASTER_SCL_IO,
        .sda_io_num = MY_I2C_MASTER_SDA_IO,
        .intr_priority = 0,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&master_config, &s_master_handle));
};

esp_err_t my_i2c_check_device(uint8_t addr)
{
    if (!s_master_handle) {
        return ESP_FAIL;
    }
    return i2c_master_probe(s_master_handle, addr, 500);
};

void my_i2c_foreach_addr()
{
    if (s_master_handle) {
        for (size_t i = 1; i < INT8_MAX; i++) // 尝试扫描0x00以外的地址
        {
            if (i2c_master_probe(s_master_handle, i, 500) == ESP_OK) {
                ESP_LOGI("I2C", "I2C device found at 0x%02x", i);
            }
        }
    }
}

esp_err_t my_ina226_init(void)
{
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (!my_226_handle.dev) // 设备未初始化
    {
        ina226_config_t dev_cfg = I2C_INA226_CONFIG_DEFAULT;
        dev_cfg.i2c_address = my_226_handle.addr;
        dev_cfg.max_current = my_226_handle.setting.max_current;
        dev_cfg.shunt_resistance = my_226_handle.setting.shunt_resistance;
        dev_cfg.averaging_mode = my_226_handle.setting.averaging_mode;
        dev_cfg.shunt_voltage_conv_time = my_226_handle.setting.shunt_voltage_conv_time;
        dev_cfg.bus_voltage_conv_time = my_226_handle.setting.bus_voltage_conv_time;
        dev_cfg.operating_mode = my_226_handle.setting.operating_mode;

        ret = ina226_init(s_master_handle, &dev_cfg, (ina226_handle_t *)&my_226_handle.dev);
    }
    else if (!my_226_handle.is_connected) // 设备已初始化但未连接
    {
        ret = ina226_reset(my_226_handle.dev);
    }

    if (ret == ESP_OK) {
        my_226_handle.is_connected = 1;
        my_226_handle.retry_count = 0;
    }
    return ret;
};
void my_ina226_task_start(uint32_t StackDepth, unsigned int Priority, int core_id)
{
    if (s_ina226_task_handle) {
        return;
    }
    xTaskCreatePinnedToCore(my_ina226_task, "ina226", StackDepth, NULL, Priority, &s_ina226_task_handle, core_id);
}

void my_ina226_task(void *pvParameters)
{
    my_ina_i2c_master_bus_init();
    esp_err_t ret = ESP_OK;
    for (;;) {
        // 每次循环开始时，先确认设备是否已连接
        ret = my_i2c_check_device(my_226_handle.addr);
        if (ret == ESP_OK && my_226_handle.is_connected) // 设备已连接且已进行配置
        {
            ina226_get_bus_voltage(my_226_handle.dev, &my_226_handle.result.bus_vol);
            ina226_get_shunt_voltage(my_226_handle.dev, &my_226_handle.result.shunt_vol);
            ina226_get_current(my_226_handle.dev, &my_226_handle.result.current);
            ina226_get_power(my_226_handle.dev, &my_226_handle.result.power);
        }
        else if (ret == ESP_OK && !my_226_handle.is_connected) // 设备可能是重新连接
        {
            my_ina226_init(); // 重新进行配置
        }
        else if (ret != ESP_OK && my_226_handle.is_connected) {
            my_226_handle.retry_count++;
            if (my_226_handle.retry_count > 5) {
                my_226_handle.is_connected = 0;
            }
        }
        else {
            my_226_handle.retry_count++;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    };

    ina226_delete(my_226_handle.dev);
    my_226_handle.dev = NULL;
    vTaskDelete(NULL);
};

// void my_i2c_init(void)
// {
//     i2c_master_bus_config_t master_config = {
//         .clk_source = I2C_CLK_SRC_DEFAULT,
//         .i2c_port = 0,
//         .scl_io_num = MY_I2C_MASTER_SCL_IO,
//         .sda_io_num = MY_I2C_MASTER_SDA_IO,
//         .intr_priority = 0,
//         .glitch_ignore_cnt = 7,
//         .flags.enable_internal_pullup = true,
//     };
//     i2c_master_bus_handle_t master_handle;
//     i2c_new_master_bus(&master_config, &master_handle);

//     i2c_device_config_t dev_config = {
//         .dev_addr_length = I2C_ADDR_BIT_LEN_7,
//         .device_address = MY_I2C_INA_ADDRESS,
//         .scl_speed_hz = 100000,
//     };
//     i2c_master_dev_handle_t dev_handle;

//     i2c_master_bus_add_device(master_handle, &dev_config, &dev_handle);
//     i2c_master_transmit();
//     i2c_master_probe(master_handle, MY_I2C_INA_ADDRESS)
// }
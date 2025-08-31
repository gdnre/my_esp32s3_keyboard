/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_log.h"
// #include "nvs_flash.h"
#include "esp_bt.h"
#include "sdkconfig.h"

#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/hid/ble_svc_hid.h"
#else
#include "esp_bt_defs.h"
#if CONFIG_BT_BLE_ENABLED
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#endif
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#endif

#include "esp_hidd.h"
#include "my_esp_hid_gap.h"
#include "my_ble_hid.h"
#include "my_init.h"
#include "my_usb_hid.h"
#include "my_usb.h" //用于检查usb状态，如果usb hid设备已连接，就不设置output报告

__attribute__((weak)) void my_ble_state_change_cb(uint8_t is_connected)
{
    return;
};

static const char *TAG = "my ble hid";

typedef struct
{
    TaskHandle_t task_hdl;
    esp_hidd_dev_t *hid_dev;
    uint8_t protocol_mode;
    uint8_t *buffer;
} local_param_t;

static local_param_t s_ble_hid_param = {0};

uint8_t my_ble_inited = 0;     // 基本的蓝牙协议栈是否初始化
uint8_t my_ble_hid_inited = 0; // 蓝牙hid设备是否初始化
uint8_t my_ble_hid_usable = 0; // 蓝牙hid设备是否已经可用
#if CONFIG_BT_NIMBLE_ENABLED
uint8_t my_nimble_running = 0; // nimble任务是否正在运行
#endif

static esp_hid_raw_report_map_t ble_hid_report_maps[] = {
    {.data = NULL,
     .len = 0},
};

static esp_hid_device_config_t ble_hid_config = {
    .vendor_id = 0x16C0,
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = "ESP32S3BLEKB",
    .manufacturer_name = "Espressif",
    .serial_number = "1.0",
    .report_maps = ble_hid_report_maps,
    .report_maps_len = 1};

// 在ble gap验证成功后也会被调用
void my_ble_hidd_usable_event_cb(void)
{
    my_ble_hid_usable = 1;
#if CONFIG_BT_NIMBLE_ENABLED
    esp_hidd_dev_battery_set(s_ble_hid_param.hid_dev, 50);
#endif
    my_ble_state_change_cb(my_ble_hid_usable);
}
#if CONFIG_BT_NIMBLE_ENABLED
//
static void my_nimble_output_event_cb(uint8_t report_type, uint8_t report_id, uint8_t *data, uint8_t data_size)
{

    if ((my_usbd_get_used_type() & MY_USBD_HID) && my_usbd_ready())
    {
        return;
    }

    switch (report_type)
    {
    case BLE_SVC_HID_RPT_TYPE_OUTPUT:
        if (report_id == my_kb_led_report.report_id && data_size > 0)
        {
            // ESP_LOGE(__func__, "out id:%d, data: %d ", report_id, data[0]);
            if (data_size == 1)
            {
                my_kb_led_report.raw = data[0];
            }
            else if (data_size == 2)
            {
                my_kb_led_report.raw = data[1];
            }
            else
            {
                return;
            }
        }
        else if (report_id == 0 && data_size == 2 && data[0] == my_kb_led_report.report_id)
        {
            // ESP_LOGE(__func__, "out id:%d, data: %d ", data[0], data[1]);
            my_kb_led_report.raw = data[1];
        }
        else
        {
            return;
        }
        esp_event_post(MY_HID_EVTS, MY_HID_OUTPUT_GET_REPORT_EVENT, &my_kb_led_report.raw, 1, 0);
        break;
    default:
        break;
    }
}
#endif

static void my_ble_hidd_event_cb(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
    static const char *TAG = __func__;

    switch (event)
    {
    case ESP_HIDD_START_EVENT:
    {
        ESP_LOGD(TAG, "START");
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_CONNECT_EVENT:
    {
        my_ble_hid_usable = 1;
        ESP_LOGD(TAG, "CONNECT");
        break;
    }
    case ESP_HIDD_PROTOCOL_MODE_EVENT:
    {
        ESP_LOGD(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index, param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
        break;
    }
    case ESP_HIDD_CONTROL_EVENT:
    {
        ESP_LOGD(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index, param->control.control ? "EXIT_" : "");
        if (param->control.control)
        {
            // exit suspend
            my_ble_hidd_usable_event_cb();
        }
        else
        {
            my_ble_hid_usable = 0;
            ESP_LOGD(TAG, "SUSPEND");
        }
        break;
    }
#if !CONFIG_BT_NIMBLE_ENABLED // nimble没有支持输出报告，使用另外的函数
    case ESP_HIDD_OUTPUT_EVENT:
    {
        ESP_LOGD(TAG, "get hid report size: %d, report id: %d ", param->output.length, param->output.report_id);
        // ESP_LOG_BUFFER_HEX(__func__, param->output.data, param->output.length);
        break;
    }
#endif
    case ESP_HIDD_FEATURE_EVENT:
    {
        ESP_LOGD(TAG, "feature report");
        // ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDD_DISCONNECT_EVENT:
    {
        my_ble_hid_usable = 0;
#if CONFIG_BT_NIMBLE_ENABLED
        if (my_nimble_running)
#endif
            esp_hid_ble_gap_adv_start();
        my_ble_state_change_cb(my_ble_hid_usable);
        break;
    }
    case ESP_HIDD_STOP_EVENT:
    {
        my_ble_hid_usable = 0;
        ESP_LOGD(TAG, "STOP");
        break;
    }
    default:
        break;
    }
    return;
}

#if CONFIG_BT_NIMBLE_ENABLED
void ble_hid_device_host_task(void *param)
{
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}
void ble_store_config_init(void);
#endif

uint8_t my_ble_hidd_usable_check()
{
    return my_ble_hid_usable;
}

esp_err_t my_ble_hidd_batttery_level_set(uint8_t level)
{
    if (!my_ble_hid_usable)
    {
        return ESP_FAIL;
    }

    return esp_hidd_dev_battery_set(s_ble_hid_param.hid_dev, level);
}

esp_err_t my_ble_hidd_send_report(size_t report_id, uint8_t *data, size_t length)
{
    if (!my_ble_hid_usable)
    {
        return ESP_FAIL;
    }
    esp_err_t ret = esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, report_id, data, length);
    return ret;
}

esp_err_t my_ble_hid_start(void)
{
    esp_err_t ret = ESP_OK;
    ble_hid_report_maps[0].data = hid_report_descriptor;
    ble_hid_report_maps[0].len = hid_report_descriptor_len;
    // 初始化蓝牙栈
    if (!my_ble_inited)
    {
        my_nvs_init();
        my_esp_default_event_loop_init();
        my_get_chip_mac();

        ret = my_esp_hid_gap_init(HID_DEV_MODE);
        ESP_ERROR_CHECK(ret);

        ret = my_esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_GENERIC, ble_hid_config.device_name);
        ESP_ERROR_CHECK(ret);
#if CONFIG_BT_NIMBLE_ENABLED
        my_nimble_register_report_cb(BLE_SVC_HID_RPT_TYPE_OUTPUT, my_nimble_output_event_cb);
        ble_store_config_init();
        ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
#elif CONFIG_BT_BLE_ENABLED
        ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler);
        ESP_ERROR_CHECK(ret);
#endif
        my_ble_inited = 1;
    }
    // 初始化蓝牙hid设备栈
    if (!my_ble_hid_inited)
    {
        ret = esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, my_ble_hidd_event_cb, &s_ble_hid_param.hid_dev);
        my_ble_hid_inited = 1;
    }

#if CONFIG_BT_NIMBLE_ENABLED
    if (!my_nimble_running && (esp_nimble_enable(ble_hid_device_host_task) == ESP_OK))
    {
        my_nimble_running = 1;
    }
#endif
    return ret;
}

esp_err_t my_ble_hid_stop(void)
{
    if (!my_ble_hid_inited)
    {
        return ESP_OK;
    }
    esp_err_t ret = esp_hidd_dev_deinit(s_ble_hid_param.hid_dev);
    ESP_ERROR_CHECK(ret);
    ble_gatts_reset();
    ble_gatts_stop();
    my_ble_hid_usable = 0;
    my_ble_hid_inited = 0;
    s_ble_hid_param.hid_dev = NULL;
    return ret;
}

esp_err_t my_ble_stop(void)
{
#if CONFIG_BT_NIMBLE_ENABLED
    if (my_nimble_running)
    {
        my_nimble_running = 0;
        my_ble_inited = 0;
        esp_err_t ret = esp_nimble_disable();
        ESP_ERROR_CHECK(ret);
        ret = my_ble_hid_stop();
        ESP_ERROR_CHECK(ret);
        ret = my_esp_hid_gap_deinit();
        ESP_ERROR_CHECK(ret);
        return ret;
    }
    return ESP_OK;
#else
    esp_err_t ret = my_ble_hid_stop();
    if (ret != ESP_OK)
    {
        return ret;
    }
    my_ble_inited = 0;
    ret = esp_bluedroid_disable();
    if (ret == ESP_OK)
    {
        ret = esp_bluedroid_deinit();
    }
    return ret;
#endif
}

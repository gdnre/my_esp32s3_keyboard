/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* DESCRIPTION:
根据官方示例修改，示例使用到fat文件系统，当设备(esp32)连接到主机时，会取消fat文件系统挂载，当断开时，会自动挂载fat文件系统
 * This example contains code to make ESP32 based device recognizable by USB-hosts as a USB Mass Storage Device.
 * It either allows the embedded application i.e. example to access the partition or Host PC accesses the partition over USB MSC.
 * They can't be allowed to access the partition at the same time.
 * For different scenarios and behaviour, Refer to README of this example.
 */

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_partition.h"
#include "sdkconfig.h"
#include "tusb_msc_storage.h"
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#ifdef CONFIG_EXAMPLE_STORAGE_MEDIA_SDMMC
#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#include "sdmmc_cmd.h"
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif // CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
#endif
#include "my_msc.h"
#include "my_usb.h"

static const char *TAG = "my msc";

static void storage_mount_changed_cb(tinyusb_msc_event_t *event)
{
    ESP_LOGD(TAG, "Storage mounted to application: %s", event->mount_changed_data.is_mounted ? "Yes" : "No");
}

static wl_handle_t my_msc_wl_handle = WL_INVALID_HANDLE;
esp_err_t my_msc_init(const char *partition_label)
{
    const esp_partition_t *fat_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, partition_label);
    if (!fat_part)
        return ESP_ERR_NOT_FOUND;
    if (my_msc_wl_handle != WL_INVALID_HANDLE)
        return ESP_OK;

    esp_err_t ret = wl_mount(fat_part, &my_msc_wl_handle);
    if (ret != ESP_OK)
        return ret;

    const tinyusb_msc_spiflash_config_t config_spi = {
        .wl_handle = my_msc_wl_handle,
        .callback_mount_changed = storage_mount_changed_cb,
        .mount_config = VFS_FAT_MOUNT_DEFAULT_CONFIG(),
    };
    ret = tinyusb_msc_storage_init_spiflash(&config_spi);
    if (ret != ESP_OK) {
        wl_handle_t temp = my_msc_wl_handle;
        my_msc_wl_handle = WL_INVALID_HANDLE;
        wl_unmount(temp);
    }
    return ret;
}

// 依次卸载msc配置的fatfs，删除msc handle使回调失效，取消
void my_msc_deinit(void)
{

    tinyusb_msc_storage_unmount();
    tinyusb_msc_storage_deinit();
    if (my_msc_wl_handle == WL_INVALID_HANDLE)
        return;

    wl_handle_t temp = my_msc_wl_handle;
    my_msc_wl_handle = WL_INVALID_HANDLE;
    if (wl_unmount(temp) != ESP_OK) {
        ESP_LOGW(TAG, "%s NOTICE! can't unmount wl_handle", __func__);
    }
    ESP_LOGI(TAG, "USB MSC deinitialization DONE");
}

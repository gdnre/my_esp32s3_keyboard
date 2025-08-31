#include "my_usb.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_partition.h"
#include "my_init.h"
#include "my_msc.h"
#include "my_usb_hid.h"
#include "my_usbd_descriptor.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include <stdlib.h>

// 注意：在设备切换功能时可能导致识别延迟，具体表现为主机过1、2s才正确连接设备。
// 如果一直使用usbhid，或者偶尔切换msc时，都没有问题，体感从jtag切换到msc后出现该问题的概率大。
// 不确定是电路问题还是软件问题还是hub芯片问题

static const char *TAG = "my usb";

extern tusb_desc_device_t my_msc_device_descriptor;
extern tusb_desc_device_t my_hid_device_descriptor;
const char *MY_HID_EVTS = "MY_HID_EVTS";

// my_usb_state_change_cb_t my_usb_state_change_cb = NULL;
int8_t my_usbd_used_type = MY_USBD_NULL;
uint8_t my_usb_connected = 0;
int8_t my_usbd_get_used_type()
{
    return my_usbd_used_type;
}

uint8_t my_usbd_ready()
{
    return tud_ready();
}

esp_err_t my_usb_start(int8_t device_type)
{
    if (device_type >= MY_USBD_MAX)
        return ESP_ERR_NOT_SUPPORTED;
    esp_err_t ret = ESP_OK;
    if (device_type == MY_USBD_NULL) {
        if (my_usbd_used_type == MY_USBD_NULL)
            return ESP_OK;
        if (my_usbd_used_type & MY_USBD_MSC) {
            my_msc_deinit();
        }
        if (my_usbd_used_type & MY_USBD_HID) {
            my_usb_hid_deinit();
        }
        my_usbd_used_type = MY_USBD_NULL;
        return tinyusb_driver_uninstall();
    }
    else if (my_usbd_used_type != MY_USBD_NULL) // && device_type != MY_USBD_NULL
    {                                           // 当已经进行过初始化，没有解除安装时，返回错误
        return ESP_FAIL;
    }

    tinyusb_config_t tusb_cfg = {
        .external_phy = false,
    };
    if (device_type == MY_USBD_MSC) {
        ret = my_msc_init(NULL);
        my_msc_device_descriptor.idProduct = my_chip_mac16 + 1;
        tusb_cfg.device_descriptor = &my_msc_device_descriptor;
        tusb_cfg.string_descriptor = string_msc_desc_arr;
        tusb_cfg.string_descriptor_count = string_msc_desc_arr_size;
        tusb_cfg.configuration_descriptor = msc_fs_configuration_desc;
    }
    else if (device_type == MY_USBD_HID) {
        my_hid_device_descriptor.idProduct = my_chip_mac16;
        tusb_cfg.device_descriptor = &my_hid_device_descriptor;
        tusb_cfg.string_descriptor = string_hid_desc_arr;
        tusb_cfg.string_descriptor_count = string_hid_desc_arr_size;
        tusb_cfg.configuration_descriptor = hid_configuration_descriptor;
    }
    else {
        return ESP_FAIL;
    }
    ret = tinyusb_driver_install(&tusb_cfg);

    if (ret == ESP_OK) {
        if (device_type == MY_USBD_HID) {
            my_usb_hid_init();
        }

        my_usbd_used_type = device_type;
    }

    return ret;
}

__attribute__((weak)) void my_usb_state_change_cb(uint8_t is_mount)
{
    return;
};

// Invoked when device is unmounted
// hid设备可能基本不会触发
void tud_umount_cb(void)
{
    my_usb_connected = 0;
    if (my_usbd_get_used_type() & MY_USBD_MSC) {
        tinyusb_msc_storage_mount(NULL);
    }
    if (my_usbd_get_used_type() & MY_USBD_HID) {
        my_usb_state_change_cb(0);
    }
}

// Invoked when device is mounted (configured)
void tud_mount_cb(void)
{
    my_usb_connected = 1;
    if (my_usbd_get_used_type() & MY_USBD_MSC) {
        tinyusb_msc_storage_unmount();
    }
    if (my_usbd_get_used_type() & MY_USBD_HID) {
        my_usb_state_change_cb(1);
    }
}

// Invoked when usb bus is suspended
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    my_usb_connected = 0;
    if (my_usbd_get_used_type() & MY_USBD_HID) {
        my_usb_state_change_cb(0);
    }
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    my_usb_connected = 1;
    if (my_usbd_get_used_type() & MY_USBD_HID) {
        my_usb_state_change_cb(1);
    }
}

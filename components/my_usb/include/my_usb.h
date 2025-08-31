#pragma once
#include "stdint.h"
#include "sdkconfig.h"
#include "esp_err.h"

typedef enum
{
    MY_USBD_NULL = -1,
    MY_USBD_DEFAULT = 0x00,
    MY_USBD_MSC = 0x01,
    MY_USBD_HID = 0x02,
    MY_USBD_MSC_HID = MY_USBD_MSC | MY_USBD_HID,
    MY_USBD_COMPOSITE = 0x04,
    MY_USBD_MAX,
} my_usbd_type_t;

// typedef void (*my_usb_state_change_cb_t)(uint8_t state);
// extern my_usb_state_change_cb_t my_usb_state_change_cb;

void my_usb_state_change_cb(uint8_t is_mount);

// 获取当前初始化的usb设备类型
int8_t my_usbd_get_used_type();
uint8_t my_usbd_ready();

/// @brief 开启usb，或者切换usb功能
/// @param device_type 传入-1，表示关闭usb，1表示msc模式，2表示hid模式，如果之前已经进行过初始化，需要先传入-1关闭后再传入新的参数。
/// @return
esp_err_t my_usb_start(int8_t device_type);

extern uint8_t my_usb_connected;
extern const char *MY_HID_EVTS;
typedef enum
{
    MY_HID_OUTPUT_GET_REPORT_EVENT = 0x01,
} my_hid_event_id_t;
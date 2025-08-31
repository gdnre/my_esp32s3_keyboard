#include "my_usbd_descriptor.h"
#include "my_init.h"
#include "tinyusb.h"

#define MY_MSC_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_INOUT_DESC_LEN)

#pragma region msc描述符
// msc字符串描述符
char const *string_msc_desc_arr[] = {
    MY_USBD_STR_DESC,
    "esp32s3 MSC", // 4. MSC
};
uint8_t string_msc_desc_arr_size = sizeof(string_msc_desc_arr) / sizeof(string_msc_desc_arr[0]);

// msc设备描述符
tusb_desc_device_t my_msc_device_descriptor = {
    .bLength = sizeof(my_msc_device_descriptor),
    .bDescriptorType = TUSB_DESC_DEVICE, // 设备描述符
    .bcdUSB = 0x0200,                    // usb版本2.0
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,
    .idProduct = 0x4003, // my_chip_mac16+1
    .bcdDevice = 0x010,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01};

// msc配置描述符
uint8_t const msc_fs_configuration_desc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, MY_MSC_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(0, 4, EDPT_OUT_01, EDPT_IN_01, 64),
};

#pragma endregion

#pragma region hid描述符

// 键盘字符串描述符
char const *string_hid_desc_arr[] = {
    MY_USBD_HID_STR_DESC,
    "esp32s3 kb",
};
uint8_t string_hid_desc_arr_size = sizeof(string_hid_desc_arr) / sizeof(string_hid_desc_arr[0]);

// hid设备描述符
tusb_desc_device_t my_hid_device_descriptor = {
    .bLength = sizeof(my_hid_device_descriptor),
    .bDescriptorType = TUSB_DESC_DEVICE, // 设备描述符
    .bcdUSB = 0x0200,                    // usb版本2.0
    .bDeviceClass = TUSB_CLASS_UNSPECIFIED,
    .bDeviceSubClass = TUSB_CLASS_UNSPECIFIED,
    .bDeviceProtocol = TUSB_CLASS_UNSPECIFIED,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,
    .idProduct = 0x4002, // my_chip_mac16
    .bcdDevice = 0x010,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01};

#define MY_KB_HID_DESCRIPTOR                                                                        \
    0x05, 0x01,                                            /* Usage Page (Generic Desktop Ctrls) */ \
        0x09, 0x06,                                        /* Usage (Keyboard) */                   \
        0xA1, 0x01,                                        /* Collection (Application) */           \
        0x85, MY_REPORTID_KEYBOARD,                        /*   Report ID (1) */                    \
        0x05, 0x07,                                        /*   Usage Page (Kbrd/Keypad) */         \
        0x15, 0x00,                                        /*   Logical Minimum (0) */              \
        0x25, 0x01, /*Logical Maximum (1) */               /* Modifier Keys 1B */                   \
        0x19, 0xE0,                                        /*   Usage Minimum (0xE0) */             \
        0x29, 0xE7,                                        /*   Usage Maximum (0xE7) */             \
        0x75, 0x01,                                        /*   Report Size (1) */                  \
        0x95, 0x08,                                        /*   Report Count (8) */                 \
        0x81, 0x02, /*Input (Data,Var,Abs) */              /* Reserved 1B */                        \
        0x75, 0x01,                                        /*   Report Size (1) */                  \
        0x95, 0x08,                                        /*   Report Count (8) */                 \
        0x81, 0x03, /*Input (Const,Var,Abs) */             /* LED Output 1B (Host → Device) */      \
        0x05, 0x08,                                        /*   Usage Page (LEDs) */                \
        0x19, 0x01,                                        /*   Usage Minimum (Num Lock) */         \
        0x29, 0x08,                                        /*   Usage Maximum (Do Not Disturb) */   \
        0x75, 0x01,                                        /*   Report Size (1) */                  \
        0x95, 0x08,                                        /*   Report Count (8) */                 \
        0x91, 0x02, /*Output (Data,Var,Abs,Non-volatile)*/ /* Keycodes 17B */                       \
        0x05, 0x07,                                        /*   Usage Page (Kbrd/Keypad) */         \
        0x19, 0x00,                                        /*   Usage Minimum (0x00) */             \
        0x29, 0x87,                                        /*   Usage Maximum (0x87) */             \
        0x75, 0x01,                                        /*   Report Size (1) */                  \
        0x95, 0x88,                                        /*   Report Count (136) */               \
        0x81, 0x02,                                        /*   Input (Data,Var,Abs) */             \
                                                                                                    \
        0xC0 /* End Collection */

// hid报告描述符
const uint8_t hid_report_descriptor[] = {
    MY_KB_HID_DESCRIPTOR,
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(MY_REPORTID_CONSUMER))};

uint16_t hid_report_descriptor_len = sizeof(hid_report_descriptor);

// hid配置描述符
const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),
    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_INOUT_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), EDPT_OUT_01, EDPT_IN_01, CFG_TUD_ENDPOINT0_SIZE, 1)};

#pragma endregion

// const char my_kb_hid_descriptor[] =
//     {
//         0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
//         0x09, 0x06, // Usage (Keyboard)
//         0xA1, 0x01, // Collection (Application)

//         0x85, MY_REPORTID_KEYBOARD, //   Report ID (99)
//         0x05, 0x07,                 //   Usage Page (Kbrd/Keypad)
//         0x15, 0x00,                 //   Logical Minimum (0)
//         0x25, 0x01,                 //   Logical Maximum (1)

//         // 修饰键 1B
//         0x19, 0xE0, //   Usage Minimum (0xE0)
//         0x29, 0xE7, //   Usage Maximum (0xE7)
//         0x75, 0x01, //   Report Size (1)
//         0x95, 0x08, //   Report Count (8)
//         0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

//         // 保留 1B
//         0x75, 0x01, //   Report Size (1)
//         0x95, 0x08, //   Report Count (8)
//         0x81, 0x03, //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

//         // led状态（主机到设备） 1B
//         0x05, 0x08, //   Usage Page (LEDs)
//         0x19, 0x01, //   Usage Minimum (Num Lock)
//         0x29, 0x08, //   Usage Maximum (Do Not Disturb)
//         0x75, 0x01, //   Report Size (1)
//         0x95, 0x08, //   Report Count (8)
//         0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)

//         // 按键 17B
//         0x05, 0x07, //   Usage Page (Kbrd/Keypad)
//         0x19, 0x00, //   Usage Minimum (0x00)
//         0x29, 0x87, //   Usage Maximum (0x87)
//         0x75, 0x01, //   Report Size (1)
//         0x95, 0x88, //   Report Count (136)
//         0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

//         0xC0, // End Collection
//               // 54 bytes
// };

// const char my_consumer_hid_descriptor[] = {
//     TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(MY_REPORTID_CONSUMER))};
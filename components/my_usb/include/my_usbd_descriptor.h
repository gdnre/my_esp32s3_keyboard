#pragma once
// #include "tinyusb.h"
#include "stdbool.h"
#include "stdint.h"

typedef enum {
    MY_REPORTID_KEYBOARD = 0x01,
    MY_REPORTID_CONSUMER = 0x02,
    MY_REPORTID_SYSTEM,
    MY_REPORTID_MOUSE,
    MY_REPORTID_GAMEPAD,
    // MY_REPORTID_LIGHTING,
    // MY_REPORTID_ABSMOUSE,
    // MY_REPORTID_GENERIC_INOUT,
    // MY_REPORTID_FIDO_U2F,
} my_report_id_t;

typedef struct __attribute__((packed)) {
    uint8_t report_id;
    uint8_t modifier;
    uint8_t reserved;
    uint8_t keycodes[17];
} my_keyboard_report_t;

typedef struct __attribute__((packed)) {
    uint8_t report_id;
    union {
        uint8_t raw;
        struct
        {
            uint8_t num_lock : 1;
            uint8_t caps_lock : 1;
            uint8_t scroll_lock : 1;
            uint8_t compose : 1;
            uint8_t kana : 1;
            uint8_t shift : 1;
            uint8_t unused : 2;
        };
    };
} my_kb_led_report_t;

typedef struct __attribute__((packed)) {
    uint8_t report_id;
    union {
        uint8_t consumer_code[2];
        uint16_t consumer_code16;
    };

} my_consumer_report_t;

enum {
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN = 0x80, // 1000 0000

    EDPT_OUT_01 = 0x01, // 0000 0001
    EDPT_IN_01 = 0x81,  // 1000 0001
};

#define MY_USBD_STR_DESC                   \
    (char[]){0x09, 0x04},                  \
        "Espressif",   /*1: Manufacturer*/ \
        "TinyUSB MSC", /*2: Product*/      \
        "01"           /*3: Serials*/

#define MY_USBD_HID_STR_DESC               \
    (char[]){0x09, 0x04},                  \
        "Espressif",   /*1: Manufacturer*/ \
        "TinyUSB HID", /*2: Product*/      \
        "01"           /*3: Serials*/
// extern tusb_desc_device_t my_msc_device_descriptor;
// extern tusb_desc_device_t my_hid_device_descriptor;

extern uint8_t const msc_fs_configuration_desc[];

extern char const *string_msc_desc_arr[];
extern uint8_t string_msc_desc_arr_size;

extern char const *string_hid_desc_arr[];
extern uint8_t string_hid_desc_arr_size;

extern const uint8_t hid_report_descriptor[];
extern uint16_t hid_report_descriptor_len;
#define MY_HID_REPORT_DESC_LEN 80

extern const uint8_t hid_configuration_descriptor[];

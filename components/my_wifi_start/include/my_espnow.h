#pragma once
#include "esp_err.h"
#include "esp_now.h"
#include "sdkconfig.h"

typedef enum {
    MY_ESPNOW_DATA_TYPE_CONTROL = 0,
    MY_ESPNOW_DATA_TYPE_ACK = 1,
    MY_ESPNOW_DATA_TYPE_NACK,
    MY_ESPNOW_DATA_TYPE_ACK_END,
    MY_ESPNOW_DATA_TYPE_HID_INPUT,      // 数据是hid input报告，收到后要通过usb发送给主机
    MY_ESPNOW_DATA_TYPE_GET_HID_OUTPUT, // 无数据，收到后将本机存储的output报告发送给发送方
    MY_ESPNOW_DATA_TYPE_HID_OUTPUT,     // 数据是hid output报告，收到后要更新报告状态，触发回调
    MY_ESPNOW_DATA_TYPE_SCAN,
    MY_ESPNOW_DATA_TYPE_PAIRING,
    MY_ESPNOW_DATA_TYPE_I_AM_ONLINE, // 无数据，可以用于判断对方已上线，收到后将espnow设置为已连接
    MY_ESPNOW_DATA_TYPE_MAX_NUM,
} my_espnow_data_type_t;

typedef enum {
    MY_ESPNOW_CONTROL_RESET_PEER = 1,
} my_espnow_control_type_t;

typedef enum {
    MY_ESPNOW_HID_INPUT_REPORT = 0,
    MY_ESPNOW_HID_OUTPUT_REPORT = 1,
    MY_ESPNOW_HID_REPORT_TYPE_MAX = 2,
} my_espnow_hid_report_type_t;

typedef enum {
    MY_ESPNOW_FSM_BLOCK = 0,

    // 需要执行操作的状态

    MY_ESPNOW_FSM_SCAN,
    MY_ESPNOW_FSM_WAIT_PAIRING,

    // 以下为只接收单播的状态

    MY_ESPNOW_FSM_CHECK_PEER,
    MY_ESPNOW_FSM_CONNECTED,

    MY_ESPNOW_FSM_STATES_NUM,
} my_espnow_fsm_state_t; // 状态机状态

#define MY_ESPNOW_DATA_PAY_LOAD_MAXLEN 24

typedef struct
{
    uint8_t type;
    uint8_t data_len;
    uint16_t seq_num;
    uint16_t crc;
    union {
        struct
        {
            uint8_t need_ack : 1; // 是否需要发送ack
            uint8_t reserved : 7;
        };
        uint8_t raw;
    } flag;
    uint8_t reserved;
} __attribute__((packed)) my_espnow_data_head_t;

typedef struct
{
    my_espnow_data_head_t head;
    uint8_t payload[MY_ESPNOW_DATA_PAY_LOAD_MAXLEN];
} __attribute__((packed)) my_espnow_data_t;
#define MY_ESPNOW_MAX_DATA_LEN (sizeof(my_espnow_data_t))
#define MY_ESPNOW_MIN_DATA_LEN (sizeof(my_espnow_data_head_t))

typedef struct
{
    esp_now_peer_info_t peer_info;
    uint16_t send_seq_num;
    uint16_t recv_seq_num;
    uint16_t continuous_send_fail;
    uint8_t target_channel;
} my_espnow_device_t;

enum {
    MY_ESPNOW_PARAM_TYPE_SEND,
    MY_ESPNOW_PARAM_TYPE_RECV,
};

typedef struct
{
    uint8_t dev_index;
    my_espnow_data_t data;
} my_espnow_send_param_t;

typedef struct
{
    uint8_t is_broadcast;
    uint8_t src_mac[ESP_NOW_ETH_ALEN];
    my_espnow_data_t data;
} my_espnow_recv_param_t;

typedef struct
{
    uint8_t src_mac[ESP_NOW_ETH_ALEN];
    my_espnow_data_head_t data;
} my_espnow_ack_param_t;

typedef struct
{
    QueueHandle_t send_queue;
    QueueHandle_t recv_queue;
    QueueHandle_t ack_queue; // 发送数据后接收到ack的队列
    TaskHandle_t send_task_handle;
    TaskHandle_t recv_task_handle;
    my_espnow_device_t *devices;
    uint8_t device_num;
    uint8_t def_dev; // 当前默认使用的设备索引
    my_espnow_fsm_state_t fsm_state;
} my_espnow_handle_t;
typedef my_espnow_handle_t *my_espnow_handle_p;

esp_err_t my_espnow_init();
esp_err_t my_espnow_deinit();
esp_err_t my_espnow_re_pairing();
esp_err_t my_espnow_wait_recv_data(my_espnow_recv_param_t *recv_buf, BaseType_t *b_ret, uint32_t max_wait_time, uint8_t ack_broadcast);
// 向默认设备发送数据
// 具体为向发送队列中添加数据，数据指定发送对象为默认设备
// 数据会拷贝为副本
// 成功将数据添加到队列后返回espok(注意不保证数据发送成功)， 返回ESP_ERR_INVALID_ARG时，说明数据有问题，推荐不要尝试重发，其它返回值可以考虑重试
esp_err_t my_espnow_send_data_default(my_espnow_data_type_t data_type, uint8_t need_ack, uint8_t *payload_data, uint8_t data_len);
esp_err_t my_espnow_send_hid_output_report(uint8_t *data_with_id, uint8_t data_len);
esp_err_t my_espnow_send_hid_input_report(uint8_t *data_with_id, uint8_t data_len);

// 接收器接收到数据后，通过usb向键盘发送键盘报告的回调
void my_espnow_reciver_recv_input_report_cb(uint8_t *data, uint8_t data_len);
// 接收器接收到主机发送的输出报文后，向设备同步输出报文
void my_espnow_reciver_recv_get_output_cb();
void my_espnow_device_recv_output_report_cb(uint8_t *data, uint8_t data_len);



#define MY_ESPNOW_DEFAULT_ACK_DATA                 \
    {                                              \
        .type = MY_ESPNOW_DATA_TYPE_ACK,           \
        .data_len = sizeof(my_espnow_data_head_t), \
        .seq_num = 0,                              \
        .crc = 0,                                  \
        .flag.need_ack = 0,                        \
        .reserved = 0,                             \
    }

#define MY_ESPNOW_DEFAULT_NACK_DATA                \
    {                                              \
        .type = MY_ESPNOW_DATA_TYPE_NACK,          \
        .data_len = sizeof(my_espnow_data_head_t), \
        .seq_num = 0,                              \
        .crc = 0,                                  \
        .flag.need_ack = 0,                        \
        .reserved = 0,                             \
    }

#define MY_ESPNOW_DEFAULT_SCAN_DATA               \
    {                                             \
        .head = {                                 \
            .type = MY_ESPNOW_DATA_TYPE_SCAN,     \
            .data_len = sizeof(my_espnow_data_t), \
            .seq_num = 0,                         \
            .crc = 0,                             \
            .flag.need_ack = 1,                   \
            .reserved = 0,                        \
        },                                        \
        .payload = {0},                           \
    }

#define MY_ESPNOW_DEFAULT_PAIRING_DATA            \
    {                                             \
        .head = {                                 \
            .type = MY_ESPNOW_DATA_TYPE_PAIRING,  \
            .data_len = sizeof(my_espnow_data_t), \
            .seq_num = 0,                         \
            .crc = 0,                             \
            .flag.need_ack = 1,                   \
            .reserved = 0,                        \
        },                                        \
        .payload = {0},                           \
    }

#define MY_ESPNOW_DEFAULT_BROADCAST_DEVICE         \
    {                                              \
        .peer_info = {                             \
            .peer_addr = ESPNOW_BRAODCAST_MAC_ARR, \
            .lmk = MY_ESPNOW_LMK,                  \
            .encrypt = false,                      \
            .channel = 0,                          \
            .ifidx = MY_ESPNOW_WIFI_IF_INDEX,      \
            .priv = NULL,                          \
        },                                         \
        .recv_seq_num = 0,                         \
        .send_seq_num = 0,                         \
        .target_channel = MY_ESPNOW_CHANNEL,       \
    }
//

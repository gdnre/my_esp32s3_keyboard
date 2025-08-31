#pragma once
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_now.h"
#include "my_espnow.h"

#define MY_WIFI_DELAY_MAX pdMS_TO_TICKS(1000)

typedef enum
{
    MY_WIFI_SET_STOP = (1 << 0),
    MY_WIFI_SET_STA = (1 << 1),
    MY_WIFI_SET_AP = (1 << 2),
    MY_WIFI_SET_APSTA = (1 << 3),
    MY_WIFI_SET_ESPNOW = (1 << 4),
    MY_WIFI_SET_OPERATE_BITS_MASK = (MY_WIFI_SET_STOP | MY_WIFI_SET_STA | MY_WIFI_SET_AP | MY_WIFI_SET_APSTA | MY_WIFI_SET_ESPNOW),
    MY_STA_CONNECTED_BIT = (1 << 5),
    MY_STA_DISCONNECTED_BIT = (1 << 6),
    MY_STA_EVENT_BITS_MASK = (MY_STA_CONNECTED_BIT | MY_STA_DISCONNECTED_BIT),
    MY_AP_CONNECTED_BIT = (1 << 7),

} my_wifi_event_group_bit_t;

typedef struct
{
    uint8_t enable : 1;
    uint8_t inited : 1;
    uint8_t running : 1;
    uint8_t connected : 1;
    uint8_t user_data : 4;
} my_wifi_feature_state_t;

typedef struct
{
    struct
    {
        uint8_t netif_inited : 1;
        uint8_t wifi_inited : 1;
        uint8_t wifi_start : 1;
    };
    uint8_t used_features;
    uint8_t if_mode;
    uint8_t channel;
    wifi_second_chan_t second_chan;
    TaskHandle_t task_handle;
    EventGroupHandle_t evt_group_handle;
    my_wifi_feature_state_t sta;
    my_wifi_feature_state_t ap;
    my_wifi_feature_state_t espnow;
} my_wifi_info_t;

extern my_wifi_info_t s_wifi_info;

void my_espnow_data_prepare(my_espnow_data_t *data, uint16_t seq_num);
esp_err_t my_espnow_save_peer_to_nvs(uint8_t dev_index);
esp_err_t my_espnow_delete_peer_from_nvs();
esp_err_t my_espnow_add_peer(const uint8_t *peer_mac, uint8_t *out_index);
esp_err_t my_espnow_remove_peer(uint8_t index, const uint8_t *peer_mac);
esp_err_t my_espnow_get_device(const uint8_t *mac, my_espnow_device_t **out_device, uint8_t *out_index);
void my_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);
void my_espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len);
void my_espnow_send_task(void *pvParameter);
void my_espnow_recv_task(void *pvParameter);
esp_err_t my_espnow_send_ack(const uint8_t *peer_mac, uint16_t seq_num, uint8_t is_nack);

typedef void (*my_espnow_fsm_state_handler_t)(my_espnow_fsm_state_t state, void *arg);
// MY_ESPNOW_FSM_WAIT_PAIRING状态接收队列处理程序。
// 进入时，会清除当前所有广播地址以外的配对信息和已连接状态。
// 之后等待接收MY_ESPNOW_DATA_TYPE_SCAN数据，收到数据后将源地址加入peer列表，然后发送ack。
// 对方设备接收到ack后，应该发送MY_ESPNOW_DATA_TYPE_PAIRING数据，之后设备会直接认为配对成功，并回复ack。
// 对方设备即使没收到ack，我方设备也不会知道，想重新配对就通过按键设置重新进入该状态。
// 该程序应该由发送方（设备方、键盘方），而非接收器使用
void my_espnow_fsm_wait_pairing_recv_handler(my_espnow_fsm_state_t enter_state, void *task_handle);
// MY_ESPNOW_FSM_SCAN状态发送配对请求处理程序。
// 该程序应该由接收器使用，和my_espnow_fsm_wait_pairing_recv_handler为对应状态，用于设备之间配对。
// 该程序会不断切换信道发送扫描请求
void my_espnow_fsm_scan_send_handler(my_espnow_fsm_state_t enter_state, void *task_handle);
// 连接状态时，通用的espnow发送队列处理程序
void my_espnow_fsm_general_send_handler(my_espnow_fsm_state_t enter_state, void *task_handle);
// 连接状态时，通用的espnow接收队列处理程序
void my_espnow_fsm_general_recv_handler(my_espnow_fsm_state_t enter_state, void *task_handle);

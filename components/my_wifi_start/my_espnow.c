// todo 配对后，设备方发送自己使用的信道，让接收器切换到对应信道
// todo 配对后，双方交换lmk加密
// todo 接收器检查收到的hid报告，如果是按下状态，定期向设备轮询，确认设备在线，否则要及时释放按键
#include "esp_crc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "my_espnow.h"
#include "my_init.h"
#include "my_usb_hid.h"
#include "my_wifi_private.h"
#include "my_wifi_start.h"

static const char *TAG = "my espnow";

#pragma region // 变量和宏
#if CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0 == 1
#define MY_ESPNOW_TASK_CORE_ID 0
#else
#define MY_ESPNOW_TASK_CORE_ID 1
#endif
#define MY_ESPNOW_QUEUE_LENGTH (8)
#define MY_ESPNOW_PMK "GdnreEspNowKBPmk"
#define MY_ESPNOW_LMK "GdnreEspNowKBLmk"
#define MY_ESPNOW_WIFI_IF_INDEX ESP_IF_WIFI_STA
#define MY_ESPNOW_CHANNEL 6
#define MY_ESPNOW_MAX_PEER_NUM 1 // 除了广播设备外，最多允许的peer数量
// #define MY_ESPNOW_WAKE_WINDOW (200)
// #define MY_ESPNOW_WAKE_INTERVAL (1000)
#define ESPNOW_BRAODCAST_MAC_ARR {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
static uint8_t espnow_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const char *my_nvs_namespace = "my_now";
static nvs_handle_t my_nvs_handle;
#define IS_BROADCAST_ADDR(addr) (memcmp(addr, espnow_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)
#define IS_SAME_ADDR(addr1, addr2) (memcmp(addr1, addr2, ESP_NOW_ETH_ALEN) == 0)

static my_espnow_device_t s_devices[MY_ESPNOW_MAX_PEER_NUM + 1] = {
    MY_ESPNOW_DEFAULT_BROADCAST_DEVICE,
};

static my_espnow_handle_t s_handle = {
    .send_queue = NULL,
    .recv_queue = NULL,
    .ack_queue = NULL,
    .send_task_handle = NULL,
    .recv_task_handle = NULL,
    .devices = s_devices,
    .device_num = 0,
    .def_dev = 0,
    .fsm_state = MY_ESPNOW_FSM_BLOCK,
};

#define MY_ESPNOW_TASK_NUM 2
#define SEND_TASK_INDEX 0
#define RECV_TASK_INDEX 1
#define MY_ESPNOW_TASK_PRIORITY 2
static TaskHandle_t wait_send_cb_tasks_handle[MY_ESPNOW_TASK_NUM] = {NULL, NULL};
my_espnow_handle_p my_espnow_handle = NULL;
wifi_bandwidth_t wifi_bw_before = WIFI_BW_HT20;

#if CONFIG_MY_DEVICE_IS_RECEIVER
static my_espnow_fsm_state_handler_t my_espnow_fsm_recv_handlers[MY_ESPNOW_FSM_STATES_NUM] = {
    NULL,                               /*MY_ESPNOW_FSM_BLOCK*/
    my_espnow_fsm_scan_send_handler,    /*MY_ESPNOW_FSM_SCAN*/
    NULL,                               /*MY_ESPNOW_FSM_WAIT_PAIRING*/
    my_espnow_fsm_general_recv_handler, /*MY_ESPNOW_FSM_CHECK_PEER*/
    my_espnow_fsm_general_recv_handler, /*MY_ESPNOW_FSM_CONNECTED*/
};
static my_espnow_fsm_state_handler_t my_espnow_fsm_send_handlers[MY_ESPNOW_FSM_STATES_NUM] = {
    NULL,                               /*MY_ESPNOW_FSM_BLOCK*/
    NULL,                               /*MY_ESPNOW_FSM_SCAN*/
    NULL,                               /*MY_ESPNOW_FSM_WAIT_PAIRING*/
    my_espnow_fsm_general_send_handler, /*MY_ESPNOW_FSM_CHECK_PEER*/
    my_espnow_fsm_general_send_handler, /*MY_ESPNOW_FSM_CONNECTED*/
};
#else
static my_espnow_fsm_state_handler_t my_espnow_fsm_recv_handlers[MY_ESPNOW_FSM_STATES_NUM] = {
    NULL,                                    /*MY_ESPNOW_FSM_BLOCK*/
    NULL,                                    /*MY_ESPNOW_FSM_SCAN*/
    my_espnow_fsm_wait_pairing_recv_handler, /*MY_ESPNOW_FSM_WAIT_PAIRING*/
    my_espnow_fsm_general_recv_handler,      /*MY_ESPNOW_FSM_CHECK_PEER*/
    my_espnow_fsm_general_recv_handler,      /*MY_ESPNOW_FSM_CONNECTED*/
};
static my_espnow_fsm_state_handler_t my_espnow_fsm_send_handlers[MY_ESPNOW_FSM_STATES_NUM] = {
    NULL,                               /*MY_ESPNOW_FSM_BLOCK*/
    NULL,                               /*MY_ESPNOW_FSM_SCAN*/
    NULL,                               /*MY_ESPNOW_FSM_WAIT_PAIRING*/
    my_espnow_fsm_general_send_handler, /*MY_ESPNOW_FSM_CHECK_PEER*/
    my_espnow_fsm_general_send_handler, /*MY_ESPNOW_FSM_CONNECTED*/
};
#endif
#pragma endregion

#pragma region // 初始化和重新配对
esp_err_t my_espnow_re_pairing()
{
    if (!my_espnow_handle)
        return ESP_ERR_NOT_ALLOWED;
#if CONFIG_MY_DEVICE_IS_RECEIVER
    if (s_handle.fsm_state == MY_ESPNOW_FSM_SCAN)
        return ESP_ERR_INVALID_STATE;
    s_handle.fsm_state = MY_ESPNOW_FSM_SCAN;
#else
    if (s_handle.fsm_state == MY_ESPNOW_FSM_WAIT_PAIRING)
        return ESP_ERR_INVALID_STATE;
    s_handle.fsm_state = MY_ESPNOW_FSM_WAIT_PAIRING;
#endif
    return ESP_OK;
}

esp_err_t my_espnow_init()
{
    if (my_espnow_handle)
        return ESP_ERR_INVALID_STATE;
    s_handle.send_queue = xQueueCreate(MY_ESPNOW_QUEUE_LENGTH, sizeof(my_espnow_send_param_t));
    assert(s_handle.send_queue);
    s_handle.recv_queue = xQueueCreate(MY_ESPNOW_QUEUE_LENGTH, sizeof(my_espnow_recv_param_t));
    assert(s_handle.recv_queue);
    s_handle.ack_queue = xQueueCreate(1, sizeof(my_espnow_ack_param_t));
    assert(s_handle.ack_queue);
    esp_err_t ret = ESP_OK;

    ESP_ERROR_CHECK(esp_now_init());
    esp_wifi_get_bandwidth(MY_ESPNOW_WIFI_IF_INDEX, &wifi_bw_before);
    if (wifi_bw_before != WIFI_BW_HT20)
        esp_wifi_set_bandwidth(MY_ESPNOW_WIFI_IF_INDEX, WIFI_BW_HT20);
    ESP_ERROR_CHECK(esp_now_register_send_cb(my_espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(my_espnow_recv_cb));

    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)MY_ESPNOW_PMK));

    if (s_devices[0].target_channel) {
        ret = esp_wifi_set_channel(s_devices[0].target_channel, WIFI_SECOND_CHAN_NONE);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_wifi_set_channel failed");
            esp_wifi_get_channel(&s_wifi_info.channel, &s_wifi_info.second_chan);
        }
        else {
            s_wifi_info.channel = s_devices[0].target_channel;
            s_wifi_info.second_chan = WIFI_SECOND_CHAN_NONE;
        }
    }
    s_devices[0].peer_info.channel = 0;
    ESP_ERROR_CHECK(esp_now_add_peer(&s_devices[0].peer_info)); // 添加广播设备
    ESP_LOGI(TAG, "espnow start in channel %d", s_wifi_info.channel);
#if CONFIG_MY_DEVICE_IS_RECEIVER
    s_handle.fsm_state = MY_ESPNOW_FSM_SCAN;
#else
    s_handle.fsm_state = MY_ESPNOW_FSM_WAIT_PAIRING;
#endif

    ret = nvs_open(my_nvs_namespace, NVS_READWRITE, &my_nvs_handle);
    if (ret == ESP_OK) {
        uint8_t peer_mac[ESP_NOW_ETH_ALEN];
        size_t mac_len = ESP_NOW_ETH_ALEN;
        ret = nvs_get_blob(my_nvs_handle, "peer_mac", peer_mac, &mac_len);
        if (ret == ESP_OK) {
            ret = my_espnow_add_peer(peer_mac, NULL);
            if (ret == ESP_OK) {
                ESP_LOGD(TAG, "find peer in nvs, Add peer success");
                s_handle.fsm_state = MY_ESPNOW_FSM_CHECK_PEER;
                // 对于接收器，要获取信道信息并切换信道
                uint8_t _chan;
                ret = nvs_get_u8(my_nvs_handle, "peer_chan", &_chan);
                if (ret == ESP_OK) {
                    s_devices[s_handle.def_dev].target_channel = _chan;
                    ret = esp_wifi_set_channel(_chan, WIFI_SECOND_CHAN_NONE);
                    if (ret != ESP_OK)
                        ESP_LOGW(TAG, "get channel from nvs but set fail :%d", _chan);
                    else
                        ESP_LOGI(TAG, "set channel from nvs: %d", _chan);
                }
            }
        }
        // nvs_commit(my_nvs_handle);
        nvs_close(my_nvs_handle);
    }

    my_espnow_handle = &s_handle;
    s_wifi_info.espnow.inited = 1;
    xTaskCreatePinnedToCore(my_espnow_send_task, "espnow_s", 3072, &s_handle.send_task_handle, MY_ESPNOW_TASK_PRIORITY, &s_handle.send_task_handle, MY_ESPNOW_TASK_CORE_ID);
    assert(s_handle.send_task_handle);
    xTaskCreatePinnedToCore(my_espnow_recv_task, "espnow_r", 3072, &s_handle.recv_task_handle, MY_ESPNOW_TASK_PRIORITY, &s_handle.recv_task_handle, MY_ESPNOW_TASK_CORE_ID);
    assert(s_handle.recv_task_handle);
    s_wifi_info.espnow.running = 1;
    return ESP_OK;
}

esp_err_t my_espnow_deinit()
{
    if (!my_espnow_handle)
        return ESP_ERR_INVALID_STATE;
    s_wifi_info.espnow.inited = 0;
    s_wifi_info.espnow.running = 0;
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();

    if (s_handle.send_task_handle) {
        vTaskDelete(s_handle.send_task_handle);
        s_handle.send_task_handle = NULL;
    }
    if (s_handle.recv_task_handle) {
        vTaskDelete(s_handle.recv_task_handle);
        s_handle.recv_task_handle = NULL;
    }
    esp_now_deinit();

    if (s_handle.send_queue) {
        vQueueDelete(s_handle.send_queue);
        s_handle.send_queue = NULL;
    }
    if (s_handle.recv_queue) {
        vQueueDelete(s_handle.recv_queue);
        s_handle.recv_queue = NULL;
    }
    if (s_handle.ack_queue) {
        s_handle.ack_queue = NULL;
    }
    if (wifi_bw_before != WIFI_BW_HT20)
        esp_wifi_set_bandwidth(MY_ESPNOW_WIFI_IF_INDEX, wifi_bw_before);
    my_espnow_handle = NULL;
    s_handle.device_num = 0;
    s_handle.def_dev = 0;
    s_handle.fsm_state = MY_ESPNOW_FSM_BLOCK;
    s_wifi_info.espnow.connected = 0;
    return ESP_OK;
}
#pragma endregion

#pragma region // 循环处理任务
void my_espnow_fsm_wait_pairing_recv_handler(my_espnow_fsm_state_t enter_state, void *task_handle)
{
    // enter state MY_ESPNOW_FSM_WAIT_PAIRING
    my_espnow_recv_param_t recv_buf;
    BaseType_t b_ret;
    esp_err_t ret;

    ESP_LOGI(TAG, "fsm enter %d", enter_state);

    if (s_wifi_info.espnow.connected) {
        s_wifi_info.espnow.connected = 0;
        my_wifi_state_change_cb(8, s_wifi_info.espnow.connected);
    }

    while (s_handle.device_num > 0) {
        esp_now_del_peer(s_devices[s_handle.device_num].peer_info.peer_addr);
        s_handle.device_num--;
        s_handle.def_dev = 0;
    }
    my_espnow_delete_peer_from_nvs();

    while (s_handle.fsm_state == enter_state) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (xQueueReceive(s_handle.recv_queue, &recv_buf, MY_WIFI_DELAY_MAX) != pdTRUE)
            continue;
        if (recv_buf.data.head.type == MY_ESPNOW_DATA_TYPE_CONTROL &&
            recv_buf.data.head.reserved == MY_ESPNOW_CONTROL_RESET_PEER)
            break;
        // 先验证数据类型，如果收到了查找设备的广播请求
        if (recv_buf.data.head.type == MY_ESPNOW_DATA_TYPE_SCAN) {
            if (my_espnow_add_peer(recv_buf.src_mac, NULL) == ESP_OK) // 将地址加入peer列表
            {
                uint8_t *_peer_mac = s_devices[s_handle.def_dev].peer_info.peer_addr;
                ret = my_espnow_send_ack(_peer_mac, recv_buf.data.head.seq_num, 0);
                uint8_t _retry = 4;
                uint8_t _pair_fail = 1;
                int64_t start_time = esp_timer_get_time();
                do {
                    if (xQueueReceive(s_handle.recv_queue, &recv_buf, pdTICKS_TO_MS(500)) != pdTRUE) {
                        _retry--;
                        continue;
                    }
                    if ((recv_buf.data.head.type == MY_ESPNOW_DATA_TYPE_SCAN)) {
                        if (esp_timer_get_time() - start_time > 10000000)
                            break;
                        continue;
                    }
                    if ((recv_buf.data.head.type != MY_ESPNOW_DATA_TYPE_PAIRING) ||
                        !IS_SAME_ADDR(recv_buf.src_mac, _peer_mac)) {
                        _retry--;
                        continue;
                    }
                    ret = my_espnow_send_ack(recv_buf.src_mac, recv_buf.data.head.seq_num, 0);
                    _pair_fail = 0;
                } while (_retry && _pair_fail);

                if (!_pair_fail) // 配对成功，将当前设备地址存入nvs
                {
                    my_espnow_save_peer_to_nvs(s_handle.def_dev);
                    s_handle.fsm_state = MY_ESPNOW_FSM_CHECK_PEER;
                    ESP_LOGI(TAG, "pairing success");
                    if (!s_wifi_info.espnow.connected) {
                        s_wifi_info.espnow.connected = 1;
                        my_wifi_state_change_cb(8, s_wifi_info.espnow.connected);
                    }
                }
                else {
                    my_espnow_remove_peer(s_handle.def_dev, NULL);
                    ESP_LOGW(TAG, "pairing fail, out of retry count");
                }
            }
        }
    }

    // exit state
}

void my_espnow_fsm_scan_send_handler(my_espnow_fsm_state_t enter_state, void *task_handle)
{
    my_espnow_ack_param_t ack_buf;
    BaseType_t b_ret = pdTRUE;
    esp_err_t ret = ESP_FAIL;
    uint16_t delay_time = pdMS_TO_TICKS(500);
    my_espnow_device_t *bt_dev = &s_devices[0];
    my_espnow_data_t scan_data = MY_ESPNOW_DEFAULT_SCAN_DATA;
    uint8_t target_channel = s_wifi_info.channel > 0 ? s_wifi_info.channel : 1;
    TaskHandle_t t_handle = *((TaskHandle_t *)task_handle);
    if (s_wifi_info.espnow.connected) {
        s_wifi_info.espnow.connected = 0;
        my_wifi_state_change_cb(8, s_wifi_info.espnow.connected);
    }
    while (s_handle.device_num > 0) {
        esp_now_del_peer(s_devices[s_handle.device_num].peer_info.peer_addr);
        s_handle.device_num--;
        s_handle.def_dev = 0;
    }
    my_espnow_delete_peer_from_nvs();
    esp_wifi_set_channel(MY_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    int64_t change_chan_time = esp_timer_get_time();
    while (s_handle.fsm_state == enter_state) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (esp_timer_get_time() - change_chan_time > 5000000) {
            esp_wifi_get_channel(&s_wifi_info.channel, &s_wifi_info.second_chan);
            if (s_wifi_info.channel != target_channel) {
                ret = esp_wifi_set_channel(target_channel, WIFI_SECOND_CHAN_NONE);
                ESP_LOGI(TAG, "change channel from %d to %d ,ret %d", s_wifi_info.channel, target_channel, ret);
            }
            target_channel = (target_channel + 1) % 14;
            target_channel = target_channel > 0 ? target_channel : 1;
            change_chan_time = esp_timer_get_time();
        }

        bt_dev->send_seq_num++;
        my_espnow_data_prepare(&scan_data, bt_dev->send_seq_num);
        xQueueReceive(s_handle.ack_queue, &ack_buf, 0);
        ulTaskNotifyTake(pdTRUE, 0);
        wait_send_cb_tasks_handle[SEND_TASK_INDEX] = t_handle;
        ret = esp_now_send(bt_dev->peer_info.peer_addr, (uint8_t *)&scan_data, scan_data.head.data_len);
        if (ret != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50)))
            wait_send_cb_tasks_handle[SEND_TASK_INDEX] = NULL;
        if (xQueueReceive(s_handle.ack_queue, &ack_buf, pdMS_TO_TICKS(500)) != pdTRUE)
            continue;
        if (ack_buf.data.seq_num > bt_dev->send_seq_num)
            continue;
        if (my_espnow_add_peer(ack_buf.src_mac, NULL) != ESP_OK) {
            ESP_LOGE(TAG, "add peer error unknown fail");
            vTaskDelay(delay_time);
            continue;
        }
        my_espnow_device_t *_dev = &s_devices[s_handle.def_dev];
        my_espnow_data_t pairing_data = MY_ESPNOW_DEFAULT_PAIRING_DATA;
        _dev->send_seq_num++;
        my_espnow_data_prepare(&pairing_data, _dev->send_seq_num);
        uint8_t _retry = 5;
        uint8_t _pair_fail = 1;
        uint16_t _wait_time = pdMS_TO_TICKS(50);
        do {
            if (b_ret != pdTRUE)
                vTaskDelay(1);
            b_ret = xQueueReceive(s_handle.ack_queue, &ack_buf, 0);
            if (b_ret == pdTRUE && ack_buf.data.seq_num == pairing_data.head.seq_num && IS_SAME_ADDR(ack_buf.src_mac, _dev->peer_info.peer_addr)) {
                _pair_fail = 0;
                break;
            }
            ulTaskNotifyTake(pdTRUE, 0);
            wait_send_cb_tasks_handle[SEND_TASK_INDEX] = t_handle;
            ret = esp_now_send(ack_buf.src_mac, (uint8_t *)&pairing_data, pairing_data.head.data_len);
            if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50)))
                wait_send_cb_tasks_handle[SEND_TASK_INDEX] = NULL;
            _wait_time += pdMS_TO_TICKS(10);
            if (_wait_time >= pdMS_TO_TICKS(100))
                _wait_time = pdMS_TO_TICKS(50);
            int64_t _start_time, _last_time_ms;
            _start_time = esp_timer_get_time();
            b_ret = xQueueReceive(s_handle.ack_queue, &ack_buf, _wait_time);
            if (b_ret != pdTRUE) {
                _retry--;
                continue;
            }

            if (ack_buf.data.seq_num == pairing_data.head.seq_num && IS_SAME_ADDR(ack_buf.src_mac, _dev->peer_info.peer_addr)) {
                _pair_fail = 0;
                break;
            }
            _last_time_ms = pdTICKS_TO_MS(_wait_time) - ((esp_timer_get_time() - _start_time) / 1000);
            if (pdMS_TO_TICKS(_last_time_ms) > 0)
                vTaskDelay(pdMS_TO_TICKS(_last_time_ms));
            _retry--;
        } while (_retry && _pair_fail);
        if (!_pair_fail) // 配对成功，将当前设备地址存入nvs
        {
            my_espnow_save_peer_to_nvs(s_handle.def_dev);
            s_handle.fsm_state = MY_ESPNOW_FSM_CHECK_PEER;
            ESP_LOGI(TAG, "pairing success");
        }
        else {
            my_espnow_remove_peer(s_handle.def_dev, NULL);
            ESP_LOGW(TAG, "pairing fail, out of retry count");
        }
    }

    ESP_LOGI(TAG, "exit scan state");
}

#if CONFIG_MY_DEVICE_IS_RECEIVER
uint8_t empty_arr[MY_ESPNOW_DATA_PAY_LOAD_MAXLEN] = {0};
int64_t last_get_dev_online_time = 0;
int64_t last_send_check_online_time = 0;
uint8_t need_check_online = 0;
#endif

void my_espnow_fsm_general_send_handler(my_espnow_fsm_state_t enter_state, void *task_handle)
{
    my_espnow_send_param_t send_buf;
    my_espnow_ack_param_t ack_buf;
    BaseType_t b_ret = pdFALSE;
    esp_err_t ret = ESP_FAIL;
    TaskHandle_t t_handle = *((TaskHandle_t *)task_handle);
    int64_t cur_time = 0;
    ESP_LOGI(TAG, "fsm general send, enter state: %d", enter_state);
#if !CONFIG_MY_DEVICE_IS_RECEIVER
    if (!s_wifi_info.espnow.connected) {
        if (my_espnow_send_data_default(MY_ESPNOW_DATA_TYPE_GET_HID_OUTPUT, 1, NULL, 0) == ESP_ERR_TIMEOUT)
            ESP_LOGW(TAG, "send queue timeout");
    }
#endif

    while (s_handle.fsm_state == enter_state) {
        vTaskDelay(pdMS_TO_TICKS(1));
        b_ret = xQueueReceive(s_handle.send_queue, &send_buf, MY_WIFI_DELAY_MAX);
        if (b_ret != pdTRUE ||
            send_buf.data.head.data_len < MY_ESPNOW_MIN_DATA_LEN ||
            send_buf.data.head.data_len > MY_ESPNOW_MAX_DATA_LEN)
            continue;
        if (send_buf.dev_index < s_handle.device_num)
            continue;
        my_espnow_device_t *_dev = &s_devices[send_buf.dev_index];
        if (IS_BROADCAST_ADDR(_dev->peer_info.peer_addr)) // 配对外的状态不发送广播包
            continue;
        _dev->send_seq_num++;
        my_espnow_data_prepare(&send_buf.data, _dev->send_seq_num);
        uint8_t _retry = 4;
        uint16_t _wait_time = 0;
        // uint32_t notify_value = 0;
        do {
            if (b_ret != pdTRUE) { vTaskDelay(1); }                 // 第一次进入时应该都是pdTRUE，不会延迟
            b_ret = xQueueReceive(s_handle.ack_queue, &ack_buf, 0); // 读取ack数据
            if (b_ret == pdTRUE &&
                ack_buf.data.seq_num == send_buf.data.head.seq_num &&
                IS_SAME_ADDR(ack_buf.src_mac, _dev->peer_info.peer_addr)) // 如果是正确ack，直接跳出循环
                break;
            ulTaskNotifyTake(pdTRUE, 0);                           // 先清除任务通知
            wait_send_cb_tasks_handle[SEND_TASK_INDEX] = t_handle; // 让发送回调知道当前任务需要接收通知
            ret = esp_now_send(_dev->peer_info.peer_addr, (uint8_t *)&send_buf.data, send_buf.data.head.data_len);
            if (!send_buf.data.head.flag.need_ack) // 如果不需要ack，发送完毕，不管哪一步没有发送出去都不管
                break;
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50)); // 等待通知，如果超时，或许也需要重新发送
            wait_send_cb_tasks_handle[SEND_TASK_INDEX] = NULL;
            _wait_time += pdMS_TO_TICKS(3);
            int64_t _start_time, _last_time_ms;
            _start_time = esp_timer_get_time();
            if (xQueueReceive(s_handle.ack_queue, &ack_buf, _wait_time) != pdTRUE) {
                _retry--;
                continue;
            }
            if (ack_buf.data.seq_num == send_buf.data.head.seq_num && IS_SAME_ADDR(ack_buf.src_mac, _dev->peer_info.peer_addr))
                break; // 序号和地址都对，说明接收成功
            // 序号或地址不对，但有收到数据，继续等待
            // 当数据不对时，等待到预计时间
            _last_time_ms = pdTICKS_TO_MS(_wait_time) - ((esp_timer_get_time() - _start_time) / 1000);
            if (pdMS_TO_TICKS(_last_time_ms) > 0)
                vTaskDelay(pdMS_TO_TICKS(_last_time_ms));
            _retry--;
        } while (_retry > 0);
#if CONFIG_MY_DEVICE_IS_RECEIVER
        cur_time = esp_timer_get_time();
        if (_retry <= 0) { // 如果没有收到ack
            _dev->continuous_send_fail++;
            if (need_check_online && send_buf.data.head.type == MY_ESPNOW_DATA_TYPE_I_AM_ONLINE) {
                if (cur_time > last_get_dev_online_time + 1000000) { // 如果超过一定时间没有在线，情况按键报告，并设置为不用再检查
                    if (my_hid_remove_keycode_all(1, 1) > 0) {
                        my_hid_send_keyboard_report();
                        vTaskDelay(pdMS_TO_TICKS(2));
                    }
                    if (my_hid_add_consumer_code16(0) != MY_HID_NO_NEED_OPERATE) {
                        my_hid_send_consumer_report();
                        vTaskDelay(pdMS_TO_TICKS(1));
                    }
                    need_check_online = false;
                }
            }
        }
        else {
            _dev->continuous_send_fail = 0;
            if (need_check_online) {                 //&& send_buf.data.head.type == MY_ESPNOW_DATA_TYPE_I_AM_ONLINE
                last_get_dev_online_time = cur_time; // 正确发送时，更新最后在线时间
            }
        }
#else
        if (_retry <= 0) { // 数据发送失败/没有收到ack等等
            _dev->continuous_send_fail++;
        }
        else {
            _dev->continuous_send_fail = 0;
        }
#endif
        if (s_wifi_info.espnow.connected && _dev->continuous_send_fail > 5) {
            s_wifi_info.espnow.connected = 0;
            my_wifi_state_change_cb(8, s_wifi_info.espnow.connected);
        }
        else if (!s_wifi_info.espnow.connected && _dev->continuous_send_fail == 0) {
            s_wifi_info.espnow.connected = 1;
            my_wifi_state_change_cb(8, s_wifi_info.espnow.connected);
        }
    }
}

void my_espnow_fsm_general_recv_handler(my_espnow_fsm_state_t enter_state, void *task_handle)
{
    my_espnow_recv_param_t recv_buf;
    BaseType_t b_ret;
    esp_err_t ret;
    int64_t cur_time = 0;
    while (s_handle.fsm_state == enter_state) {
#if CONFIG_MY_DEVICE_IS_RECEIVER
        cur_time = esp_timer_get_time();
        if (need_check_online && cur_time > last_send_check_online_time + 1000000) {
            if (my_espnow_send_data_default(MY_ESPNOW_DATA_TYPE_I_AM_ONLINE, 1, NULL, 0) == ESP_OK) {
                last_send_check_online_time = cur_time;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
#else
        vTaskDelay(pdMS_TO_TICKS(2));
#endif
        ret = my_espnow_wait_recv_data(&recv_buf, &b_ret, MY_WIFI_DELAY_MAX, 0);
        if (ret != ESP_OK)
            continue;
        switch (recv_buf.data.head.type) {
            case MY_ESPNOW_DATA_TYPE_HID_INPUT:
                uint8_t report_len = recv_buf.data.head.data_len - sizeof(my_espnow_data_head_t);
                my_espnow_reciver_recv_input_report_cb((uint8_t *)recv_buf.data.payload, report_len);
#if CONFIG_MY_DEVICE_IS_RECEIVER
                last_get_dev_online_time = esp_timer_get_time();
                last_send_check_online_time = last_get_dev_online_time;
                uint8_t *mem_check_start = NULL;
                if (report_len == my_keyboard_report_size + 1 ||
                    report_len == my_consumer_report_size + 1) {
                    mem_check_start = &recv_buf.data.payload[1];
                    report_len--;
                }
                else if (report_len == my_keyboard_report_size ||
                         report_len == my_consumer_report_size + 1) {
                    mem_check_start = recv_buf.data.payload;
                }
                if (mem_check_start) {
                    bool is_empty = (memcmp(mem_check_start, empty_arr, report_len) == 0);
                    if (is_empty) {
                        need_check_online = false;
                    }
                    else {
                        need_check_online = true;
                    }
                }
#endif
                break;
            case MY_ESPNOW_DATA_TYPE_GET_HID_OUTPUT:
                my_espnow_reciver_recv_get_output_cb();
                break;
            case MY_ESPNOW_DATA_TYPE_HID_OUTPUT:
                my_espnow_device_recv_output_report_cb((uint8_t *)recv_buf.data.payload, recv_buf.data.head.data_len - sizeof(my_espnow_data_head_t));
                break;
            default:
                break;
        }
    }
}
// 用来处理hid output等数据，ack包应该在send_task中处理
void my_espnow_recv_task(void *pvParameter)
{
    for (;;) {
        if (my_espnow_fsm_recv_handlers[s_handle.fsm_state]) {
            my_espnow_fsm_recv_handlers[s_handle.fsm_state](s_handle.fsm_state, pvParameter);
        }
        else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    vTaskDelete(NULL);
}

void my_espnow_send_task(void *pvParameter)
{

    for (;;) {
        if (my_espnow_fsm_send_handlers[s_handle.fsm_state]) {
            my_espnow_fsm_send_handlers[s_handle.fsm_state](s_handle.fsm_state, pvParameter);
        }
        else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    vTaskDelete(NULL);
}
#pragma endregion

#pragma region // 系统回调
void my_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_FAIL)
        return;
    // 实际上应该只能有一个任务等待发送完成，不需要数组，待定
    for (size_t i = 0; i < MY_ESPNOW_TASK_NUM; i++) {
        if (wait_send_cb_tasks_handle[i]) {
            xTaskNotifyGive(wait_send_cb_tasks_handle[i]);
            wait_send_cb_tasks_handle[i] = NULL;
            return;
        }
    }
}

void my_espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len)
{
    if (!my_espnow_handle || //! esp_now_info->src_addr ||
        (data_len < MY_ESPNOW_MIN_DATA_LEN) || (data_len > MY_ESPNOW_MAX_DATA_LEN) ||
        (s_handle.fsm_state == MY_ESPNOW_FSM_BLOCK))
    // 如果状态没有定义处理函数，不要将数据发送到队列，避免每次都阻塞
    {
        return;
    }
    my_espnow_data_t *data_p = (my_espnow_data_t *)data;
    if (data_p->head.data_len != data_len) // 如果数据长度不一致，可能不是目标数据，不需要校验直接丢弃
    {
        ESP_LOGD(TAG, "data_len error: %d, %d", data_p->head.data_len, data_len);
        return;
    }
    uint16_t crc, crc_cal;
    crc = data_p->head.crc;
    data_p->head.crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, data, data_len);
    if (crc != crc_cal) {
        ESP_LOGD(TAG, "crc error: %04x, %04x", crc, crc_cal);
        return;
    }
    // 验证数据完毕，准备中继到接收队列
    if ((data_p->head.type == MY_ESPNOW_DATA_TYPE_ACK || data_p->head.type == MY_ESPNOW_DATA_TYPE_NACK)) { // ack数据处理
        if (data_len > MY_ESPNOW_MIN_DATA_LEN) {
            ESP_LOGD(TAG, "ack data must less then sizeof %d, now: %d", MY_ESPNOW_MIN_DATA_LEN, data_len);
            return;
        }
        if (IS_BROADCAST_ADDR(esp_now_info->des_addr)) {
            ESP_LOGD(TAG, "ack should not be broadcast");
            return;
        }
        my_espnow_ack_param_t ack_param;
        memcpy(ack_param.src_mac, esp_now_info->src_addr, ESP_NOW_ETH_ALEN);
        memcpy(&ack_param.data, data, data_len);
        xQueueOverwrite(s_handle.ack_queue, &ack_param);
    }
    else { // 非ack数据处理
        my_espnow_recv_param_t param;
        param.is_broadcast = IS_BROADCAST_ADDR(esp_now_info->des_addr);
        // 配对完毕后，不接收一切广播数据
        if (param.is_broadcast && s_handle.fsm_state > MY_ESPNOW_FSM_WAIT_PAIRING)
            return;
        memcpy(param.src_mac, esp_now_info->src_addr, ESP_NOW_ETH_ALEN);
        memcpy(&param.data, data, data_len);
        if (xQueueSend(s_handle.recv_queue, &param, 512) != pdTRUE) {
            ESP_LOGD(TAG, "send queue full");
        }
    }
}
#pragma endregion

#pragma region // 发送和接收处理
esp_err_t my_espnow_wait_recv_data(my_espnow_recv_param_t *recv_buf, BaseType_t *b_ret, uint32_t max_wait_time, uint8_t ack_broadcast)
{
    if (!recv_buf || !b_ret)
        return ESP_ERR_INVALID_ARG;
    *b_ret = xQueueReceive(s_handle.recv_queue, recv_buf, max_wait_time);
    if (*b_ret != pdTRUE)
        return ESP_ERR_TIMEOUT;
    my_espnow_device_t *_dev = NULL;
    esp_err_t ret = my_espnow_get_device(recv_buf->src_mac, &_dev, NULL);
    if (ret != ESP_OK) // 如果数据从没有添加的设备发送过来，不继续处理
        return ESP_ERR_NOT_FOUND;
    if ((!recv_buf->is_broadcast || ack_broadcast) && recv_buf->data.head.flag.need_ack)
        ret = my_espnow_send_ack(recv_buf->src_mac, recv_buf->data.head.seq_num, 0);
    if (_dev->recv_seq_num == recv_buf->data.head.seq_num) // 如果当前设备接收序号和该数据序号相等，说明已经接收过数据，不用再处理
        return ESP_ERR_INVALID_STATE;
    _dev->recv_seq_num = recv_buf->data.head.seq_num;
    return ESP_OK;
}

esp_err_t my_espnow_send_data_default(my_espnow_data_type_t data_type, uint8_t need_ack, uint8_t *payload_data, uint8_t data_len)
{
    if (!my_espnow_handle || s_handle.device_num == 0)
        return ESP_ERR_NOT_FINISHED;
    if (data_len > MY_ESPNOW_DATA_PAY_LOAD_MAXLEN || (!payload_data && (data_len > 0)) || data_type >= MY_ESPNOW_DATA_TYPE_MAX_NUM)
        return ESP_ERR_INVALID_ARG;
    my_espnow_send_param_t send_param = {
        .data = {
            .head = {
                .type = data_type,
                .flag.need_ack = (need_ack > 0),
                .data_len = data_len + sizeof(my_espnow_data_head_t),
            },
            .payload = {0},
        },
        .dev_index = s_handle.def_dev,
    };
    if (data_len)
        memcpy(send_param.data.payload, payload_data, data_len);
    BaseType_t q_ret = xQueueSend(s_handle.send_queue, &send_param, 0);
    if (q_ret == pdTRUE)
        return ESP_OK;
    else
        return ESP_ERR_TIMEOUT;
}

esp_err_t my_espnow_send_hid_input_report(uint8_t *data_with_id, uint8_t data_len)
{
    return my_espnow_send_data_default(MY_ESPNOW_DATA_TYPE_HID_INPUT, 1, data_with_id, data_len);
}

esp_err_t my_espnow_send_hid_output_report(uint8_t *data_with_id, uint8_t data_len)
{
    return my_espnow_send_data_default(MY_ESPNOW_DATA_TYPE_HID_OUTPUT, 1, data_with_id, data_len);
}

esp_err_t my_espnow_send_ack(const uint8_t *peer_mac, uint16_t seq_num, uint8_t is_nack)
{
    my_espnow_data_head_t ack_data = MY_ESPNOW_DEFAULT_ACK_DATA;
    if (is_nack)
        ack_data.type = MY_ESPNOW_DATA_TYPE_NACK;
    ack_data.seq_num = seq_num;
    ack_data.crc = esp_crc16_le(UINT16_MAX, (uint8_t *)&ack_data, ack_data.data_len);
    return esp_now_send(peer_mac, (uint8_t *)&ack_data, ack_data.data_len);
}

void my_espnow_data_prepare(my_espnow_data_t *data, uint16_t seq_num)
{
    if (!data)
        return;
    data->head.seq_num = seq_num;
    data->head.crc = 0;
    data->head.crc = esp_crc16_le(UINT16_MAX, (uint8_t *)data, data->head.data_len);
}
#pragma endregion

#pragma region // peer设备管理
esp_err_t my_espnow_add_peer(const uint8_t *peer_mac, uint8_t *out_index)
{
    if (s_handle.device_num >= MY_ESPNOW_MAX_PEER_NUM)
        return ESP_ERR_NO_MEM;
    if (esp_now_is_peer_exist(peer_mac))
        return ESP_ERR_NOT_ALLOWED;
    esp_err_t ret = ESP_OK;
    s_handle.device_num++;
    esp_now_peer_info_t *_peer_info = &s_handle.devices[s_handle.device_num].peer_info;
    memcpy(_peer_info->peer_addr, peer_mac, ESP_NOW_ETH_ALEN);
    memcpy(_peer_info->lmk, MY_ESPNOW_LMK, sizeof(_peer_info->lmk));
    _peer_info->channel = 0;
    _peer_info->encrypt = false; // 如果对方没有添加设备，加密后对方无法接收到数据
    _peer_info->ifidx = MY_ESPNOW_WIFI_IF_INDEX;
    memcpy(_peer_info->lmk, MY_ESPNOW_LMK, sizeof(_peer_info->lmk));
    _peer_info->priv = NULL;
    ret = esp_now_add_peer(_peer_info);
    if (ret == ESP_OK) {
        s_handle.def_dev = s_handle.device_num;
        s_devices[s_handle.device_num].recv_seq_num = 0;
        s_devices[s_handle.device_num].send_seq_num = 0;
        if (out_index)
            *out_index = s_handle.device_num;
    }
    else {
        s_handle.device_num--;
    }
    return ret;
}

esp_err_t my_espnow_remove_peer(uint8_t index, const uint8_t *peer_mac)
{
    if (!index && !peer_mac)
        return ESP_ERR_INVALID_ARG;

    esp_err_t ret = ESP_OK;
    if (!index && peer_mac) {
        uint8_t temp = 0;
        ret = my_espnow_get_device(peer_mac, NULL, &temp);
        if (ret != ESP_OK)
            return ESP_ERR_NOT_FOUND;
        index = temp;
    }
    if (index > s_handle.device_num)
        return ESP_ERR_INVALID_ARG;
    ret = esp_now_del_peer(s_devices[index].peer_info.peer_addr);
    if (ret == ESP_OK) {
        s_handle.device_num--;
        if (s_handle.device_num < s_handle.def_dev) {
            s_handle.def_dev = s_handle.device_num;
        }
    }
    return ret;
}

esp_err_t my_espnow_save_peer_to_nvs(uint8_t dev_index)
{
    if (dev_index <= 0 || dev_index > s_handle.device_num)
        return ESP_ERR_INVALID_SIZE;
    if (!esp_now_is_peer_exist(s_devices[dev_index].peer_info.peer_addr))
        return ESP_ERR_INVALID_MAC;

    esp_err_t ret = nvs_open(my_nvs_namespace, NVS_READWRITE, &my_nvs_handle);
    if (ret == ESP_OK) {
        ret = nvs_set_blob(my_nvs_handle, "peer_mac", s_devices[dev_index].peer_info.peer_addr, ESP_NOW_ETH_ALEN);
        if (ret == ESP_OK) {
            esp_wifi_get_channel(&s_wifi_info.channel, &s_wifi_info.second_chan);
            nvs_set_u8(my_nvs_handle, "peer_chan", s_wifi_info.channel);
        }
        nvs_commit(my_nvs_handle);
        nvs_close(my_nvs_handle);
    }
    return ret;
}

esp_err_t my_espnow_delete_peer_from_nvs()
{
    esp_err_t ret = nvs_open(my_nvs_namespace, NVS_READWRITE, &my_nvs_handle);
    if (ret == ESP_OK) {
        nvs_erase_key(my_nvs_handle, "peer_chan");
        ret = nvs_erase_key(my_nvs_handle, "peer_mac");
        nvs_commit(my_nvs_handle);
        nvs_close(my_nvs_handle);
    }
    return ret;
}

esp_err_t my_espnow_get_device(const uint8_t *mac, my_espnow_device_t **out_device, uint8_t *out_index)
{
    if (s_handle.device_num == 0)
        return ESP_ERR_NOT_FOUND;
    if (!out_device && !out_index)
        return ESP_ERR_INVALID_ARG;
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    for (size_t i = 1; i <= s_handle.device_num; i++) {
        if (IS_SAME_ADDR(s_devices[i].peer_info.peer_addr, mac)) {
            ret = ESP_OK;
            if (out_device)
                *out_device = &s_devices[i];
            if (out_index)
                *out_index = i;
            return ret;
        }
    }
    return ret;
}
#pragma endregion

__attribute__((weak)) void my_espnow_reciver_recv_input_report_cb(uint8_t *data, uint8_t data_len)
{
    return;
};

__attribute__((weak)) void my_espnow_reciver_recv_get_output_cb()
{
    return;
};

__attribute__((weak)) void my_espnow_device_recv_output_report_cb(uint8_t *data, uint8_t data_len)
{
    // ESP_LOGI(TAG, "weak function my_espnow_device_recv_output_report_cb() called");
    return;
};

// wifi是为了启动http服务器，关闭http服务器时也关闭wifi
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include <string.h>
// #include "esp_log.h"
#include "lwip/apps/netbiosns.h"
#include "mdns.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "netdb.h"

#include "my_config.h"
#include "my_espnow.h"
#include "my_file_server.h"
#include "my_init.h"
#include "my_wifi_config.h"
#include "my_wifi_private.h"
#include "my_wifi_start.h"

#pragma region
static const char *TAG = "my wifi start";
static const char *TAG_AP = "my ap";
static const char *TAG_STA = "my sta";

static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;

static int s_retry_num = 0;

#ifdef CONFIG_MY_STA_USE_MDNS
uint8_t my_sta_use_mdns = CONFIG_MY_STA_USE_MDNS;
#else
uint8_t my_sta_use_mdns = 0;
#endif

#ifdef CONFIG_MY_STA_MDNS_HOSTNAME
char *my_mdns_hostname = CONFIG_MY_STA_MDNS_HOSTNAME;
#else
char *my_mdns_hostname = "esp32";
#endif

#ifdef CONFIG_MY_WIFI_STA_USE_STATIC_IP
uint8_t my_sta_use_static_ip = CONFIG_MY_WIFI_STA_USE_STATIC_IP;
#else
uint8_t my_sta_use_static_ip = 0;
#endif
uint8_t my_static_ip_change = 1;

#if CONFIG_MY_WIFI_STA_USE_STATIC_IP
char my_sta_ip[16] = CONFIG_MY_WIFI_STA_IP;
char my_sta_netmask[16] = CONFIG_MY_WIFI_STA_NETMASK;
char my_sta_gateway[16] = CONFIG_MY_WIFI_STA_GATEWAY;
char my_sta_dns1[16] = CONFIG_MY_WIFI_STA_DNS1;
char my_sta_dns2[16] = CONFIG_MY_WIFI_STA_DNS2;
#else
char my_sta_ip[16] = "";
char my_sta_netmask[16] = "";
char my_sta_gateway[16] = "";
char my_sta_dns1[16] = "";
char my_sta_dns2[16] = "";
#endif

char my_sta_ssid[32] = CONFIG_ESP_WIFI_STA_SSID;
char my_sta_password[64] = CONFIG_ESP_WIFI_STA_PASSWORD;
uint8_t my_sta_max_retry = CONFIG_ESP_STA_MAXIMUM_RETRY;

char my_ap_ssid[32] = CONFIG_ESP_WIFI_AP_SSID;
char my_ap_password[64] = CONFIG_ESP_WIFI_AP_PASSWORD;
uint8_t my_ap_channel = CONFIG_ESP_WIFI_AP_CHANNEL;
uint8_t my_ap_max_sta_conn = CONFIG_ESP_MAX_STA_CONN_AP;
uint8_t my_ap_staconnected_num = 0;

RTC_DATA_ATTR char my_ap_hostname[32] = {0};
RTC_DATA_ATTR char my_sta_hostname[32] = {0};
#pragma endregion
my_wifi_info_t s_wifi_info = {
    .netif_inited = 0,
    .wifi_inited = 0,
    .wifi_start = 0,
    .used_features = 0,
    .if_mode = 0,
    .channel = 0,
    .second_chan = 0,
    .task_handle = NULL,
    .evt_group_handle = NULL,
    .sta = {0},
    .ap = {0},
    .espnow = {0},
};

static void my_sta_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void my_ap_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void my_sta_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void my_ap_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void my_mdns_start();
void my_wifi_task(void *arg);
esp_err_t my_set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type);
esp_err_t my_sta_set_static_ip();

void my_sta_config_init(void)
{
    wifi_config_t sta_config = {
        .sta = {
            .scan_method = WIFI_FAST_SCAN,
            // .failure_retry_cnt = my_sta_max_retry, //在连接失败事件中尝试重连，而非自动尝试重连
            .failure_retry_cnt = 0,
            .threshold.authmode = MY_ESP_STA_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .channel = my_ap_channel,
        },
    };
    uint8_t id_len = strlen(my_sta_ssid);
    uint8_t pwd_len = strlen(my_sta_password);
    memcpy(sta_config.sta.ssid, my_sta_ssid, id_len + 1);
    memcpy(sta_config.sta.password, my_sta_password, pwd_len + 1);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_LOGD(TAG_STA, "sta init ok, ssid: %s, password: %s", sta_config.sta.ssid, sta_config.sta.password);
}

void my_ap_config_init(void)
{
    
    if (strcmp(my_ap_ssid, "NONE") == 0) {
        my_get_chip_mac();
        sprintf(my_ap_ssid, "ESP32AP_%s", my_chip_macstr_short);
    }

    uint8_t id_len = strlen(my_ap_ssid);
    uint8_t pwd_len = strlen(my_ap_password);
    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = id_len,
            .channel = my_ap_channel,
            .max_connection = my_ap_max_sta_conn,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    memcpy(ap_config.ap.ssid, my_ap_ssid, id_len + 1);
    memcpy(ap_config.ap.password, my_ap_password, pwd_len + 1);

    if (pwd_len == 0)
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d", my_ap_ssid, my_ap_password, my_ap_channel);
}

// 根据sta的dns，设置ap的dns，本应用中用不到
void softap_set_dns_addr_with_sta(esp_netif_t *esp_netif_ap, esp_netif_t *esp_netif_sta)
{
    esp_netif_dns_info_t dns;
    esp_netif_get_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns);
    uint8_t dhcps_offer_option = DHCPS_OFFER_DNS;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(esp_netif_ap));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(esp_netif_ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_offer_option, sizeof(dhcps_offer_option)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif_ap, ESP_NETIF_DNS_MAIN, &dns));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(esp_netif_ap));
}

void my_wifi_init_internal()
{
    if (!s_wifi_info.netif_inited) {
        my_nvs_init();
        my_esp_default_event_loop_init();
        my_get_chip_mac();
        ESP_ERROR_CHECK(esp_netif_init());
        s_wifi_info.netif_inited = 1;
    }

    if (s_wifi_info.sta.enable && !sta_netif) {
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &my_sta_wifi_event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &my_sta_ip_event_handler, NULL));
        sta_netif = esp_netif_create_default_wifi_sta();
        if (my_sta_hostname[0] == 0) {
            sprintf(my_sta_hostname, "ESP32S3-KB-%s", my_chip_macstr_short);
        }
        esp_netif_set_hostname(sta_netif, my_sta_hostname);
    }

    if (s_wifi_info.ap.enable && !ap_netif) {
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &my_ap_wifi_event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &my_ap_ip_event_handler, NULL));
        ap_netif = esp_netif_create_default_wifi_ap();
        if (my_ap_hostname[0] == 0) {
            sprintf(my_ap_hostname, "ESP32S3-AP-%s", my_chip_macstr_short);
        }
        esp_netif_set_hostname(sta_netif, my_ap_hostname);
    }

    if (!s_wifi_info.wifi_inited) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        esp_wifi_set_country_code("CN", 1);
        // esp_wifi_set_ps(WIFI_PS_NONE);
        s_wifi_info.wifi_inited = 1;
    }
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (s_wifi_info.sta.enable)
        mode |= WIFI_MODE_STA;
    if (s_wifi_info.ap.enable)
        mode |= WIFI_MODE_AP;
    if (mode && (s_wifi_info.if_mode != mode)) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
        s_wifi_info.if_mode = mode;
    }

    if (s_wifi_info.sta.enable && !s_wifi_info.sta.inited) {
        if (wake_up_count <= 1)
            my_sta_config_init();
        if (my_sta_use_mdns)
            my_mdns_start();
        s_wifi_info.sta.inited = 1;
    }

    if (s_wifi_info.ap.enable && !s_wifi_info.ap.inited) {
        if (wake_up_count <= 1)
            my_ap_config_init();
        s_wifi_info.ap.inited = 1;
    }
}

esp_err_t my_wifi_set_features(uint8_t target_features)
{
    if (target_features == s_wifi_info.used_features) {
        ESP_LOGD(TAG, "WiFi features already set");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_wifi_info.evt_group_handle) {
        ESP_LOGI(TAG, "create WiFi event group");
        s_wifi_info.evt_group_handle = xEventGroupCreate();
        assert(s_wifi_info.evt_group_handle);
    }

    if (!s_wifi_info.task_handle) {
        xTaskCreatePinnedToCore(my_wifi_task, "my wifi task", 4096, NULL, 1, &s_wifi_info.task_handle, 1);
        assert(s_wifi_info.task_handle);
    }
    EventBits_t _bits = 0;
    if (target_features & 8)
        _bits = MY_WIFI_SET_ESPNOW;
    else if ((target_features & 1) && (target_features & 2))
        _bits = MY_WIFI_SET_APSTA;
    else if (target_features & 1)
        _bits = MY_WIFI_SET_STA;
    else if (target_features & 2)
        _bits = MY_WIFI_SET_AP;
    else
        _bits = MY_WIFI_SET_STOP;
    _bits = xEventGroupSetBits(s_wifi_info.evt_group_handle, _bits);
    return ESP_OK;
};

esp_err_t my_wifi_start_internal(my_wifi_event_group_bit_t operate_bit)
{
    if (!operate_bit || (operate_bit & MY_WIFI_SET_STOP))
        return ESP_ERR_INVALID_ARG;
    wifi_mode_t target_if_mode = WIFI_MODE_NULL;
    uint8_t target_features = 0;
    // esp_err_t ret = ESP_FAIL;

    if (operate_bit & MY_WIFI_SET_ESPNOW) {
        target_if_mode = WIFI_MODE_STA;
        target_features = 8;
        s_wifi_info.espnow.enable = 1;
        s_wifi_info.ap.enable = 0;
        s_wifi_info.sta.enable = 0;
    }
    else if (operate_bit & MY_WIFI_SET_STA) {
        target_if_mode = WIFI_MODE_STA;
        target_features = 1;
        s_wifi_info.sta.enable = 1;
        s_wifi_info.ap.enable = 0;
        s_wifi_info.espnow.enable = 0;
    }
    else if (operate_bit & MY_WIFI_SET_AP) {
        target_if_mode = WIFI_MODE_AP;
        target_features = 2;
        s_wifi_info.ap.enable = 1;
        s_wifi_info.sta.enable = 0;
        s_wifi_info.espnow.enable = 0;
    }
    else if (operate_bit & MY_WIFI_SET_APSTA) {
        target_if_mode = WIFI_MODE_APSTA;
        target_features = 3;
        s_wifi_info.ap.enable = 1;
        s_wifi_info.sta.enable = 1;
        s_wifi_info.espnow.enable = 0;
    }

    my_wifi_init_internal();

    if (!s_wifi_info.espnow.enable && s_wifi_info.espnow.inited) {
        my_espnow_deinit();
        s_wifi_info.espnow.inited = 0;
    }

    if (s_wifi_info.if_mode != target_if_mode) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(target_if_mode));
        s_wifi_info.if_mode = target_if_mode;
    }

    if (!s_wifi_info.wifi_start) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_info.wifi_start = 1;
    }

    if (s_wifi_info.espnow.enable && !s_wifi_info.espnow.inited) // MY_WIFI_MODE_ESPNOW
    {
        my_espnow_init();
        s_wifi_info.espnow.inited = 1;
    }

    s_wifi_info.used_features = target_features;
    s_wifi_info.ap.running = s_wifi_info.ap.enable;
    s_wifi_info.sta.running = s_wifi_info.sta.enable;
    s_wifi_info.espnow.running = s_wifi_info.espnow.enable;

    return ESP_OK;
}

esp_err_t my_wifi_stop_internal()
{
    if (!s_wifi_info.wifi_start)
        return ESP_ERR_INVALID_STATE;
    // esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_OK) {
        if (s_wifi_info.espnow.inited) {
            my_espnow_deinit();
            s_wifi_info.espnow.inited = 0;
        }
        s_wifi_info.used_features = 0;
        // s_wifi_info.if_mode = WIFI_MODE_NULL;
        s_wifi_info.ap.running = 0;
        s_wifi_info.sta.running = 0;
        s_wifi_info.espnow.running = 0;
        s_wifi_info.wifi_start = 0;
    }
    return ret;
}

static void my_sta_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data)
{
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            if (s_wifi_info.sta.enable)
                esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            s_wifi_info.sta.connected = 1;
            if (!s_wifi_info.sta.enable)
                esp_wifi_disconnect();
            else if (my_sta_use_static_ip)
                my_sta_set_static_ip(sta_netif);
            xEventGroupClearBits(s_wifi_info.evt_group_handle, MY_STA_DISCONNECTED_BIT);
            xEventGroupSetBits(s_wifi_info.evt_group_handle, MY_STA_CONNECTED_BIT);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            s_wifi_info.sta.connected = 0;
            if (s_wifi_info.sta.enable) {
                if (s_retry_num < my_sta_max_retry) {
                    esp_wifi_connect();
                    s_retry_num++;
                }
                else
                    my_wifi_state_change_cb(WIFI_MODE_STA, 0);
            }
            else {
                my_wifi_state_change_cb(WIFI_MODE_STA, 0);
            }
            xEventGroupClearBits(s_wifi_info.evt_group_handle, MY_STA_CONNECTED_BIT);
            xEventGroupSetBits(s_wifi_info.evt_group_handle, MY_STA_DISCONNECTED_BIT);
            break;
        default:
            break;
    }
}

static void my_sta_ip_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            s_retry_num = 0;
            s_wifi_info.sta.connected = 1;
            if (!s_wifi_info.sta.enable)
                esp_wifi_disconnect();
            my_wifi_state_change_cb(WIFI_MODE_STA, 1);
            break;
        default:
            break;
    }
}

static void my_ap_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data)
{
    switch (event_id) {
        case WIFI_EVENT_AP_STACONNECTED:
            s_wifi_info.ap.connected = 1;
            s_wifi_info.ap.user_data++;
            s_wifi_info.ap.user_data = s_wifi_info.ap.user_data > my_ap_max_sta_conn ? my_ap_max_sta_conn : s_wifi_info.ap.user_data;
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            s_wifi_info.ap.user_data--;
            if (s_wifi_info.ap.user_data == 0) {
                s_wifi_info.ap.connected = 0;
                my_wifi_state_change_cb(WIFI_MODE_AP, s_wifi_info.ap.user_data);
            }
        default:
            break;
    }
}

static void my_ap_ip_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    switch (event_id) {
        case IP_EVENT_AP_STAIPASSIGNED:
            my_wifi_state_change_cb(WIFI_MODE_AP, s_wifi_info.ap.user_data);
            break;
        default:
            break;
    }
}

static void my_mdns_start()
{
    if (mdns_init() != ESP_OK) {
        ESP_LOGE(TAG, "mdns init fail");
        return;
    }
    mdns_hostname_set(my_mdns_hostname);
    mdns_instance_name_set(my_mdns_hostname);
    esp_err_t ret = mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    netbiosns_init();
    netbiosns_set_name(my_mdns_hostname);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mdns service add fail");
        return;
    }
    // ESP_LOGI(TAG, "mdns start ok");
}

void my_wifi_task(void *arg)
{
    ESP_LOGI(TAG, "my wifi task start");
    EventBits_t _bits;
    for (;;) {
        _bits = xEventGroupWaitBits(s_wifi_info.evt_group_handle, MY_WIFI_SET_OPERATE_BITS_MASK, pdTRUE, pdFAIL, portMAX_DELAY);
        ESP_LOGI(TAG, "my wifi task get bit 0x%lx", _bits);
        if (_bits & MY_WIFI_SET_STOP) {
            if (my_file_server_running()) {
                my_file_server_stop();
            }
            my_wifi_stop_internal();
        }
        else if (_bits & MY_WIFI_SET_ESPNOW) {
            s_wifi_info.sta.enable = 0; // 先将sta设置为关闭，防止断开连接事件尝试重连
            if (my_file_server_running()) {
                my_file_server_stop();
            }
            if (s_wifi_info.sta.connected) // 之前是sta模式时，如果已连接，要断开连接
            {
                EventBits_t conn_bit;
                int8_t retry = 3;
                do {
                    esp_wifi_disconnect();
                    conn_bit = xEventGroupWaitBits(s_wifi_info.evt_group_handle, MY_STA_EVENT_BITS_MASK, pdTRUE, pdFAIL, MY_WIFI_DELAY_MAX);
                    if (conn_bit & MY_STA_DISCONNECTED_BIT)
                        retry = 0;
                    else
                        retry--;
                } while (retry > 0);
            }
            // else if (s_wifi_info.sta.running) // 理论上存在一种情况，切换到sta模式后马上切换到espnow，但还没连接到ap,不知道wifi底层如何处理，总之在事件回调中会尝试断开连接
            // {
            //     //esp_wifi_sta_get_ap_info();
            //     esp_wifi_scan_stop();
            //     esp_wifi_disconnect();
            // }
            my_wifi_start_internal(_bits);
        }
        else if ((_bits & MY_WIFI_SET_STA) ||
                 (_bits & MY_WIFI_SET_APSTA) ||
                 (_bits & MY_WIFI_SET_AP)) {

            uint8_t espnow_before = s_wifi_info.espnow.enable;
            if (espnow_before) { // 之前是espnow模式时，关闭espnow，理论上不应该被执行，因为在更上层，espnow开启时不允许开启正常的WiFi功能
                my_espnow_deinit();
                s_wifi_info.espnow.enable = 0;
            }

            my_wifi_start_internal(_bits);

            if (espnow_before && s_wifi_info.sta.enable) { // 之前是espnow模式时，要手动运行wifi connect
                esp_err_t ret = esp_wifi_connect();
                if (ret != ESP_OK)
                    ESP_LOGI(TAG, "esp_wifi_connect() failed: %d", ret);
            }
            if (!my_file_server_running()) {
                my_file_server_start();
            }
        }
    }
    vTaskDelete(NULL);
}

esp_err_t my_set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type)
{
    if (addr && (addr != IPADDR_NONE)) {
        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = addr;
        dns.ip.type = IPADDR_TYPE_V4;
        esp_netif_set_dns_info(netif, type, &dns);
    }
    return ESP_OK;
}

// 不检查dns是否设置成功
esp_err_t my_sta_set_static_ip()
{
    if (!my_static_ip_change) {
        return ESP_OK;
    }

    if (strcmp(my_sta_ip, "") == 0 ||
        strcmp(my_sta_gateway, "") == 0 ||
        strcmp(my_sta_netmask, "") == 0 ||
        strcmp(my_sta_dns1, "") == 0) {
        my_sta_use_static_ip = 0;
        ESP_LOGE(TAG_STA, "one of static ip configs has no value, static ip not set. (dns2 is optional)");
        return ESP_ERR_INVALID_ARG;
    }
    if (!sta_netif)
        return ESP_ERR_INVALID_STATE;
    esp_err_t ret = esp_netif_dhcpc_stop(sta_netif);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_STA, "dhcpc stop failed, reason: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG_STA, "dhcpc stop ok, set static ip");
    esp_netif_ip_info_t ip_info;
    memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
    ip_info.ip.addr = esp_ip4addr_aton(my_sta_ip);
    ip_info.gw.addr = esp_ip4addr_aton(my_sta_gateway);
    ip_info.netmask.addr = esp_ip4addr_aton(my_sta_netmask);
    ret = esp_netif_set_ip_info(sta_netif, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_STA, "set static ip fail,restart dhcp client ");
        esp_netif_dhcpc_start(sta_netif);
        return ret;
    }

    ESP_LOGI(TAG_STA, "set static ip ok,ip:%s gateway:%s netmask:%s", my_sta_ip, my_sta_gateway, my_sta_netmask);

    my_set_dns_server(sta_netif, esp_ip4addr_aton(my_sta_dns1), ESP_NETIF_DNS_MAIN);
    if (strcmp(my_sta_dns2, "") != 0)
        my_set_dns_server(sta_netif, esp_ip4addr_aton(my_sta_dns2), ESP_NETIF_DNS_BACKUP);
    my_static_ip_change = 0;
    return ESP_OK;
}

__attribute__((weak)) void my_wifi_state_change_cb(uint8_t wifi_mode, int8_t is_connected)
{
    ESP_LOGI(TAG, "weak change");
    return;
}
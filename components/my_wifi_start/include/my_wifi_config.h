#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define MY_ESP_STA_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#define DHCPS_OFFER_DNS 0x02

extern uint8_t my_sta_use_mdns;

extern char *my_mdns_hostname;

extern uint8_t my_sta_use_static_ip; // 设置了该参数后，在sta连接事件中会设置静态ip
extern uint8_t my_static_ip_change;  // 如果要修改静态ip，需要将该参数设为1
extern char my_sta_ip[16];
extern char my_sta_netmask[16];
extern char my_sta_gateway[16];
extern char my_sta_dns1[16];
extern char my_sta_dns2[16];

RTC_DATA_ATTR extern char my_sta_ssid[32];
RTC_DATA_ATTR extern char my_sta_password[64];
extern uint8_t my_sta_max_retry;

RTC_DATA_ATTR extern char my_ap_ssid[32];
RTC_DATA_ATTR extern char my_ap_password[64];
RTC_DATA_ATTR extern uint8_t my_ap_channel;
extern uint8_t my_ap_max_sta_conn;
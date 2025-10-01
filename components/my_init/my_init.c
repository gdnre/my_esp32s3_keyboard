#include "my_init.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include <stdio.h>

uint8_t my_chip_mac[6] = {0};
uint8_t my_chip_mac_size = sizeof(my_chip_mac);
uint16_t my_chip_mac16 = 0;
char my_chip_macstr_short[5] = {0};

static uint8_t nvs_ok = 0;
static uint8_t event_loop_ok = 0;
RTC_DATA_ATTR static uint8_t get_mac_ok = 0;

// static const char *TAG = "my_init";

void my_nvs_init(void)
{
    if (nvs_ok) {
        return;
    }
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    nvs_ok = 1;
}

void my_esp_default_event_loop_init(void)
{
    if (event_loop_ok) {
        return;
    }

    esp_err_t ret = esp_event_loop_create_default();
    ESP_ERROR_CHECK(ret);
    event_loop_ok = 1;
}

void my_get_chip_mac(void)
{
    if (get_mac_ok) {
        return;
    }

    esp_err_t ret = esp_read_mac(my_chip_mac, ESP_MAC_EFUSE_FACTORY);
    ESP_ERROR_CHECK(ret);
    my_chip_mac16 = (my_chip_mac[4] << 8) | my_chip_mac[5];
    snprintf(my_chip_macstr_short, sizeof(my_chip_macstr_short), "%02x%02x", my_chip_mac[4], my_chip_mac[5]);
    my_chip_macstr_short[sizeof(my_chip_macstr_short) - 1] = '\0';
    get_mac_ok = 1;
}
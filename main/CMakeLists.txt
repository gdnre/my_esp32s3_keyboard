idf_component_register(SRCS "main.c" "my_ota.c" "my_esp_timer.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES app_update esp_adc my_config
                    REQUIRES  my_init my_wifi_start my_fat_mount json my_usb my_input_keys my_keyboard my_display my_ble_hid my_ina226 my_led_strips)
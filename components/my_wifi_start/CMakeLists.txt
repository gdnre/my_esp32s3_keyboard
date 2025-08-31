idf_component_register(SRCS "my_espnow.c" "my_wifi_start.c" "my_espnow.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi nvs_flash my_init esp_timer
                    PRIV_REQUIRES my_config my_file_server)

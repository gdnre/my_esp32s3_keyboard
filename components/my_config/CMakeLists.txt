idf_component_register(SRCS "my_config.c"
                    INCLUDE_DIRS "include"  
                    REQUIRES my_wifi_start my_keyboard esp_event nvs_flash my_fat_mount json)

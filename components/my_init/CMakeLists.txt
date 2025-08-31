idf_component_register(SRCS "my_init.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash esp_event esp_hw_support)

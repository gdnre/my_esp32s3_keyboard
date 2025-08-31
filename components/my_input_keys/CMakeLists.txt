idf_component_register(SRCS "my_input_keys.c" "my_gpio_keys.c" "my_matrix_keys.c" "my_encoder_keys.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_gpio esp_timer)

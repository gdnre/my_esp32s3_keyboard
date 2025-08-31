idf_component_register(SRCS "my_keyboard.c" "my_kb_key_handler.c"
                    INCLUDE_DIRS "include"
                    REQUIRES my_input_keys my_usb my_ble_hid my_config my_wifi_start)

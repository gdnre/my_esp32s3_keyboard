idf_component_register(SRCS "my_display.c" "my_lvgl_init.c" "my_lvgl_fs.c" "my_lvgl_styles.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_ledc my_config my_fat_mount)

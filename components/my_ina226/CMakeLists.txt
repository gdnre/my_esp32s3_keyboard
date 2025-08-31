idf_component_register(SRCS "my_ina226.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_i2c my_config)

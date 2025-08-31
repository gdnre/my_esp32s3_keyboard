set(srcs "my_ble_hid.c"
         "my_esp_hid_gap.c")
set(include_dirs "include")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_hid
                       PRIV_REQUIRES my_init my_usb)

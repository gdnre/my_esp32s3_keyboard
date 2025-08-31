set(srcs "my_usb.c" "my_msc.c" "my_usb_hid.c" "my_usbd_descriptor.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    REQUIRES  my_fat_mount esp_event my_init)
# target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--whole-archive" ${COMPONENT_LIB} "-Wl,--no-whole-archive"  )
# 禁用函数级优化（防止 -ffunction-sections + --gc-sections 删除未显式调用的函数）
# set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY COMPILE_OPTIONS -fno-function-sections)
# set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_FLAGS -Wl,--gc-sections)

# 关闭整个组件的优化等级
# set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY COMPILE_OPTIONS -O0)
# set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY COMPILE_OPTIONS -fno-gc-sections)
# set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 3)
# idf_component_add_link_dependency(FROM espressif__esp_tinyusb)
# idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
# target_include_directories(${tusb_lib} PRIVATE my_msc_cb.c)
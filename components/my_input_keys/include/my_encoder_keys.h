#pragma once
#include "my_input_base.h"

typedef struct
{
    uint8_t a_pin;            // 编码器引脚之一，类似时钟，在上升或下降沿检测b pin状态
    uint8_t b_pin;            // 通过该引脚判断编码器转动方向
    int8_t active_level;      // 编码器触发时的电平
    uint8_t pull_def;         // 引脚是否默认上下拉
    uint8_t change_direction; // 交换编码器的转动方向
} my_input_encoder_keys_config_t;

typedef void *my_encoder_keys_handle_t;

/// @brief 将按键配置复制到内部结构体中，并初始化每个按键配置，并非线程安全，需要确保引脚数组长度等于引脚数，编码器始终只有两个按键，0号为顺时针，1号为逆时针
/// @param encoder_conf 矩阵按键配置
/// @param handle 返回一个指针，传入值不能为null，返回的指针指向内部结构体_my_encoder_keys_handle_t
/// @return 初始化成功返回1，失败返回0，已经初始化返回-1
int8_t my_encoder_keys_config_init(my_input_encoder_keys_config_t *encoder_conf, my_encoder_keys_handle_t *handle);

void my_encoder_keys_io_init(my_encoder_keys_handle_t handle);

// 检查编码器引脚状态并触发对应按键事件
// 如果编码器旋钮有任意被触发，返回1，否则返回0，无法根据返回值判断哪个按键被按下
uint8_t check_encoder_keys(my_encoder_keys_handle_t handle);

uint8_t my_encoder_key_register_event_cb(int16_t id, my_input_key_event_t event, my_input_key_event_cb_t cb);

void my_encoder_keys_intr_dis(my_encoder_keys_handle_t handle);

void my_encoder_keys_intr_en(my_encoder_keys_handle_t handle);

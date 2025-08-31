// todo 将返回错误码和esp_err_t统一
#pragma once
#include "my_input_base.h"

typedef struct
{
    uint8_t row_num;           // 矩阵按键行数，默认行输入，列输出
    uint8_t col_num;           // 矩阵按键列数，默认列输出，行输入
    int16_t total_num;         // 矩阵按键总数，配置时可以不管，总是初始化为行数*列数
    uint8_t pull_def;          // 开启默认上拉或下拉，根据active_level自动配置
    int8_t active_level;       // 按键按下时的电平
    uint8_t switch_input_pins; // 由行输入列输出切换为列输入行输出
    uint8_t *row_pins;         // 行引脚数组
    uint8_t *col_pins;         // 列引脚数组
} my_input_matrix_config_t;

typedef void *my_matrix_keys_handle_t;

/// @brief 将矩阵按键配置复制到内部结构体中，并初始化每个按键配置，并非线程安全，需要确保引脚数组长度等于引脚数
/// @param matrix_conf 矩阵按键配置
/// @param handle 返回一个指针，传入值不能为null，返回的指针指向内部结构体_my_matrix_keys_handle_t
/// @return 初始化成功返回1，失败返回0，已经初始化返回-1
int8_t my_matrix_keys_config_init(my_input_matrix_config_t *matrix_conf, my_matrix_keys_handle_t *handle);

/// @brief 初始化按键io
/// @param handle 调用init函数获得的指针，必须指向已初始化的_my_matrix_keys_handle_t结构体
/// @return void
void my_matrix_keys_io_init(my_matrix_keys_handle_t handle);

/// @brief 将out端全都设置为指定电平
/// @param handle init函数返回的指针
/// @param is_active_level 是否设置为active_level电平，传入1设置为该电平，0设置为相反电平
void my_matrix_set_out_to_level(my_matrix_keys_handle_t handle, uint8_t is_active_level);

/// @brief 将指定输出引脚设置为触发电平，其它输出引脚设置为非触发电平
/// @param handle init函数返回的指针
/// @param out_index 要设置的输出引脚
/// @return 实际设置的输出引脚，正常情况下返回out_index的值，如果输入非法，返回0xff
uint8_t my_matrix_scan_one_line(my_matrix_keys_handle_t handle, uint8_t out_index);

/// @brief 依次扫描所有行列，如果并执行按键回调函数
/// @param handle init函数返回的指针
/// @return 如果有任意按键为触发状态，返回1，否则返回0
uint8_t check_matrix_keys(my_matrix_keys_handle_t handle);

/// @brief 同check_matrix_keys，但会记录上一列的扫描结果，设置了下一列电平后再处理上一列结果，避免使用delay函数等待引脚电平稳定，任务唤醒时，需要配合my_matrix_scan_one_line先设置一列函数。
/// @param handle
/// @return 返回有多少个按键现在处于按下状态。//todo：注意uint8的限制，以后有需要再修改所有相关函数返回值
/// @note 如果回调中有高优先级任务且比较耗时，可能会导致无法响应极快的按键速度，但一般人类应该无法达到此速度
uint8_t check_matrix_keys_faster(my_matrix_keys_handle_t handle);

uint8_t my_matrix_key_register_event_cb(int16_t id, my_input_key_event_t event, my_input_key_event_cb_t cb);

uint8_t my_matrix_key_register_event_cb2(uint8_t row, uint8_t col, my_input_key_event_t event, my_input_key_event_cb_t cb);

/// @brief 传入行列号，返回id，当切换输入输出时，id会不同，所以配置时需要该函数将行列转换为实际id
/// @param row 行号或输入索引
/// @param col 列号或输出索引
/// @param is_io_index 是否是in_pins，out_pins的索引号
/// @return 输入合法时，返回id，不合法时，返回-1
/// @note 只有在使用in_pins，out_pins时才需要使用is_io_index，其它情况传入0
int16_t my_matrix_to_id(uint8_t row_or_in, uint8_t col_or_out, uint8_t is_io_index);

// 传入id，返回行列号，成功返回0，失败返回1
uint8_t my_id_to_matrix(int16_t id, uint8_t is_io_index, uint8_t *out_row, uint8_t *out_col);

// 取消矩阵按键输入端的中断
void my_matrix_in_keys_intr_dis(my_matrix_keys_handle_t handle);

// 启用矩阵按键输入端中断
void my_matrix_in_keys_intr_en(my_matrix_keys_handle_t handle);

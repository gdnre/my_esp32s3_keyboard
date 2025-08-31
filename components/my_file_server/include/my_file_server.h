#pragma once

// #include "sdkconfig.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t my_file_server_start();
esp_err_t my_file_server_stop();
int8_t my_file_server_running();

/// @brief 将指定长度字符串中的特定字符替换为另一字符
/// @param str 字符串指针，字符串必须可编辑
/// @param str_len 字符串长度，不包括末尾的'\0’
/// @param old_char 旧字符
/// @param new_char 要替换为的新字符
/// @param direction 0从头开始替换，1从末尾开始替换
/// @param max_count 最多要替换的字符数，如果为0则替换所有
/// @return 返回实际替换的字符数
int my_str_replace_char(char *, size_t, int, int, uint8_t, int);

#ifdef __cplusplus
}
#endif

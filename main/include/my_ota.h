#pragma once
#include "esp_err.h"

void my_print_app_desc(void);

/**
 * @brief 使用fat分区中的文件进行ota
 *
 *          1.如果UPDATE_OK_PATH文件存在，或UPDATE_BIN_PATH文件不存在，直接返回ESP_OK；
 *
 *          2.如果文件大小为0或没有另一个ota分区或分区小于文件大小，返回ESP_FAIL，并删除UPDATE_BIN_PATH文件;
 *
 *          3.之后调用esp_ota_begin，并从文件中读取数据通过esp_ota_write写入ota分区;
 *
 *          4.如果最后esp_ota_end返回ok，将下次启动分区设置为新分区；
 *
 *          5.删除UPDATE_BIN_PATH文件，并新建UPDATE_OK_PATH文件指示已经升级过（防止删除bin文件失败）。
 * @param  void
 * @return ESP_OK：成功，ESP_ERR_INVALID_STATE：可能已经用该程序升级过，ESP_ERR_NOT_FOUND：没有更新文件， 其它：失败
 */
esp_err_t my_fat_ota_start(void);

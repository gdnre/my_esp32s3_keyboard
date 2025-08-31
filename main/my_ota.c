#include "my_ota.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "my_fat_mount.h"

#ifndef UPDATE_BIN_PATH
#define UPDATE_BIN_PATH "/__bin_/update.bin"
#endif
#ifndef UPDATE_OK_PATH
#define UPDATE_OK_PATH "/__bin_/OK.README.txt"
#endif

const char *TAG = "my_ota";

void my_print_app_desc(void)
{
    // 打印ota分区数量
    // ESP_LOGI(TAG, "ota partition num in partition table: %d", esp_ota_get_app_partition_count());

    // 打印当前应用运行的分区label
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (running)
        ESP_LOGI(TAG, LOG_COLOR_PURPLE "running partition label: %s" LOG_COLOR_I, running->label);

    // 打印当前应用分区的ota状态
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        switch (ota_state) {
            case ESP_OTA_IMG_NEW:
                ESP_LOGI(TAG, "ota_state: ESP_OTA_IMG_NEW");
                break;
            case ESP_OTA_IMG_PENDING_VERIFY:
                ESP_LOGI(TAG, "ota_state: ESP_OTA_IMG_PENDING_VERIFY");
                break;
            case ESP_OTA_IMG_VALID:
                ESP_LOGI(TAG, "ota_state: ESP_OTA_IMG_VALID");
                break;
            case ESP_OTA_IMG_INVALID:
                ESP_LOGI(TAG, "ota_state: ESP_OTA_IMG_INVALID");
                break;
            case ESP_OTA_IMG_ABORTED:
                ESP_LOGI(TAG, "ota_state: ESP_OTA_IMG_ABORTED");
                break;
            default:
                ESP_LOGW(TAG, "ota_state: unknown, state: %x", ota_state);
                break;
        }
    }
    else
        ESP_LOGW(TAG, "esp_ota_get_state_partition error");

    // const esp_app_desc_t *app_des = esp_app_get_description();
    // ESP_LOGI(TAG, "project name: %s", app_des->project_name);
    // ESP_LOGI(TAG, "version: %s", app_des->version);
    // ESP_LOGI(TAG, "check rollback: %d", esp_ota_check_rollback_is_possible());
}

esp_err_t my_fat_ota_start(void)
{
    const char *_update_ok_path = UPDATE_OK_PATH;
    const char *_update_file_path = UPDATE_BIN_PATH;
    FILE *fp = my_ffopen(_update_ok_path, "r");
    if (fp) {
        ESP_LOGI(TAG, "update ok before");
        my_ffclose(fp);
        return ESP_ERR_INVALID_STATE;
    }
    fp = my_ffopen(_update_file_path, "rb");
    // 如果没有更新文件，直接返回
    if (!fp)
        return ESP_ERR_NOT_FOUND;

    esp_err_t ret = ESP_OK;

    // 获取文件大小
    long f_size = my_file_get_size_raw(fp);
    fseek(fp, 0, SEEK_SET);
    // 获取要写入更新镜像的分区
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    ESP_GOTO_ON_FALSE((update_partition && update_partition->size >= f_size), ESP_FAIL, fail_after_open, __func__, "can't find update partition or no enough place");
    ESP_GOTO_ON_FALSE(f_size > 0, ESP_FAIL, fail_after_open, __func__, "update file size is zero");

    ESP_LOGI(__func__, "find update file in fat drive: %s, size: %ld KB", _update_file_path, f_size / 1024);
    ESP_LOGI(__func__, "next update partition: %s, size: %ld KB", update_partition->label, update_partition->size / 1024);

    esp_ota_handle_t ota_handle = -1;
    ESP_GOTO_ON_ERROR(esp_ota_begin(update_partition, f_size, &ota_handle), fail_after_open, __func__, "esp_ota_begin failed");

    size_t buffer_size = 1024 * 10;
    uint8_t *buffer = malloc(buffer_size);
    ESP_GOTO_ON_FALSE(buffer, ESP_FAIL, ota_fail, __func__, "malloc buffer failed");
    size_t read_size = 0;
    do {
        read_size = fread(buffer, 1, buffer_size, fp);
        ret = esp_ota_write(ota_handle, buffer, read_size);
        ESP_GOTO_ON_ERROR(ret, ota_fail, __func__, "ota write error");
    } while (read_size > 0);
    ret = esp_ota_end(ota_handle);
    free(buffer);

    if (ret == ESP_OK) {
        esp_ota_set_boot_partition(update_partition);
    }
    else {
        ESP_LOGE(TAG, "ota_end failed");
    }

    my_ffclose(fp);
    fp = NULL;
    my_fremove(_update_file_path);

    fp = my_ffopen(_update_ok_path, "w");
    if (fp) {
        const char *str = "please delete this file to update";
        fprintf(fp, "%s\n已经向分区label为%s的分区写入数据，更新数据文件路径为%s，数据大小为%ldKB", str, update_partition->label, _update_file_path, f_size / 1024);
        // fwrite(str, 1, strlen(str), fp);
        my_ffclose(fp);
        fp = NULL;
    }

    return ret;

ota_fail:
    esp_ota_abort(ota_handle);
    free(buffer);
fail_after_open:
    my_ffclose(fp);
    fp = NULL;
    my_fremove(_update_file_path);
    return ret;
}
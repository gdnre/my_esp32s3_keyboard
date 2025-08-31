/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* HTTP File Server Example, SD card / SPIFFS mount functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdkconfig.h"
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "driver/spi_common.h"
#include "errno.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "soc/soc_caps.h"
#include <stdio.h>

#include "my_fat_mount.h"

static const char *TAG = "my fat mount";

#define MY_FAT_BASE_PATH_MAX_LEN 32
#define MY_FAT_LABEL_MAX_LEN 16 // sizeof(((esp_partition_t *)0)->label)

#if CONFIG_MY_FAT_USE_CUSTOM_SETTING
// 需要和分区表中fat分区的name/label保持一致
static const char custom_base_path[MY_FAT_BASE_PATH_MAX_LEN + 1] = CONFIG_MY_FAT_CUSTOM_BASE_PATH;
static const char custom_partition_label[MY_FAT_LABEL_MAX_LEN + 1] = CONFIG_MY_FAT_CUSTOM_PARTITION_LABEL;
#endif

RTC_DATA_ATTR char used_fat_base_path[MY_FAT_BASE_PATH_MAX_LEN + 1] = {'\0'};
RTC_DATA_ATTR char used_fat_label[MY_FAT_LABEL_MAX_LEN + 1] = {'\0'};
static const char *default_base_path = "/spiflash";

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
uint8_t my_fat_mounted = 0;
uint64_t my_fat_total_bytes = 0;
uint64_t my_fat_free_bytes = 0;

uint8_t my_fat_is_mount()
{
    return my_fat_mounted;
}
// 使用传入的参数，设置fat根目录和分区
// 如果任一参数已经设置，返回ESP_ERR_INVALID_STATE，但另一参数仍会被设置
static esp_err_t _set_used_fat_str(const char *fat_path, const char *fat_label)
{
    esp_err_t ret = ESP_OK;

    if (used_fat_base_path[0] == '\0') {
        if (fat_path) {
            int path_len = strlen(fat_path);
            if (path_len > MY_FAT_BASE_PATH_MAX_LEN) {
                ESP_LOGI(TAG, "fat base path too long: %s", fat_path);
                return ESP_ERR_NO_MEM;
            }
            strlcpy(used_fat_base_path, fat_path, path_len + 1);
        }
        else {
#if CONFIG_MY_FAT_USE_CUSTOM_SETTING
            int path_len = strlen(custom_base_path);
            strlcpy(used_fat_base_path, custom_base_path, path_len + 1);
#else
            int path_len = strlen(default_base_path);
            strlcpy(used_fat_base_path, default_base_path, path_len + 1);
#endif
        }
    }

    if (used_fat_label[0] == '\0') {
        if (fat_label) {
            int label_len = strlen(fat_label);
            if (label_len > MY_FAT_LABEL_MAX_LEN) {
                ESP_LOGI(TAG, "fat partition label too long: %s", fat_label);
                used_fat_base_path[0] = '\0';
                return ESP_ERR_NO_MEM;
            }
            strlcpy(used_fat_label, fat_label, label_len + 1);
        }
        else {
#if CONFIG_MY_FAT_USE_CUSTOM_SETTING
            int label_len = strlen(custom_partition_label);
            strlcpy(used_fat_label, custom_partition_label, label_len + 1);
#else
            char *_l = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL)->label;
            int label_len = strlen(_l);
            strlcpy(used_fat_label, _l, label_len + 1);
#endif
        }
    }
    return ret;
}

esp_err_t my_mount_fat(const char *base_path, const char *partition_label)
{
    esp_err_t ret = _set_used_fat_str(base_path, partition_label);

    if (ret != ESP_OK)
        return ret;
    if (my_fat_mounted) {
        return ESP_OK;
    }
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE, // >= 4096 ? CONFIG_WL_SECTOR_SIZE * 2 : CONFIG_WL_SECTOR_SIZE * 4,
        .use_one_fat = false};
    ret = esp_vfs_fat_spiflash_mount_rw_wl(used_fat_base_path, used_fat_label, &mount_config, &s_wl_handle);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to mount FATFS (%s), try format", esp_err_to_name(ret));
        mount_config.format_if_mount_failed = true;
        ret = esp_vfs_fat_spiflash_mount_rw_wl(used_fat_base_path, used_fat_label, &mount_config, &s_wl_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(ret));
            return ret;
        }
    }
    ESP_LOGI(__func__, "mount fat success");
    my_fat_mounted = 1;
    return ret;
}

esp_err_t my_unmount_fat(void)
{
    if (s_wl_handle == WL_INVALID_HANDLE) {
        return ESP_OK;
    }
    my_fat_mounted = 0;
    wl_handle_t tmp_handle = s_wl_handle;
    s_wl_handle = WL_INVALID_HANDLE;
    esp_err_t ret = esp_vfs_fat_spiflash_unmount_rw_wl(used_fat_base_path, tmp_handle);
    used_fat_base_path[0] = '\0';
    used_fat_label[0] = '\0';
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Unmounted FATfs successed");
    }
    else {
        ESP_LOGW(TAG, "Unmount FATfs failed");
    }
    return ret;
}

esp_err_t my_fat_get_size_info(uint64_t *out_total_bytes, uint64_t *out_free_bytes)
{
    if (s_wl_handle == WL_INVALID_HANDLE) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!out_free_bytes && !out_total_bytes) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = esp_vfs_fat_info(used_fat_base_path, &my_fat_total_bytes, &my_fat_free_bytes);
    if (ret != ESP_OK) {
        return ret;
    }
    if (out_total_bytes) {
        *out_total_bytes = my_fat_total_bytes;
    }
    if (out_free_bytes) {
        *out_free_bytes = my_fat_free_bytes;
    }
    return ESP_OK;
}

bool my_file_exist(const char *path)
{
    if (s_wl_handle == WL_INVALID_HANDLE || !path) {
        return false;
    }
    char path_buf[MY_FAT_MAX_PATH_LEN] = {0};
    snprintf(path_buf, MY_FAT_MAX_PATH_LEN, "%s%s", used_fat_base_path, path);
    return (access(path_buf, F_OK) == 0);
}

int my_ffstat(const char *path, struct stat *out_st)
{
    if (s_wl_handle == WL_INVALID_HANDLE || !path || !out_st) {
        return -1;
    }
    char path_buf[MY_FAT_MAX_PATH_LEN] = {0};
    snprintf(path_buf, MY_FAT_MAX_PATH_LEN, "%s%s", used_fat_base_path, path);
    return stat(path_buf, out_st);
}

DIR *my_opendir(const char *path)
{
    if (s_wl_handle == WL_INVALID_HANDLE || !path) {
        return NULL;
    }
    char path_buf[MY_FAT_MAX_PATH_LEN] = {0};
    snprintf(path_buf, MY_FAT_MAX_PATH_LEN, "%s%s", used_fat_base_path, path);
    return opendir(path_buf);
}

int my_closedir(DIR *d)
{
    if (d) {
        return closedir(d);
    }
    return -1;
}

int my_mkdir(const char *path, uint32_t mode)
{
    if (s_wl_handle == WL_INVALID_HANDLE || !path) {
        return -1;
    }
    if (mode == 0) {
        mode = 0755;
    }
    char path_buf[MY_FAT_MAX_PATH_LEN] = {0};
    snprintf(path_buf, MY_FAT_MAX_PATH_LEN, "%s%s", used_fat_base_path, path);
    return mkdir(path_buf, mode);
}

int my_rmdir(const char *path)
{
    if (s_wl_handle == WL_INVALID_HANDLE || !path || strlen(path) < 2) {
        return -1;
    }
    char path_buf[MY_FAT_MAX_PATH_LEN] = {0};
    sniprintf(path_buf, MY_FAT_MAX_PATH_LEN, "%s%s", used_fat_base_path, path);
    return rmdir(path_buf);
}

int my_rename(const char *old_path, const char *new_path)
{
    if (s_wl_handle == WL_INVALID_HANDLE || !old_path || !new_path) {
        return -1;
    }
    size_t old_path_len = strlen(old_path);
    size_t new_path_len = strlen(new_path);
    if (old_path_len < 2 || new_path_len < 2) {
        return -1;
    }
    char new_buf[MY_FAT_MAX_PATH_LEN] = {0};
    sniprintf(new_buf, MY_FAT_MAX_PATH_LEN, "%s%s", used_fat_base_path, new_path);
    if (access(new_buf, F_OK) == 0) {
        return -1;
    }
    char old_buf[MY_FAT_MAX_PATH_LEN] = {0};
    sniprintf(old_buf, MY_FAT_MAX_PATH_LEN, "%s%s", used_fat_base_path, old_path);
    return rename(old_buf, new_buf);
}

FILE *my_ffopen(const char *path, const char *mode)
{
    if (s_wl_handle == WL_INVALID_HANDLE || !path) {
        return NULL;
    }
    char path_buf[MY_FAT_MAX_PATH_LEN] = {0};
    snprintf(path_buf, MY_FAT_MAX_PATH_LEN, "%s%s", used_fat_base_path, path);
    if (!mode) {
        return fopen(path_buf, "r");
    }
    return fopen(path_buf, mode);
}

int my_fftruncate(FILE *fp, long length)
{
    if (fp) {
        return ftruncate(fileno(fp), length);
    }
    return -1;
}

int my_ffclose(FILE *fp)
{
    if (fp) {
        return fclose(fp);
    }
    return -1;
}

size_t my_ffwrite(const void *buf, size_t size, size_t num, FILE *fp)
{
    if (fp) {
        size_t ret = fwrite(buf, size, num, fp);
        return ret;
    }
    return 0;
}

size_t my_ffread(void *buf, size_t size, size_t num, FILE *fp)
{
    if (fp) {
        return fread(buf, size, num, fp);
    }
    return 0;
}

int my_ffseek(FILE *fp, long offset, int whence)
{
    if (fp) {
        return fseek(fp, offset, whence);
    }
    return -1;
}

long my_fftell(FILE *fp)
{
    if (fp) {
        return ftell(fp);
    }
    return -1;
}

int my_fremove(const char *path)
{
    if (s_wl_handle == WL_INVALID_HANDLE || !path || strlen(path) < 2) {
        return -1;
    }
    char path_buf[MY_FAT_MAX_PATH_LEN] = {0};
    sniprintf(path_buf, MY_FAT_MAX_PATH_LEN, "%s%s", used_fat_base_path, path);
    return remove(path_buf);
}

long my_file_get_size_raw(FILE *fp)
{
    if (fp) {
        fseek(fp, 0, SEEK_END);
        return ftell(fp);
    }

    return -1;
}

long my_ffget_size(FILE *fp)
{
    if (fp) {
        long cur = ftell(fp);
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, cur, SEEK_SET);
        return size;
    }
    return -1;
}

// 使用open()打开文件，返回文件描述符
// flags为O_RDONLY等
// mode为如果要新建文件时，文件的权限
int my_open_fast(const char *path, int flags, uint32_t mode)
{
    if (s_wl_handle == WL_INVALID_HANDLE || !path) {
        return -1;
    }
    if (mode == 0) {
        mode = 0755;
    }
    char path_buf[MY_FAT_MAX_PATH_LEN] = {0};
    snprintf(path_buf, MY_FAT_MAX_PATH_LEN, "%s%s", used_fat_base_path, path);
    return open(path_buf, flags, mode);
}

int my_close_fast(int fd)
{
    if (s_wl_handle == WL_INVALID_HANDLE || fd < 0) {
        return -1;
    }
    return close(fd);
}

int my_write_fast(int fd, const void *buf, size_t nbytes)
{
    if (s_wl_handle == WL_INVALID_HANDLE || fd < 0) {
        return -1;
    }
    return write(fd, buf, nbytes);
}

int my_read_fast(int fd, void *out_buf, size_t nbytes)
{
    if (s_wl_handle == WL_INVALID_HANDLE || fd < 0) {
        return -1;
    }
    return read(fd, out_buf, nbytes);
}

off_t my_lseek(int fd, off_t offset, int whence)
{
    if (s_wl_handle == WL_INVALID_HANDLE || fd < 0) {
        return -1;
    }
    return lseek(fd, offset, whence);
}

long my_get_size_fast(int fd)
{
    if (fd < 0) {
        return -1;
    }
    off_t cur = lseek(fd, 0, SEEK_CUR);
    long size = lseek(fd, 0, SEEK_END);
    lseek(fd, cur, SEEK_SET);
    return size;
}

int my_fileno(FILE *fp)
{
    if (!fp) {
        return -1;
    }
    return fileno(fp);
}

int my_ftruncate(int fd, long length)
{
    if (fd < 0) {
        return -1;
    }

    return ftruncate(fd, length);
}

#pragma once
#include "dirent.h"
#include "esp_err.h"
#include <fcntl.h>
// #include <stdio.h>
// #include <string.h>

#define MY_OPEN_FLAG_R O_RDONLY
#define MY_OPEN_FLAG_R_PLUS O_RDWR
#define MY_OPEN_FLAG_W O_WRONLY | O_CREAT | O_TRUNC
#define MY_OPEN_FLAG_W_PLUS O_RDWR | O_CREAT | O_TRUNC
#define MY_OPEN_FLAG_A O_WRONLY | O_CREAT | O_APPEND
#define MY_OPEN_FLAG_A_PLUS O_RDWR | O_CREAT | O_APPEND

#define UPDATE_ROOT_PATH "/__bin_/"
#define UPDATE_BIN_PATH "/__bin_/update.bin"
#define UPDATE_OK_PATH "/__bin_/OK.README.txt"
// #define MY_FAT_MAX_PATH_LEN CONFIG_FATFS_MAX_LFN + 1
#define MY_FAT_MAX_PATH_LEN 256

uint8_t my_fat_is_mount();

/// @brief 初始化fat，参数设置为NULL则使用默认参数，该组件只支持挂载一个fat实例，修改目录或分区要先调用my_unmount_fat()
/// @param base_path 挂载的根目录名，默认为/spiflash
/// @param partition_label fat分区的label/name，默认为第一个fat分区
/// @return ESP_OK：挂载成功，其它：挂载失败或已挂载
esp_err_t my_mount_fat(const char *base_path, const char *partition_label);

/// @brief 取消挂载已初始化的fat
/// @param  void
/// @return ESP_OK：取消挂载成功，其它：取消挂载失败或还未调用my_mount_fat()
esp_err_t my_unmount_fat(void);

esp_err_t my_fat_get_size_info(uint64_t *out_total_bytes, uint64_t *out_free_bytes);
// 使用access检查文件是否存在
bool my_file_exist(const char *path);

int my_ffstat(const char *path, struct stat *out_st);
DIR *my_opendir(const char *path);
int my_closedir(DIR *d);
// 新建文件夹，上级目录必须存在才能创建成功
// mode指定文件夹权限，如0755，当输入0时，使用0755
// 成功返回0
int my_mkdir(const char *path, uint32_t mode);
int my_rmdir(const char *path);
// 重命名文件或目录，注意new_path必须不存在
int my_rename(const char *old_path, const char *new_path);
// 传入fat文件系统根目录下的文件路径，如/exp.txt，必须要有/，函数会自动加上根路径，根路径为my_fat中设置的使用中目录，两路径总长不能超过255字节
// 如果mode为null，默认使用"r"，如果要修改数据，使用"r+"，如果要删除并写入一个文件，使用"w"，如果要追加数据，使用"a"，如果要删除并读写一个文件，使用"w+"，如果要追加并读取一个文件，使用"a+"
// mode参数可以添加b，比如rb，rb+，用于以二进制格式打开文件，不加时默认加了t
// 返回fopen()的返回值
FILE *my_ffopen(const char *path, const char *mode);

int my_ffclose(FILE *fp);

size_t my_ffwrite(const void *buf, size_t size, size_t num, FILE *fp);

/// @brief 读取数据到buf
/// @param buf 数据存储buffer
/// @param size 要读取的单位数据的大小，每单位数据只有读取成功或未读取两种可能
/// @param num 要读取的单位数据的数量，总读取字节数为size * num
/// @param fp 一个打开的文件指针
/// @return 实际读取的单位数据数量
size_t my_ffread(void *buf, size_t size, size_t num, FILE *fp);

// 将文件指针移动到指定位置指定偏移量处，位置可以选择SEEK_SET，SEEK_CUR，SEEK_END，
int my_ffseek(FILE *fp, long offset, int whence);

// 返回当前文件指针相对于文件开头的偏移量
long my_fftell(FILE *fp);

int my_fremove(const char *path);

// 获取文件位置，文件不能以a或a+模式打开
//!!!!注意使用该函数后，文件读写指针会指向文件末尾，读写文件时要先重置指针位置
long my_file_get_size_raw(FILE *fp);

// 获取文件位置，文件不能以a或a+模式打开
// 该函数会记录文件之前的指针位置，并在获取文件大小后恢复原来的位置，注意如果在不同任务中调用，可能存在线程安全问题
long my_ffget_size(FILE *fp);

// 改变文件大小，文件必须可写
int my_fftruncate(FILE *fp, long length);

int my_open_fast(const char *path, int flags, uint32_t mode);

int my_close_fast(int fd);

int my_write_fast(int fd, const void *buf, size_t nbytes);

int my_read_fast(int fd, void *out_buf, size_t nbytes);

off_t my_lseek(int fd, off_t offset, int whence);

long my_get_size_fast(int fd);

int my_fileno(FILE *fp);

int my_ftruncate(int fd, long length);

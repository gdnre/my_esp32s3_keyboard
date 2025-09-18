#pragma once
#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "sdkconfig.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "my_config.h"
#include "my_fat_mount.h"
#include "my_http_start.h"

// 比较文件名后缀是否为指定值
#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

#define MY_HTTP_RESP_TYPE_HTML "text/html; charset=utf-8"
#define MY_HTTP_RESP_TYPE_JSON "application/json; charset=utf-8"
#define MY_HTTP_RESP_TYPE_JS "text/javascript; charset=utf-8"
#define MY_HTTP_RESP_TYPE_CSS "text/css; charset=utf-8"
#define MY_HTTP_RESP_TYPE_PDF "application/pdf"
#define MY_HTTP_RESP_TYPE_JPG "image/jpeg"
#define MY_HTTP_RESP_TYPE_GIF "image/gif"
#define MY_HTTP_RESP_TYPE_ICO "image/x-icon"
#define MY_HTTP_RESP_TYPE_PNG "image/png"
#define MY_HTTP_RESP_TYPE_TEXT "text/plain; charset=utf-8"
#define MY_HTTP_RESP_TYPE_XML "text/xml; charset=utf-8"
#define MY_HTTP_RESP_TYPE_BIN "application/octet-stream"

#define MY_URI_WEB_GET "/web/*?"
#define MY_URI_API_FILEGET_GET "/api/fileget"
#define MY_URI_API_FILELIST_GET "/api/filelist"
#define MY_URI_API_DEV_ANY "/api/dev/*?"
#define MY_URI_FILESERVER_GET "/fileserver/?"
#define MY_URI_API_FILEUPLOAD_POST "/api/fileupload"
#define MY_URI_API_FILEDELETE_ANY "/api/filedelete"
#define MY_URI_API_RENAME_GET "/api/rename"
#define MY_URI_API_DIR_GET "/api/dir"
#define MY_URI_BASE_GET "/?*"

#define MY_URI_API_DEV_CURCONFIG_SUB "/curconfig"
#define MY_URI_API_DEV_DEFCONFIG_SUB "/defconfig"
#define MY_URI_API_DEV_NVSCONFIG_SUB "/nvsconfig"
#define MY_URI_API_DEV_KBDCONFIG_SUB "/kbdconfig"
#define MY_URI_API_DEV_GETINFO_SUB "/getinfo"
#define MY_URI_API_DEV_RESTART_SUB "/restart"
#define MY_URI_API_DEV_UPDATE_SUB "/update"
#define MY_URI_API_DEV_SETCONFIG_SUB "/setconfig"

// #define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN)
#define FILE_PATH_MAX MY_FAT_MAX_PATH_LEN
#define MAX_FILE_SIZE (0x350000)
#define MAX_FILE_SIZE_STR "3.4MB"
#define SCRATCH_BUFSIZE 8192
// #define SCRATCH_BUFSIZE CONFIG_LWIP_TCP_SND_BUF_DEFAULT
#define MY_BIGGER_BUFFER_SIZE SCRATCH_BUFSIZE * 4
typedef struct {
    char scratch[SCRATCH_BUFSIZE];
} my_file_server_data_t;

extern const char *path_query_key;
extern const char *default_index_path;
extern const char web_base_path[];
extern const uint8_t web_base_path_len;

extern const char *favicon_path;

extern const char *fileserverDir_path;
extern const char *fileserver_index_path;
extern const char *fileserver_script_path;
extern const char *fileserver_style_path;

extern const char *controlDir_path;
extern const char *control_index_path;
extern const char *control_script_path;
extern const char *control_style_path;

extern const char *dev_html_path;
extern const char *dev_script_path;
extern const char *dev_style_path;

extern const char *kbd_html_path;
extern const char *kbd_script_path;
extern const char *kbd_style_path;

void my_fs_set_server_files();
// 处理url中的特殊字符，将+替换为空格，将%xx中的xx以16进制转换为对应的字符值
// 可以根据out_count来判断是否有解析特殊字符，如果出错，out_count为负数
// 注意：如果url_can_edit为1，则将数据复制到url_str中，并返回url_str，否则返回值是另外malloc的内存，需要调用者free
char *my_http_url_decoder(char *url_str, size_t url_len, int *out_count, uint8_t url_can_edit);
/*根据文件扩展名设置HTTP响应内容类型 */
esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename);
// 根据文件路径设置文件名响应头：Content-Disposition: attachment; filename="name"
// 传入的路径如果以/结尾，将不设置任何内容
// 传入的路径可以不包含/，只有文件名
// 整个内容长度不能超过127
esp_err_t set_file_name_from_path(httpd_req_t *req, const char *file_path, char *file_name_buf, uint8_t buf_size, uint8_t is_attachment);
// 发送重定向响应，重定向到新的路径
void my_http_send_redirect_resp(httpd_req_t *req, const char *location);
/**
 * 获取URI中的特定查询参数，默认为提取路径信息。
 * @param req http请求对象。
 * @param out_path 输出参数，用于存储生成的完整路径的缓冲区。
 * @param query_key 要获取的查询参数的key，如果传入null，使用默认值path
 * @param path_buf_size 分配给path_out缓冲区的大小，用于确保不会发生缓冲区溢出。
 * @return ESP_OK:成功
 */
esp_err_t my_get_path_from_query(httpd_req_t *req, char *out_path, const char *query_key, size_t path_buf_size);

// 获取uri中路径部分的长度，即hostname/path?xxx中第一个/之后、?之前的长度
// 出错则返回-1，等于uri长度说明uri只包含路径
int my_uri_get_path_len(const char *uri);

// 发送文件响应，如果文件小于SCRATCH_BUFSIZE，单次发送，否则使用chunk传输文件
esp_err_t my_http_resp_send_file(httpd_req_t *req, const char *file_path);

// 注意：目录路径必须以/结尾，返回的json字符串使用完毕后要用free释放
char *get_file_list_json(const char *dir_path);

// 触发设备的ota更新，用于更新设备固件，会设置检测更新选项，并自动重启
esp_err_t my_fs_device_update_set(httpd_req_t *req);

esp_err_t my_fs_restart_device(httpd_req_t *req);

esp_err_t my_fs_send_current_configs(httpd_req_t *req, uint8_t convert_nvs, uint8_t convert_key);

esp_err_t my_fs_send_default_configs(httpd_req_t *req);
esp_err_t my_fs_send_device_info_json(httpd_req_t *req);

esp_err_t my_fs_device_setconfig(httpd_req_t *req);

esp_err_t my_file_rename_handler(httpd_req_t *req);

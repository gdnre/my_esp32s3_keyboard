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
#include "my_file_server.h"
#include "my_fs_private.h"
#include "my_http_start.h"

static const char *TAG = "my_file_server";

const char *path_query_key = "path";
const char *default_index_path = "/web/index.html";
const char web_base_path[] = "/web/";
const uint8_t web_base_path_len = sizeof(web_base_path) - 1;

const char *favicon_path = "/web/favicon.ico";

const char *fileserverDir_path = "/web/fileserver/";
const char *fileserver_index_path = "/web/fileserver/index.html";
const char *fileserver_script_path = "/web/fileserver/script.js";
const char *fileserver_style_path = "/web/fileserver/style.css";

const char *controlDir_path = "/web/control/";
const char *control_index_path = "/web/control/index.html";
const char *control_script_path = "/web/control/script.js";
const char *control_style_path = "/web/control/style.css";
static my_file_server_data_t *server_data = NULL;

// 匹配/*?的默认重定向路径
static esp_err_t
index_redirect_handler(httpd_req_t *req)
{
    int uri_len = my_uri_get_path_len(req->uri);

    if (uri_len <= 1) { // 当路径为/或没有/时，重定向到默认的index.html
        // 首先尝试重定向到/web/index.html，这个页面默认不存在，用户可以自己上传
        if (my_file_exist(default_index_path))
            my_http_send_redirect_resp(req, default_index_path);
        // 如果没有默认页面，重定向到默认的导航页面
        else if (my_file_exist(control_index_path))
            my_http_send_redirect_resp(req, control_index_path);
        // 理论上不该不存在，但如果还是不存在，重定向到fileserver的页面
        else
            my_http_send_redirect_resp(req, fileserver_index_path);
        return ESP_OK;
    }
    else { // 其它情况，重定向到/web/path
        uri_len = strlen(req->uri);
        char *file_path = malloc(web_base_path_len + uri_len + 1);
        if (file_path) {
            strlcpy(file_path, web_base_path, web_base_path_len);
            strlcpy(file_path + web_base_path_len - 1, req->uri, uri_len + 1);
            my_http_send_redirect_resp(req, file_path);
            free(file_path);
            return ESP_OK;
        }
        else {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memery");
            return ESP_FAIL;
        }
    }
}

static esp_err_t
download_get_file_handler(httpd_req_t *req)
{
    char file_path[FILE_PATH_MAX] = {0};
    esp_err_t ret = my_get_path_from_query(req, file_path, NULL, sizeof(file_path));
    if (ret == ESP_ERR_NO_MEM) {
        ESP_LOGI(TAG, "get file path no memery");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "host no memery");
        return ESP_ERR_NO_MEM;
    }
    else if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "get path from uri: unknown error");
        return ret;
    }
    if (file_path[0] != '/') {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "only support absolute path");
        return ESP_FAIL;
    }

    size_t path_len = strlen(file_path);
    if (file_path[path_len - 1] == '/') {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "can't download directory");
        return ESP_FAIL;
    }
    int replace_count = 0;
    my_http_url_decoder(file_path, path_len, &replace_count, 1);
    if (!my_file_exist(file_path)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    return my_http_resp_send_file(req, file_path);
}

// 发送目录中的文件列表
static esp_err_t
file_list_handler(httpd_req_t *req)
{
    char file_path[FILE_PATH_MAX] = {0};
    esp_err_t ret = my_get_path_from_query(req, file_path, NULL, sizeof(file_path));
    if (ret == ESP_ERR_NO_MEM) {
        ESP_LOGI(TAG, "get file path no memery");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "host no memery");
        return ESP_ERR_NO_MEM;
    }
    else if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "get path from uri: unknown error");
        return ret;
    }
    if (file_path[0] != '/') {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "only support absolute path");
        return ESP_FAIL;
    }
    size_t path_len = strlen(file_path);
    if (file_path[path_len - 1] != '/') {
        if (path_len >= sizeof(file_path) - 1) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "get path from uri: path too long");
            return ESP_FAIL;
        }
        file_path[path_len] = '/';
        path_len++;
        file_path[path_len] = '\0';
    }
    int replace_count = 0;
    my_http_url_decoder(file_path, path_len, &replace_count, 1);
    struct stat path_stat;
    if (my_ffstat(file_path, &path_stat) != 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Path does not exist");
        return ESP_FAIL;
    }

    // 检查路径是否为目录
    if (!S_ISDIR(path_stat.st_mode)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Path is not dir");
        return ESP_FAIL;
    }

    // 如果是目录，返回目录的文件列表
    char *json_data = get_file_list_json(file_path);
    if (!json_data) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "get file list fail");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_JSON);
    httpd_resp_send(req, json_data, strlen(json_data));
    free(json_data);
    return ESP_OK;
}

// 接受上传的文件
static esp_err_t
upload_post_handler(httpd_req_t *req)
{
    char file_path[FILE_PATH_MAX] = {0};
    esp_err_t ret = my_get_path_from_query(req, file_path, NULL, sizeof(file_path));
    if (ret == ESP_ERR_NO_MEM) {
        ESP_LOGI(TAG, "get file path no memery");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "host no memery");
        return ESP_ERR_NO_MEM;
    }
    else if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "get path from uri: unknown error");
        return ret;
    }
    if (file_path[0] != '/') {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "only support absolute path");
        return ESP_FAIL;
    }
    size_t path_len = strlen(file_path);
    if (file_path[path_len - 1] == '/') {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "can't upload directory");
        return ESP_FAIL;
    }
    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than " MAX_FILE_SIZE_STR "!");
        return ESP_FAIL;
    }

    int replace_count = 0;
    my_http_url_decoder(file_path, path_len, &replace_count, 1);
    // FILE *fp = my_ffopen(file_path, "wb");
    // if (!fp) {
    //     httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
    //     return ESP_FAIL;
    // }
    // int fd = my_fileno(fp);

    int fd = my_open_fast(file_path, MY_OPEN_FLAG_W, 0755);
    if (fd < 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    int remaining_byte = req->content_len;
    char *buf = NULL;
    int buf_total_size = 0;
    uint8_t is_alloc_buf = 0;

    ESP_LOGI(TAG, "Receiving file : %s, size: %d, fileno: %d", file_path, remaining_byte, fd);
    int64_t t2 = 0;
    int64_t t1 = my_get_time();
    if (remaining_byte >= MY_BIGGER_BUFFER_SIZE) { // 数据大时，用一个更大的buffer，对spiflash没啥用，最快写入速度也就60kB左右
        int alloc_size = MY_BIGGER_BUFFER_SIZE;
        buf = (char *)malloc(alloc_size);
        if (!buf) {
            alloc_size = 0;
        }
        buf_total_size = alloc_size;
        is_alloc_buf = 1;
    }
    if (!buf) {
        buf = ((my_file_server_data_t *)req->user_ctx)->scratch;
        buf_total_size = SCRATCH_BUFSIZE;
        is_alloc_buf = 0;
    }
    int buf_remaining_size = buf_total_size;
    char *buf_ptr = buf;
    int received_byte = 0;
    uint8_t retry_count = 0;

    int16_t write_count = 0;
    t1 = my_get_time();
    while (remaining_byte > 0) {
        while (remaining_byte > 0 && buf_remaining_size > 0) {
            received_byte = httpd_req_recv(req, buf_ptr, buf_remaining_size);
            if (received_byte <= 0) {
                if (received_byte == HTTPD_SOCK_ERR_TIMEOUT && retry_count < 10) {
                    retry_count++;
                    continue;
                }
                else
                    goto recv_err;
            }
            else { // if (received_byte > 0)
                retry_count = 0;
                remaining_byte -= received_byte;
                buf_ptr += received_byte;
                buf_remaining_size -= received_byte;
            }
        }
        received_byte = buf_ptr - buf;
        // if (received_byte && (received_byte != my_ffwrite(buf, 1, received_byte, fp))) { goto recv_err; }
        if (received_byte && (received_byte != my_write_fast(fd, buf, received_byte))) { goto recv_err; }
        buf_ptr = buf;
        buf_remaining_size = buf_total_size;

        write_count++;
    }
    // my_ffclose(fp);
    my_close_fast(fd);
    t2 = my_get_time();
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");
    if (is_alloc_buf) { free(buf); }
    ESP_LOGI(TAG, "File storage complete, write count: %d, cost:%.2fms, speed: %.3fKB/s",
             write_count, (t2 - t1) / 1000.0f, req->content_len / ((t2 - t1) / 1000000.0f) / 1024.0f);
    return ESP_OK;
recv_err:
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
    // my_ffclose(fp);
    my_close_fast(fd);
    my_fremove(file_path);
    if (is_alloc_buf) { free(buf); }
    ESP_LOGI(TAG, "File reception or write failed!");
    return ESP_FAIL;
}

static esp_err_t
file_delete_post_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST || req->method != HTTP_GET) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "only support POST or GET method");
        return ESP_FAIL;
    }

    char file_path[FILE_PATH_MAX] = {0};
    esp_err_t ret = my_get_path_from_query(req, file_path, NULL, sizeof(file_path));
    if (ret == ESP_ERR_NO_MEM) {
        ESP_LOGI(TAG, "get file path no memery");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "host no memery");
        return ESP_ERR_NO_MEM;
    }
    else if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "get path from uri: unknown error");
        return ret;
    }
    if (file_path[0] != '/') {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "only support absolute path");
        return ESP_FAIL;
    }
    size_t path_len = strlen(file_path);
    if (file_path[path_len - 1] == '/') {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "can't delete directory");
        return ESP_FAIL;
    }
    int replace_count = 0;
    my_http_url_decoder(file_path, path_len, &replace_count, 1);
    struct stat file_stat;
    if (my_ffstat(file_path, &file_stat) != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }
    if (S_ISDIR(file_stat.st_mode)) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "can't delete directory");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleting file : %s", file_path);
    my_fremove(file_path);

    // httpd_resp_set_status(req, HTTPD_200);
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}

// 匹配/api/dir?type=xx&path=/xx
static esp_err_t
dir_operate_handler(httpd_req_t *req)
{
    char file_path[FILE_PATH_MAX] = {0};
    esp_err_t ret = my_get_path_from_query(req, file_path, NULL, sizeof(file_path));
    if (ret == ESP_ERR_NO_MEM) {
        ESP_LOGI(TAG, "get file path no memery");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "host no memery");
        return ESP_ERR_NO_MEM;
    }
    else if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "get path from uri: unknown error");
        return ret;
    }
    if (file_path[0] != '/') {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "only support absolute path");
        return ESP_FAIL;
    }
    if (file_path[1] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "you are accessing the root path");
        return ESP_FAIL;
    }
    char operate_type[8] = {0};
    my_get_path_from_query(req, operate_type, "type", sizeof(operate_type));
    if (operate_type[0] == 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "can't get dir operate type");
        return ESP_FAIL;
    }
    uint8_t type_len = strlen(operate_type);
    if (type_len < 2) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "unsupport operate type");
        return ESP_FAIL;
    }

    // 归一化处理，路径末尾都加上/
    size_t path_len = strlen(file_path);
    if (file_path[path_len - 1] != '/') {
        if (path_len >= sizeof(file_path) - 1) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "get path from uri: path too long");
            return ESP_FAIL;
        }
        file_path[path_len] = '/';
        path_len++;
        file_path[path_len] = '\0';
    }
    int replace_count = 0;
    my_http_url_decoder(file_path, path_len, &replace_count, 1);

    struct stat path_stat;
    int ret_stat = my_ffstat(file_path, &path_stat);
    if (strncmp(operate_type, "mk", 2) == 0) {
        // 新建文件夹
        if (ret_stat == 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Path already exists");
            return ESP_FAIL;
        }
        if (my_mkdir(file_path, 0755) != 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "mkdir failed");
            return ESP_FAIL;
        }
        httpd_resp_sendstr(req, "mkdir ok");
        ESP_LOGI(TAG, "mkdir : %s", file_path);
    }
    else if (strncmp(operate_type, "rm", 2) == 0) {
        // 删除文件夹
        if (ret_stat != 0) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Path does not exist");
            return ESP_FAIL;
        }
        if (!S_ISDIR(path_stat.st_mode)) {
            httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Path is not dir");
            return ESP_FAIL;
        }
        if (my_rmdir(file_path) != 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "rmdir failed");
            return ESP_FAIL;
        }
        httpd_resp_sendstr(req, "rmdir ok");
        ESP_LOGI(TAG, "rmdir : %s", file_path);
    }
    return ESP_OK;
}

// 匹配/api/dev/*?，直接控制设备，或修改配置
// 如果读取配置是直接读取fat中的文件，修改配置时，最好使用my_config里的函数，将修改后的配置保存在fat中
static esp_err_t
device_control_handler(httpd_req_t *req)
{
    static uint8_t uri_min_len = strlen("/api/dev");
    int uri_len = my_uri_get_path_len(req->uri); // 获取uri路径部分长度
    if (uri_len < uri_min_len) {                 // 理论上不会执行，因为在匹配时就已经过滤了
        goto uri_unsupport;
    }
    const char *last_uri = req->uri + uri_min_len; // 获取剔除前缀后的路径开头
    uri_len = uri_len - uri_min_len;               // 获取最终路径长度
    // GET 请求
    if (req->method == HTTP_GET) {
        if (uri_len == 0) { // 发送设备在线， /api/dev
            httpd_resp_sendstr(req, "device online");
            return ESP_OK;
        }
        else if (uri_len == 10) { // 获取配置 /api/dev/xxxconfig
            if (strncasecmp(last_uri, MY_URI_API_DEV_CURCONFIG_SUB, uri_len) == 0) {
                // 获取设备当前配置 /api/dev/curconfig
                return my_fs_send_current_configs(req, 1, 1);
            }
            else if (strncasecmp(last_uri, MY_URI_API_DEV_DEFCONFIG_SUB, uri_len) == 0) {
                // 获取设备默认配置 /api/dev/defconfig
                return my_fs_send_default_configs(req);
            }
            else if (strncasecmp(last_uri, MY_URI_API_DEV_NVSCONFIG_SUB, uri_len) == 0) {
                // 获取设备当前的nvs配置 /api/dev/curconfig
                return my_fs_send_current_configs(req, 1, 0);
            }
            else if (strncasecmp(last_uri, MY_URI_API_DEV_KBDCONFIG_SUB, uri_len) == 0) {
                // 获取设备当前的键盘配置 /api/dev/kbdconfig
                return my_fs_send_current_configs(req, 0, 1);
            }
            else if (strncmp(last_uri, MY_URI_API_DEV_SETCONFIG_SUB, uri_len) == 0) {
                // 从现存的文件读取配置 /api/dev/setconfig?path=/config/path
                return my_fs_device_setconfig(req);
            }
        }
        else if (uri_len == 8) {
            if (strncmp(last_uri, MY_URI_API_DEV_GETINFO_SUB, uri_len) == 0) {
                // 发送一些设备信息 /api/dev/info
                return my_fs_send_device_info_json(req);
            }
            else if (strncmp(last_uri, MY_URI_API_DEV_RESTART_SUB, uri_len) == 0) {
                // 重启设备 /api/dev/restart
                return my_fs_restart_device(req);
            }
        }
        else if (uri_len == 7 && strncmp(last_uri, MY_URI_API_DEV_UPDATE_SUB, uri_len) == 0) { // 触发设备更新 /api/dev/update
            return my_fs_device_update_set(req);
        }
    }
    // POST 请求
    else if (req->method == HTTP_POST) {
        if (uri_len == 10) {
            if (strncmp(last_uri, MY_URI_API_DEV_SETCONFIG_SUB, uri_len) == 0) {
                // 上传设备配置 /api/dev/setconfig?path=/config/path
                return my_fs_device_setconfig(req);
            }
        }
    }
    // 其它请求
    else {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "method not allowed");
        return ESP_FAIL;
    }
uri_unsupport:
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unsupported path, possible path(start with /api/dev):{/curconfig, /defconfig, /nvsconfig, /kbdconfig, /getinfo, /restart, /update, /setconfig} ");
    return ESP_FAIL;
}

// 匹配/fileserver/?
static esp_err_t
fileserver_index_handler(httpd_req_t *req)
{
    if (!my_file_exist(fileserver_index_path)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "can't find fileserver index file");
        return ESP_FAIL;
    }
    my_http_send_redirect_resp(req, fileserver_index_path);
    return ESP_OK;
}

// 匹配/web/*?路径，当路径存在目标文件时，返回文件，不处理文件夹
static esp_err_t
default_index_handler(httpd_req_t *req)
{
    int path_len = my_uri_get_path_len(req->uri);
    if (path_len < web_base_path_len - 1 || path_len >= FILE_PATH_MAX) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "uri error or path too long");
        return ESP_FAIL;
    }
    // 到这里的uri有两种情况：
    //  /web/path/of/fileordir：
    //  /web：重定向到可能存在的index.html
    // 应该不会存在的情况，但处理下：任意不以/web开头的路径
    // 注意路径不区分大小写
    if (strncasecmp(req->uri, web_base_path, web_base_path_len - 1) != 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "uri not supported");
        return ESP_FAIL;
    }
    if (path_len == web_base_path_len - 1 ||
        (path_len == web_base_path_len && req->uri[web_base_path_len - 1] == '/')) {
        index_redirect_handler(req);
        return ESP_OK;
    }
    if (req->uri[web_base_path_len - 1] != '/') {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "uri not supported");
        return ESP_FAIL;
    }
    // 到这里的uri开头应该都为/web/*
    char *file_path = malloc(path_len + 1);
    if (!file_path) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory for malloc path");
        return ESP_FAIL;
    }
    strncpy(file_path, req->uri, path_len);
    file_path[path_len] = '\0';
    int replace_count = 0;
    my_http_url_decoder(file_path, path_len, &replace_count, 1);
    struct stat file_stat;
    if (my_ffstat(file_path, &file_stat) != 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "file not found");
        return ESP_FAIL;
    }
    if (S_ISDIR(file_stat.st_mode)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "path is a directory, not supported");
        return ESP_FAIL;
    }
    return my_http_resp_send_file(req, file_path);
}

esp_err_t
my_file_server_start()
{
    if (server_data) {
        return ESP_OK;
    }
    server_data = malloc(sizeof(my_file_server_data_t));
    if (!server_data) {
        ESP_LOGW(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    ESP_ERROR_CHECK(my_http_start());
    my_fs_set_server_files();

    httpd_uri_t index_uri = {
        .uri = MY_URI_WEB_GET,
        .method = HTTP_GET,
        .handler = default_index_handler,
        .user_ctx = server_data};
    httpd_register_uri_handler(my_http_server_handle, &index_uri);

    httpd_uri_t file_download = {
        .uri = MY_URI_API_FILEGET_GET,
        .method = HTTP_GET,
        .handler = download_get_file_handler,
        .user_ctx = server_data};
    httpd_register_uri_handler(my_http_server_handle, &file_download);

    httpd_uri_t file_list_get = {
        .uri = MY_URI_API_FILELIST_GET,
        .method = HTTP_GET,
        .handler = file_list_handler,
        .user_ctx = server_data};
    httpd_register_uri_handler(my_http_server_handle, &file_list_get);

    httpd_uri_t device_control_and_config = {
        .uri = MY_URI_API_DEV_ANY,
        .method = HTTP_ANY,
        .handler = device_control_handler,
        .user_ctx = server_data};
    httpd_register_uri_handler(my_http_server_handle, &device_control_and_config);

    httpd_uri_t fileserver_uri = {
        .uri = MY_URI_FILESERVER_GET,
        .method = HTTP_GET,
        .handler = fileserver_index_handler,
        .user_ctx = server_data};
    httpd_register_uri_handler(my_http_server_handle, &fileserver_uri);

    httpd_uri_t file_upload = {
        .uri = MY_URI_API_FILEUPLOAD_POST,
        .method = HTTP_POST,
        .handler = upload_post_handler,
        .user_ctx = server_data};
    httpd_register_uri_handler(my_http_server_handle, &file_upload);

    httpd_uri_t file_delete = {
        .uri = MY_URI_API_FILEDELETE_ANY,
        .method = HTTP_ANY,
        .handler = file_delete_post_handler,
        .user_ctx = server_data};
    httpd_register_uri_handler(my_http_server_handle, &file_delete);

    httpd_uri_t file_rename = {
        .uri = MY_URI_API_RENAME_GET,
        .method = HTTP_GET,
        .handler = my_file_rename_handler,
        .user_ctx = server_data};
    httpd_register_uri_handler(my_http_server_handle, &file_rename);

    httpd_uri_t dir_operate = {
        .uri = MY_URI_API_DIR_GET,
        .method = HTTP_GET,
        .handler = dir_operate_handler,
        .user_ctx = server_data};
    httpd_register_uri_handler(my_http_server_handle, &dir_operate);

    httpd_uri_t redirect_uri = {
        .uri = MY_URI_BASE_GET,
        .method = HTTP_GET,
        .handler = index_redirect_handler,
        .user_ctx = server_data};
    httpd_register_uri_handler(my_http_server_handle, &redirect_uri);
    return ESP_OK;
}

esp_err_t
my_file_server_stop()
{
    if (!server_data) {
        return ESP_OK;
    }
    esp_err_t ret = my_http_stop(NULL);
    free(server_data);
    server_data = NULL;
    return ret;
}

int8_t my_file_server_running()
{
    return (server_data != NULL);
}
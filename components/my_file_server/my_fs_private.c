#include "my_fs_private.h"
#include "cJSON.h"
#include "esp_system.h"
#include "my_config.h"
#include "my_file_server.h"
static const char *TAG = "my fs private";

void my_fs_set_server_files()
{
    if (!my_file_exist(fileserverDir_path)) {
        if (!my_file_exist(web_base_path)) {
            if (my_mkdir(web_base_path, 0755) != 0) {
                ESP_LOGW(TAG, "create web base dir %s failed", web_base_path);
                return;
            }
        }
        if (my_mkdir(fileserverDir_path, 0755) != 0) {
            ESP_LOGW(TAG, "create file server dir %s failed", fileserverDir_path);
            return;
        }
    }
    if (!my_file_exist(fileserver_index_path)) {
        FILE *fp = my_ffopen(fileserver_index_path, "w");
        if (fp) {
            MY_BINARY_FILE_DECLARE(index_html);
            my_ffwrite(index_html_start, 1, index_html_size, fp);
            my_ffclose(fp);
        }
    }
    if (!my_file_exist(fileserver_script_path)) {
        FILE *fp = my_ffopen(fileserver_script_path, "w");
        if (fp) {
            MY_BINARY_FILE_DECLARE(script_js);
            my_ffwrite(script_js_start, 1, script_js_size, fp);
            my_ffclose(fp);
        }
    }
    if (!my_file_exist(fileserver_style_path)) {
        FILE *fp = my_ffopen(fileserver_style_path, "w");
        if (fp) {
            MY_BINARY_FILE_DECLARE(style_css);
            my_ffwrite(style_css_start, 1, style_css_size, fp);
            my_ffclose(fp);
        }
    }
    if (!my_file_exist(favicon_path)) {
        FILE *fp = my_ffopen(favicon_path, "w");
        if (fp) {
            MY_BINARY_FILE_DECLARE(favicon_ico);
            my_ffwrite(favicon_ico_start, 1, favicon_ico_size, fp);
            my_ffclose(fp);
        }
    }
    if (!my_file_exist(controlDir_path)) {
        if (my_mkdir(controlDir_path, 0755) != 0) {
            ESP_LOGW(TAG, "create control dir %s failed", controlDir_path);
            return;
        }
    }

    if (!my_file_exist(control_index_path)) {
        FILE *fp = my_ffopen(control_index_path, "w");
        if (fp) {
            MY_BINARY_FILE_DECLARE(control_html);
            my_ffwrite(control_html_start, 1, control_html_size, fp);
            my_ffclose(fp);
        }
    }

    if (!my_file_exist(control_script_path)) {
        FILE *fp = my_ffopen(control_script_path, "w");
        if (fp) {
            MY_BINARY_FILE_DECLARE(control_js);
            my_ffwrite(control_js_start, 1, control_js_size, fp);
            my_ffclose(fp);
        }
    }

    if (!my_file_exist(control_style_path)) {
        FILE *fp = my_ffopen(control_style_path, "w");
        if (fp) {
            MY_BINARY_FILE_DECLARE(control_css);
            my_ffwrite(control_css_start, 1, control_css_size, fp);
            my_ffclose(fp);
        }
    }
}

int my_str_replace_char(char *str, size_t str_len, int old_char, int new_char, uint8_t direction, int max_count)
{
    size_t remaining_len = str_len;
    int count = 0;
    char *target_ptr = str;
    if (max_count <= 0) {
        max_count = INT_MAX;
    }
    if (direction == 0) {
        target_ptr = memchr(target_ptr, old_char, remaining_len);
        while (target_ptr && count < max_count) {
            *target_ptr = new_char;
            target_ptr++;
            count++;
            remaining_len = str_len - (target_ptr - str);
            target_ptr = memchr(target_ptr, old_char, remaining_len);
        }
    }
    else {
        target_ptr = memrchr(target_ptr, old_char, remaining_len);
        while (target_ptr && count < max_count) {
            *target_ptr = new_char;
            target_ptr--;
            count++;
            remaining_len = 1 + target_ptr - str;
            target_ptr = memrchr(target_ptr, old_char, remaining_len);
        }
    }
    return count;
}

char *my_http_url_decoder(char *url_str, size_t url_len, int *out_count, uint8_t url_can_edit)
{
    char *buf = (char *)malloc(url_len + 1);
    if (!buf) {
        ESP_LOGI(TAG, "url decoder no memory");
        if (out_count) {
            *out_count = -257;
        }
        return NULL;
    }
    int remaining_url_len = url_len;
    char *buf_ptr = buf;
    char *url_ptr = url_str;
    long num16 = 0;
    char num16_str[3] = "00";
    char *num16_end = NULL;

    size_t byte_to_copy = 0;
    int replace_count = 0;
    char *target_ptr = memchr(url_ptr, '%', remaining_url_len); // 先查找是否包含任意特殊字符
    while (target_ptr) {                                        // 指向%
        replace_count++;
        byte_to_copy = target_ptr - url_ptr;
        memcpy(buf_ptr, url_ptr, byte_to_copy);                                      // 拷贝从url_ptr(包括)到target_ptr(不包括)的字节，即到当前%之前的字节
        replace_count += my_str_replace_char(buf_ptr, byte_to_copy, '+', ' ', 0, 0); // 将+替换为空格
        buf_ptr += byte_to_copy;                                                     // buf指针指向拷贝字节的后一个字节
        url_ptr = target_ptr + 3;                                                    // url指针指向%xx后的一个字节
        remaining_url_len = url_len - (target_ptr - url_str);                        // 包括%，剩余的字符串长度
        if (remaining_url_len < 3) {                                                 // 剩余字符串至少要有%xx
            break;
        }
        remaining_url_len -= 3;                    // 剩余字符串长度减去%xx的长度
        memcpy(num16_str, target_ptr + 1, 2);      // 拷贝%xx中的xx
        num16 = strtol(num16_str, &num16_end, 16); // 解析xx为数字
        if (num16_end != num16_str + 2) {          // 如果解析最后的字符不是xx后的字节，说明格式有错误
            ESP_LOGI(TAG, "decoder 0x%s may cost error", num16_str);
            free(buf);
            if (out_count) {
                *out_count = -258;
            }
            return NULL;
        }
        else if (num16 < 0 || num16 > UINT8_MAX) { // 如果解析出的数字小于0或超过8位，说明值有问题
            ESP_LOGI(TAG, "decoder 0x%s result is %ld, out of range", num16_str, num16);
            free(buf);
            if (out_count) {
                *out_count = -1;
            }
            return NULL;
        }
        *buf_ptr = (uint8_t)num16; // 将解析出的数字代替%xx存入buf
        buf_ptr++;                 // buf指针指向下一个字节
        target_ptr = memchr(url_ptr, '%', remaining_url_len);
    }

    if (remaining_url_len > 0) {
        memcpy(buf_ptr, url_ptr, remaining_url_len);
        replace_count += my_str_replace_char(buf_ptr, remaining_url_len, '+', ' ', 0, 0);
        buf_ptr += remaining_url_len;
    }
    *buf_ptr = '\0';
    if (out_count) {
        *out_count = replace_count;
    }
    if (url_can_edit) {
        if (replace_count) {
            memcpy(url_str, buf, url_len);
        }
        free(buf);
        buf = url_str;
    }
    return buf;
}

esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_HTML);
    }
    else if (IS_FILE_EXT(filename, ".json")) {
        return httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_JSON);
    }
    else if (IS_FILE_EXT(filename, ".js")) {
        return httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_JS);
    }
    else if (IS_FILE_EXT(filename, ".css")) {
        return httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_CSS);
    }
    else if (IS_FILE_EXT(filename, ".png")) {
        return httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_PNG);
    }
    else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_JPG);
    }
    else if (IS_FILE_EXT(filename, ".xml")) {
        return httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_XML);
    }
    else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_ICO);
    }
    else if (IS_FILE_EXT(filename, ".bin")) {
        return httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_BIN);
    }
    else if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_PDF);
    }

    return httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_TEXT);
}

esp_err_t set_file_name_from_path(httpd_req_t *req, const char *file_path, char *file_name_buf, uint8_t buf_size, uint8_t is_attachment)
{
    if (!file_path || !file_name_buf) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *file_name = strrchr(file_path, '/');
    size_t file_name_len = 0;
    if (file_name) {
        file_name_len = strlen(file_name);
        if (file_name_len <= 1) {
            return ESP_ERR_INVALID_ARG;
        }
        file_name++;
        file_name_len--;
    }
    else {
        file_name = file_path;
        file_name_len = strlen(file_name);
    }
    if (is_attachment)
        snprintf(file_name_buf, buf_size, "attachment; filename=\"%s\"", file_name);
    else
        snprintf(file_name_buf, buf_size, "inline; filename=\"%s\"", file_name);
    return httpd_resp_set_hdr(req, "Content-Disposition", file_name_buf);
}

void my_http_send_redirect_resp(httpd_req_t *req, const char *location)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_send(req, NULL, 0); // Response body can be empty
}

esp_err_t my_get_path_from_query(httpd_req_t *req, char *out_path, const char *query_key, size_t path_buf_size)
{
    if (!out_path || !path_buf_size || !req) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *_key = path_query_key;
    if (query_key) {
        _key = query_key;
    }
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    char *query = malloc(query_len + 1);
    if (query) {
        ESP_LOGD(TAG, "Get query string, len:%d", query_len);
        esp_err_t ret = httpd_req_get_url_query_str(req, query, query_len + 1);
        if (ret == ESP_OK) {
            ret = httpd_query_key_value(query, _key, out_path, path_buf_size);
        }
        free(query);
        return ret;
    }
    else {
        return ESP_ERR_NO_MEM;
    }
}

int my_uri_get_path_len(const char *uri)
{
    if (!uri) {
        return -1;
    }
    int uri_len = strlen(uri);
    char *chr = strpbrk(uri, "?#");
    if (!chr) {
        return uri_len;
    }
    else {
        return chr - uri;
    }
}

esp_err_t my_http_resp_send_file(httpd_req_t *req, const char *file_path)
{
    FILE *fp = my_ffopen(file_path, "rb");
    if (!fp) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        return ESP_FAIL;
    }
    char file_name[128] = {0};
    set_content_type_from_file(req, file_path);
    set_file_name_from_path(req, file_path, file_name, 128, 0);
    esp_err_t ret = ESP_FAIL;
    size_t read_bytes;
    long file_size = my_ffget_size(fp);

    char *buf = NULL;
    size_t buf_size = 0;
    if (file_size <= SCRATCH_BUFSIZE) {
        buf = ((my_file_server_data_t *)req->user_ctx)->scratch;
        buf_size = SCRATCH_BUFSIZE;
    }
    else {
        buf = (char *)malloc(file_size);
        buf_size = file_size;
    }
    // int64_t t1, t2;
    if (buf) {
        // t1 = my_get_time();
        read_bytes = my_ffread(buf, 1, file_size, fp);
        if (read_bytes == file_size) {
            ret = httpd_resp_send(req, buf, read_bytes);
            my_ffclose(fp);
            if (file_size > SCRATCH_BUFSIZE)
                free(buf);
            // t2 = my_get_time();
            // if (ret != ESP_OK) { ESP_LOGI(TAG, "send file failed, ret: %d", ret); }
            // else {
            //     ESP_LOGI(TAG, "File sending complete, cost:%.2fms, speed:%.2fKB/s",
            //              (t2 - t1) / 1000.0f, file_size / ((t2 - t1) / 1000000.0f) / 1024.0f);
            // }
            return ret;
        }
        ESP_LOGI(TAG, "read file failed, read_bytes: %u but file_size: %ld", read_bytes, file_size);
        if (file_size > SCRATCH_BUFSIZE)
            free(buf);
    }

    buf = ((my_file_server_data_t *)req->user_ctx)->scratch;
    buf_size = SCRATCH_BUFSIZE;
    char file_size_str[12] = {0}; // 最多11位数字
    snprintf(file_size_str, 12, "%ld", file_size);
    // ESP_LOGI(TAG, "Sending file : %s, size: %s", file_path, file_size_str);
    httpd_resp_set_hdr(req, "Content-Length", file_size_str);
    httpd_resp_set_hdr(req, "X-Total-Bytes", file_size_str);
    // 客户端todo：客户端可以通过这个响应头和接收到的数据大小，判断数据是否传输完，用来判断错误

    // t1 = my_get_time();
    do {
        read_bytes = my_ffread(buf, 1, buf_size, fp);
        if (read_bytes > 0) {
            if (httpd_resp_send_chunk(req, buf, read_bytes) != ESP_OK) {
                my_ffclose(fp);
                httpd_resp_send_chunk(req, NULL, 0);
                ESP_LOGI(TAG, "File sending failed!");
                return ESP_FAIL;
            }
        }
    } while (read_bytes != 0);
    my_ffclose(fp);
    httpd_resp_send_chunk(req, NULL, 0);
    // t2 = my_get_time();
    // ESP_LOGI(TAG, "File sending complete, cost:%.2fms, speed:%.2fKB/s", (t2 - t1) / 1000.0f, file_size / ((t2 - t1) / 1000000.0f) / 1024.0f);
    return ESP_OK;
}

char *get_file_list_json(const char *dir_path)
{
    DIR *dir = my_opendir(dir_path);
    if (!dir) {
        ESP_LOGI(TAG, "open dir fail: %s", dir_path);
        return NULL; // 目录打开失败
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "dirPath", dir_path);

    cJSON *file_list = cJSON_CreateArray();
    struct dirent *entry;
    char file_path[FILE_PATH_MAX];

    // ESP_LOGI(TAG, "list dir: %s", dir_path);
    while ((entry = readdir(dir)) != NULL) {
        // if (strcmp(entry->d_name, ".") == 0 ||
        //     strcmp(entry->d_name, "..") == 0 ||
        //     strcasecmp(entry->d_name, "System Volume Information") == 0) {
        //     continue; // 忽略当前目录和上级目录
        // }// 让客户端过滤，方便看到所有文件按
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", entry->d_name);
        cJSON_AddBoolToObject(item, "isDirectory", entry->d_type == DT_DIR);
        snprintf(file_path, FILE_PATH_MAX, "%s%s", dir_path, entry->d_name);
        if (entry->d_type != DT_DIR) {
            struct stat file_stat;
            if (my_ffstat(file_path, &file_stat) == 0) {
                cJSON_AddNumberToObject(item, "size", file_stat.st_size);
            }
        }

        cJSON_AddItemToArray(file_list, item);
    }
    closedir(dir);
    cJSON_AddItemToObject(root, "fileList", file_list);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

esp_err_t my_fs_device_update_set(httpd_req_t *req)
{
    if (!my_file_exist(UPDATE_BIN_PATH)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "can't find update file, please upload file to path: " UPDATE_BIN_PATH);
        return ESP_FAIL;
    }
    if (my_file_exist(UPDATE_OK_PATH) && my_fremove(UPDATE_OK_PATH) != 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "can't remove the update ok flag path: " UPDATE_BIN_PATH ", device will not update when this file exists.");
        return ESP_FAIL;
    }
    const char *resp_str = "Device will auto restart and try update after 1s. \nPlease wait device update complete and don't operate device until device startup. \nUpdate may take a few minutes. If device can't auto restart, please restart device manual.";

    if (!my_cfg_check_update.data.u8) {
        my_cfg_check_update.data.u8 = 1;
        if (my_cfg_save_config_to_nvs(&my_cfg_check_update) != ESP_OK) {
            resp_str = "save check update configs to nvs error, device may not update.";
        }
    }
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_sendstr(req, resp_str);
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (my_cfg_event_post(MY_CFG_EVENT_RESTART_DEVICE, NULL, 0, pdMS_TO_TICKS(1000)) != ESP_OK) {
        ESP_LOGW(TAG, "update device post restart event timeout");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t my_fs_restart_device(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "device will restart after 1s");
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (my_cfg_event_post(MY_CFG_EVENT_RESTART_DEVICE, NULL, 0, pdMS_TO_TICKS(1000)) != ESP_OK) {
        ESP_LOGW(TAG, "post restart event timeout");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t my_fs_send_current_configs(httpd_req_t *req, uint8_t convert_nvs, uint8_t convert_key)
{
    char *json_str = my_cfg_to_json_str(convert_nvs, convert_key);
    if (!json_str) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "get configs failed, no memory");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_JSON);
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ESP_OK;
}

esp_err_t my_fs_send_default_configs(httpd_req_t *req)
{
    if (!my_file_exist(defCfgs_path)) {
        httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_TEXT);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no default config file, may auto create when next restart");
        if (my_cfg_out_def_config.data.u8 == 0) {
            my_cfg_out_def_config.data.u8 = 1;
            my_cfg_save_config_to_nvs(&my_cfg_out_def_config);
        }
        return ESP_FAIL;
    }
    return my_http_resp_send_file(req, defCfgs_path);
    // FILE *fp = my_ffopen(defCfgs_path, "r");
    // if (!fp) {
    //     httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no default config file, may auto create when next restart");
    //     if (my_cfg_out_def_config.data.u8 == 0) {
    //         my_cfg_out_def_config.data.u8 = 1;
    //         my_cfg_save_config_to_nvs(&my_cfg_out_def_config);
    //     }
    //     return ESP_FAIL;
    // }
    // long file_size = my_ffget_size(fp);
    // uint8_t *buf = malloc(file_size);
    // if (!buf) {
    //     httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "get configs failed, no memory");
    //     return ESP_FAIL;
    // }
    // file_size = my_ffread(buf, 1, file_size, fp);
    // httpd_resp_set_type(req, "application/json; charset=utf-8");
    // httpd_resp_send(req, (const char *)buf, file_size);
    // free(buf);
    // my_ffclose(fp);
    // return ESP_OK;
}

esp_err_t my_fs_send_device_info_json(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "can't create json object");
        return ESP_FAIL;
    }
    uint64_t fat_total_bytes, fat_free_bytes;
    my_fat_get_size_info(&fat_total_bytes, &fat_free_bytes);
    cJSON_AddNumberToObject(root, "fatTotalBytes", fat_total_bytes);
    cJSON_AddNumberToObject(root, "fatFreeBytes", fat_free_bytes);
    cJSON_AddNumberToObject(root, "kbColNum", MY_COL_NUM);
    cJSON_AddNumberToObject(root, "kbRowNum", MY_ROW_NUM);
#ifdef MY_ENCODER_CHANGE_DIRECTION
    cJSON_AddBoolToObject(root, "encoderSupport", true);
#endif
    cJSON_AddNumberToObject(root, "vbusVoltage", my_vbus_vol);
    cJSON_AddNumberToObject(root, "batVoltage", my_bat_vol);
    cJSON_AddNumberToObject(root, "ina226BusVol", my_ina226_data.bus_vol);
    cJSON_AddNumberToObject(root, "ina226Current", my_ina226_data.current);
    cJSON_AddNumberToObject(root, "ina226Power", my_ina226_data.power);

    cJSON_AddNumberToObject(root, "freeHeap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "freeInternalHeap", esp_get_free_internal_heap_size());
    cJSON_AddNumberToObject(root, "freeHeapMin", esp_get_minimum_free_heap_size());
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, MY_HTTP_RESP_TYPE_JSON);
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ESP_OK;
}

// /api/dev/setconfig?path=/config/path
// 如果body没有数据，但有path参数，查看对应路径文件是否存在，存在则设置为配置文件
// 如果body有数据，且有path参数，将数据保存到path中，并设置为配置文件
// 如果body有数据，但没有path参数，不保存文件，只应用配置
// 其它情况，返回错误
esp_err_t my_fs_device_setconfig(httpd_req_t *req)
{
    if (req->content_len <= 0 && req->method != HTTP_GET) {
        return httpd_resp_sendstr(req, "get setconfig operation, but no body data, only support GET method to set configs with existing path");
    }
    char *json_data = NULL;
    long json_data_len = 0;

    char file_path[FILE_PATH_MAX] = {0};
    my_get_path_from_query(req, file_path, NULL, sizeof(file_path));
    if (file_path[0] != 0)
        my_http_url_decoder(file_path, strlen(file_path), NULL, 1);
    if (req->method == HTTP_POST) {
        // 对于post请求，表示要接收数据，因为cjson需要整个json对象，所以必须放在一个buffer内
        json_data_len = req->content_len;
        json_data = malloc(json_data_len + 1);
        if (!json_data) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memery");
            return ESP_ERR_NO_MEM;
        }
        int remaining_byte = req->content_len;
        int received_byte = 0;
        char *buf_ptr = json_data;
        uint8_t retry_count = 0;
        while (remaining_byte > 0) {
            received_byte = httpd_req_recv(req, buf_ptr, remaining_byte);
            if (received_byte <= 0) {
                if (received_byte == HTTPD_SOCK_ERR_TIMEOUT && retry_count < 10) {
                    retry_count++;
                    continue;
                }
                else {
                    free(json_data);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recive data fail, timeout or other error");
                    return ESP_FAIL;
                }
            }
            retry_count = 0;
            remaining_byte -= received_byte;
            buf_ptr += received_byte;
        }
        json_data[req->content_len] = '\0';
        // 接收数据完成
    }
    else if (req->method == HTTP_GET) {
        // 对于get请求，表示要用现成文件中的数据
        FILE *fp = my_ffopen(file_path, "r");
        if (!fp) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "file not found, may be you need use POST method to set configs or use fileserver upload file first");
            return ESP_FAIL;
        }
        json_data_len = my_ffget_size(fp);
        json_data = malloc(json_data_len + 1);
        if (!json_data) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
            return ESP_ERR_NO_MEM;
        }
        json_data_len = my_ffread(json_data, 1, json_data_len, fp);
        json_data[json_data_len] = '\0';
    }
    esp_err_t ret;
    char config_type[16] = {0};
    // 没有实际用到这个参数，因为处理配置的函数会判断有什么类型的配置
    ret = my_get_path_from_query(req, config_type, "type", sizeof(config_type));
    // 将数据转换为cjson对象
    cJSON *root = cJSON_ParseWithLength(json_data, json_data_len);
    int change_count = 0;
    ret = my_json_cfgs_parse(root, &change_count, NULL);
    if (root) {
        cJSON_Delete(root);
    }
    if (ret != ESP_OK) {
        free(json_data);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "json parse error, convert str to cjson failed or parse cjson object failed");
        return ret;
    }
    char resp_str[128] = {0};
    // 解析并存储配置结束
    // 如果是get请求，不需要处理接收的数据
    if (req->method == HTTP_GET) {
        sprintf(resp_str, "set config with exist file success, change %d configs", change_count);
        goto send_ok;
    }
    // 对于post请求，保存数据
    if (file_path[0] != 0 && file_path[0] == '/') {
        FILE *fp = my_ffopen(file_path, "w");
        if (fp) {
            my_ffwrite(json_data, 1, json_data_len, fp);
            my_ffclose(fp);
        }
    }
    sprintf(resp_str, "parse json success, change %d configs", change_count);
send_ok:
    free(json_data);
    httpd_resp_sendstr(req, resp_str);
    config_type[0] = 0; // 这里只是懒得声明新变量，实际获取的参数和变量名不匹配
    my_get_path_from_query(req, config_type, "restart", sizeof(config_type));
    if (config_type[0] == 'n' && config_type[1] == '\0') {
        // 如果restart参数设置为n，则不自动重启设备
        return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return ESP_OK;
}

// 匹配/api/rename?old_path=xxx&new_path=xxx
esp_err_t my_file_rename_handler(httpd_req_t *req)
{
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "rename need 2 params: old_path and new_path");
        return ESP_FAIL;
    }
    char *query_str = malloc(query_len + 1);
    if (!query_str) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memery");
        return ESP_ERR_NO_MEM;
    }
    httpd_req_get_url_query_str(req, query_str, query_len + 1);
    char old_path[FILE_PATH_MAX] = {0};
    httpd_query_key_value(query_str, "old_path", old_path, FILE_PATH_MAX);
    char new_path[FILE_PATH_MAX] = {0};
    httpd_query_key_value(query_str, "new_path", new_path, FILE_PATH_MAX);
    if (my_rename(old_path, new_path) != 0) {
        snprintf(query_str, query_len + 1, "rename<%s>2<%s>err", old_path, new_path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, query_str);
        free(query_str);
        return ESP_FAIL;
    };
    httpd_resp_sendstr(req, "rename success");
    free(query_str);
    return ESP_OK;
}

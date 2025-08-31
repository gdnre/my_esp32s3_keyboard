#include "my_config.h"

#include <stdio.h>

#include "cJSON.h"
#include "esp_crc.h"
#include "esp_timer.h"
#include "my_fat_mount.h"
#include "my_init.h"
#include "my_keyboard.h"
#include "my_wifi_config.h"
#include "string.h"

ESP_EVENT_DEFINE_BASE(MY_CONFIG_EVENTS);
ESP_EVENT_DEFINE_BASE(MY_STATE_CHANGE_EVENTS);

esp_event_loop_handle_t my_config_handle = NULL;
int64_t _my_t1 = 0;
int64_t _my_t2 = 0;

static const char *TAG = "my config";
const char *my_cfg_nvs_namespace = "my_cfg";
const char my_cfg_base_path[] = "/.configs/";
const uint8_t cfg_base_path_len = sizeof(my_cfg_base_path) - 1;
const char my_cfg_dataDir_path[] = "/.configs/.data/";
const char configIndex_path[] = "/.configs/configIndex.json";
const char configTemplate_path[] = "/.configs/configTemplate";
const char nvsCfgs_path[] = "/.configs/.data/nvsCfgs.json";
const char keyCfgs_path[] = "/.configs/.data/keyCfgs.json";
const char totalCfgs_path[] = "/.configs/.data/totalCfgs.json";
const char defCfgs_path[] = "/.configs/.data/defCfgs.json";
const char keyConfigsBin_path[] = "/.configs/.data/keys.bin";

my_nvs_cfg_t my_cfg_boot_mode = {.name = "bootMode", .size = 1, .is_ptr = 0, .data.u8 = MY_BOOT_MODE_DEFAULT};
my_nvs_cfg_t my_cfg_next_boot_mode = {.name = "nBootMode", .size = 1, .is_ptr = 0, .data.u8 = MY_BOOT_MODE_DEFAULT};
my_nvs_cfg_t my_cfg_check_update = {.name = "checkUpdate", .size = 1, .is_ptr = 0, .data.u8 = 0};

my_nvs_cfg_t my_cfg_out_def_config = {.name = "outDef", .size = 1, .is_ptr = 0, .data.u8 = 1};

my_nvs_cfg_t my_cfg_usb = {.name = "usb", .size = 1, .is_ptr = 0, .data.u8 = 0};
my_nvs_cfg_t my_cfg_ble = {.name = "ble", .size = 1, .is_ptr = 0, .data.u8 = 0};
my_nvs_cfg_t my_cfg_use_display = {.name = "display", .size = 1, .is_ptr = 0, .data.u8 = 1};
my_nvs_cfg_t my_cfg_brightness = {.name = "brightness", .size = 1, .is_ptr = 0, .data.u8 = 100};
my_nvs_cfg_t my_cfg_wifi_mode = {.name = "wifiMode", .size = 1, .is_ptr = 0, .data.u8 = 0};
my_nvs_cfg_t my_cfg_sleep_time = {.name = "sleepTime", .size = 2, .is_ptr = 0, .data.u16 = 1200};
my_nvs_cfg_t my_cfg_sleep_enable = {.name = "sleepEn", .size = 1, .is_ptr = 0, .data.u8 = 1};
my_nvs_cfg_t my_cfg_log_level = {.name = "logLevel", .size = 1, .is_ptr = 0, .data.u8 = ESP_LOG_WARN};

my_nvs_cfg_t *my_nvs_cfg_list_to_get[] = {&my_cfg_out_def_config, &my_cfg_usb, &my_cfg_ble, &my_cfg_use_display, &my_cfg_brightness, &my_cfg_wifi_mode, &my_cfg_sleep_time, &my_cfg_sleep_enable, &my_cfg_log_level, NULL};
uint8_t my_nvs_cfg_list_to_get_size = (sizeof(my_nvs_cfg_list_to_get) / sizeof(my_nvs_cfg_list_to_get[0])) - 1;

my_nvs_cfg_t *my_nvs_cfg_list_for_json[] = {&my_cfg_boot_mode, &my_cfg_next_boot_mode, &my_cfg_check_update, &my_cfg_out_def_config, &my_cfg_usb, &my_cfg_ble, &my_cfg_use_display, &my_cfg_brightness, &my_cfg_wifi_mode, &my_cfg_sleep_time, &my_cfg_sleep_enable, &my_cfg_log_level, NULL};
uint8_t my_nvs_cfg_list_for_json_size = (sizeof(my_nvs_cfg_list_for_json) / sizeof(my_nvs_cfg_list_for_json[0])) - 1;

my_feature_state_t my_cfg_usb_hid_state;
my_feature_state_t my_cfg_ble_state;
my_feature_state_t my_cfg_ap_state;
my_feature_state_t my_cfg_sta_state;
my_feature_state_t my_cfg_espnow_state;
my_feature_state_t my_cfg_display_state;
my_feature_state_t my_cfg_adc_state;
my_feature_state_t my_cfg_ina226_state;
my_lvgl_widgets_info_t my_lv_info;

my_feature_state_t my_cfg_fn_sw_state;
uint8_t my_cfg_kb_led_raw_state;

static nvs_handle_t s_nvs_handle;
static int8_t s_nvs_is_open = 0;

#pragma region nvs配置相关
esp_err_t my_cfg_nvs_open()
{
    if (s_nvs_is_open) {
        return ESP_OK;
    }
    esp_err_t ret = nvs_open(my_cfg_nvs_namespace, NVS_READWRITE, &s_nvs_handle);
    if (ret == ESP_OK) {
        s_nvs_is_open = 1;
    }
    return ret;
}

esp_err_t my_cfg_nvs_close()
{
    if (!s_nvs_is_open) {
        return ESP_OK;
    }
    s_nvs_is_open = 0;
    nvs_close(s_nvs_handle);
    s_nvs_handle = NULL;
    return ESP_OK;
}

esp_err_t my_cfg_get_boot_mode(void)
{
    my_nvs_init();
    esp_err_t ret = my_cfg_nvs_open();
    ESP_ERROR_CHECK(ret);

    // 读取下次启动模式
    ret = nvs_get_u8(s_nvs_handle, my_cfg_next_boot_mode.name, &my_cfg_boot_mode.data.u8);
    assert(ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND);
    if (my_cfg_boot_mode.data.u8 == MY_BOOT_MODE_MSC) { // 如果下次启动模式为msc，将下次启动模式设置为默认
        ret = nvs_set_u8(s_nvs_handle, my_cfg_next_boot_mode.name, MY_BOOT_MODE_DEFAULT);
        ESP_ERROR_CHECK(ret);
        // 下次启动要检查有没有更新
        ret = nvs_set_u8(s_nvs_handle, my_cfg_check_update.name, 1);
        ret = nvs_commit(s_nvs_handle);
        ESP_ERROR_CHECK(ret);
        my_cfg_nvs_close();
    }
    else // 如果下次启动模式为默认，获取my_cfg_check_update参数
    {
        ret = nvs_get_u8(s_nvs_handle, my_cfg_check_update.name, &my_cfg_check_update.data.u8);
        assert(ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND);
        // 如果这次要检查更新，将nvs中存储的值设置为0，防止下次检查更新
        if (my_cfg_check_update.data.u8) {
            nvs_set_u8(s_nvs_handle, my_cfg_check_update.name, 0);
            ret = nvs_commit(s_nvs_handle);
        }
        // nvs_get_u8(s_nvs_handle, my_cfg_out_def_config.name, &my_cfg_out_def_config.data.u8);
        // if (my_cfg_out_def_config.data.u8) {
        //     nvs_set_u8(s_nvs_handle, my_cfg_out_def_config.name, 0);
        //     ret = nvs_commit(s_nvs_handle);
        // }
    }
    // my_cfg_nvs_close();
    return ret;
}

esp_err_t my_cfg_erase_nvs_configs(void)
{
    esp_err_t ret = my_cfg_nvs_open();
    ESP_ERROR_CHECK(ret);
    for (size_t i = 0; i < my_nvs_cfg_list_to_get_size; i++) {
        if (my_nvs_cfg_list_to_get[i] && !my_nvs_cfg_list_to_get[i]->is_ptr) {
            nvs_erase_key(s_nvs_handle, my_nvs_cfg_list_to_get[i]->name);
        }
    }
    nvs_erase_key(s_nvs_handle, "apSSID");
    nvs_erase_key(s_nvs_handle, "apPasswd");
    nvs_erase_key(s_nvs_handle, "staSSID");
    nvs_erase_key(s_nvs_handle, "staPasswd");

    my_cfg_nvs_close();
    return ret;
}

esp_err_t my_cfg_get_nvs_str(const char *key, char **out_str, size_t *out_len)
{
    if (!key || (!out_str && !out_len)) {
        return ESP_ERR_INVALID_ARG;
    }
    my_cfg_nvs_open();
    size_t str_len = 0;
    esp_err_t ret = nvs_get_str(s_nvs_handle, key, NULL, &str_len);
    if (str_len <= 0) {
        return ret;
    }
    if (out_len) {
        *out_len = str_len;
    }
    if (!out_str) {
        return ESP_OK;
    }
    char *buf = (char *)malloc(str_len + 1);
    if (buf) {
        ret = nvs_get_str(s_nvs_handle, key, buf, &str_len);
        if (ret != ESP_OK) {
            free(buf);
            return ret;
        }
        buf[str_len] = '\0';
        if (out_str) {
            *out_str = buf;
        }
        return ret;
    }
    else {
        return ESP_ERR_NO_MEM;
    }
}

esp_err_t my_cfg_get_nvs_str_static(const char *key, char *out_buf, size_t *out_len, size_t buf_size)
{
    if (!key || (!out_buf && !out_len) || (buf_size && !out_buf)) {
        return ESP_ERR_INVALID_ARG;
    }
    my_cfg_nvs_open();
    size_t str_len = 0;
    esp_err_t ret = nvs_get_str(s_nvs_handle, key, NULL, &str_len);
    if (str_len <= 0) {
        if (ret == ESP_OK && buf_size) {
            out_buf[0] = '\0';
        }
        return ret;
    }
    if (out_len) { *out_len = str_len; }
    if (!out_buf) { return ESP_OK; }
    if (buf_size < str_len + 1) { return ESP_ERR_NO_MEM; }
    ret = nvs_get_str(s_nvs_handle, key, out_buf, &str_len);
    out_buf[str_len] = '\0';
    return ret;
}

esp_err_t my_cfg_get_nvs_configs(void)
{
    esp_err_t ret = my_cfg_nvs_open();
    ESP_ERROR_CHECK(ret);
    for (size_t i = 0; i < my_nvs_cfg_list_to_get_size; i++) {
        if (my_nvs_cfg_list_to_get[i] && !my_nvs_cfg_list_to_get[i]->is_ptr) {
            switch (my_nvs_cfg_list_to_get[i]->size) {
                case 1:
                    ret |= nvs_get_u8(s_nvs_handle, my_nvs_cfg_list_to_get[i]->name, &my_nvs_cfg_list_to_get[i]->data.u8);
                    break;
                case 2:
                    ret |= nvs_get_u16(s_nvs_handle, my_nvs_cfg_list_to_get[i]->name, &my_nvs_cfg_list_to_get[i]->data.u16);
                    break;
                case 4:
                    ret |= nvs_get_u32(s_nvs_handle, my_nvs_cfg_list_to_get[i]->name, &my_nvs_cfg_list_to_get[i]->data.u32);
                    break;
                default:
                    break;
            }
        }
    }
    my_cfg_get_nvs_str_static("apSSID", my_ap_ssid, NULL, sizeof(my_ap_ssid));
    my_cfg_get_nvs_str_static("apPasswd", my_ap_password, NULL, sizeof(my_ap_password));
    my_cfg_get_nvs_str_static("staSSID", my_sta_ssid, NULL, sizeof(my_sta_ssid));
    my_cfg_get_nvs_str_static("staPasswd", my_sta_password, NULL, sizeof(my_sta_password));
    my_cfg_nvs_close();
    return ret;
}

esp_err_t my_cfg_save_config_to_nvs(my_nvs_cfg_t *cfg)
{
    if (!cfg || cfg->is_ptr) {
        return ESP_ERR_INVALID_ARG;
    }
    my_cfg_nvs_open();
    esp_err_t ret = ESP_OK;
    switch (cfg->size) {
        case 1:
            ret = nvs_set_u8(s_nvs_handle, cfg->name, cfg->data.u8);
            break;
        case 2:
            ret = nvs_set_u16(s_nvs_handle, cfg->name, cfg->data.u16);
            break;
        case 4:
            ret = nvs_set_u32(s_nvs_handle, cfg->name, cfg->data.u32);
            break;
        default:
            return ESP_ERR_INVALID_ARG;
            break;
    }
    ret = nvs_commit(s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(__func__, "nvs_commit error");
    }
    return ret;
}
#pragma endregion

#pragma region 事件循环相关
void my_config_event_loop_init(void)
{
    if (my_config_handle) { return; }
    esp_event_loop_args_t my_config_loop_args =
        {
            .queue_size = 16,
            .task_name = "cfg_loop_task",
            .task_priority = 3,
            .task_stack_size = 4096,
            .task_core_id = 1,
        };
    ESP_ERROR_CHECK(esp_event_loop_create(&my_config_loop_args, &my_config_handle));
}

void my_config_event_loop_deinit(void)
{
    if (!my_config_handle) { return; }
    esp_event_loop_delete(my_config_handle);
    my_config_handle = NULL;
}

esp_event_loop_handle_t my_cfg_get_loop_handle(void)
{
    return my_config_handle;
}

esp_err_t my_cfg_event_post(int32_t event_id, const void *event_data, size_t event_data_size, TickType_t ticks_to_wait)
{
    if (!my_config_handle) {
        return ESP_FAIL;
    }
    esp_err_t ret = esp_event_post_to(my_config_handle, MY_CONFIG_EVENTS, event_id, event_data, event_data_size, ticks_to_wait);
    return ret;
}

esp_err_t my_cfg_event_handler_instance_register(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *instance)
{
    if (!my_config_handle) {
        return ESP_FAIL;
    }
    esp_err_t ret = esp_event_handler_instance_register_with(my_config_handle, MY_CONFIG_EVENTS, event_id, event_handler, event_handler_arg, instance);
    return ret;
}

esp_err_t my_cfg_event_handler_register(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg)
{
    if (!my_config_handle) {
        return ESP_FAIL;
    }
    esp_err_t ret = esp_event_handler_register_with(my_config_handle, MY_CONFIG_EVENTS, event_id, event_handler, event_handler_arg);
    return ret;
}

esp_err_t my_cfg_event_handler_instance_unregister(int32_t event_id, esp_event_handler_instance_t instance)
{
    if (!my_config_handle) {
        return ESP_FAIL;
    }
    esp_err_t ret = esp_event_handler_instance_unregister_with(my_config_handle, MY_CONFIG_EVENTS, event_id, instance);
    return ret;
}

esp_err_t my_cfg_event_handler_unregister(int32_t event_id, esp_event_handler_t event_handler)
{
    if (!my_config_handle) {
        return ESP_FAIL;
    }
    esp_err_t ret = esp_event_handler_unregister_with(my_config_handle, MY_CONFIG_EVENTS, event_id, event_handler);
    return ret;
}
#pragma endregion

#pragma region json配置相关
esp_err_t my_cfg_set_config_dir()
{
    esp_err_t ret = ESP_OK;
    if (!my_file_exist(my_cfg_base_path)) {
        if (my_mkdir(my_cfg_base_path, 0755) != 0) {
            ret = ESP_FAIL;
        }
    }
    if (!my_file_exist(my_cfg_dataDir_path)) {
        if (my_mkdir(my_cfg_dataDir_path, 0755) != 0) {
            ret = ESP_FAIL;
        }
    }
    return ret;
}

static esp_err_t my_cfg_set_configIndex_value(uint8_t configChange, const char *configPath);
typedef enum my_json_config_type_t {
    MY_JSON_CFG_TYPE_DEVCONFIGSOBJ = (1 << 0),
    MY_JSON_CFG_TYPE_KEYCONFIGSARR = (1 << 1),
    MY_JSON_CFG_TYPE_SINGLE_NVS_CONFIGS = (1 << 2),
    MY_JSON_CFG_TYPE_SINGLE_KEY_CONFIGS = (1 << 3),
} my_json_config_type_t;

// 传入.json文件路径，返回cjson对象，注意cjson对象不使用后要手动调用cJSON_Delete()释放
static cJSON *my_cfg_get_cjson_from_path(const char *path)
{
    if (!path)
        return NULL;
    FILE *fp = my_ffopen(path, "r");
    if (!fp)
        return NULL;
    long file_size = my_file_get_size_raw(fp);
    char *buf = malloc(file_size);
    if (!buf)
        return NULL;
    my_ffseek(fp, 0, SEEK_SET);
    my_ffread(buf, 1, file_size, fp);
    my_ffclose(fp); // fp = NULL;
    cJSON *root = cJSON_ParseWithLength(buf, file_size);
    free(buf); // buf = NULL;
    return root;
}

// 从configIndex_path中查找要导入的配置文件路径，返回路径字符串，注意不使用后需要手动调用free()释放
// 还会检查configChange字段，要设置为true才会返回文件路径
static esp_err_t my_cfg_get_configPath(char **out_configPath)
{
    if (!out_configPath)
        return ESP_ERR_INVALID_ARG;
    cJSON *root = my_cfg_get_cjson_from_path(configIndex_path);
    if (!root) {
        ESP_LOGI(TAG, "can't find path: %s, create it", configIndex_path);
        my_cfg_set_configIndex_value(0, " ");
        return ESP_ERR_NOT_SUPPORTED;
    }
    cJSON *configChangeJson = cJSON_GetObjectItem(root, "configChange");
    if (!configChangeJson || !cJSON_IsTrue(configChangeJson)) {
        cJSON_Delete(root);
        ESP_LOGI(TAG, "no configChange item or value is false");
        return ESP_ERR_NOT_ALLOWED;
    }
    cJSON *configPathJson = cJSON_GetObjectItem(root, "configPath");
    char *configPathExt = cJSON_GetStringValue(configPathJson);
    esp_err_t ret = ESP_OK;
    if (configPathExt) // 如果在json中找到了配置目录项且是字符串
    {
        uint16_t str_len = strlen(configPathExt);
        if (str_len > 0) {
            bool is_absolute_path = (configPathExt[0] == '/');
            str_len = is_absolute_path ? str_len : str_len + cfg_base_path_len;
            char *configPath = malloc(str_len + 1);
            if (configPath) {
                sniprintf(configPath, str_len + 1, "%s%s", is_absolute_path ? "" : my_cfg_base_path, configPathExt);
                *out_configPath = configPath;
                cJSON_Delete(root);
                return ESP_OK;
            }
            else
                ret = ESP_ERR_NO_MEM;
        }
    }
    else
        ret = ESP_FAIL;

    cJSON_Delete(root);
    return ret;
}

/// @brief
/// 设置configIndex.json中存储配置路径和是否读取配置项目的值，如果没有对应键，还会添加键
/// @param configChange
/// 指示是否尝试读取json配置，可以设置为0或1，设为0则不会读取json配置，其它值意味着不更改，默认为0
/// @param configPath
/// 指示要读取的配置路径，路径以/开头则为基于根路径的绝对路径，否则为/.configs文件夹下路径，不支持..等返回上一目录的路径，如果传入null则不更改，默认值为""
/// @return 成功以写入模式打开文件，则返回ESP_OK
static esp_err_t my_cfg_set_configIndex_value(uint8_t configChange, const char *configPath)
{
    if (configChange > 1 && !configPath) { return ESP_ERR_INVALID_ARG; }
    cJSON *root = my_cfg_get_cjson_from_path(configIndex_path);
    if (!root) {
        my_cfg_set_config_dir();
        root = cJSON_CreateObject();
    }
    cJSON *configChangeItem = cJSON_GetObjectItem(root, "configChange");
    if (configChangeItem && (configChange <= 1)) {
        cJSON_SetBoolValue(configChangeItem, configChange);
    }
    else if (!configChangeItem) // 如果没有configChange数据项，添加对应数据项
    {
        if (configChange > 1) { configChange = 0; } // 如果传入的值大于1，认为不设置该项，默认设置为false
        cJSON_AddBoolToObject(root, "configChange", configChange);
    }
    cJSON *configPathItem = cJSON_GetObjectItem(root, "configPath");
    if (configPathItem && configPath) {
        cJSON_SetValuestring(configPathItem, configPath);
    }
    else if (!configPathItem) {
        if (!configPath)
            configPath = "";
        cJSON_AddStringToObject(root, "configPath", configPath);
    }
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) {
        return ESP_FAIL;
    }
    esp_err_t ret = ESP_OK;
    FILE *fp = my_ffopen(configIndex_path, "w");
    if (fp) {
        size_t json_size = strlen(json_str);
        my_ffwrite(json_str, 1, json_size, fp);
        // size_t write_size = my_ffwrite(json_str, 1, json_size, fp);
        my_ffclose(fp);
    }
    else {
        ret = ESP_ERR_INVALID_ARG;
    }
    free(json_str);
    return ret;
}

// 解析一组json对象格式的按键配置，如果配置和当前配置不同，则修改当前配置
// 返回修改的数量
// json对象中应该包括ioType，layer信息，以及具体的按键配置数组keysArr
static int my_key_configs_json_obj_parse(cJSON *json_obj)
{
    if (!json_obj) {
        return -1;
    }
    cJSON *ioType_item = cJSON_GetObjectItem(json_obj, "ioType");
    cJSON *layer_item = cJSON_GetObjectItem(json_obj, "layer");
    cJSON *keysArr = cJSON_GetObjectItem(json_obj, "keysArr");

    if ((ioType_item && layer_item && keysArr) &&
        (cJSON_IsNumber(ioType_item) && cJSON_IsNumber(layer_item) && cJSON_IsArray(keysArr))) {

        uint8_t ioType = ioType_item->valueint;
        uint8_t layer = layer_item->valueint;
        if (ioType >= MY_INPUT_KEY_TYPE_BASE_AND_NUM || layer >= MY_TOTAL_LAYER) {
            return -1;
        }
        uint16_t key_max_id = *(my_kb_keys_num_arr[ioType]);
        int change_cfg_num = 0;
        int arr_size = cJSON_GetArraySize(keysArr);

        cJSON *key_cfg_obj, *id_item, *type_item, *value_item;
        uint16_t id;
        uint8_t type;
        my_kb_key_config_t *key_cfg = NULL;

        for (size_t k = 0; k < arr_size; k++) {
            key_cfg_obj = cJSON_GetArrayItem(keysArr, k);
            if (!key_cfg_obj) {
                continue;
            }
            id_item = cJSON_GetObjectItem(key_cfg_obj, "id");
            type_item = cJSON_GetObjectItem(key_cfg_obj, "type");
            if ((!id_item || !type_item) ||
                (!cJSON_IsNumber(id_item) || !cJSON_IsNumber(type_item))) {
                continue;
            }
            id = id_item->valueint;
            type = type_item->valueint;
            if (id > key_max_id || type >= MY_KEYCODE_TYPE_NUM) {
                continue;
            }
            key_cfg = &my_kb_keys_config[layer][ioType][id];
            switch (type) {
                case MY_KEYCODE_NONE:
                case MY_EMPTY_KEY:
                case MY_FN_KEY:
                case MY_FN2_KEY:
                case MY_FN_SWITCH_KEY:
                    if (key_cfg->type == type) {
                        continue;
                    }
                    key_cfg->type = type;
                    key_cfg->code16 = 0;
                    change_cfg_num++;
                    break;
                case MY_KEYBOARD_CODE:
                case MY_KEYBOARD_CHAR:
                    value_item = cJSON_GetObjectItem(key_cfg_obj, "value");
                    if (!value_item || !cJSON_IsNumber(value_item)) {
                        continue;
                    }
                    uint8_t value8 = value_item->valueint;
                    if (key_cfg->type == type && key_cfg->code8 == value8) {
                        continue;
                    }
                    key_cfg->type = type;
                    key_cfg->code8 = value8;
                    change_cfg_num++;
                    break;
                case MY_CONSUMER_CODE:
                case MY_ESP_CTRL_KEY:
                    value_item = cJSON_GetObjectItem(key_cfg_obj, "value");
                    if (!value_item || !cJSON_IsNumber(value_item)) {
                        continue;
                    }
                    uint16_t value16 = value_item->valueint;
                    if (key_cfg->type == type && key_cfg->code16 == value16) {
                        continue;
                    }
                    key_cfg->type = type;
                    key_cfg->code16 = value16;
                    change_cfg_num++;
                    break;
                default:
                    break;
            }
        }
        return change_cfg_num;
    }
    return -1;
}

// 从json数组中尝试读取键盘按键配置，如果读取到的配置和默认配置不同，则设置对应配置，最后还会将其所有配置保存到fat中
// 如果在获取配置之前执行，是与默认配置比较，如果在获取配置之后执行，是与保存的配置比较
// 也就是说，如果某些其它函数修改了按键配置但没保存到fat中，之后该相同配置就无法保存到fat中
// 返回有几组配置被修改
static int my_keyConfigsArr_parse(cJSON *json_arr, uint8_t force_save)
{
    if (!json_arr) {
        return -1;
    }
    int arr_size = cJSON_GetArraySize(json_arr);
    cJSON *item = NULL;
    int set_cfg_num = 0;

    for (int i = 0; i < arr_size; i++) {
        item = cJSON_GetArrayItem(json_arr, i);
        if (my_key_configs_json_obj_parse(item) > 0) {
            set_cfg_num++;
        }
    }
    if (set_cfg_num > 0 || force_save) {
        my_key_configs_save_to_bin(NULL);
    }

    return set_cfg_num;
}

/// @brief 从json对象中获取字符串值，返回值会使用malloc分配内存
/// @param obj json对象
/// @param key 要获取的字符串的键
/// @param cmp_str 要比较的字符串，传入null则不比较，如果不为null且获取的字符串等于比较字符串，则返回该传入的比较字符串指针
/// @return 获取成功则返回字符串指针，失败返回null，注意字符串不使用后要用free释放，但比较字符串可能为静态值，自行判断能否释放
static char *my_cjson_copy_str_from_item(cJSON *obj, const char *key, char *cmp_str)
{
    if (!obj || !key) {
        return NULL;
    }
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!item) {
        return NULL;
    }
    char *str = cJSON_GetStringValue(item);
    if (!str) {
        return NULL;
    }
    if (cmp_str && strcmp(str, cmp_str) == 0) {
        return cmp_str;
    }
    size_t str_len = strlen(str);
    char *str_copy = (char *)malloc(str_len + 1);
    if (str_copy) {
        snprintf(str_copy, str_len + 1, "%s", str);
        return str_copy;
    }
    else {
        return NULL;
    }
}

/// @brief 从json对象中获取字符串值，将字符串复制到传入的缓冲区
/// @param obj json对象
/// @param key 要获取的字符串的键
/// @param cmp_str 要比较的字符串，传入null则不比较，如果不为null且获取的字符串等于比较字符串，则返回该传入的比较字符串指针或null
/// @param out_buf 已分配的缓冲区，返回获取到的字符串
/// @param buf_size 缓冲区大小
/// @return 获取成功则返回out_buf，失败返回null，和比较字符串相等时，如果cmp_str不等于out_buf则返回cmp_str，否则返回null
static char *my_cjson_copy_str_from_item_static(cJSON *obj, const char *key, char *cmp_str, char *out_buf, size_t buf_size)
{
    if (!obj || !key || !out_buf) {
        return NULL;
    }
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!item) {
        return NULL;
    }
    char *str = cJSON_GetStringValue(item);
    if (!str) {
        return NULL;
    }
    if (cmp_str && strcmp(str, cmp_str) == 0) {
        if (out_buf == cmp_str) {
            return NULL;
        }
        return cmp_str;
    }
    size_t str_len = strlen(str);
    if (buf_size < str_len + 1) {
        return NULL;
    }
    memcpy(out_buf, str, str_len);
    out_buf[str_len] = '\0';
    return out_buf;
}

// 注意：不应该在运行时多次调用该函数，最好只在开机时调用
// 根据my_nvs_cfg_list_for_json中设置的成员，依次尝试读取对应的json配置，如果存在配置，则覆盖现存值并将新值保存在nvs中
// 尝试读取ap、sta的ssid和密码，分配内存后让对应变量指针指向该内存，并将字符串保存在nvs中
static int my_devConfigsObj_parse(cJSON *json_obj)
{
    if (!json_obj) {
        return -1;
    }
    int set_item_num = 0;
    cJSON *item = NULL;
    my_cfg_nvs_open();
    for (size_t i = 0; i < my_nvs_cfg_list_for_json_size; i++) {
        if (my_nvs_cfg_list_for_json[i]->is_ptr) {
            continue;
        }
        item = cJSON_GetObjectItem(json_obj, my_nvs_cfg_list_for_json[i]->name);
        if (item && cJSON_IsNumber(item)) {
            switch (my_nvs_cfg_list_for_json[i]->size) {
                case 1:
                    uint8_t value8 = item->valueint;
                    if (my_nvs_cfg_list_for_json[i]->data.u8 != value8) {
                        my_nvs_cfg_list_for_json[i]->data.u8 = value8;
                        my_cfg_save_config_to_nvs(my_nvs_cfg_list_for_json[i]);
                        set_item_num++;
                    }
                    break;
                case 2:
                    uint16_t value16 = item->valueint;
                    if (my_nvs_cfg_list_for_json[i]->data.u16 != value16) {
                        my_nvs_cfg_list_for_json[i]->data.u16 = value16;
                        my_cfg_save_config_to_nvs(my_nvs_cfg_list_for_json[i]);
                        set_item_num++;
                    }
                    break;
                case 4:
                    uint32_t value32 = item->valueint;
                    if (my_nvs_cfg_list_for_json[i]->data.u32 != value32) {
                        my_nvs_cfg_list_for_json[i]->data.u32 = value32;
                        my_cfg_save_config_to_nvs(my_nvs_cfg_list_for_json[i]);
                        set_item_num++;
                    }
                    break;
                default:
                    set_item_num--;
                    continue;
                    break;
            }
        }
    }
    char *str = NULL;

    str = my_cjson_copy_str_from_item_static(json_obj, "apSSID", my_ap_ssid, my_ap_ssid, sizeof(my_ap_ssid));
    if (str) {
        nvs_set_str(s_nvs_handle, "apSSID", str);
        set_item_num++;
        str = NULL;
    }

    str = my_cjson_copy_str_from_item_static(json_obj, "apPasswd", my_ap_password, my_ap_password, sizeof(my_ap_password));
    if (str) {
        nvs_set_str(s_nvs_handle, "apPasswd", str);
        set_item_num++;
        str = NULL;
    }

    str = my_cjson_copy_str_from_item_static(json_obj, "staSSID", my_sta_ssid, my_sta_ssid, sizeof(my_sta_ssid));
    if (str) {
        nvs_set_str(s_nvs_handle, "staSSID", str);
        set_item_num++;
        str = NULL;
    }
    str = my_cjson_copy_str_from_item_static(json_obj, "staPasswd", my_sta_password, my_sta_password, sizeof(my_sta_password));
    if (str) {
        nvs_set_str(s_nvs_handle, "staPasswd", str);
        set_item_num++;
        str = NULL;
    }
    nvs_commit(s_nvs_handle);
    // my_cfg_nvs_close();
    return set_item_num;
}

esp_err_t my_json_cfgs_parse(cJSON *root, int *out_change_count, uint8_t *out_config_type)
{
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }
    int cfgs_to_change = 0;
    esp_err_t ret = ESP_OK;
    uint8_t execed_cfg = 0;
    if (cJSON_IsObject(root)) {
        cJSON *devConfigsObj = cJSON_GetObjectItem(root, "devConfigsObj");
        if (devConfigsObj) {
            execed_cfg |= MY_JSON_CFG_TYPE_DEVCONFIGSOBJ;
            cfgs_to_change += my_devConfigsObj_parse(devConfigsObj);
            ESP_LOGI(TAG, "load device nvs configs from json, load number: %d", cfgs_to_change);
        }
        cJSON *keyConfigsArr = cJSON_GetObjectItem(root, "keyConfigsArr");
        if (keyConfigsArr) {
            execed_cfg |= MY_JSON_CFG_TYPE_KEYCONFIGSARR;
            cfgs_to_change += my_keyConfigsArr_parse(keyConfigsArr, 0);
            ESP_LOGI(TAG, "load keyboard key configs from json,total load number: %d", cfgs_to_change);
        }
        // maybe todo: 直接读取单条nvs配置或者单组按键配置
        if (out_change_count) {
            *out_change_count = cfgs_to_change;
        }
        if (out_config_type) {
            *out_config_type = execed_cfg;
        }
    }
    else if (cJSON_IsArray(root)) {
        execed_cfg |= MY_JSON_CFG_TYPE_KEYCONFIGSARR;
        cfgs_to_change += my_keyConfigsArr_parse(root, 0);
        if (out_change_count) {
            *out_change_count = cfgs_to_change;
        }
        if (out_config_type) {
            *out_config_type = execed_cfg;
        }
    }
    else {
        ret = ESP_FAIL;
    }
    return ret;
}

esp_err_t my_cfg_get_json_configs()
{
    esp_err_t ret = ESP_OK;
    char *configPath = NULL;

    my_cfg_get_configPath(&configPath);
    if (!configPath) {
        ret = my_cfg_set_config_dir();
        return ESP_ERR_NOT_FOUND;
    }
    // 读取配置文件
    if (my_file_exist(configPath)) {
        ESP_LOGI(TAG, "start get configs from: %s", configPath);
        cJSON *root = my_cfg_get_cjson_from_path(configPath);
        if (root) {
            ret = my_json_cfgs_parse(root, NULL, NULL);
            cJSON_Delete(root);
            if (ret == ESP_OK) {
                my_cfg_save_cfgs_to_fat(nvsCfgs_path, keyCfgs_path, totalCfgs_path);
            }
        }
        else {
            ESP_LOGI(TAG, "file content maybe not json");
        }
    }
    else {
        ESP_LOGI(TAG, "config file: %s not exist", configPath);
    }
    my_cfg_set_configIndex_value(0, NULL);
    free(configPath);
    return ret;
}

// 将单个按键配置的内容输出为cjson对象
static cJSON *my_key_config_to_json_obj(my_kb_key_config_t *cfg, uint16_t id)
{
    if (!cfg) {
        return NULL;
    }
    cJSON *cfgObj = cJSON_CreateObject();
    if (!cfgObj) {
        return NULL;
    }
    if (!cJSON_AddNumberToObject(cfgObj, "id", id)) {
        goto fail_and_free;
    }
    if (!cJSON_AddNumberToObject(cfgObj, "type", cfg->type)) {
        goto fail_and_free;
    }
    switch (cfg->type) {
        case MY_KEYCODE_NONE:
        case MY_EMPTY_KEY:
        case MY_FN_KEY:
        case MY_FN2_KEY:
        case MY_FN_SWITCH_KEY:
            return cfgObj;
            break;
        case MY_KEYBOARD_CODE:
        case MY_KEYBOARD_CHAR:
            if (!cJSON_AddNumberToObject(cfgObj, "value", cfg->code8)) {
                goto fail_and_free;
            }
            return cfgObj;
            break;
        case MY_CONSUMER_CODE:
        case MY_ESP_CTRL_KEY:
            if (!cJSON_AddNumberToObject(cfgObj, "value", cfg->code16)) {
                goto fail_and_free;
            }
            return cfgObj;
            break;
        default:
            break;
    }
fail_and_free:
    cJSON_Delete(cfgObj);
    return NULL;
}

// 将一组按键配置保存到cjson数组，并和按键类型和按键所对应层一起存储为cjson对象
static cJSON *my_key_config_arr_to_json_obj(uint8_t ioType, uint8_t layer, my_kb_key_config_t *keysArr, uint16_t cfg_num)
{
    if (!keysArr || !cfg_num ||
        ioType >= MY_INPUT_KEY_TYPE_BASE_AND_NUM || layer >= MY_TOTAL_LAYER) {
        return NULL;
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    if (!cJSON_AddNumberToObject(root, "ioType", ioType)) {
        goto fail_and_free;
    }
    if (!cJSON_AddNumberToObject(root, "layer", layer)) {
        goto fail_and_free;
    }
    cJSON *jsonArr = cJSON_AddArrayToObject(root, "keysArr");
    if (!jsonArr) {
        goto fail_and_free;
    }
    cJSON *item = NULL;
    for (size_t i = 0; i < cfg_num; i++) {
        item = my_key_config_to_json_obj(&keysArr[i], i);
        if (!item) {
            goto fail_and_free;
        }
        if (!cJSON_AddItemToArray(jsonArr, item)) {
            goto fail_and_free;
        }
    }
    return root;
fail_and_free:
    cJSON_Delete(root);
    return NULL;
}

// 将当前的所有按键配置输出为cjson数组
static cJSON *my_key_configs_to_json_arr()
{
    cJSON *rootArr = cJSON_CreateArray();
    if (!rootArr) {
        goto fail_and_free;
    }
    cJSON *item = NULL;
    for (size_t ioType = 0; ioType < MY_INPUT_KEY_TYPE_BASE_AND_NUM; ioType++) {
        for (size_t layer = 0; layer < MY_TOTAL_LAYER; layer++) {
            item = my_key_config_arr_to_json_obj(ioType, layer, my_kb_keys_config[layer][ioType], *my_kb_keys_num_arr[ioType]);
            if (!item) {
                goto fail_and_free;
            }
            if (!cJSON_AddItemToArray(rootArr, item)) {
                goto fail_and_free;
            }
        }
    }
    return rootArr;
fail_and_free:
    cJSON_Delete(rootArr);
    return NULL;
}

// 将非指针类型的nvs数据存储到传入的cjson对象中
static cJSON *my_nvs_cfg_add_to_json_obj(cJSON *root, my_nvs_cfg_t *cfg)
{
    if (!root || !cfg || cfg->is_ptr) {
        return NULL;
    }
    switch (cfg->size) {
        case 1:
            return cJSON_AddNumberToObject(root, cfg->name, cfg->data.u8);
            break;
        case 2:
            return cJSON_AddNumberToObject(root, cfg->name, cfg->data.u16);
            break;
        case 4:
            return cJSON_AddNumberToObject(root, cfg->name, cfg->data.u32);
            break;
        default:
            break;
    }
    return NULL;
}

// 将当前nvs配置转为cjson对象
static cJSON *my_nvs_cfgs_to_json_obj(uint8_t *out_add_num)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON *item = NULL;
    uint8_t add_num = 0;
    for (size_t i = 0; i < my_nvs_cfg_list_for_json_size; i++) {
        item = my_nvs_cfg_add_to_json_obj(root, my_nvs_cfg_list_for_json[i]);
        if (item) {
            add_num++;
        }
    }
    item = cJSON_AddStringToObject(root, "apSSID", my_ap_ssid);
    if (item) {
        add_num++;
    }
    item = cJSON_AddStringToObject(root, "apPasswd", my_ap_password);
    if (item) {
        add_num++;
    }
    item = cJSON_AddStringToObject(root, "staSSID", my_sta_ssid);
    if (item) {
        add_num++;
    }
    item = cJSON_AddStringToObject(root, "staPasswd", "");
    if (item) {
        add_num++;
    }
    ESP_LOGI(TAG, "add %d nvs configs to json", add_num);
    if (out_add_num) {
        *out_add_num = add_num;
    }
    return root;
}

// 将当前的nvs和按键配置转换为cjson对象
static cJSON *my_cfg_to_json_obj(uint8_t convert_nvs, uint8_t convert_key)
{
    if (!convert_nvs && !convert_key) {
        return NULL;
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    if (convert_nvs) {
        cJSON *devConfigsObj = my_nvs_cfgs_to_json_obj(NULL);
        if (!devConfigsObj) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToObject(root, "devConfigsObj", devConfigsObj);
    }
    if (convert_key) {
        cJSON *keyConfigsArr = my_key_configs_to_json_arr();
        if (!keyConfigsArr) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToObject(root, "keyConfigsArr", keyConfigsArr);
    }
    return root;
}

char *my_cfg_to_json_str(uint8_t convert_nvs, uint8_t convert_key)
{
    if (!convert_nvs && !convert_key) {
        return NULL;
    }
    cJSON *root = my_cfg_to_json_obj(convert_nvs, convert_key);
    if (!root) {
        return NULL;
    }
    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return str;
}

esp_err_t my_cfg_save_cfgs_to_fat(const char *nvs_cfg_path, const char *key_cfg_path, const char *total_cfg_path)
{
    if (!nvs_cfg_path && !key_cfg_path && !total_cfg_path) {
        return ESP_ERR_INVALID_ARG;
    }
    if (nvs_cfg_path) {
        char *nvs_cfg_str = my_cfg_to_json_str(1, 0);
        if (nvs_cfg_str) {
            FILE *fp = my_ffopen(nvs_cfg_path, "w");
            if (fp) {
                size_t nvs_cfg_str_len = strlen(nvs_cfg_str);
                my_ffwrite(nvs_cfg_str, 1, nvs_cfg_str_len, fp);
                my_ffclose(fp);
            }
            free(nvs_cfg_str);
            ESP_LOGI(TAG, "save nvs configs to %s", nvs_cfg_path);
        }
    }
    if (key_cfg_path) {
        char *key_cfg_str = my_cfg_to_json_str(0, 1);
        if (key_cfg_str) {
            FILE *fp = my_ffopen(key_cfg_path, "w");
            size_t key_cfg_str_len = strlen(key_cfg_str);
            if (fp) {
                my_ffwrite(key_cfg_str, 1, key_cfg_str_len, fp);
                my_ffclose(fp);
            }
            free(key_cfg_str);
            ESP_LOGI(TAG, "save key configs to %s", key_cfg_path);
        }
    }
    if (total_cfg_path) {
        char *total_cfg_str = my_cfg_to_json_str(1, 1);
        if (total_cfg_str) {
            FILE *fp = my_ffopen(total_cfg_path, "w");
            size_t total_cfg_str_len = strlen(total_cfg_str);
            if (fp) {
                my_ffwrite(total_cfg_str, 1, total_cfg_str_len, fp);
                my_ffclose(fp);
            }
            free(total_cfg_str);
            ESP_LOGI(TAG, "save total configs to %s", total_cfg_path);
        }
    }
    return ESP_OK;
}

esp_err_t my_cfg_save_configs_template_to_fat()
{
    if (my_app_main_is_return || !my_cfg_out_def_config.data.u8) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ESP_OK;
    if (!my_file_exist(configTemplate_path)) {
        my_cfg_set_config_dir();
        ret = my_cfg_save_cfgs_to_fat(NULL, NULL, configTemplate_path);
        ESP_LOGI(TAG, "save configs template to path:%s ret:%d", configTemplate_path, ret);
    }
    if (!my_file_exist(defCfgs_path)) {
        my_cfg_set_config_dir();
        my_cfg_save_cfgs_to_fat(NULL, NULL, defCfgs_path);
        ESP_LOGI(TAG, "save default configs to path:%s ret:%d", configTemplate_path, ret);
    }
    my_cfg_out_def_config.data.u8 = 0;
    my_cfg_save_config_to_nvs(&my_cfg_out_def_config);
    return ret;
}

// 将当前按键配置数据原样输出为二进制数据存储
static esp_err_t my_key_config_data_to_bin(my_key_config_bin_data_t *out_buf, uint8_t ioType, uint8_t layer, const uint8_t *data, uint16_t data_size)
{
    if (!out_buf || !data || !data_size) {
        return ESP_ERR_INVALID_ARG;
    }
    out_buf->ioType = ioType;
    out_buf->layer = layer;
    out_buf->size = data_size + sizeof(my_key_config_bin_data_t);
    out_buf->crc = 0;
    memcpy(out_buf->data, data, data_size);
    out_buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t *)out_buf, out_buf->size);
    return ESP_OK;
}

esp_err_t my_key_configs_save_to_bin(uint16_t *out_write_bytes)
{
    FILE *fp = my_ffopen(keyConfigsBin_path, "wb");
    if (fp) {
        uint16_t data_size = 0;
        uint16_t total_size = 0;
        uint16_t key_num = 0;
        uint16_t write_size = 0;
        my_key_config_bin_data_t *buf = NULL;
        for (size_t ioType = 0; ioType < MY_INPUT_KEY_TYPE_BASE_AND_NUM; ioType++) {
            for (size_t layer = 0; layer < MY_TOTAL_LAYER; layer++) {
                key_num = *my_kb_keys_num_arr[ioType];
                data_size = sizeof(my_kb_key_config_t) * key_num;
                total_size = sizeof(my_key_config_bin_data_t) + data_size;
                buf = malloc(total_size);
                if (buf) {
                    my_key_config_data_to_bin(buf, ioType, layer, (uint8_t *)my_kb_keys_config[layer][ioType], data_size);
                    write_size += my_ffwrite(buf, 1, total_size, fp);
                    free(buf);
                    buf = NULL;
                }
            }
        }
        my_ffclose(fp);
        if (out_write_bytes) {
            *out_write_bytes = write_size;
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

// 解析二进制格式的按键数据，如果crc正确且大小符合，则会覆盖当前的按键配置，
static esp_err_t my_key_config_bin_data_parse(my_key_config_bin_data_t *buf, uint16_t buf_size, uint16_t *out_parse_size)
{
    if (!buf || buf_size < sizeof(my_key_config_bin_data_t) || buf->size > buf_size) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t crc = buf->crc;
    buf->crc = 0;
    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t *)buf, buf->size);
    if (crc != buf->crc) {
        return ESP_ERR_INVALID_CRC;
    }
    uint16_t data_size = buf->size - sizeof(my_key_config_bin_data_t);
    uint16_t target_size = (*my_kb_keys_num_arr[buf->ioType]) * sizeof(my_kb_key_config_t);
    if (data_size != target_size) {
        return ESP_ERR_INVALID_CRC;
    }
    memcpy(my_kb_keys_config[buf->layer][buf->ioType], buf->data, target_size);
    if (out_parse_size) {
        *out_parse_size = buf->size;
    }
    return ESP_OK;
}

esp_err_t my_key_configs_load_from_bin()
{
    FILE *fp = my_ffopen(keyConfigsBin_path, "rb");
    if (!fp) {
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t ret = ESP_FAIL;
    uint16_t file_size = my_file_get_size_raw(fp);
    if (file_size < sizeof(my_key_config_bin_data_t)) {
        goto delete_file_and_return;
    }
    uint8_t *buf = malloc(file_size);
    if (!buf) {
        ret = ESP_ERR_NO_MEM;
        goto close_file_and_return;
    }
    my_ffseek(fp, 0, SEEK_SET);
    uint16_t remaining_size = my_ffread(buf, 1, file_size, fp);
    if (remaining_size != file_size) {
        free(buf);
        ret = ESP_FAIL;
        goto close_file_and_return;
    }
    uint16_t parse_size = 0;
    uint8_t *data_head_ptr = buf;
    while (remaining_size > sizeof(my_key_config_bin_data_t)) {
        data_head_ptr += parse_size;
        parse_size = 0;
        ret = my_key_config_bin_data_parse((my_key_config_bin_data_t *)data_head_ptr, remaining_size, &parse_size);
        if (ret == ESP_OK) {
            remaining_size -= parse_size;
        }
        else if (ret == ESP_ERR_INVALID_CRC) {
            free(buf);
            goto delete_file_and_return;
        }
        else {
            break;
        }
    }
    free(buf);
close_file_and_return:
    my_ffclose(fp);
    return ret;
delete_file_and_return: // 如果文件可能存在问题，删除文件
    my_ffclose(fp);
    my_fremove(keyConfigsBin_path);
    return ret;
}
#pragma endregion

int64_t my_get_time()
{
    return esp_timer_get_time();
}
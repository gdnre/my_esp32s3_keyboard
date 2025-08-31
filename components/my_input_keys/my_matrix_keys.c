#include "my_matrix_keys.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "stdlib.h"
#include "string.h"

typedef struct
{
    // 假装有个锁变量
    uint8_t in_num;
    uint8_t out_num;
    uint8_t *in_pins;
    uint8_t *out_pins;
    uint8_t cur_scan_line_index; // 当前扫描out引脚索引
    uint64_t last_scan_line_time;
    int8_t last_in_pins_level[MY_MAX_IN_PINS_NUM];
    my_input_matrix_config_t matrix_config;
    my_key_info_t *matrix_keys;
} _my_matrix_keys_handle_t;

uint64_t my_matrix_bit_mask_all = 0;
uint64_t my_matrix_bit_mask_in = 0;
uint64_t my_matrix_bit_mask_out = 0;

static const char *TAG = "my_matrix_keys";
static uint8_t matrix_key_init_flag = 0;

static _my_matrix_keys_handle_t _matrix_keys_handle = {
    .in_num = 0,
    .out_num = 0,
    .in_pins = NULL,
    .out_pins = NULL,
    .cur_scan_line_index = 0xff,
    .last_scan_line_time = 0,
    .last_in_pins_level = {0},
    .matrix_config = (my_input_matrix_config_t){
        .row_num = 0,
        .col_num = 0,
        .active_level = 1,
        .switch_input_pins = 0,
        .row_pins = NULL,
        .col_pins = NULL},
    .matrix_keys = NULL};

static void IRAM_ATTR my_matrix_keys_isr_handler(void *arg)
{
    if (my_input_task_handle) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        // xTaskNotifyFromISR(my_input_task_handle, arg, eSetBits, NULL);
        vTaskNotifyGiveFromISR(my_input_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
};

int8_t my_matrix_keys_config_init(my_input_matrix_config_t *matrix_conf, my_matrix_keys_handle_t *handle)
{
    if (matrix_key_init_flag || *handle) {
        ESP_LOGW(TAG, "my_matrix_key_init already init");
        return -1;
    }
    if (!matrix_conf || !handle) {
        return 0;
    }

    // 复制config中内容
    _matrix_keys_handle.matrix_config.row_num = matrix_conf->row_num;
    _matrix_keys_handle.matrix_config.col_num = matrix_conf->col_num;
    if (!matrix_conf->row_num || !matrix_conf->col_num)
        return 0;

    _matrix_keys_handle.matrix_config.total_num = matrix_conf->row_num * matrix_conf->col_num;
    _matrix_keys_handle.matrix_config.pull_def = matrix_conf->pull_def;
    _matrix_keys_handle.matrix_config.active_level = matrix_conf->active_level;
    _matrix_keys_handle.matrix_config.switch_input_pins = matrix_conf->switch_input_pins;

    _matrix_keys_handle.matrix_config.row_pins = (uint8_t *)calloc(matrix_conf->row_num, sizeof(uint8_t));
    if (_matrix_keys_handle.matrix_config.row_pins == NULL) {
        ESP_LOGE(TAG, "my_matrix_key_init malloc matrix_config.row_pins failed");
        return 0;
    }
    _matrix_keys_handle.matrix_config.col_pins = (uint8_t *)calloc(matrix_conf->col_num, sizeof(uint8_t));
    if (_matrix_keys_handle.matrix_config.col_pins == NULL) {
        ESP_LOGE(TAG, "my_matrix_key_init malloc matrix_config.col_pins failed");
        free(_matrix_keys_handle.matrix_config.row_pins);
        _matrix_keys_handle.matrix_config.row_pins = NULL;
        return 0;
    }

    _matrix_keys_handle.matrix_keys = (my_key_info_t *)calloc(_matrix_keys_handle.matrix_config.total_num, sizeof(my_key_info_t));
    if (_matrix_keys_handle.matrix_keys == NULL) {
        ESP_LOGE(TAG, "my_matrix_key_init malloc matrix_keys failed");
        free(_matrix_keys_handle.matrix_config.row_pins);
        free(_matrix_keys_handle.matrix_config.col_pins);
        _matrix_keys_handle.matrix_config.row_pins = NULL;
        _matrix_keys_handle.matrix_config.col_pins = NULL;
        _matrix_keys_handle.matrix_config.row_num = 0;
        _matrix_keys_handle.matrix_config.col_num = 0;
        return 0;
    }

    memcpy(_matrix_keys_handle.matrix_config.row_pins, matrix_conf->row_pins, matrix_conf->row_num * sizeof(uint8_t));
    memcpy(_matrix_keys_handle.matrix_config.col_pins, matrix_conf->col_pins, matrix_conf->col_num * sizeof(uint8_t));
    for (int i = 0; i < _matrix_keys_handle.matrix_config.total_num; i++) {
        _matrix_keys_handle.matrix_keys[i] = (my_key_info_t){
            .id = i,
            .type = MY_INPUT_KEY_MATRIX,
            .active_level = matrix_conf->active_level,
            .pull_def = matrix_conf->pull_def,
            .state = MY_KEY_IDLE,
            .pressed_timer = 0,
            .released_timer = 0};
    }
    if (_matrix_keys_handle.matrix_config.switch_input_pins) {
        _matrix_keys_handle.in_num = _matrix_keys_handle.matrix_config.col_num;
        _matrix_keys_handle.out_num = _matrix_keys_handle.matrix_config.row_num;
        _matrix_keys_handle.in_pins = _matrix_keys_handle.matrix_config.col_pins;
        _matrix_keys_handle.out_pins = _matrix_keys_handle.matrix_config.row_pins;
    }
    else {
        _matrix_keys_handle.in_num = _matrix_keys_handle.matrix_config.row_num;
        _matrix_keys_handle.out_num = _matrix_keys_handle.matrix_config.col_num;
        _matrix_keys_handle.in_pins = _matrix_keys_handle.matrix_config.row_pins;
        _matrix_keys_handle.out_pins = _matrix_keys_handle.matrix_config.col_pins;
    }

    *handle = &_matrix_keys_handle;
    matrix_key_init_flag = 1;
    return 1;
}

// 和gpio不同，矩阵键盘active_level应该都相同
void my_matrix_keys_io_init(my_matrix_keys_handle_t handle)
{
    if (!handle) {
        return;
    }
    _my_matrix_keys_handle_t *h = (_my_matrix_keys_handle_t *)handle;
    my_input_matrix_config_t *matrix_conf = &h->matrix_config;

    uint8_t *_input_pins = h->in_pins;
    uint8_t *_output_pins = h->out_pins;
    uint8_t in_num = h->in_num;
    uint8_t out_num = h->out_num;

    my_matrix_bit_mask_in = 0;
    my_matrix_bit_mask_out = 0;

    for (size_t i = 0; i < in_num; i++) {
        my_matrix_bit_mask_in |= (1llu << _input_pins[i]);
    }
    for (size_t i = 0; i < out_num; i++) {
        my_matrix_bit_mask_out |= (1llu << _output_pins[i]);
    }

    // 输入引脚要配置中断、可选的上拉
    // 输出引脚默认设置为!active_level，无中断
    gpio_config_t in_io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config_t out_io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT, // 不设置为开漏输出时，可能导致外部上拉没法被接地
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    in_io_conf.pin_bit_mask = my_matrix_bit_mask_in;
    out_io_conf.pin_bit_mask = my_matrix_bit_mask_out;
    gpio_config(&in_io_conf);
    gpio_config(&out_io_conf);

    for (size_t i = 0; i < out_num; i++) {
        gpio_set_level(_output_pins[i], !matrix_conf->active_level);
    }
    for (size_t i = 0; i < in_num; i++) {
        switch (matrix_conf->active_level) {
            case 0:
                if (matrix_conf->pull_def)
                    gpio_pullup_en(_input_pins[i]);
                gpio_set_intr_type(_input_pins[i], GPIO_INTR_NEGEDGE);
                break;
            case 1:
                if (matrix_conf->pull_def)
                    gpio_pulldown_en(_input_pins[i]);
                gpio_set_intr_type(_input_pins[i], GPIO_INTR_POSEDGE);
            default:
                break;
        }
        gpio_isr_handler_add(_input_pins[i], my_matrix_keys_isr_handler, &_input_pins[i]);
        gpio_intr_disable(_input_pins[i]);
    }
}

void my_matrix_set_out_to_level(my_matrix_keys_handle_t handle, uint8_t is_active_level)
{
    if (!handle)
        return;
    _my_matrix_keys_handle_t *h = (_my_matrix_keys_handle_t *)handle;
    uint8_t _level = is_active_level ? h->matrix_config.active_level : !h->matrix_config.active_level;
    for (size_t i = 0; i < h->out_num; i++) {
        gpio_set_level(h->out_pins[i], _level);
    }
    h->cur_scan_line_index = 0xff;
}

uint8_t my_matrix_scan_one_line(my_matrix_keys_handle_t handle, uint8_t out_index)
{
    if (!handle)
        return 0xff;
    _my_matrix_keys_handle_t *h = (_my_matrix_keys_handle_t *)handle;
    if (out_index >= h->out_num)
        return 0xff;

    uint8_t _idle_level = !h->matrix_config.active_level;
    for (size_t j = 0; j < h->out_num; j++) {
        if (j == out_index)
            gpio_set_level(h->out_pins[out_index], h->matrix_config.active_level);
        else
            gpio_set_level(h->out_pins[j], _idle_level);
    }
    h->cur_scan_line_index = out_index;
    h->last_scan_line_time = esp_timer_get_time();
    return h->cur_scan_line_index;
}

uint8_t check_matrix_keys(my_matrix_keys_handle_t handle)
{
    if (!handle) {
        return 0;
    }

    _my_matrix_keys_handle_t *h = (_my_matrix_keys_handle_t *)handle;
    uint16_t any_active = 0;
    int _level = -1;
    my_key_info_t *_key_info = NULL;

    uint8_t _in_num = h->in_num;
    uint8_t _out_num = h->out_num;
    uint8_t *_in_pins = h->in_pins;
    uint8_t *_out_pins = h->out_pins;

    uint8_t _active_level = h->matrix_config.active_level;
    uint8_t _idle_level = !_active_level;
    int16_t _id = -1;

    for (size_t c_out = 0; c_out < _out_num; c_out++) {
        gpio_set_level(_out_pins[c_out], _active_level);
        for (size_t j = 0; j < _out_num; j++) {
            if (j == c_out) {
                continue;
            }
            else {
                gpio_set_level(_out_pins[j], _idle_level);
            }
        }

        esp_rom_delay_us(20); // 延迟使电平稳定后再读数
        // ESP_LOGI(TAG, "=========col %d============", c_out);
        for (size_t c_in = 0; c_in < _in_num; c_in++) {
            _id = my_matrix_to_id(c_in, c_out, 1);
            _level = gpio_get_level(_in_pins[c_in]);
            _key_info = &h->matrix_keys[_id];
            any_active += update_key_info(_key_info, _level);
            // if (_level == _active_level)
            // {
            //     // ESP_LOGI("matrix_keys", "row:%d, col:%d, id:%d, active level:%d, level:%d, active:%d", c_in, c_out, _id, _key_info->active_level, _level, any_active);
            //     // gpio_dump_io_configuration(stdout, my_matrix_bit_mask_out);
            // }
            // ESP_LOGI(TAG,"any_active:%d",any_active);
        }
    }

    return any_active > UINT8_MAX ? UINT8_MAX : any_active;
}

uint8_t check_matrix_keys_faster(my_matrix_keys_handle_t handle)
{
    // maybo never todo: 直接操作io寄存器来获得更快的速度
    if (!handle) {
        return 0;
    }
    _my_matrix_keys_handle_t *h = (_my_matrix_keys_handle_t *)handle;
    // 先设置最后一列电平，检测该列，再进行初始化，如果外面的函数已经设置了，检查设置后经过的事件
    if (h->cur_scan_line_index >= h->out_num) // 如果当前扫描的列大于列数，说明没有扫描过列，从0开始扫描
    {
        if (my_matrix_scan_one_line(handle, 0) >= h->out_num) // 如果还是扫描不了，说明程序有问题
            abort();
    }

    uint16_t any_active = 0;
    my_key_info_t *_key_info = NULL;

    uint8_t _idle_level = !h->matrix_config.active_level;
    uint8_t _cur_out_index = h->cur_scan_line_index;
    uint8_t _next_out_index = 0xff;
    // uint64_t scan_gap_us = esp_timer_get_time() - h->last_scan_line_time;
    int16_t _id = -1;
    // if (scan_gap_us < 20)
    // {
    //     esp_rom_delay_us(20 - scan_gap_us);
    // }
    for (size_t i = 1; i < h->out_num; i++) // 扫描(out_num-1)次，每次循环完毕后，都会将_next_out_index列设置为触发电平
    {
        // _next_out_index = (h->cur_scan_line_index + i) % h->out_num;
        // _cur_out_index = (h->cur_scan_line_index + i - 1) % h->out_num;

        _next_out_index = (_cur_out_index + 1) % h->out_num;

        for (size_t k = 0; k < h->in_num; k++) // 记录当前输出列下输入引脚的扫描结果
        {
            h->last_in_pins_level[k] = gpio_get_level(h->in_pins[k]);
        }

        // 设置下一列输出引脚
        for (size_t j = 0; j < h->out_num; j++) {
            if (j == _next_out_index)
                gpio_set_level(h->out_pins[_next_out_index], h->matrix_config.active_level);
            else
                gpio_set_level(h->out_pins[j], _idle_level);
        }
        h->last_scan_line_time = esp_timer_get_time();

        for (size_t k = 0; k < h->in_num; k++) // 之前已记录当前列下所有行输入结果，直接执行
        {
            _id = my_matrix_to_id(k, _cur_out_index, 1);
            _key_info = &h->matrix_keys[_id];
            any_active += update_key_info(_key_info, h->last_in_pins_level[k]);
        }
        _cur_out_index = (_cur_out_index + 1) % h->out_num;
        // // 如果电平转换速度慢，取消注释
        // scan_gap_us = esp_timer_get_time() - h->last_scan_line_time;
        // if (scan_gap_us < 20)
        // {
        //     esp_rom_delay_us(20 - scan_gap_us);
        // }
    }
    h->cur_scan_line_index = _next_out_index;
    for (size_t k = 0; k < h->in_num; k++) // 最后一次扫描结果不会在循环中记录和执行，循环结束后额外添加
    {
        h->last_in_pins_level[k] = gpio_get_level(h->in_pins[k]);
        _id = my_matrix_to_id(k, h->cur_scan_line_index, 1);
        _key_info = &h->matrix_keys[_id];
        any_active += update_key_info(_key_info, h->last_in_pins_level[k]);
    }
    return any_active > UINT8_MAX ? UINT8_MAX : any_active;
}

uint8_t my_matrix_key_register_event_cb(int16_t id, my_input_key_event_t event, my_input_key_event_cb_t cb)
{
    if (!matrix_key_init_flag) {
        ESP_LOGE(TAG, "gpio key %d register event cb fail ,gpio keys handle is null", id);
        return 0;
    }

    if (id < 0 || id >= _matrix_keys_handle.matrix_config.total_num) {
        ESP_LOGW(TAG, "gpio key %d register event cb fail ,id is out of range", id);
        return 0;
    }
    if (event < 0 || event >= MY_KEY_EVENT_NUM) {
        ESP_LOGW(TAG, "gpio key %d register event cb fail ,event is out of range", id);
        return 0;
    }
    _matrix_keys_handle.matrix_keys[id].event_cbs[event] = cb;
    return 1;
}

int16_t my_matrix_to_id(uint8_t row, uint8_t col, uint8_t is_io_index)
{
    if (!is_io_index && (row >= _matrix_keys_handle.matrix_config.row_num || col >= _matrix_keys_handle.matrix_config.col_num)) // 如果不是io索引号，行列不能大于矩阵的行列数
    {
        return -1;
    }

    if (!is_io_index || !_matrix_keys_handle.matrix_config.switch_input_pins) { // 对于不交换io
        return (row * _matrix_keys_handle.matrix_config.col_num + col);
    }
    else // 对于交换io，且是io引脚
    {
        return (col * _matrix_keys_handle.matrix_config.col_num + row);
    }
}

uint8_t my_matrix_key_register_event_cb2(uint8_t row, uint8_t col, my_input_key_event_t event, my_input_key_event_cb_t cb)
{
    int16_t _id = my_matrix_to_id(row, col, 0);
    return my_matrix_key_register_event_cb(_id, event, cb);
}

uint8_t my_id_to_matrix(int16_t id, uint8_t is_io_index, uint8_t *out_row, uint8_t *out_col)
{
    if (id < 0 || id >= _matrix_keys_handle.matrix_config.total_num) {
        return 1;
    }

    if (!is_io_index || !_matrix_keys_handle.matrix_config.switch_input_pins) {
        // ESP_LOGI(TAG, "id_to_matrix: id=%d", id);
        *out_row = id / _matrix_keys_handle.matrix_config.col_num;
        *out_col = id % _matrix_keys_handle.matrix_config.col_num;
        // ESP_LOGI(TAG, "id_to_matrix: row=%d, col=%d", *out_row, *out_col);
    }
    else {
        *out_col = id / _matrix_keys_handle.matrix_config.col_num;
        *out_row = id % _matrix_keys_handle.matrix_config.col_num;
    }
    return 0;
}

void my_matrix_in_keys_intr_dis(my_matrix_keys_handle_t handle)
{
    if (!handle) {
        // ESP_LOGE(__func__, "matrix keys handle is null");
        return;
    }
    _my_matrix_keys_handle_t *h = (_my_matrix_keys_handle_t *)handle;

    for (size_t i = 0; i < h->in_num; i++) {
        gpio_intr_disable(h->in_pins[i]);
    };
}

void my_matrix_in_keys_intr_en(my_matrix_keys_handle_t handle)
{
    if (!handle) {
        // ESP_LOGE(__func__, "matrix keys handle is null");
        return;
    }
    _my_matrix_keys_handle_t *h = (_my_matrix_keys_handle_t *)handle;
    for (size_t i = 0; i < h->in_num; i++) {
        gpio_intr_enable(h->in_pins[i]);
    }
}
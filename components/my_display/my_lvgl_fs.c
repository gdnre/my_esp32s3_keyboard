#include "esp_lvgl_port.h"
#include "my_config.h"
#include "my_display.h"
#include "my_fat_mount.h"
#include "my_lvgl_private.h"

#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "my lv fs";

bool my_lv_fs_ready_cb(lv_fs_drv_t *drv);
void *my_lv_fs_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode);
lv_fs_res_t my_lv_fs_close_cb(lv_fs_drv_t *drv, void *file_p);
lv_fs_res_t my_lv_fs_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br);
lv_fs_res_t my_lv_fs_write_cb(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw);
lv_fs_res_t my_lv_fs_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence);
lv_fs_res_t my_lv_fs_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p);

static lv_fs_drv_t posix_fs_drv;

void my_lvgl_fs_init(void)
{
    lv_fs_drv_init(&posix_fs_drv);

    posix_fs_drv.letter = 'D';
    posix_fs_drv.cache_size = 2048;

    posix_fs_drv.ready_cb = my_lv_fs_ready_cb;

    posix_fs_drv.open_cb = my_lv_fs_open_cb;
    posix_fs_drv.close_cb = my_lv_fs_close_cb;
    posix_fs_drv.read_cb = my_lv_fs_read_cb;
    posix_fs_drv.write_cb = my_lv_fs_write_cb;
    posix_fs_drv.seek_cb = my_lv_fs_seek_cb;
    posix_fs_drv.tell_cb = my_lv_fs_tell_cb;

    posix_fs_drv.dir_open_cb = NULL;
    posix_fs_drv.dir_read_cb = NULL;
    posix_fs_drv.dir_close_cb = NULL;

    posix_fs_drv.user_data = &my_fat_is_mount;

    lv_fs_drv_register(&posix_fs_drv);
}

bool my_lv_fs_ready_cb(lv_fs_drv_t *drv)
{
    return my_fat_is_mount();
};

void *my_lv_fs_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    // 假设path格式为S:/folder/file.txt，到这一步时不需要盘符，但在使用lv...set_src时需要加上盘符
    const char *target_path = path;
    FILE *fp = NULL;
    if (mode == LV_FS_MODE_RD) {
        fp = my_ffopen(target_path, "r");
    }
    else if (mode == LV_FS_MODE_WR) {
        fp = my_ffopen(target_path, "w");
    }
    else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) {
        fp = my_ffopen(target_path, "r+");
    }
    return fp;
}

lv_fs_res_t my_lv_fs_close_cb(lv_fs_drv_t *drv, void *file_p)
{
    int ret = my_ffclose(file_p);
    file_p = NULL;
    if (ret == 0) {
        return LV_FS_RES_OK;
    };
    return LV_FS_RES_FS_ERR;
}

lv_fs_res_t my_lv_fs_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    *br = my_ffread(buf, 1, btr, file_p);
    return LV_FS_RES_OK;
}

// buf 是要写入的数据； btw 是要写入的字节数； bw 是实际写入的字节数
lv_fs_res_t my_lv_fs_write_cb(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw)
{

    *bw = my_ffwrite(buf, 1, btw, file_p);
    return LV_FS_RES_OK;
}

lv_fs_res_t my_lv_fs_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    if (my_ffseek(file_p, pos, whence) == 0) {
        return LV_FS_RES_OK;
    }
    return LV_FS_RES_FS_ERR;
}

lv_fs_res_t my_lv_fs_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    uint32_t pos = my_fftell(file_p);
    *pos_p = pos;
    if (pos > 0) {
        return LV_FS_RES_OK;
    }
    return LV_FS_RES_FS_ERR;
}

#pragma once
#include "esp_err.h"
#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t my_msc_init(const char *partition_label);

    void my_msc_deinit(void);

#ifdef __cplusplus
}
#endif

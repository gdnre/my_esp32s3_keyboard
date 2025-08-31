#pragma once
#include <stdio.h>
#include "esp_err.h"
uint8_t my_ina226_is_connected();

esp_err_t my_ina226_get_result(float *bus_vol, float *current, float *power, float *shunt_vol);

void my_ina226_task_start(uint32_t StackDepth, unsigned int Priority, int core_id);

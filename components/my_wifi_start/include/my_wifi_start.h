#pragma once
#include "esp_err.h"
#include "sdkconfig.h"

#ifdef CONFIG_MY_DEVICE_IS_RECEIVER
// 这个宏之后要废弃，是没用sdkconfig时定义的
#define MY_ESPNOW_IS_RECEIVER CONFIG_MY_DEVICE_IS_RECEIVER
#else
#define MY_ESPNOW_IS_RECEIVER 0
#endif

// 根据传入的模式启动wifi，如果传入和之前不同的参数，可以切换模式，注意已经初始化的网络接口，即使调用my_wifi_stop可能也不会释放，需要重启
// 如果启用了espnow，wifi if会切换为sta模式，且强制关闭正常的sta和ap功能，防止信道乱跳
esp_err_t my_wifi_set_features(uint8_t mode);

// 在wifi事件中被调用，运行任务是sys_evt，响应sta模式断开连接(超过重连次数)、得到ip，响应ap模式分配ip、有设备断开连接
// ap模式回调中，设备连接不会触发回调，必须分配ip才会触发回调，但设备断开会触发一次回调，每次设备连接事件中，连接设备数会加一，断开连接事件中设备数会加一，value参数为连接设备数，理论上它为0时才代表没有设备连接
// espnow设备连接状态改变时也会触发该回调
void my_wifi_state_change_cb(uint8_t wifi_mode, int8_t is_connected);

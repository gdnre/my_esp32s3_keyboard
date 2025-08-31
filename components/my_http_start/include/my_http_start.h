#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

extern httpd_handle_t my_http_server_handle;
extern httpd_config_t my_http_config;
extern bool my_default_http_server_started;

esp_err_t my_http_start_with_config(httpd_config_t *c, httpd_handle_t *s);

esp_err_t my_http_start(void);



/// @brief 停止http服务器
/// @param httpd_handle_ptr 要停止的服务器句柄，如果为null，则停止默认的服务器
/// @return 
esp_err_t my_http_stop(httpd_handle_t *httpd_handle_ptr);

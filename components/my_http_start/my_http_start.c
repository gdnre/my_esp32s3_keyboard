#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>

#include "my_http_start.h"

#include "esp_log.h"

httpd_handle_t my_http_server_handle = NULL;
httpd_config_t my_http_config = HTTPD_DEFAULT_CONFIG();
bool my_default_http_server_started = 0;

static const char *TAG = "my http start";

esp_err_t my_http_start_with_config(httpd_config_t *httpd_config, httpd_handle_t *out_server_handle)
{
    httpd_config_t *p_config = &my_http_config;
    httpd_handle_t *p_server = &my_http_server_handle;
    if (httpd_config)
    {
        p_config = httpd_config;
    }
    if (out_server_handle)
    {
        p_server = out_server_handle;
    }

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", p_config->server_port);
    if (httpd_start(p_server, p_config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t my_http_start(void)
{
    my_http_config.uri_match_fn = httpd_uri_match_wildcard;
    my_http_config.max_uri_handlers = 16;
    my_http_config.max_resp_headers = 12;
    if (my_http_start_with_config(NULL, NULL) == ESP_OK)
    {
        my_default_http_server_started = 1;
    }
    else
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t my_http_stop(httpd_handle_t *httpd_handle_ptr)
{
    httpd_handle_t tmp = NULL;
    if (httpd_handle_ptr && *httpd_handle_ptr)
    {
        tmp = *httpd_handle_ptr;
        *httpd_handle_ptr = NULL;
    }
    else if (my_http_server_handle)
    {
        tmp = my_http_server_handle;
        my_default_http_server_started = 0;
        my_http_server_handle = NULL;
        
    }
    else
    {
        return ESP_FAIL;
    }
    return httpd_stop(tmp);
}
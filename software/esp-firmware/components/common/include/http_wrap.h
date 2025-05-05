/**
 * http_wrap.h
 * 
 * Contains functions that wrap esp_http_client functions to allow mocking without
 * mocking the entire esp_http_client component separately.
 */

#ifndef HTTP_WRAP_H_4_26_25
#define HTTP_WRAP_H_4_26_25

#include "esp_err.h"
#include "esp_http_client.h"


esp_http_client_handle_t wrap_http_client_init(const esp_http_client_config_t *config);
esp_err_t wrap_http_client_open(esp_http_client_handle_t client, int write_len);
int wrap_http_client_read(esp_http_client_handle_t client, char *buffer, int len);
esp_err_t wrap_http_client_set_url(esp_http_client_handle_t client, const char *url);
int64_t wrap_http_client_fetch_headers(esp_http_client_handle_t client);
esp_err_t wrap_http_client_close(esp_http_client_handle_t client);
int wrap_http_client_get_status_code(esp_http_client_handle_t client);
esp_err_t wrap_http_client_flush_response(esp_http_client_handle_t client, int *len);
esp_err_t wrap_http_client_cleanup(esp_http_client_handle_t client);

#endif /* HTTP_WRAP_H_4_26_25 */
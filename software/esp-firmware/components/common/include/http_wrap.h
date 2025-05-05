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


esp_err_t wrap_http_client_open(esp_http_client_handle_t client, int write_len);
int wrap_http_client_read(esp_http_client_handle_t client, char *buffer, int len);

#endif /* HTTP_WRAP_H_4_26_25 */
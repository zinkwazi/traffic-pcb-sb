/**
 * api_connect.h
 * 
 * Contains functions for connecting to and retrieving data from the server.
 */

#ifndef API_CONNECT_H_
#define API_CONNECT_H_

#include <stdint.h>

#include "esp_http_client.h"
#include "esp_err.h"

#include "circular_buffer.h"
#include "main_types.h"

#define API_ERR_REMOVE_DATA 0x52713
#define MAX_URL_LEN 512 // URL lengths should not be longer than this. Use static_assert() to ensure this.

esp_err_t openServerFile(int64_t *contentLength, esp_http_client_handle_t client, const char *URL, int retryNum);
esp_err_t getServerSpeeds(LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client, char *URL, int retryNum);

#endif /* API_CONNECT_H_ */
/**
 * api_connect.h
 * 
 * Contains functions for connecting to and retrieving data from the server.
 */

#ifndef API_CONNECT_H_
#define API_CONNECT_H_

#include "circular_buffer.h"

#include "esp_http_client.h"
#include "esp_err.h"

#include <stdint.h>

#define API_ERR_REMOVE_DATA 0x52713

struct LEDData {
    uint32_t ledNum;
    uint32_t speed;
};

typedef struct LEDData LEDData;

esp_err_t getServerSpeeds(LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client, char *URL, int retryNum);

#endif /* API_CONNECT_H_ */
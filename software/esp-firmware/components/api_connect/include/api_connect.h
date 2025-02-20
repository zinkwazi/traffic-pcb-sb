/**
 * api_connect.h
 * 
 * Contains functions for connecting to and retrieving data from the server.
 */

#ifndef API_CONNECT_H_
#define API_CONNECT_H_

#include "esp_http_client.h"
#include "esp_err.h"

#include <stdint.h>

// TODO: use this struct for LED speeds instead of uint8_t
struct LEDData {
    uint32_t ledNum;
    uint32_t speed;
};

typedef struct LEDData LEDData;

esp_err_t tomtomGetServerSpeeds(uint8_t ledSpeeds[], int ledSpeedsLen, esp_http_client_handle_t client, char *URL, int retryNum);

#endif /* API_CONNECT_H_ */
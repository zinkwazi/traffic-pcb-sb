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

#define API_ERR_REMOVE_DATA 0x52713

struct LEDData {
    uint16_t ledNum;
    /* The speed of the LED, with negative values specifying special LED types.*/
    int8_t speed;
};

typedef struct LEDData LEDData;

esp_err_t getServerSpeeds(LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client, char *URL, int retryNum);
esp_err_t getServerSpeedsWithAddendums(LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client, char *fileURL, int retryNum);

#endif /* API_CONNECT_H_ */
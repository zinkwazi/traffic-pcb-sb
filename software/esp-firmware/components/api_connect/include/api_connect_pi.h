/**
 * api_connect_pi.h
 * 
 * test_api_connect.c testing features and access to api_connect.c 
 * internal functions for unit testing.
 */

#ifndef API_CONNECT_PI_H_
#define API_CONNECT_PI_H_
#ifndef CONFIG_DISABLE_TESTING_FEATURES

/* Include the public interface */
#include "api_connect.h"

#include "api_connect_config.h"

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "esp_http_client.h"

#include "circular_buffer.h"

/*******************************************/
/*            INTERNAL FUNCTIONS           */
/*******************************************/

esp_err_t nextCSVEntryFromMark(LEDData *data, CircularBuffer *circBuf, char *buf, uint32_t bufSize);
esp_err_t getNextResponseBlock(char *output, int *outputLen, esp_http_client_handle_t client);
esp_err_t readServerSpeedDataPreinit(CircularBuffer *circBuf, LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client);

#if USE_ADDENDUMS == true
esp_err_t getServerSpeedsWithAddendums(LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client, const char *fileURL, int retryNum);
esp_err_t parseMetadata(char **dataStart, char *block, int blockLen, char *metadata, int *metadataLen);
#else
esp_err_t getServerSpeedsNoAddendums(LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client, const char *URL, int retryNum);
esp_err_t readServerSpeedData(LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client);
#endif

/*******************************************/
/*            TESTING FEATURES             */
/*******************************************/

int getResponseBlockSize(void);
int getCircBufSize(void);

#endif /* CONFIG_DISABLE_TESTING_FEATURES */
#endif /* API_CONNECT_PI_H_ */
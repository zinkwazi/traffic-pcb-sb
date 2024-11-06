/**
 * tomtom.h
 */

#ifndef TOMTOM_H_
#define TOMTOM_H_

/* IDF component includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "lwip/sys.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

/* Tomtom component includes */
#include "api_config.h"

/* Public component interface */
enum Direction {
    NORTH,
    SOUTH,
};

typedef enum Direction Direction;

struct tomtomHttpHandlerParams {
    uint result;
    esp_err_t err;
    char *prevBuffer; /* an array of size RCV_BUFFER_SIZE */
};

esp_err_t establishWifiConnection(char *wifiSSID, char* wifiPass);
esp_http_client_handle_t tomtomCreateHttpHandle(struct tomtomHttpHandlerParams *storage);
esp_err_t tomtomDestroyHttpHandle(esp_http_client_handle_t tomtomHandle);
esp_err_t tomtomRequestSpeed(unsigned int *result, esp_http_client_handle_t tomtomHandle, struct tomtomHttpHandlerParams *storage, char *apiKey, unsigned short ledNum, Direction dir);

/* Private component functions */
esp_err_t tomtomRequestPerform(unsigned int *result, esp_http_client_handle_t tomtomHandle, struct tomtomHttpHandlerParams *storage, const char *url);
esp_err_t tomtomHandler(esp_http_client_event_t *evt);

#endif /* TOMTOM_H_ */
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

#define RCV_BUFFER_SIZE (20) // max: "\"currentSpeed\": 65\0"

/* Public component interface */
enum Direction {
    NORTH,
    SOUTH,
};

typedef enum Direction Direction;

struct tomtomHttpHandlerParams {
    uint result;
    esp_err_t err;
    char prevBuffer[RCV_BUFFER_SIZE];
};

struct tomtomClient {
    esp_http_client_handle_t httpHandle;
    char *apiKey;
    struct tomtomHttpHandlerParams handlerParams;
};

typedef struct tomtomClient tomtomClient;

esp_err_t tomtomInitClient(tomtomClient *client, char *apiKey);
esp_err_t tomtomDestroyClientHandle(tomtomClient *client);
esp_err_t tomtomRequestSpeed(unsigned int *result, tomtomClient *client, float longitude, float latitude, int retryNum);

/* Private component functions */
esp_err_t tomtomRequestPerform(unsigned int *result, tomtomClient *client, const char *url, int retryNum);
esp_err_t tomtomHandler(esp_http_client_event_t *evt);

#endif /* TOMTOM_H_ */
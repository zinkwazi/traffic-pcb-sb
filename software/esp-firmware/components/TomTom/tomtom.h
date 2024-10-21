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
#include "cJSON.h"

/* Tomtom component includes */
#include "api_config.h"

/* Public component interface */
enum Direction {
    NORTH,
    SOUTH,
};

typedef enum Direction Direction;


esp_err_t establishWifiConnection(void);
esp_http_client_handle_t tomtomCreateHttpHandle(void);
esp_err_t tomtomDestroyHttpHandle(esp_http_client_handle_t tomtomHandle);
esp_err_t tomtomRequestSpeed(uint *result, esp_http_client_handle_t tomtomHandle, uint16_t ledNum, Direction dir);

/* Private component functions */
esp_err_t tomtomRequestPerform(uint *result, esp_http_client_handle_t tomtomHandle, const char *url);
esp_err_t tomtomHandler(esp_http_client_event_t *evt);

/* static variables */
static int s_retryNum;
static EventGroupHandle_t s_wifiEventGroup;

/**
 * Resets tomtom static variables to their initial state.
 * I remember something from my class about this not
 * always happening when the reset button is pressed,
 * rather it always happens on a new flash. I'm not 
 * sure if this is necessary, but better to be safe.
 */
static inline void resetStaticVars(void) {
    s_retryNum = 0;
    s_wifiEventGroup = NULL;
}

#endif /* TOMTOM_H_ */
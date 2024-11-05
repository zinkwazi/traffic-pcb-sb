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

/**
 * Helper struct for tomtomHandler, which is used
 * to return the results of the function totomtomRequestPerform,
 * which recieves results through a void pointer.
 */
struct requestResult {
    uint result;
    esp_err_t error;
};

esp_err_t establishWifiConnection(char *wifiSSID, char* wifiPass);
esp_http_client_handle_t tomtomCreateHttpHandle(struct requestResult *storage);
esp_err_t tomtomDestroyHttpHandle(esp_http_client_handle_t tomtomHandle);
esp_err_t tomtomRequestSpeed(unsigned int *result, esp_http_client_handle_t tomtomHandle, struct requestResult *storage, char *apiKey, unsigned short ledNum, Direction dir);

/* Private component functions */
esp_err_t tomtomRequestPerform(unsigned int *result, esp_http_client_handle_t tomtomHandle, struct requestResult *storage, const char *url);
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
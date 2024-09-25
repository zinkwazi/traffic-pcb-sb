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

esp_err_t tomtomRequestSpeed(uint *result, unsigned short ledNum, Direction dir);
esp_err_t establishWifiConnection(void);
void helloWorldExample(void);

/* Private component functions */
esp_err_t tomtomRequestPerform(uint *result, const char *url);
esp_err_t tomtomHandler(esp_http_client_event_t *evt);

/* static variables */
static int s_retryNum;
static EventGroupHandle_t s_wifiEventGroup;

/**
 * Sets the tomtom API configuration to what is set
 * in 'api_config.h'. Configuration can manually be changed later.
 * Importantly, this causes s_tomtomConfig to be a SHALLOW
 * copy of s_defaultTomtomConfig.
 */
static inline void useDefaultTomtomConfig(void) {
    // s_tomtomConfig.url = s_defaultTomtomConfig.url;
    // s_tomtomConfig.auth_type = s_defaultTomtomConfig.auth_type;
    // s_tomtomConfig.method = s_defaultTomtomConfig.method;
    // s_tomtomConfig.event_handler = s_defaultTomtomConfig.event_handler;
    // s_tomtomConfig.buffer_size = s_defaultTomtomConfig.buffer_size;
}

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
    useDefaultTomtomConfig();
}

#endif /* TOMTOM_H_ */
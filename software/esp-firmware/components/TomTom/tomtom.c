/**
 * tomtom.c
 * 
 * This file is meant to hold functionality necessary for the tomtom
 * task, which retrieves information from the developer.tomtom.com
 * traffic API. Specifically, this task uses the traffic flow segment
 * data, which "provides information about the speeds and travel
 * times of the road fragment closest to the given coordinates".
 */

#include "tomtom.h"

/* IDF component includes */
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_check.h"
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
#include "led_locations.h"

#define TAG "TomTom"

#define DOUBLE_STR_SIZE (12) // max: -123.123456

/* The URL before the latitude of the point */
#define API_URL_PREFIX (API_ENDPOINT_URL API_STYLE "/10/json?key=" \
                API_KEY "&point=")

/* The URL between the latitude and longitude of the point */
#define API_URL_BETWEEN (",")

/* The URL after the longitude of the point */
#define API_URL_POSTFIX ("&unit=" \
                API_UNIT "&openLr=" API_SEND_OPENLR)

#define LAT_NDX (sizeof(API_URL_PREFIX) - 1)

#define LONG_NDX (LAT_NDX + DOUBLE_STR_SIZE - 1 + sizeof(API_URL_BETWEEN) - 1)

#define URL_LENGTH (LONG_NDX + \
                    DOUBLE_STR_SIZE - 1 + \
                    sizeof(API_URL_POSTFIX) - 1)

/**
 * Forms the proper TomTom request URL given an LED location and
 * a destination string of minimum size URL_LENGTH.
 * 
 * Requires: String is of minimum size URL_LENGTH.
 */
esp_err_t tomtomFormRequestURL(char *urlStr, const LEDLoc *led) {
    uint i;
    int lenDouble;
    /* input guards */
    ESP_RETURN_ON_FALSE(
        (urlStr != NULL && led != NULL), ESP_FAIL,
        TAG, "tomtomFormRequestURL given a NULL input"
    );
    /* begin forming request URL */
    strcpy(urlStr, API_URL_PREFIX);
    /* append latitude into string */
    lenDouble = snprintf(&urlStr[LAT_NDX], DOUBLE_STR_SIZE, "%lf", led->latitude);
    ESP_RETURN_ON_FALSE(
        (lenDouble > 0), ESP_FAIL,
        TAG, "failed to convert double to string"
    );
    ESP_RETURN_ON_FALSE(
        (lenDouble <= DOUBLE_STR_SIZE), ESP_FAIL,
        TAG, "expected shorter string size to represent double"
    );
    if (lenDouble < DOUBLE_STR_SIZE) {
        // pad the string with '0' to remove garbage starting from null terminator
        for (i = LAT_NDX + lenDouble; i < LAT_NDX + DOUBLE_STR_SIZE - 1; i++) {
            urlStr[i] = '0';
        }
        urlStr[LAT_NDX + DOUBLE_STR_SIZE - 1] = '\0'; // allows concatenation
    }
    strcat(urlStr, API_URL_BETWEEN);
    /* append longitude into string */
    lenDouble = snprintf(&urlStr[LONG_NDX], DOUBLE_STR_SIZE, "%lf", led->longitude);
    ESP_RETURN_ON_FALSE(
        (lenDouble > 0), ESP_FAIL,
        TAG, "failed to convert double to string"
    );
    ESP_RETURN_ON_FALSE(
        (lenDouble <= DOUBLE_STR_SIZE), ESP_FAIL,
        TAG, "expected shorter string size to represent double"
    );
    if (lenDouble < DOUBLE_STR_SIZE) {
        // pad the string with '0' to remove garbage starting from null terminator
        for (i = LAT_NDX + lenDouble; i < LAT_NDX + DOUBLE_STR_SIZE - 1; i++) {
            urlStr[i] = '0';
        }
        urlStr[LONG_NDX + DOUBLE_STR_SIZE - 1] = '\0'; // allows concatenation
    }
    strcat(urlStr, API_URL_POSTFIX);
    return ESP_OK;
}

const LEDLoc* getLED(unsigned short ledNum, Direction dir) {
    /* map led num 329 and 330 to reasonable numbers */ 
    ledNum = (ledNum == 329) ? 325 : ledNum;
    ledNum = (ledNum == 330) ? 326 : ledNum;
    /* input guards */
    if (ledNum < 1 || ledNum > 326) {
        ESP_LOGE("tomtom", "requested led location for invalid LED hardware number");
        return NULL;
    }
    /* get correct LED location */
    switch (dir) {
        case NORTH:
            return &northLEDLocs[ledNum - 1];
        case SOUTH:
            return &southLEDLocs[ledNum - 1];
        default:
            return NULL;
    }
}


/**
 * Performs a blocking Tomtom API request and returns the speed
 * associated with the hardware led number.
 */
esp_err_t tomtomRequestSpeed(uint *result, unsigned short ledNum, Direction dir) {
    char urlStr[URL_LENGTH];
    const LEDLoc *led;
    /* debug logging */
    ESP_LOGD(TAG, "tomtomRequestSpeed(%p,%u,%d)", result, ledNum, dir);
    /* input guards */
    ESP_RETURN_ON_FALSE(
        (result != NULL), ESP_FAIL,
        TAG, "tomtomRequestSpeed provided NULL result pointer"
    );
    led = getLED(ledNum, dir);
    ESP_RETURN_ON_FALSE(
        (led != NULL), ESP_FAIL,
        TAG, "tomtomRequestSpeed provided invalid led location"
    );
    /* create http URL and perform request */
    ESP_RETURN_ON_ERROR(
        tomtomFormRequestURL(urlStr, led),
        TAG, "failed to form request url"
    );
    ESP_RETURN_ON_ERROR(
        tomtomRequestPerform(result, urlStr),
        TAG, "failed to perform API request"
    );
    return ESP_OK;
}

/**
 * A struct to pass the result of tomtomHandler
 * to the calling function. 
 */
struct requestResult {
    uint result;
    esp_err_t error;
};

/**
 * Handler for recieving responses from the TomTom API.
 */
esp_err_t tomtomHandler(esp_http_client_event_t *evt)
{
    static char buffer[RCV_BUFFER_SIZE];
    static uint content_size;
    cJSON *json, *curr;
    struct requestResult *reqResult = (struct requestResult *) evt->user_data;
    /* input guards */
    ESP_RETURN_ON_FALSE(
        (reqResult != NULL), ESP_FAIL,
        TAG, "API handler called with NULL result pointer"
    );
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            memset(buffer, 0, RCV_BUFFER_SIZE);
            content_size = 0;
            break;
        case HTTP_EVENT_ON_DATA:
            if (evt->data != NULL) {
                memcpy(buffer + content_size, evt->data, evt->data_len);
                content_size += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            json = cJSON_Parse(buffer);
            if (json == NULL) {
                ESP_LOGE("tomtom", "request response could not be parsed as JSON");
                reqResult->error = ESP_FAIL;
                return ESP_FAIL;
            }
            json = json->child;
            for (curr = json->child; !cJSON_IsNull(curr); curr = curr->next) {
                if (strcmp(curr->string, "currentSpeed") != 0) {
                    continue;
                }
                if (!cJSON_IsNumber(curr)) {
                    ESP_LOGE("tomtom", "request response contains speed that is not a number");
                    reqResult->error = ESP_FAIL;
                    return ESP_FAIL;
                } else {
                    reqResult->error = ESP_OK;
                    reqResult->result = curr->valueint;
                }
                break;
            }
            break;
        default: 
            break;
    }
    fflush(stdout);
    return ESP_OK;
}

/**
 * A handler that recieves wifi connection events. See establishWifiConnection.
 */
static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retryNum < MAX_RETRY_WIFI_CONNECT) {
            esp_wifi_connect();
            s_retryNum++;
        } else {
            xEventGroupSetBits(s_wifiEventGroup, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retryNum = 0;
        xEventGroupSetBits(s_wifiEventGroup, WIFI_CONNECTED_BIT);
    }
}

/**
 * Establishes a wifi connection with the AP
 * specified by WIFI_SSID and WIFI_PASS in 'api_config.h'.
 * 
 * Requires:
 * - NVS initialized.
 * - TCP/IP stack initialized.
 * - default event loop created.
 * - default WIFI STA created (esp_netif_create_default_wifi_sta called).
 * - WIFI task started (esp_wifi_init called).
 */
esp_err_t establishWifiConnection(void)
{
    esp_event_handler_instance_t instanceAnyID, instanceAnyIP;
    EventBits_t bits;
    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .scan_method = WIFI_SCAN_METHOD,
            .threshold.authmode = WIFI_AUTH_MODE,
        },
    };
    /* debug logging */
    ESP_LOGD(TAG, "establishWifiConnection()");
    /* register wifi event handlers */
    s_wifiEventGroup = xEventGroupCreate();
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            connect_handler,
                                            NULL,
                                            &instanceAnyID),
        TAG, "Failed to register ANY_ID wifi event handler"
    );
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            connect_handler,
                                            NULL,
                                            &instanceAnyIP),
        TAG, "Failed to register ANY_IP wifi event handler"
    );
    /* attempt to connect to AP */
    ESP_RETURN_ON_ERROR(
        esp_wifi_set_mode(WIFI_MODE_STA),
        TAG, "failed to set wifi to STA mode"
    );
    ESP_RETURN_ON_ERROR(
        esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg),
        TAG, "Failed to set the wifi configuration with AP_SSID:%s and PASS:%s", 
        wifi_cfg.sta.ssid, wifi_cfg.sta.password
    );
    ESP_RETURN_ON_ERROR(
        esp_wifi_start(),
        TAG, "Failed to start wifi"
    );
    bits = xEventGroupWaitBits(s_wifiEventGroup,
                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                               pdFALSE,
                               pdFALSE,
                               portMAX_DELAY);
    ESP_RETURN_ON_FALSE(
        (bits & ~WIFI_FAIL_BIT), ESP_FAIL,
        TAG, "recieved wifi fail bit from default event group"
    );
    return ESP_OK;
}

/**
 * Performs an HTTP request with the parameters currently
 * stored in s_tomtomConfig, which has defaults that can
 * be configured in 'api_config.h'. Blocks while waiting
 * for a response from the server.
 * 
 * Returns:
 * ESP_OK if executed successfully and the segment speed
 * is stored in result. ESP_FAIL otherwise and result 
 * is left untouched.
 * 
 * Requires:
 * - WIFI connection (establishWifiConnection called).
 * - TLS initialized (esp_tls_init called).
 */
esp_err_t tomtomRequestPerform(uint *result, const char *url)
{
    struct requestResult reqResult = {
        .error = ESP_FAIL,
        .result = 0,
    };
    const esp_http_client_config_t s_defaultTomtomConfig = {
        .url = url,
        .auth_type = API_AUTH_TYPE,
        .method = API_METHOD,
        .event_handler = tomtomHandler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = &reqResult,
    };
    /* debug logging */
    ESP_LOGD(TAG, "tomtomRequestPerform(%p,%s)", result, url);
    /* input guards */
    ESP_RETURN_ON_FALSE(
        (result != NULL), ESP_FAIL,
        TAG, "tomtomRequestPerform called with NULL result pointer"
    );
    /* perform API request */
    esp_http_client_handle_t httpClientHandle = esp_http_client_init(&s_defaultTomtomConfig);
    ESP_RETURN_ON_FALSE(
        (httpClientHandle != NULL), ESP_FAIL,
        TAG, "failed to create http client handle"
    );
    ESP_RETURN_ON_ERROR(
        esp_http_client_perform(httpClientHandle), // blocks when requesting
        TAG, "failed to perform http request"
    );
    ESP_RETURN_ON_ERROR(
        esp_http_client_cleanup(httpClientHandle),
        TAG, "failed to cleanup http client handle"
    );
    ESP_RETURN_ON_FALSE(
        (reqResult.error == ESP_OK), ESP_FAIL,
        TAG, "recieved an error from the http client event handler"
    );
    *result = reqResult.result;
    return ESP_OK;
}
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
#include "esp_random.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

/* Tomtom component includes */
#include "api_config.h"
#include "led_locations.h"

#define TAG "TomTom"

#define RCV_BUFFER_SIZE (20) // max: "\"currentSpeed\": 65\0"
#define DOUBLE_STR_SIZE (12) // max: -123.123456
#define MAX_SPEED_SIZE (4) // allows for speeds of 999

/* The URL before the latitude of the point */
#define API_URL_PREFIX (API_ENDPOINT_URL API_STYLE "/10/json?key=")

#define API_URL_POINT ("&point=")

/* The URL between the latitude and longitude of the point */
#define API_URL_BETWEEN (",")

/* The URL after the longitude of the point */
#define API_URL_POSTFIX ("&unit=" \
                API_UNIT "&openLr=" API_SEND_OPENLR)

#define LAT_NDX(apiKey) (sizeof(API_URL_PREFIX) + sizeof(API_URL_POINT) + strlen(apiKey) - 1)

#define LONG_NDX(apiKey) (LAT_NDX(apiKey) + DOUBLE_STR_SIZE - 1 + sizeof(API_URL_BETWEEN) - 1)

#define URL_LENGTH(apiKey) (LONG_NDX(apiKey) + \
                    DOUBLE_STR_SIZE - 1 + \
                    sizeof(API_URL_POSTFIX))

/* Custom error codes */
#define TOMTOM_ERR_OFFSET 0xe000
#define TOMTOM_NO_SPEED -(TOMTOM_ERR_OFFSET + 1) // Defines that the function was unable to parse a speed

/**
 * Forms the proper TomTom request URL given an LED location and
 * a destination string of minimum size URL_LENGTH.
 * 
 * Requires: String is of minimum size URL_LENGTH.
 */
esp_err_t tomtomFormRequestURL(char *urlStr, char *apiKey, const LEDLoc *led) {
    int lenDouble;
    /* input guards */
    ESP_RETURN_ON_FALSE(
        (urlStr != NULL && led != NULL), ESP_FAIL,
        TAG, "tomtomFormRequestURL given a NULL input"
    );
    /* begin forming request URL */
    strcpy(urlStr, API_URL_PREFIX);
    strcat(urlStr, apiKey);
    strcat(urlStr, API_URL_POINT);
    /* append latitude into string */
    lenDouble = snprintf(&urlStr[strlen(urlStr)], DOUBLE_STR_SIZE, "%lf", led->latitude);
    ESP_RETURN_ON_FALSE(
        (lenDouble > 0), ESP_FAIL,
        TAG, "failed to convert double to string"
    );
    strcat(urlStr, API_URL_BETWEEN);
    /* append longitude into string */
    lenDouble = snprintf(&urlStr[strlen(urlStr)], DOUBLE_STR_SIZE, "%lf", led->longitude);
    ESP_RETURN_ON_FALSE(
        (lenDouble > 0), ESP_FAIL,
        TAG, "failed to convert double to string"
    );
    strcat(urlStr, API_URL_POSTFIX);
    return ESP_OK;
}

/**
 * Gets the physical coordinates of the road segment
 * corresponding to the LED designated by ledNum, which
 * is the hardware number in Kicad of the LED.
 * 
 * Parameters:
 *  - ledNum: The Kicad hardware number of the LED.
 *  - dir: The direction of travel of the road segment.
 * 
 * Returns: A pointer to a struct containing the longitude
 *          and latitude denoting the road segment. NULL if
 *          an error occurred.
 */
const LEDLoc* getLED(unsigned short ledNum, Direction dir) {
    const LEDLoc *ledLocs = NULL;
    /* map led num 329 and 330 to reasonable numbers */ 
    ledNum = (ledNum == 329) ? 325 : ledNum;
    ledNum = (ledNum == 330) ? 326 : ledNum;
    /* input guards */
    if (ledNum < 1 || ledNum > 326) {
        return NULL;
    }
    /* retrieve correct LED location array */
    switch (dir) {
        case NORTH:
            ledLocs = northLEDLocs;
            break;
        case SOUTH:
            ledLocs = southLEDLocs;
            break;
        default:
            return NULL;
    }
    /* get correct LED location */
    if (ledLocs[ledNum - 1].flowSpeed == 0) {
        return NULL;
    } else if (ledLocs[ledNum - 1].flowSpeed < 0) {
        /* flow speed is the negative of the correct LED hardware number */
        if (-(ledLocs[ledNum - 1].flowSpeed) <= 0) {
            return NULL;
        }
        return getLED(-(ledLocs[ledNum - 1].flowSpeed), dir);
    }
    return &(ledLocs[ledNum - 1]);
}

/**
 * Performs a blocking Tomtom API request and returns the speed
 * associated with the hardware led number and direction of travel.
 * 
 * Parameters:
 *  - result: A pointer to where the requested speed will be returned.
 *  - ledNum: The Kicad hardware number of the LED.
 *  - dir: The direction of travel of the road segment.
 * 
 * Returns: ESP_OK if successful.
 */
esp_err_t tomtomRequestSpeed(unsigned int *result, esp_http_client_handle_t tomtomHandle, struct requestResult *storage, char *apiKey, unsigned short ledNum, Direction dir) {
    char *urlStr = malloc(URL_LENGTH(apiKey));
    const LEDLoc *led;
    /* input guards */
    if (result == NULL) {
        return ESP_FAIL;
    }
    led = getLED(ledNum, dir);
    if (led == NULL) {
        return ESP_FAIL;
    }
    /* create http URL and perform request */
    ESP_RETURN_ON_ERROR(
        tomtomFormRequestURL(urlStr, apiKey, led),
        TAG, "failed to form request url"
    );
    ESP_RETURN_ON_ERROR(
        tomtomRequestPerform(result, tomtomHandle, storage, urlStr),
        TAG, "failed to perform API request"
    );
    free(urlStr);
    return ESP_OK;
}

/**
 * Parses the speed field denoted by 'currentSpeed' from the JSON
 * response provided in chunks through currBuffer. The function
 * should be called sequentially with chunks of the response because
 * the field may cross a chunk boundary, which this function detects.
 * 
 * This function must be called with currBuffer equal to NULL to denote
 * that a new response will be provided in subsequent calls to the function.
 * This allows the function to reset its internal state.
 * 
 * Parameters:
 *  - result: a pointer to where the parsed speed will be placed, if found.
 *  - currBuffer: the current chunk of the response.
 *  - currLen: the length of currBuffer.
 * 
 * Returns: ESP_OK if currBuffer != NULL and the speed is found and placed in 
 *          result successfully,
 *          ESP_OK if currBuffer == NULL and internal state is reset successfully,
 *          TOMTOM_NO_SPEED if no errors occurred but the speed was not found,
 *          ESP_FAIL if an error occurred.
 */
esp_err_t tomtomParseSpeed(unsigned int *result, char *chunk, unsigned int chunkLen) {
    /* This string is used to denote where the beginning
    of the target data is in the http response */
    static const char targetPrefix[] = "\"currentSpeed\":";
    /* This character denotes the end of the target data */
    static const char targetPostfix = ',';
    /* Stores the last bytes of the previous data event, which 
    will be used to determine if the beginning of the current 
    data event contains the speed data being targeted */
    static char prevBuffer[RCV_BUFFER_SIZE];
    /* Stores the retreived target speed characters from the response. */
    char speedBuffer[MAX_SPEED_SIZE];
    int dataNdx, speedNdx, prefixNdx;
    /* reset static variables if requested */
    if (chunk == NULL) {
        memset(prevBuffer, 0, RCV_BUFFER_SIZE);
        return ESP_OK;
    }
    /* input guards */
    if (result == NULL) {
        return ESP_FAIL;
    }
    /* iterate through previous data buffer to determine proper 
    prefix index to start at in the current data buffer */
    for (dataNdx = 0, prefixNdx = 0; dataNdx < strlen(prevBuffer) && prefixNdx < strlen(targetPrefix); dataNdx++, prefixNdx++) {
        if (prevBuffer[dataNdx] != targetPrefix[prefixNdx]) {
            if (prefixNdx > 0) {
                dataNdx--; // restart search at this index
            }
            prefixNdx = -1; // the data is incorrect, restart search
        }
    }
    if (prefixNdx == strlen(targetPrefix)) {
        /* prevDataBuffer contains targetPrefix and the remaining
        bytes are a portion of the target speed characters */
        for (speedNdx = 0; dataNdx < strlen(prevBuffer) && prevBuffer[dataNdx] != targetPostfix; dataNdx++, speedNdx++) {
            if (speedNdx >= strlen(speedBuffer)) {
                ESP_LOGE(TAG, "length of speed from http response was unexpectedly long");
                return ESP_FAIL;
            }
            speedBuffer[speedNdx] = prevBuffer[dataNdx];
        }
        speedBuffer[speedNdx] = '\0'; // complete string until it can be finished
    }
    /* iterate through current data buffer to find the 
    prefix location, starting at the prefixNdx determined
    by the search through the previous data buffer */
    for (dataNdx = 0; dataNdx < chunkLen && prefixNdx < strlen(targetPrefix); dataNdx++, prefixNdx++) {
        if (chunk[dataNdx] != targetPrefix[prefixNdx]) {
            if (prefixNdx > 0) {
                dataNdx--; // restart search at this index
            }
            prefixNdx = -1; // the data is incorrect, restart search
        }
    }
    if (prefixNdx == strlen(targetPrefix)) {
        /* the current data buffer contains targetPrefix, thus 
        extract the speed and return if targetPostfix is found */
        for (speedNdx = 0; dataNdx < chunkLen && chunk[dataNdx] != targetPostfix; speedNdx++, dataNdx++) {
            if (speedNdx >= MAX_SPEED_SIZE) {
                return ESP_FAIL;
            }
            speedBuffer[speedNdx] = chunk[dataNdx];
        }
        if (chunk[dataNdx] == targetPostfix) {
            speedBuffer[speedNdx] = '\0';
            *result = atoi(speedBuffer); // TODO: replace with a safe alternative to atoi
            return ESP_OK;
        }
    } else {
        /* the current data buffer does not contain targetPrefix */
        strncpy(prevBuffer, &(chunk[chunkLen - RCV_BUFFER_SIZE]), RCV_BUFFER_SIZE); // prepare for the next chunk
    }
    return TOMTOM_NO_SPEED;
}

struct connectHandlerEventData {
    int *retryNum;
    EventGroupHandle_t wifiEventGroup;
};

/**
 * A handler that recieves wifi connection events. See establishWifiConnection.
 */
static void connectHandler(void *arg, esp_event_base_t eventBase,
                            int32_t eventId, void* eventData)
{
    struct connectHandlerEventData *data = (struct connectHandlerEventData *) arg;
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        if (*(data->retryNum) < MAX_RETRY_WIFI_CONNECT) {
            esp_wifi_connect();
            (*(data->retryNum))++;
        } else {
            xEventGroupSetBits(data->wifiEventGroup, WIFI_FAIL_BIT);
        }
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        *(data->retryNum) = 0;
        xEventGroupSetBits(data->wifiEventGroup, WIFI_CONNECTED_BIT);
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
esp_err_t establishWifiConnection(char *wifiSSID, char* wifiPass)
{
    esp_err_t ret;
    esp_event_handler_instance_t instanceAnyID, instanceAnyIP;
    EventBits_t bits;
    wifi_config_t wifi_cfg = {
        .sta = {
            .scan_method = WIFI_SCAN_METHOD,
            .threshold.authmode = WIFI_AUTH_MODE,
        },
    };
    const unsigned int staSSIDLen = sizeof(wifi_cfg.sta.ssid) / sizeof(wifi_cfg.sta.ssid[0]);
    const unsigned int staPassLen = sizeof(wifi_cfg.sta.password) / sizeof(wifi_cfg.sta.password[0]);
    for (unsigned int i = 0; i < staSSIDLen; i++) {
        wifi_cfg.sta.ssid[i] = ((uint8_t *) wifiSSID)[i];
    }
    for (unsigned int i = 0; i < staPassLen; i++) {
        wifi_cfg.sta.password[i] = ((uint8_t *) wifiPass)[i];
    }
    /* register wifi event handlers */
    int retryNum = 0;
    struct connectHandlerEventData eventData = {
        .retryNum = &retryNum,
        .wifiEventGroup = xEventGroupCreate(),
    };
    if (eventData.wifiEventGroup == NULL) {
        return ESP_FAIL;
    }
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            connectHandler,
                                            &eventData,
                                            &instanceAnyID);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            connectHandler,
                                            &eventData,
                                            &instanceAnyIP);
    if (ret != ESP_OK) {
        return ret;
    }
    /* attempt to connect to AP */
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        return ret;
    }
    bits = xEventGroupWaitBits(eventData.wifiEventGroup,
                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                               pdFALSE,
                               pdFALSE,
                               portMAX_DELAY);
    free(eventData.wifiEventGroup);
    return (bits & ~WIFI_FAIL_BIT) ? ESP_OK : ESP_FAIL;
}

/**
 * Handler for recieving responses from the TomTom 
 * API. This function is invoked occasionally while 
 * tomtomRequestPerform is executing.
 */
esp_err_t tomtomHttpHandler(esp_http_client_event_t *evt)
{
    esp_err_t ret = TOMTOM_NO_SPEED;
    struct requestResult *reqResult = (struct requestResult *) evt->user_data;
    /* input guards */
    ESP_RETURN_ON_FALSE(
        (reqResult != NULL), ESP_FAIL,
        TAG, "http event handler called with NULL result pointer"
    );
    /* handle events */
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            reqResult->error = ESP_FAIL;
            ESP_RETURN_ON_ERROR(
                tomtomParseSpeed(NULL, NULL, 0), // indicate starting new response
                TAG, "could not reset tomtomParseSpeed internal state for new response"
            );
            break;
        case HTTP_EVENT_ON_DATA:
            ret = tomtomParseSpeed(&(reqResult->result), (char *) evt->data, evt->data_len);
            switch (ret) {
                case ESP_OK:
                    reqResult->error = ESP_OK;
                    break;
                case TOMTOM_NO_SPEED:
                    break;
                case ESP_FAIL:
                    ESP_LOGE(TAG, "failed to parse speed from http data chunk");
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * Establishes an initial HTTPS connection with 
 * the TomTom API server. The returned handle should
 * be reused via tomtomReconnect after a call to
 * tomtomDisconnect has been made. This is more
 * performant than two calls to tomtomConnect.
 * 
 * The returned handle must be cleaned up with 
 * esp_http_client_cleanup once it is known that the 
 * handle is closed and will not be reused.
 * 
 * Returns: A handle to the connection if 
 *          successful, which should be 
 *          provided to other TomTom functions.
 *          Otherwise, NULL if an error occurred.
 */
esp_http_client_handle_t tomtomCreateHttpHandle(struct requestResult *storage) {
    /* debug logging */
    ESP_LOGD(TAG, "tomtomCreateHttpHandle(%p)", storage);
    esp_http_client_config_t defaultTomTomConfig = {
        .host = "api.tomtom.com",
        .path = "/",
        .auth_type = API_AUTH_TYPE,
        .method = API_METHOD,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = tomtomHttpHandler,
        .user_data = storage,
    };
    return esp_http_client_init(&defaultTomTomConfig); // config is shared by copy
}

esp_err_t tomtomDestroyHttpHandle(esp_http_client_handle_t tomtomHandle) {
    return esp_http_client_cleanup(tomtomHandle);
}

/**
 * Performs an HTTP request to the url and attempts to
 * parse the 'currentSpeed' JSON field from the response. This
 * function blocks while waiting for the response to send.
 * 
 * Parameters:
 *  - result: A pointer to where the result of 
 *            the request will be placed.
 *  - url: The URL to request data from.
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
esp_err_t tomtomRequestPerform(unsigned int *result, esp_http_client_handle_t tomtomHandle, struct requestResult *storage, const char *url)
{
    /* input guards */
    if (result == NULL || tomtomHandle == NULL || url == NULL) {
        return ESP_FAIL;
    }
    /* update client handle with URL */
    ESP_RETURN_ON_ERROR(
        esp_http_client_set_url(tomtomHandle, url),
        TAG, "failed to set url of http client handle"
    );
    /* reset storage for request */
    storage->error = ESP_FAIL;
    storage->result = 0;
    /* perform API request */
#if CONFIG_USE_FAKE_DATA == true
    storage->error = ESP_OK;
    storage->result = esp_random() % 75;
    vTaskDelay(300 / portTICK_PERIOD_MS);
#else
    ESP_RETURN_ON_ERROR(
        esp_http_client_perform(tomtomHandle),
        TAG, "failed to make HTTPS request to TomTom"
    );
    ESP_RETURN_ON_FALSE(
        (esp_http_client_get_status_code(tomtomHandle) == 200), ESP_FAIL,
        TAG, "received bad status code from TomTom"
    );
#endif /* USE_FAKE_DATA */
    ESP_RETURN_ON_FALSE(
        (storage->error == ESP_OK), ESP_FAIL,
        TAG, "received an error code from tomtom http handler"
    );
    *result = storage->result;
    return ESP_OK;
}
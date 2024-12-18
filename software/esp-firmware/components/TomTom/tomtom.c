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

#define TAG "TomTom"


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

#define BUFFER_SIZE (2000)

/* The URL of server data (to be appended with version) */
#define URL_DATA_SERVER_NORTH (CONFIG_DATA_SERVER "/current_data/data_north_")
#define URL_DATA_SERVER_SOUTH (CONFIG_DATA_SERVER "/current_data/data_south_")

/* The file type of the server data */
#define URL_DATA_SERVER_TYPE (".json")

/* Custom error codes */
#define TOMTOM_ERR_OFFSET 0xe000
#define TOMTOM_NO_SPEED -(TOMTOM_ERR_OFFSET + 1) // Defines that the function was unable to parse a speed

/**
 * Forms the proper TomTom request URL given an LED location and
 * a destination string of minimum size URL_LENGTH.
 * 
 * Requires: String is of minimum size URL_LENGTH.
 */
esp_err_t tomtomFormRequestURL(char *urlStr, char *apiKey, float longitude, float latitude) {
    int lenDouble;
    /* input guards */
    if (urlStr == NULL) {
        return ESP_FAIL;
    }
    /* begin forming request URL */
    strcpy(urlStr, API_URL_PREFIX);
    strcat(urlStr, apiKey);
    strcat(urlStr, API_URL_POINT);
    /* append latitude into string */
    lenDouble = snprintf(&urlStr[strlen(urlStr)], DOUBLE_STR_SIZE, "%f", longitude);
    if (lenDouble <= 0) {
        return ESP_FAIL;
    }
    strcat(urlStr, API_URL_BETWEEN);
    /* append longitude into string */
    lenDouble = snprintf(&urlStr[strlen(urlStr)], DOUBLE_STR_SIZE, "%f", latitude);
    if (lenDouble <= 0) {
        return ESP_FAIL;
    }
    strcat(urlStr, API_URL_POSTFIX);
    return ESP_OK;
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
esp_err_t tomtomRequestSpeed(unsigned int *result, tomtomClient *client, float longitude, float latitude, int retryNum) {
    char *urlStr = malloc(URL_LENGTH(client->apiKey));
    /* input guards */
    if (result == NULL || client == NULL) {
        return ESP_FAIL;
    }
    /* create http URL and perform request */
    if (tomtomFormRequestURL(urlStr, client->apiKey, longitude, latitude) != ESP_OK) {
        return ESP_FAIL;
    }
    if (tomtomRequestPerform(result, client, urlStr, retryNum) != ESP_OK) {
        return ESP_FAIL;
    }
    free(urlStr);
    return ESP_OK;
}

void parseJSONIntArray(unsigned int speeds[], int speedsSize, char *json) {
    char *endNumPtr = json;
    int currNdx = 0;
    while (*json != '\0') {
        long int num = strtol(json, &endNumPtr, 10);
        if (endNumPtr - json == 0) {
            json++; // no number was found
        }
        speeds[currNdx] = num;
        currNdx++;
        if (currNdx == speedsSize) {
            return; // don't care about the rest of the array
        }
    }
}

esp_err_t tomtomGetServerSpeeds(uint8_t speeds[], int speedsSize, Direction dir, esp_http_client_handle_t client, char *version, int retryNum) {
    char urlStr[CONFIG_MAX_DATA_URL_LEN + 1];
    char *responseStr;
    /* construct url string */
    switch (dir) {
        case NORTH:
            strcpy(urlStr, URL_DATA_SERVER_NORTH);
            break;
        case SOUTH:
            strcpy(urlStr, URL_DATA_SERVER_SOUTH);
            break;
        default:
            return ESP_FAIL;
    }
    strcat(urlStr, version);
    strcat(urlStr, ".json");
    /* request data */
    if (esp_http_client_set_url(client, urlStr) != ESP_OK) {
        return ESP_FAIL;
    }
    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "failed to open connection");
        return ESP_FAIL;
    }
    int64_t contentLength = esp_http_client_fetch_headers(client);
    while (contentLength == -ESP_ERR_HTTP_EAGAIN) {
        contentLength = esp_http_client_fetch_headers(client);
    }
    if (contentLength <= 0) {
        return ESP_FAIL;
    }
    if (esp_http_client_get_status_code(client) != 200) {
        return ESP_FAIL;
    }
    responseStr = malloc(sizeof(char) * (contentLength + 100));
    if (responseStr == NULL) {
        return ESP_FAIL;
    }
    int len = esp_http_client_read(client, responseStr, contentLength);
    if (esp_http_client_close(client) != ESP_OK) {
        return ESP_FAIL;
    }
    for (int i = 0; i < contentLength && i < speedsSize; i++) {
        speeds[i] = (uint8_t) responseStr[i];
    }
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
 *  - prevBuffer: a buffer of size RCV_BUFFER_SIZE to hold the last bytes of the
 *                previous data event.
 * 
 * Returns: ESP_OK if currBuffer != NULL and the speed is found and placed in 
 *          result successfully,
 *          ESP_OK if currBuffer == NULL and internal state is reset successfully,
 *          TOMTOM_NO_SPEED if no errors occurred but the speed was not found,
 *          ESP_FAIL if an error occurred.
 */
esp_err_t tomtomParseSpeed(unsigned int *result, char *chunk, unsigned int chunkLen, char *prevBuffer) {
    /* This string is used to denote where the beginning
    of the target data is in the http response */
    static const char targetPrefix[] = "\"currentSpeed\":";
    /* This character denotes the end of the target data */
    static const char targetPostfix = ',';
    /* Stores the retreived target speed characters from the response. */
    char speedBuffer[MAX_SPEED_SIZE];
    int dataNdx, speedNdx, prefixNdx;
    /* input guards */
    if (result == NULL || chunk == NULL || prevBuffer == NULL) {
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

/**
 * Handler for recieving responses from the TomTom 
 * API. This function is invoked occasionally while 
 * tomtomRequestPerform is executing.
 */
esp_err_t tomtomHttpHandler(esp_http_client_event_t *evt)
{
    /* input guards */
    if (evt->user_data == NULL) {
        return ESP_FAIL;
    }
    /* get variables from parameters */
    esp_err_t ret = TOMTOM_NO_SPEED;
    uint *result = &((struct tomtomHttpHandlerParams *) evt->user_data)->result;
    esp_err_t *err = &((struct tomtomHttpHandlerParams *) evt->user_data)->err;
    char *prevBuffer = ((struct tomtomHttpHandlerParams *) evt->user_data)->prevBuffer;
    if (*err == ESP_OK) { // speed has already been found
        return ESP_OK;
    }
    /* handle events */
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ret = tomtomParseSpeed(result, (char *) evt->data, evt->data_len, prevBuffer);
            switch (ret) {
                case ESP_OK:
                    *err = ESP_OK;
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
 * Sets initial values necessary for proper execution
 * of TomTom functions and establishes an initial
 * HTTPS connection with the TomTom API server. The
 * returned handle should be reused, which is more
 * performant than multiple requests with different handles.
 * 
 * The returned handle must be cleaned up with tomtomCleanupClient
 * once it is known that the handle is closed and will not be reused.
 * 
 * Returns: ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t tomtomInitClient(tomtomClient *client, char *apiKey) {
    if (client == NULL || apiKey == NULL) {
        return ESP_FAIL;
    }
    esp_http_client_config_t defaultTomTomConfig = {
        .host = "api.tomtom.com",
        .path = "/",
        .auth_type = API_AUTH_TYPE,
        .method = API_METHOD,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = tomtomHttpHandler,
        .user_data = &(client->handlerParams),
    };
    client->httpHandle = esp_http_client_init(&defaultTomTomConfig); // config is shared by copy
    if (client->httpHandle == NULL) {
        return ESP_FAIL;
    }
    client->apiKey = apiKey;
    return ESP_OK;
}

/**
 * Closes resources allocated for the tomtomClientHandle.
 * 
 * Returns: ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t tomtomCleanupClient(tomtomClient *client) {
    return esp_http_client_cleanup(client->httpHandle);
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
esp_err_t tomtomRequestPerform(unsigned int *result, tomtomClient *client, const char *url, int retryNum)
{
    /* input guards */
    if (result == NULL || client == NULL || url == NULL) {
        return ESP_FAIL;
    }
    /* update client handle with URL */
    if (esp_http_client_set_url(client->httpHandle, url) != ESP_OK) {
        return ESP_FAIL;
    }
    /* reset storage for request */
    client->handlerParams.err = ESP_FAIL;
    client->handlerParams.result = 0;
    memset(client->handlerParams.prevBuffer, '\0', RCV_BUFFER_SIZE);
    /* perform API request */
#if CONFIG_USE_FAKE_DATA == true
    client->handlerParams.err = ESP_OK;
    client->handlerParams.result = esp_random() % 75;
    vTaskDelay(300 / portTICK_PERIOD_MS);
#else
    while (retryNum > 0) {
        if (esp_http_client_perform(client->httpHandle) == ESP_OK) {
            break;
        }
        if (retryNum == 1) {
            ESP_LOGE(TAG, "failed to perform http request");
        }
        retryNum--;
    }
    ESP_RETURN_ON_FALSE(
        (esp_http_client_get_status_code(client->httpHandle) == 200), ESP_FAIL,
        TAG, "received bad status code from TomTom"
    );
#endif /* USE_FAKE_DATA */
    ESP_RETURN_ON_FALSE(
        (client->handlerParams.err == ESP_OK), ESP_FAIL,
        TAG, "received an error code from tomtom http handler"
    );
    *result = client->handlerParams.result;
    return ESP_OK;
}
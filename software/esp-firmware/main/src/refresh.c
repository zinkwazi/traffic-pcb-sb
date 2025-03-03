/**
 * refresh.h
 * 
 * Contains function that handle refreshes of the LEDs.
 */

#include "refresh.h"

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_err.h"

#include "led_registers.h"
#include "api_connect.h"
#include "led_matrix.h"
#include "app_errors.h"
#include "animations.h"

#include "main_types.h"
#include "nvs_settings.h"
#include "utilities.h"

#define TAG "refresh"

/* If typical speed cannot be retrieved, default to this for all segments */
#define DEFAULT_TYPICAL_SPEED 70

/* LED color configuration */
#define SLOW_RED (0xFF)
#define SLOW_GREEN (0x00)
#define SLOW_BLUE (0x00)

#define MEDIUM_RED (0x15)
#define MEDIUM_GREEN (0x09)
#define MEDIUM_BLUE (0x00)

#define FAST_RED (0x00)
#define FAST_GREEN (0x00)
#define FAST_BLUE (0x10)

/* The URL of server data */
#define URL_DATA_FILE_TYPE ".csv"
#define URL_DATA_CURRENT_NORTH CONFIG_DATA_SERVER "/current_data/data_north_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define URL_DATA_CURRENT_SOUTH CONFIG_DATA_SERVER "/current_data/data_south_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define URL_DATA_TYPICAL_NORTH CONFIG_DATA_SERVER "/current_data/typical_north_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define URL_DATA_TYPICAL_SOUTH CONFIG_DATA_SERVER "/current_data/typical_south_" SERVER_VERSION_STR URL_DATA_FILE_TYPE

#define API_RETRY_CONN_NUM 5

void setColor(uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t percentFlow) {
    if (percentFlow < CONFIG_SLOW_CUTOFF_PERCENT) {
        *red = SLOW_RED;
        *green = SLOW_GREEN;
        *blue = SLOW_BLUE;
    } else if (percentFlow < CONFIG_MEDIUM_CUTOFF_PERCENT) {
        *red = MEDIUM_RED;
        *green = MEDIUM_GREEN;
        *blue = MEDIUM_BLUE;
    } else {
        *red = FAST_RED;
        *green = FAST_GREEN;
        *blue = FAST_BLUE;
    }
}

void updateLED(uint16_t ledNum, uint8_t percentFlow) {
    uint8_t red, green, blue;
    setColor(&red, &green, &blue, percentFlow);
    if (matSetColor(ledNum, red, green, blue) != ESP_OK ||
        matSetScaling(ledNum, 0xFF, 0xFF, 0xFF) != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to change led %d color", ledNum);
    }
}

/**
 * @brief Clears all LEDs sequentially in a particular direction.

 * 
 * @param dir  The direction that the LEDs will be cleared toward.
 */
void clearLEDs(Direction dir) {
    switch (dir) {
      case NORTH:
        ESP_LOGI(TAG, "Clearing North...");
        for (int ndx = MAX_NUM_LEDS_REG; ndx > 0; ndx--) {
            if (matSetColor(ndx, 0x00, 0x00, 0x00) != ESP_OK) {
                ESP_LOGE(TAG, "failed to change led %d color", ndx);
            }
            vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_CLEAR_PERIOD));
        }
        break;
      case SOUTH:
        ESP_LOGI(TAG, "Clearing South...");
        for (int ndx = 1; ndx < MAX_NUM_LEDS_REG + 1; ndx++) {
            if (matSetColor(ndx, 0x00, 0x00, 0x00) != ESP_OK) {
                ESP_LOGE(TAG, "failed to change led %d color", ndx);
            }
            vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_CLEAR_PERIOD));
        }
        break;
      default:
        break;
    }
}

bool mustAbort(void) {
    uint32_t notificationValue;
    return xTaskNotifyWait(0, 0, &notificationValue, 0) == pdTRUE;
}

esp_err_t refreshTypicalSpeeds(esp_http_client_handle_t client, LEDData typicalNorth[static MAX_NUM_LEDS_REG], LEDData typicalSouth[MAX_NUM_LEDS_REG]) {
    esp_err_t err;
    /* input guards */
    if (typicalSouth == NULL || typicalNorth == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* load default data */
    for (int i = 0; i < MAX_NUM_LEDS_REG; i++) {
        typicalNorth[i].ledNum = i + 1;
        typicalNorth[i].speed = DEFAULT_TYPICAL_SPEED;
        typicalSouth[i].ledNum = i + 1;
        typicalSouth[i].speed = DEFAULT_TYPICAL_SPEED;
    }
    /* retrieve Northbound data from servers */
    err = getServerSpeedsWithAddendums(typicalNorth, 
                                       MAX_NUM_LEDS_REG, 
                                       client, 
                                       URL_DATA_TYPICAL_NORTH, 
                                       API_RETRY_CONN_NUM);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to retrieve typical northbound speeds from server, searching non-volatile storage");
        err = getSpeedsFromNvs(typicalNorth, MAX_NUM_LEDS_REG, NORTH, false);
    } else {
        err = setSpeedsToNvs(typicalNorth, MAX_NUM_LEDS_REG, NORTH, false);
    }
    if (err != ESP_OK) {
        return err;
    }
    /* retrieve Southbound data from servers */
    err = getServerSpeedsWithAddendums(typicalSouth, 
                                       MAX_NUM_LEDS_REG, 
                                       client, 
                                       URL_DATA_TYPICAL_NORTH, 
                                       API_RETRY_CONN_NUM);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to retrieve typical northbound speeds from server, searching non-volatile storage");
        err = getSpeedsFromNvs(typicalSouth, MAX_NUM_LEDS_REG, SOUTH, false);
    } else {
        err = setSpeedsToNvs(typicalSouth, MAX_NUM_LEDS_REG, SOUTH, false);
    }
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

/**
 * @brief Refreshes the board, following the order given by ledOrder.
 * 
 * @param[in] ledOrder An array specifying the order of LED numbers to refresh.
 * @param[in] currSpeeds The current speeds to display on the board, where index
 *        (i - 1) corresponds to LED number i.
 * @param[in] typicalSpeeds The typical speeds of LEDs, where index (i - 1)
 *        corresponds to LED number i.
 * 
 * @returns ESP_OK if successful.
 *          REFRESH_ABORT if a task notification is received during operation.
 *          ESP_ERR_INVALID_ARG if an argument is NULL.
 */
esp_err_t refreshBoard(int32_t ledOrder[static MAX_NUM_LEDS_REG], LEDData currSpeeds[static MAX_NUM_LEDS_REG], LEDData typicalSpeeds[static MAX_NUM_LEDS_REG]) {
    /* input guards */
    if (ledOrder == NULL ||
        currSpeeds == NULL ||
        typicalSpeeds == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    /* clear board using provided ordering */
    for (int ndx = 1; ndx < MAX_NUM_LEDS_REG; ndx++) {
        int ledNum = ledOrder[ndx];
        if (matSetColor(ledNum, 0x00, 0x00, 0x00) == ESP_FAIL) {
            ESP_LOGW(TAG, "failed to clear LED %d", ledNum);
        }
    }
    /* update LEDs using provided ordering */
    for (int ndx = 1; ndx < MAX_NUM_LEDS_REG; ndx++) {
        int ledNum = ledOrder[ndx];
        if (ledNum >= MAX_NUM_LEDS_REG || typicalSpeeds[ledNum - 1].speed <= 0) {
            ESP_LOGW(TAG, "skipping LED %d update due to lack of typical speed", currSpeeds[ledNum - 1].ledNum);
            continue;
        }
        if (ledNum != currSpeeds[ledNum - 1].ledNum) {
            ESP_LOGW(TAG, "skipping bad index %d, with LED num %u", ledNum, currSpeeds[ledNum - 1].ledNum);
            continue;
        }
        if (ledNum != typicalSpeeds[ledNum - 1].ledNum) {
            ESP_LOGW(TAG, "skipping bad index %d, with typical LED num %u", ledNum, typicalSpeeds[ledNum - 1].ledNum);
            continue;
        }
        if (currSpeeds[ledNum - 1].speed < 0) {
            ESP_LOGW(TAG, "skipping led %u for led speed %d", currSpeeds[ledNum - 1].ledNum, currSpeeds[ledNum - 1].speed);
            continue;
        }
        uint32_t percentFlow = (100 * currSpeeds[ledNum - 1].speed) / typicalSpeeds[ledNum - 1].speed;
        updateLED(currSpeeds[ledNum - 1].ledNum, percentFlow);
        if (mustAbort()) {
            return REFRESH_ABORT;
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_UPDATE_PERIOD));
    }
    return ESP_OK;
}

esp_err_t handleRefresh(Direction dir, LEDData typicalSpeeds[static MAX_NUM_LEDS_REG], esp_http_client_handle_t client, ErrorResources *errRes) {
    static LEDData currSpeeds[MAX_NUM_LEDS_REG];
    esp_err_t err = ESP_OK;
    /* connect to API and query speeds */
    char *URL = (dir == NORTH) ? URL_DATA_CURRENT_NORTH : URL_DATA_CURRENT_SOUTH; 
    if (getServerSpeedsWithAddendums(currSpeeds, MAX_NUM_LEDS_REG, client, URL, API_RETRY_CONN_NUM) != ESP_OK)
    {
        /* failed to get typical north speeds from server, search nvs */
        ESP_LOGW(TAG, "failed to retrieve segment speeds from server");
        if (getSpeedsFromNvs(currSpeeds, MAX_NUM_LEDS_REG, dir, true) != ESP_OK)
        {
            throwFatalError(errRes, false);
        }
        throwNoConnError(errRes, false);
        return CONNECT_ERROR;
    } else {
        /* successfully retrieved current segment speeds from server */
        resolveNoConnError(errRes, true, false);
        ESP_LOGI(TAG, "updating segment speeds in non-volatile storage");
        if (setSpeedsToNvs(currSpeeds, MAX_NUM_LEDS_REG, dir, true) != ESP_OK) {
            ESP_LOGW(TAG, "failed to update segment speeds in non-volatile storage");
        }
    }
    ESP_LOGI(TAG, "ordering LEDs");
    int32_t ledArr[MAX_NUM_LEDS_REG];
    err = sortLEDsByDistParabolicMap(ledArr, MAX_NUM_LEDS_REG);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "failed to sort LEDs");
        return ESP_FAIL;
    }
    switch (dir) {
        case NORTH:
            break;
        case SOUTH:
            /* reverse ordering */
            for (int i = 0; i < MAX_NUM_LEDS_REG / 2; i++) {
                int32_t temp = ledArr[i];
                ledArr[i] = ledArr[MAX_NUM_LEDS_REG - i - 1];
                ledArr[MAX_NUM_LEDS_REG - i - 1] = temp;
            }
            break;
        default:
            ESP_LOGE(TAG, "received non-north/south direction");
            throwFatalError(errRes, false);
            break;
    }
    ESP_LOGI(TAG, "refreshing board");
    err = refreshBoard(ledArr, currSpeeds, typicalSpeeds);
    if (err == REFRESH_ABORT) {
        ESP_LOGI(TAG, "Quick Clearing...");
        if (matReset() != ESP_OK ||
            matSetGlobalCurrentControl(CONFIG_GLOBAL_LED_CURRENT) != ESP_OK ||
            matSetOperatingMode(NORMAL_OPERATION) != ESP_OK) 
        {
            ESP_LOGE(TAG, "failed to quick clear matrices");
        }
    }
    return err;
}
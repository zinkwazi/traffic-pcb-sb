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
    ESP_LOGI(TAG, "rgb: %u %u %u", red, green, blue);
    if (matSetColor(ledNum, red, green, blue) != ESP_OK ||
        matSetScaling(ledNum, 0xFF, 0xFF, 0xFF) != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to change led %d color", ledNum);
    }
}

/**
 * @brief Returns a pointer to the correct URL.
 * 
 * @returns A pointer to the correct URL if successful. NULL if invalid argument.
 */
char *getCorrectURL(Direction dir, SpeedCategory category) {
    char *url = NULL;
    switch (dir)
    {
      case NORTH:
        switch(category)
        {
          case LIVE:
            url = URL_DATA_CURRENT_NORTH;
            break;
          case TYPICAL:
            url = URL_DATA_TYPICAL_NORTH;
            break;
          default:
            return NULL;
        }
        break;
      case SOUTH:
      switch(category)
      {
        case LIVE:
          url = URL_DATA_CURRENT_SOUTH;
          break;
        case TYPICAL:
          url = URL_DATA_TYPICAL_SOUTH;
          break;
        default:
          return NULL;
      }
      break;
    }
    return url;
}

/**
 * @brief Clears all LEDs sequentially in a particular direction.

 * 
 * @param dir  The direction that the LEDs will be cleared toward.
 */
void clearBoard(Direction dir) {
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
        for (int ndx = 1; ndx <= MAX_NUM_LEDS_REG; ndx++) {
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

esp_err_t quickClearBoard(void)
{
    esp_err_t err;
    /* restart matrices */
    ESP_LOGI(TAG, "Quick clearing matrices");
    err = matReset();
    if (err != ESP_OK)
    {
        return err;
    }
    err = matSetGlobalCurrentControl(CONFIG_GLOBAL_LED_CURRENT);
    if (err != ESP_OK)
    {
        return err;
    }
    err = matSetOperatingMode(NORMAL_OPERATION);
    return err;
}

/**
 * @brief Updates the data stored in the provided array by querying it
 *        from the server, falling back to retrieving it from non-volatile
 *        storage if necessary.
 * 
 * @note If data is successfully retrieved from the server, the retrieved data
 *       is stored in NVS.
 * 
 * @param[out] data The destination of the retrieved data.
 * @param[in] client The HTTP client to use when retrieving data. Can be NULL,
 *        in which case only non-volatile storage will be retrieved.
 * @param[in] dir The direction of data to retrieve.
 * @param[in] category The category of data to retrieve.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid argument.
 *          ESP_FAIL if something unexpected occurred.
 */
esp_err_t refreshData(LEDData data[static MAX_NUM_LEDS_REG + 1], esp_http_client_handle_t client, Direction dir, SpeedCategory category, ErrorResources *errRes)
{
    esp_err_t err;
    char *url;
    /* input guards */
    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    /* retrieve NVS data if necessary */
    if (client == NULL)
    {
        return refreshSpeedsFromNVS(data, dir, category);
    }
    /* retrieve data from server, fallback to NVS */
    url = getCorrectURL(dir, category);
    if (url == NULL) {
        return ESP_FAIL;
    }
    err = getServerSpeedsWithAddendums(data, 
                                        MAX_NUM_LEDS_REG + 1, 
                                        client, 
                                        url, 
                                        API_RETRY_CONN_NUM);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "failed to retrieve typical speeds from server, searching non-volatile storage");
        throwNoConnError(errRes, false);
        return refreshSpeedsFromNVS(data, dir, category);
    }
    /* store new data in NVS */
    resolveNoConnError(errRes, true, false);
    err = storeSpeedsToNVS(data, dir, category);
    return err;
}

bool mustAbort(void) {
    uint32_t notificationValue;
    return xTaskNotifyWait(0, 0, &notificationValue, 0) == pdTRUE;
}

/**
 * @brief Refreshes the board following the given data and animation.

 * @param[in] currSpeeds The current speeds to display on the board, where index
 *        (i - 1) corresponds to LED number i.
 * @param[in] typicalSpeeds The typical speeds of LEDs, where index (i - 1)
 *        corresponds to LED number i.
 * @param[in] anim The animation to refresh the board using.
 * 
 * @returns ESP_OK if successful.
 *          REFRESH_ABORT if a task notification is received during operation.
 *          ESP_ERR_INVALID_ARG if an argument is NULL.
 */
esp_err_t refreshBoard(LEDData currSpeeds[static MAX_NUM_LEDS_REG + 1], LEDData typicalSpeeds[static MAX_NUM_LEDS_REG + 1], Animation anim) {
    int32_t ledOrder[MAX_NUM_LEDS_REG + 1];
    esp_err_t err;
    /* input guards */
    if (currSpeeds == NULL ||
        typicalSpeeds == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "refreshing board");
    /* generate correct ordering */
    err = orderLEDs(ledOrder, anim);
    if (err != ESP_OK)
    {
        return err;
    }
    /* clear board using provided ordering */
    for (int ndx = 0; ndx < MAX_NUM_LEDS_REG + 1; ndx++) {
        int ledNum = ledOrder[ndx];
        if (matSetColor(ledNum, 0x00, 0x00, 0x00) == ESP_FAIL) {
            ESP_LOGW(TAG, "failed to clear LED %d", ledNum);
        }
    }
    /* update LEDs using provided ordering */
    for (int ndx = 0; ndx < MAX_NUM_LEDS_REG + 1; ndx++) {
        int ledNum = ledOrder[ndx];
        if (ledNum > MAX_NUM_LEDS_REG || ledNum <= 0) {
            ESP_LOGW(TAG, "skipping out of bounds LED %d", ledNum);
            continue;
        }
        if (typicalSpeeds[ledNum - 1].speed <= 0) {
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
        if (currSpeeds[ledNum - 1].speed <= 0) {
            ESP_LOGW(TAG, "skipping led %u for led speed %d", currSpeeds[ledNum - 1].ledNum, currSpeeds[ledNum - 1].speed);
            continue;
        }
        ESP_LOGI(TAG, "updating LED: %d, speed: %d", currSpeeds[ledNum - 1].ledNum, currSpeeds[ledNum - 1].speed);
        uint32_t percentFlow = (100 * currSpeeds[ledNum - 1].speed) / typicalSpeeds[ledNum - 1].speed;
        updateLED(currSpeeds[ledNum - 1].ledNum, percentFlow);
        if (mustAbort()) {
            return REFRESH_ABORT;
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_UPDATE_PERIOD));
    }
    return ESP_OK;
}
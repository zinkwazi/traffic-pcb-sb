/**
 * refresh.c
 * 
 * Contains functionality for refreshing all of the traffic and direction LEDs
 * on the board.
 */

#include "refresh.h"
#include "refresh_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_mac.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "wrap_esp_http_client.h"

#include "animations.h"
#include "api_connect.h"
#include "app_err.h"
#include "app_errors.h"
#include "app_nvs.h"
#include "input.h"
#include "led_coordinates.h"
#include "led_matrix.h"
#include "led_registers.h"
#include "pinout.h"
#include "main_types.h"
#include "strobe.h"
#include "utilities.h"

#include "refresh_task.h"
#include "traffic_data.h"

#define TAG "refresh"

#if CONFIG_HARDWARE_VERSION == 1

// no static variables here

#elif CONFIG_HARDWARE_VERSION == 2

#define NUM_NO_REFRESH_LEDS 11

/* The LEDs that are not refreshed, which are indicator LEDs */
// TODO: have a way for typical data in refresh_task to handle this
// static const int32_t noRefreshNums[NUM_NO_REFRESH_LEDS] = {WIFI_LED_NUM,
//                                                         ERROR_LED_NUM,
//                                                         OTA_LED_NUM,
//                                                         NORTH_LED_NUM,
//                                                         SOUTH_LED_NUM,
//                                                         EAST_LED_NUM,
//                                                         WEST_LED_NUM,
//                                                         LIGHT_LED_NUM,
//                                                         MEDIUM_LED_NUM,
//                                                         HEAVY_LED_NUM,
//                                                         46, // 46 does not exist for V2_0
//                                                         };

#else
#error "Unsupported hardware version!"
#endif

static char *getCorrectURL(Direction dir, SpeedCategory category);

/**
 * @brief Initializes refresh functionality, including initialization of
 * data from the server or non-volatile storage.
 * 
 * @requires:
 * - app_errors component initialized.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * Other error codes.
 */
esp_err_t initRefresh(void)
{
    LEDData northData[MAX_NUM_LEDS_REG];
    LEDData southData[MAX_NUM_LEDS_REG];
    esp_http_client_handle_t client;
    esp_err_t err;

    /* input guards */
    if (getAppErrorsStatus() != ESP_OK) THROW_ERR(ESP_ERR_INVALID_STATE);

    /* initialize static traffic data and resources */
    err = initTrafficData();
    if (err != ESP_OK) return err;

    /* query typical data from server, falling back to nvs if necessary */
    client = initHttpClient();
    if (client == NULL) return ESP_FAIL;
    err = refreshData(northData, client, NORTH, TYPICAL);
    if (err != ESP_OK) return err;
    err = refreshData(southData, client, SOUTH, TYPICAL);
    if (err != ESP_OK) return err;

    /* update typical static traffic data */
    err = borrowTrafficData(TYPICAL, portMAX_DELAY);
    if (err != ESP_OK) return err;
    err = updateTrafficData(northData, MAX_NUM_LEDS_REG, NORTH, TYPICAL);
    if (err != ESP_OK) return err;
    err = updateTrafficData(southData, MAX_NUM_LEDS_REG, SOUTH, TYPICAL);
    if (err != ESP_OK) return err;
    err= releaseTrafficData(TYPICAL);
    if (err != ESP_OK) return err;

    /* query current data from server, falling back to nvs if necessary */
    client = initHttpClient();
    if (client == NULL) return ESP_FAIL;
    err = refreshData(northData, client, NORTH, LIVE);
    if (err != ESP_OK) return err;
    err = refreshData(southData, client, SOUTH, LIVE);
    if (err != ESP_OK) return err;

    /* update current static traffic data */
    err = borrowTrafficData(LIVE, portMAX_DELAY);
    if (err != ESP_OK) return err;
    err = updateTrafficData(northData, MAX_NUM_LEDS_REG, NORTH, LIVE);
    if (err != ESP_OK) return err;
    err = updateTrafficData(southData, MAX_NUM_LEDS_REG, SOUTH, LIVE);
    if (err != ESP_OK) return err;
    err= releaseTrafficData(LIVE);
    if (err != ESP_OK) return err;

    return ESP_OK;
}

/**
 * @brief Initializes an http client to the server.
 *
 * @note Returned client must have a call to esp_http_client_cleanup after use.
 *
 * @returns A handle to the initialized client if successful,
 *          otherwise NULL.
 */
esp_http_client_handle_t initHttpClient(void)
{
    esp_http_client_config_t httpConfig = {
        .host = CONFIG_DATA_SERVER,
        .path = "/",
        .auth_type = API_AUTH_TYPE,
        .method = API_METHOD,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = NULL,
        .user_data = NULL,
    };

    return ESP_HTTP_CLIENT_INIT(&httpConfig);
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
esp_err_t refreshData(LEDData data[], esp_http_client_handle_t client, Direction dir, SpeedCategory category)
{
    esp_err_t err;
    char *url;
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
    err = getServerSpeeds(data, 
                        MAX_NUM_LEDS_REG, 
                        client, 
                        url,
                        API_RETRY_CONN_NUM);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "searching nvs for data");
        throwNoConnError();
        return refreshSpeedsFromNVS(data, dir, category);
    }
    resolveNoConnError(true);
    return err;
}

/**
 * @brief Queues a refresh of the board following the animation with the most
 * recently updated traffic data available.
 * 
 * @param[in] dir The direction of traffic data set to be displayed.
 * @param[in] anim The animation to refresh the board with.
 *
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if anim is invalid.
 * ESP_ERR_INVALID_STATE if traffic data or the refresh task is not
 * initialized.
 * ESP_ERR_TIMEOUT if no LED frame is available to reserve or the refresh
 * task's command queue is full.
 * ESP_ERR_NOT_ALLOWED if traffic data could not be released.
 * ESP_FAIL if dir is invalid or an unexpected error occurred.
 */
esp_err_t refreshBoard(Direction dir, Animation anim) {
    LEDData currentSpeeds[MAX_NUM_LEDS_REG];
    LEDSpeed (*ledFrame)[MAX_FRAME_SIZE];
    int32_t ledOrder[MAX_NUM_LEDS_REG];
    esp_err_t err, err2;
    uint32_t ledFrameNdx;

    /* generate correct ordering */
    err = orderLEDs(ledOrder, MAX_NUM_LEDS_REG, anim, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
    if (err != ESP_OK) return err;

    /* copy current static traffic data */
    err = borrowTrafficData(LIVE, portMAX_DELAY);
    if (err != ESP_OK) return err;
    err = copyTrafficData(currentSpeeds, MAX_NUM_LEDS_REG, dir, LIVE);
    if (err != ESP_OK)
    {
        err2 = releaseTrafficData(LIVE);
        if (err2 != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to release live traffic data!");
        }
        return err;
    }
    err = releaseTrafficData(LIVE);
    if (err != ESP_OK) return err;

    /* create an LED frame using the provided ordering */
    uint32_t ledFrameLen = 0;
    err = refreshTaskReserveFrame(&ledFrame, &ledFrameNdx, 0);
    if (ESP_OK != err) return err;
    for (int32_t i = 0; i < MAX_NUM_LEDS_REG; i++)
    {
        int32_t ledNum = ledOrder[i];
        /* check for failure conditions */
        if (ledNum > MAX_NUM_LEDS_REG || ledNum <= 0)
        {
            ESP_LOGW(TAG, "skipping out of bounds LED %d", ledNum);
            continue;
        }
        if (ledNum != currentSpeeds[ledNum - 1].ledNum)
        {
            ESP_LOGW(TAG, "skipping bad index %d, with LED num %u", ledNum, currentSpeeds[ledNum - 1].ledNum);
            continue;
        }
        if (currentSpeeds[ledNum - 1].speed < 0)
        {
            ESP_LOGW(TAG, "skipping led %u for led speed %d", currentSpeeds[ledNum - 1].ledNum, currentSpeeds[ledNum - 1].speed);
            continue;
        }

        /* add LED to frame */
        (*ledFrame)[ledFrameLen].ledNum = ledNum;
        (*ledFrame)[ledFrameLen].speed = currentSpeeds[ledNum - 1].speed;
        ledFrameLen++;
    }

    /* refresh LEDs using frame */
    err = refreshTaskRefreshLEDs(ledFrameNdx, ledFrameLen, dir, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to refresh LEDs using frame %d!", ledFrameNdx);
        err2 = refreshTaskReleaseFrame(ledFrameNdx, portMAX_DELAY);
        if (err2 != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to release frame %d!", ledFrameNdx);
        }
        return err;
    }

    return ESP_OK;
}

/**
 * @brief Sends the most recently updated typical traffic data for a
 * direction to the refresh task, which needs typical data for both
 * directions before it will display any live data.
 *
 * @note LEDs with missing typical data (a negative speed or a mismatched
 * LED number) are given a typical speed of 0, which the refresh task
 * treats as always flowing freely.
 *
 * @param[in] dir The direction of typical traffic data to send.
 * @param[in] ticksToWait The maximum number of ticks to block for while
 * waiting for refresh task resources.
 *
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if traffic data or the refresh task is not
 * initialized.
 * ESP_ERR_TIMEOUT if no LED frame is available to reserve or the refresh
 * task's command queue is full.
 * ESP_ERR_NOT_ALLOWED if traffic data could not be released.
 * ESP_FAIL if dir is invalid or an unexpected error occurred.
 */
esp_err_t refreshTypical(Direction dir, TickType_t ticksToWait)
{
    LEDData typicalSpeeds[MAX_NUM_LEDS_REG];
    LEDSpeed (*ledFrame)[MAX_FRAME_SIZE];
    uint32_t ledFrameNdx;
    esp_err_t err, err2;

    /* copy typical static traffic data */
    err = borrowTrafficData(TYPICAL, portMAX_DELAY);
    if (err != ESP_OK) return err;
    err = copyTrafficData(typicalSpeeds, MAX_NUM_LEDS_REG, dir, TYPICAL);
    err2 = releaseTrafficData(TYPICAL);
    if (err != ESP_OK) return err;
    if (err2 != ESP_OK) return err2;

    /* typical frames require frame[n].ledNum == n for n in [0, MAX_NUM_LEDS_REG] */
    err = refreshTaskReserveFrame(&ledFrame, &ledFrameNdx, ticksToWait);
    if (err != ESP_OK) return err;
    (*ledFrame)[0].ledNum = 0; // LED numbers start at 1
    (*ledFrame)[0].speed = 0;
    for (uint16_t ledNum = 1; ledNum <= MAX_NUM_LEDS_REG; ledNum++)
    {
        const LEDData typical = typicalSpeeds[ledNum - 1];
        (*ledFrame)[ledNum].ledNum = ledNum;
        if (typical.ledNum != ledNum || typical.speed < 0)
        {
            (*ledFrame)[ledNum].speed = 0;
        } else
        {
            (*ledFrame)[ledNum].speed = (uint8_t) typical.speed;
        }
    }

    err = refreshTaskUpdateTypical(ledFrameNdx, MAX_NUM_LEDS_REG + 1, dir, ticksToWait);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to update typical speeds using frame %lu!", ledFrameNdx);
        err2 = refreshTaskReleaseFrame(ledFrameNdx, portMAX_DELAY);
        if (err2 != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to release frame %lu!", ledFrameNdx);
        }
        return err;
    }

    return ESP_OK;
}

/**
 * Enables night mode.
 * 
 * @note Night mode turns off traffic and direction indicator
 * LEDs until night mode is disabled.
 * 
 * @param ticksToWait The maximum number of ticks to block for.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_TIMEOUT if block time exceeded waiting for resources.
 */
esp_err_t refreshEnableNightMode(TickType_t ticksToWait)
{
    return refreshTaskEnableNightMode(ticksToWait);
}

/**
 * Disables night mode.
 * 
 * @note Night mode turns off traffic and direction indicator
 * LEDs until night mode is disabled.
 * 
 * @param ticksToWait The maximum number of ticks to block for.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_TIMEOUT if block time exceeded waiting for resources.
 */
esp_err_t refreshDisableNightMode(TickType_t ticksToWait)
{
    return refreshTaskDisableNightMode(ticksToWait);
}


/**
 * @brief Returns a pointer to the correct URL.
 * 
 * @returns A pointer to the correct URL if successful. NULL if invalid argument.
 */
static char *getCorrectURL(Direction dir, SpeedCategory category) {
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
        default:
            break;
    }
    return url;
}
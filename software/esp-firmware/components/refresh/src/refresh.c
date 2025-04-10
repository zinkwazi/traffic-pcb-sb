/**
 * refresh.c
 * 
 * Contains functionality for refreshing all of the traffic and direction LEDs
 * on the board.
 */

#include "refresh.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "animations.h"
#include "api_connect.h"
#include "app_errors.h"
#include "app_nvs.h"
#include "strobe.h"
#include "led_coordinates.h"
#include "led_matrix.h"
#include "led_registers.h"
#include "mat_err.h"
#include "pinout.h"
#include "main_types.h"
#include "utilities.h"

#include "traffic_data.h"

#define TAG "refresh"

/* The URL of server data */
#define URL_DATA_FILE_TYPE ".csv"
#define URL_DATA_CURRENT_NORTH CONFIG_DATA_SERVER "/current_data/data_north_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define URL_DATA_CURRENT_SOUTH CONFIG_DATA_SERVER "/current_data/data_south_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define URL_DATA_TYPICAL_NORTH CONFIG_DATA_SERVER "/current_data/typical_north_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define URL_DATA_TYPICAL_SOUTH CONFIG_DATA_SERVER "/current_data/typical_south_" SERVER_VERSION_STR URL_DATA_FILE_TYPE

/* TomTom HTTPS configuration */
#define API_METHOD HTTP_METHOD_GET
#define API_AUTH_TYPE HTTP_AUTH_TYPE_NONE

#define API_RETRY_CONN_NUM 5

#define MATRIX_RETRY_NUM 15

#define DEFAULT_SCALE (0xFF)
#define STROBE_LOW_SCALE (0x20)

#if CONFIG_HARDWARE_VERSION == 1

// no static variables here

#elif CONFIG_HARDWARE_VERSION == 2

#define NUM_NO_REFRESH_LEDS 11

static const int32_t noRefreshNums[NUM_NO_REFRESH_LEDS] = {WIFI_LED_NUM,
                                                        ERROR_LED_NUM,
                                                        OTA_LED_NUM,
                                                        NORTH_LED_NUM,
                                                        SOUTH_LED_NUM,
                                                        EAST_LED_NUM,
                                                        WEST_LED_NUM,
                                                        LIGHT_LED_NUM,
                                                        MEDIUM_LED_NUM,
                                                        HEAVY_LED_NUM,
                                                        46, // 46 does not exist for V2_0
                                                        }; 

#else
#error "Unsupported hardware version!"
#endif

static void setColor(uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t percentFlow);
static esp_err_t updateLED(uint16_t ledNum, uint8_t percentFlow, bool setScaling);
static char *getCorrectURL(Direction dir, SpeedCategory category);
static bool mustAbort(void);

#if CONFIG_HARDWARE_VERSION == 1

// no static functions here

#elif CONFIG_HARDWARE_VERSION == 2

static bool contains(const int32_t *arr, int32_t arrLen, int32_t ele);

#else
#error "Unsupported hardware version!"
#endif

/**
 * @brief Initializes refresh functionality, including initialization of
 * data from the server or non-volatile storage.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 */
esp_err_t initRefresh(ErrorResources *errRes)
{
    LEDData northData[MAX_NUM_LEDS_REG];
    LEDData southData[MAX_NUM_LEDS_REG];
    esp_http_client_handle_t client;
    esp_err_t err;

    /* input guards */
    if (errRes == NULL) return ESP_ERR_INVALID_ARG;
    if (errRes->errMutex == NULL) return ESP_ERR_INVALID_ARG;

    /* initialize static traffic data */
    err = initTrafficData();
    if (err != ESP_OK) return err;

    /* query typical data from server, falling back to nvs if necessary */
    client = initHttpClient();
    if (client == NULL) return ESP_FAIL;
    err = refreshData(northData, client, NORTH, TYPICAL, errRes);
    if (err != ESP_OK) return err;
    err = refreshData(southData, client, SOUTH, TYPICAL, errRes);
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
    err = refreshData(northData, client, NORTH, LIVE, errRes);
    if (err != ESP_OK) return err;
    err = refreshData(southData, client, SOUTH, LIVE, errRes);
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

    return esp_http_client_init(&httpConfig);
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
esp_err_t refreshData(LEDData data[static MAX_NUM_LEDS_REG], esp_http_client_handle_t client, Direction dir, SpeedCategory category, ErrorResources *errRes)
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
        throwNoConnError(errRes, false);
        return refreshSpeedsFromNVS(data, dir, category);
    }
    resolveNoConnError(errRes, true, false);
    return err;
}

/**
 * @brief Refreshes the board following the animation with the most recently
 * updated traffic data available.
 * 
 * @param[in] dir The direction of the traffic data set to be displayed.
 * @param[in] anim The animation to refresh the board using.
 * 
 * @returns ESP_OK if successful.
 * REFRESH_ABORT if a task notification is received during operation
 * and the board must be cleared.
 * REFRESH_ABORT_NO_CLEAR if a task notification is received before operation,
 * meaning no board clear is required.
 */
esp_err_t refreshBoard(Direction dir, Animation anim) {
    LEDData currentSpeeds[MAX_NUM_LEDS_REG];
    LEDData typicalSpeeds[MAX_NUM_LEDS_REG];
    int32_t ledOrder[MAX_NUM_LEDS_REG];
    esp_err_t err;
    /* check for a task notification */
    if (mustAbort())
    {
        return REFRESH_ABORT_NO_CLEAR;
    }

    /* generate correct ordering */
    err = orderLEDs(ledOrder, MAX_NUM_LEDS_REG, anim, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
    if (err != ESP_OK) return err;

    /* copy typical static traffic data */
    err = borrowTrafficData(TYPICAL, portMAX_DELAY);
    if (err != ESP_OK) return err;
    err = copyTrafficData(typicalSpeeds, MAX_NUM_LEDS_REG, dir, TYPICAL);
    err= releaseTrafficData(TYPICAL);
    if (err != ESP_OK) return err;

    /* copy current static traffic data */
    err = borrowTrafficData(LIVE, portMAX_DELAY);
    if (err != ESP_OK) return err;
    err = copyTrafficData(currentSpeeds, MAX_NUM_LEDS_REG, dir, LIVE);
    err= releaseTrafficData(LIVE);
    if (err != ESP_OK) return err;

    /* pause strobe queue to stop desync of newly registered strobe leds.
    The strobe task will take everything from the queue at once. */
    err = pauseStrobeRegisterLEDs(portMAX_DELAY);
    if (err != ESP_OK) return ESP_FAIL;

    /* update LEDs using provided ordering */
    TickType_t prevWake = xTaskGetTickCount();
    for (int ndx = 0; ndx < MAX_NUM_LEDS_REG; ndx++) {
        int ledNum = ledOrder[ndx];

        /* check for various failure conditions */
        if (ledNum > MAX_NUM_LEDS_REG || ledNum <= 0) {
            ESP_LOGW(TAG, "skipping out of bounds LED %d", ledNum);
            continue;
        }
        if (typicalSpeeds[ledNum - 1].speed <= 0) {
            ESP_LOGW(TAG, "skipping LED %d update due to lack of typical speed", currentSpeeds[ledNum - 1].ledNum);
            continue;
        }
        if (ledNum != currentSpeeds[ledNum - 1].ledNum) {
            ESP_LOGW(TAG, "skipping bad index %d, with LED num %u", ledNum, currentSpeeds[ledNum - 1].ledNum);
            continue;
        }
        if (ledNum != typicalSpeeds[ledNum - 1].ledNum) {
            ESP_LOGW(TAG, "skipping bad index %d, with typical LED num %u", ledNum, typicalSpeeds[ledNum - 1].ledNum);
            continue;
        }
        if (currentSpeeds[ledNum - 1].speed < 0) {
            ESP_LOGW(TAG, "skipping led %u for led speed %d", currentSpeeds[ledNum - 1].ledNum, currentSpeeds[ledNum - 1].speed);
            continue;
        }
        
        /* update LED */
        if (currentSpeeds[ledNum - 1].speed == 0)
        {
            /* register strobing for closed roads */
            /* manual set scale to initial strobe value, which will begin
            strobing after all LEDs are placed to avoid desynchronization
            between strobing LEDs. */
            (void) updateLED(ledNum, 0, true); // intentional best effort
            err = strobeRegisterLED(ledNum, DEFAULT_SCALE, STROBE_LOW_SCALE, DEFAULT_SCALE, false);
            vTaskDelay(pdMS_TO_TICKS(100)); // TEMPORARY: delay to force race condition to show
            if (err != ESP_OK) ESP_LOGW(TAG, "failed to register strobing on LED %d", ledNum);
        } else
        {
            /* update color */
            uint32_t percentFlow = (100 * currentSpeeds[ledNum - 1].speed) / typicalSpeeds[ledNum - 1].speed;
           (void) updateLED(ledNum, percentFlow, true); // intentional best effort
        }

        /* handle button presses and calculate time until next LED update */
        if (mustAbort()) {
            err = resumeStrobeRegisterLEDs(); // clear board takes care of newly registered strobe LEDs
            if (err != ESP_OK) return ESP_FAIL;
            return REFRESH_ABORT;
        }
        vTaskDelayUntil(&prevWake, pdMS_TO_TICKS(CONFIG_LED_UPDATE_PERIOD));
    }

    /* release strobe queue bc nothing else will be added */
    err = resumeStrobeRegisterLEDs();
    if (err != ESP_OK) return ESP_FAIL;

    return ESP_OK;
}

#if CONFIG_HARDWARE_VERSION == 1

/**
 * @brief Clears all LEDs sequentially in the opposite direction of that
 *        provided.
 * 
 * @param dir The direction that the LEDs will be cleared toward.
 * @param quick Whether to clear the board quickly, or to add a slight delay
 * between each LED, which causes an animation.
 * 
 * @returns ESP_OK if successful, otherwise I2C matrix issue.
 */
esp_err_t clearBoard(Direction dir, bool quick) {
    esp_err_t err;
    mat_err_t mat_err;
    TickType_t prevWake;
    int32_t ledOrder[MAX_NUM_LEDS_REG];

    /* remove all LED strobing registered by this task */
    err = strobeUnregisterAll();
    if (err != ESP_OK) return ESP_FAIL;

    
    switch (dir) {
    case NORTH:
        ESP_LOGI(TAG, "Clearing North...");
        err = orderLEDs(ledOrder, MAX_NUM_LEDS_REG, CURVED_LINE_NORTH_REVERSE, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
        if (err != ESP_OK) return err;

        prevWake = xTaskGetTickCount();
        for (int i = 0; i < MAX_NUM_LEDS_REG; i++) {
            int32_t ndx = ledOrder[i];
            
            for (int32_t i = 0; i < MATRIX_RETRY_NUM; i++)
            {
                mat_err = matSetColor(ndx, 0x00, 0x00, 0x00);
                if (mat_err == ESP_OK) break; 
            }
            if (mat_err != ESP_OK) {
                ESP_LOGE(TAG, "failed to set matrix color for led: %ld", ndx);
                return ESP_FAIL;
            }

            if (mustAbort())
            {
                return REFRESH_ABORT;
            }

            if (quick)
            {
                vTaskDelayUntil(&prevWake, pdMS_TO_TICKS(CONFIG_LED_CLEAR_PERIOD));
            }
        }
        break;
    case SOUTH:
        ESP_LOGI(TAG, "Clearing South...");
        err = orderLEDs(ledOrder, MAX_NUM_LEDS_REG, CURVED_LINE_SOUTH_REVERSE, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
        if (err != ESP_OK) return err;

        prevWake = xTaskGetTickCount();
        for (int i = 0; i < MAX_NUM_LEDS_REG; i++) {
            int32_t ndx = ledOrder[i];

            for (int32_t i = 0; i < MATRIX_RETRY_NUM; i++)
            {
                mat_err = matSetColor(ndx, 0x00, 0x00, 0x00);
                if (mat_err == ESP_OK) break;
            }
            if (mat_err != ESP_OK) {
                ESP_LOGE(TAG, "failed to set matrix color for led: %ld", ndx);
                return ESP_FAIL;
            }

            if (mustAbort())
            {
                return REFRESH_ABORT;
            }

            if (quick)
            {
                vTaskDelayUntil(&prevWake, pdMS_TO_TICKS(CONFIG_LED_CLEAR_PERIOD));
            }
            
        }
        break;
    default:
        break;
    }

    return ESP_OK;
}

/**
 * @brief Quickly sets all LEDs to off.
 * 
 * @note For V1_0, this function works by resetting each matrix.
 * 
 * @param[in] dir This is unused in V1_0.
 * 
 * @returns ESP_OK if successful
 */
esp_err_t quickClearBoard(__unused Direction dir)
{
    mat_err_t mat_err;
    esp_err_t err;

    /* remove all LED strobing registered by this task */
    err = strobeUnregisterAll();
    if (err != ESP_OK) return ESP_FAIL;

    /* restart matrices */
    ESP_LOGI(TAG, "Quick clearing matrices");

    for (int32_t i = 0; i < MATRIX_RETRY_NUM; i++)
    {
        mat_err = matReset();
        if (mat_err == ESP_OK) break;
    }
    if (mat_err != ESP_OK) return ESP_FAIL;

    for (int32_t i = 0; i < MATRIX_RETRY_NUM; i++)
    {
        mat_err = matSetGlobalCurrentControl(CONFIG_GLOBAL_LED_CURRENT);
        if (mat_err == ESP_OK) break;
    }
    if (mat_err != ESP_OK) return ESP_FAIL;

    for (int32_t i = 0; i < MATRIX_RETRY_NUM; i++)
    {
        mat_err = matSetOperatingMode(NORMAL_OPERATION);
        if (mat_err == ESP_OK) break;
    }
    if (mat_err != ESP_OK) return ESP_FAIL;
    
    return ESP_OK;
}

#elif CONFIG_HARDWARE_VERSION == 2

/**
 * @brief Clears all LEDs sequentially in the opposite direction of that
 *        provided.
 * 
 * @param dir The direction that the LEDs will be cleared toward.
 * @param quick Whether to clear LEDs quickly, or with a slight delay causing
 * a clearing animation.
 * 
 * @returns ESP_OK if successful.
 * REFRESH_ABORT if the toggle button is pressed, in which case the board should
 * be cleared by quickClearBoard.
 */
esp_err_t clearBoard(Direction dir, bool quick) {
    esp_err_t err;
    mat_err_t mat_err;
    TickType_t prevWake;
    int32_t ledOrder[MAX_NUM_LEDS_REG];

    /* remove all LED strobing registered by this task */
    err = strobeUnregisterAll();
    if (err != ESP_OK) return ESP_FAIL;

    /* set all LEDs to zero brightness, except for indicator LEDs */
    switch (dir) {
    case NORTH:
        ESP_LOGI(TAG, "Clearing North...");
        err = orderLEDs(ledOrder, MAX_NUM_LEDS_REG, CURVED_LINE_NORTH_REVERSE, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
        if (err != ESP_OK) return err;

        prevWake = xTaskGetTickCount();
        for (int i = 0; i < MAX_NUM_LEDS_REG; i++) {
            int32_t ndx = ledOrder[i];
            if (contains(noRefreshNums, NUM_NO_REFRESH_LEDS, ndx)) // only 7 elements (don't fret)
            {
                /* don't clear indicator LEDs */
                ESP_LOGW(TAG, "skipping clear of led %ld", ndx);
                continue;
            }
            
            for (int32_t i = 0; i < MATRIX_RETRY_NUM; i++)
            {
                mat_err = matSetColor(ndx, 0x00, 0x00, 0x00);
                if (mat_err == ESP_OK) break;
            }
            if (mat_err != ESP_OK) {
                ESP_LOGE(TAG, "failed to set matrix color for led: %ld", ndx);
                return ESP_FAIL;
            }

            if (mustAbort())
            {
                return REFRESH_ABORT;
            }

            if (!quick)
            {
                vTaskDelayUntil(&prevWake, pdMS_TO_TICKS(CONFIG_LED_CLEAR_PERIOD));
            }
            
        }
        break;
    case SOUTH:
        ESP_LOGI(TAG, "Clearing South...");
        err = orderLEDs(ledOrder, MAX_NUM_LEDS_REG, CURVED_LINE_SOUTH_REVERSE, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
        if (err != ESP_OK) return err;

        prevWake = xTaskGetTickCount();
        for (int i = 0; i < MAX_NUM_LEDS_REG; i++) {
            int32_t ndx = ledOrder[i];
            if (contains(noRefreshNums, NUM_NO_REFRESH_LEDS, ndx)) // only 7 elements (don't fret)
            {
                /* don't clear indicator LEDs or attempt those that don't exist */
                ESP_LOGW(TAG, "skipping clear of led %ld", ndx);
                continue;
            }

            for (int32_t i = 0; i < MATRIX_RETRY_NUM; i++)
            {
                mat_err = matSetColor(ndx, 0x00, 0x00, 0x00);
                if (mat_err == ESP_OK) break;
            }
            if (mat_err != ESP_OK) {
                ESP_LOGE(TAG, "failed to set matrix color for led: %ld", ndx);
                return ESP_FAIL;
            }

            if (mustAbort())
            {
                return REFRESH_ABORT;
            }

            if (!quick)
            {
                vTaskDelayUntil(&prevWake, pdMS_TO_TICKS(CONFIG_LED_CLEAR_PERIOD));
            }
        }
        break;
    default:
        break;
    }

    return ESP_OK;
}

/**
 * @brief Quickly sets all of the non-indicator LEDs to off.
 * 
 * @note For V2_0, this function works by manually clearing every LED. Resetting
 *       matrices is not used because that would turn off indicator LEDs.
 * 
 * @param[in] dir The direction to clear the board, which is relevant for
 * versions that have matrix LED indicator lights.
 * 
 * @returns ESP_OK always.
 */
esp_err_t quickClearBoard(Direction dir)
{
    return clearBoard(dir, true);
}

#else
#error "Unsupported hardware version!"
#endif

static bool mustAbort(void) {
    uint32_t notificationValue;
    return xTaskNotifyWait(0, 0, &notificationValue, 0) == pdTRUE; // should not consume notification
}

static void setColor(uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t percentFlow) {
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

static esp_err_t updateLED(uint16_t ledNum, uint8_t percentFlow, bool setScaling) {
    mat_err_t mat_err;
    uint8_t red, green, blue;

    /* determine and update color */
    setColor(&red, &green, &blue, percentFlow);
    for (int32_t i = 0; i < MATRIX_RETRY_NUM; i++)
    {
        mat_err = matSetColor(ledNum, red, green, blue);
        if (mat_err == ESP_OK) break;
    }
    if (mat_err != ESP_OK) return mat_err;
    if (!setScaling) return ESP_OK;

    /* set scaling if requested */
    for (int32_t i = 0; i < MATRIX_RETRY_NUM; i++)
    {
        mat_err = matSetScaling(ledNum, DEFAULT_SCALE, DEFAULT_SCALE, DEFAULT_SCALE);
        if (mat_err == ESP_OK) break;
    }
    if (mat_err != ESP_OK) return ESP_FAIL;    
    return ESP_OK;
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
    }
    return url;
}

#if CONFIG_HARDWARE_VERSION == 1

/* no static functions here */

#elif CONFIG_HARDWARE_VERSION == 2

/**
 * @brief Determines whether ele is in arr.
 * 
 * @param[in] arr The array to query.
 * @param[in] arrLen The size of arr.
 * @param[in] ele The element to query for in array.
 * 
 * @returns True if ele is in arr, otherwise false.
 */
static bool contains(const int32_t *arr, int32_t arrLen, int32_t ele)
{
    for (int32_t i = 0; i < arrLen; i++)
    {
        if (arr[i] == ele)
        {   
            return true;
        }
    }
    return false;
}

#else
#error "Unsupported hardware version!"
#endif
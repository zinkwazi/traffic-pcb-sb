/**
 * tasks.c
 * 
 * This file contains task functions
 * that allow the application to be efficient.
 */

#include <stdbool.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_https_ota.h"
#include "utilities.h"
#include "dots_commands.h"
#include "pinout.h"
#include "tasks.h"
#include "wifi.h"
#include "main_types.h"
#include "led_registers.h"

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
#define URL_DATA_FILE_TYPE ".dat"
#define URL_DATA_CURRENT_NORTH CONFIG_DATA_SERVER "/current_data/data_north_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define URL_DATA_CURRENT_SOUTH CONFIG_DATA_SERVER "/current_data/data_south_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define URL_DATA_TYPICAL_NORTH CONFIG_DATA_SERVER "/current_data/typical_north_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define URL_DATA_TYPICAL_SOUTH CONFIG_DATA_SERVER "/current_data/typical_south_" SERVER_VERSION_STR URL_DATA_FILE_TYPE

/* NVS namespace and keys */
#define WORKER_NVS_NAMESPACE "worker"
#define CURRENT_NORTH_NVS_KEY "current_north"
#define CURRENT_SOUTH_NVS_KEY "current_south"
#define TYPICAL_NORTH_NVS_KEY "typical_north"
#define TYPICAL_SOUTH_NVS_KEY "typical_south"

/* TomTom HTTPS configuration */
#define API_METHOD HTTP_METHOD_GET
#define API_AUTH_TYPE HTTP_AUTH_TYPE_NONE
#define API_RETRY_CONN_NUM 5

/* If typical speed cannot be retrieved, default to this for all segments */
#define DEFAULT_TYPICAL_SPEED 70

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

esp_err_t getSpeedsFromNvs(uint8_t *speeds, Direction dir, bool currentSpeeds) {
    nvs_handle_t nvsHandle;
    size_t size = MAX_NUM_LEDS;
    if (nvs_open(WORKER_NVS_NAMESPACE, NVS_READONLY, &nvsHandle) != ESP_OK) {
        return ESP_FAIL;
    }
    char *key = NULL;
    if (currentSpeeds) {
        key = (dir == NORTH) ? CURRENT_NORTH_NVS_KEY : CURRENT_SOUTH_NVS_KEY;
    } else {
        key = (dir == NORTH) ? TYPICAL_NORTH_NVS_KEY : TYPICAL_SOUTH_NVS_KEY;
    }
    if (nvs_get_blob(nvsHandle, key, speeds, &size) != ESP_OK) {
        return ESP_FAIL;
    }
    if (size == 0 || size / sizeof(uint8_t) != MAX_NUM_LEDS) {
        return ESP_FAIL;
    }
    nvs_close(nvsHandle);
    return ESP_OK;
}

esp_err_t setSpeedsToNvs(uint8_t *speeds, Direction dir, bool currentSpeeds) {
    nvs_handle_t nvsHandle;
    size_t size = MAX_NUM_LEDS * sizeof(uint8_t);
    esp_err_t err = ESP_OK;
    if ((err = nvs_open(WORKER_NVS_NAMESPACE, NVS_READWRITE, &nvsHandle)) != ESP_OK) {
        return ESP_FAIL;
    }
    char *key = NULL;
    if (currentSpeeds) {
        key = (dir == NORTH) ? CURRENT_NORTH_NVS_KEY : CURRENT_SOUTH_NVS_KEY;
    } else {
        key = (dir == NORTH) ? TYPICAL_NORTH_NVS_KEY : TYPICAL_SOUTH_NVS_KEY;
    }
    if (nvs_set_blob(nvsHandle, key, speeds, size) != ESP_OK) {
        return ESP_FAIL;
    }
    if (nvs_commit(nvsHandle) != ESP_OK) {
        return ESP_FAIL;
    }
    nvs_close(nvsHandle);
    return ESP_OK;
}

esp_err_t tomtomGetServerSpeeds(uint8_t speeds[], esp_http_client_handle_t client, char *URL, int retryNum) {
    char *responseStr;
    ESP_LOGD(TAG, "retrieving: %s", URL);
    /* request data */
    if (esp_http_client_set_url(client, URL) != ESP_OK) {
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
        ESP_LOGW(TAG, "contentLength <= 0");
        if (esp_http_client_close(client) != ESP_OK) {
            ESP_LOGE(TAG, "failed to close client");
        }
        return ESP_FAIL;
    }
    int status = esp_http_client_get_status_code(client);
    if (esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(TAG, "status code is %d", status);
        if (esp_http_client_close(client) != ESP_OK) {
            ESP_LOGE(TAG, "failed to close client");
        }
        return ESP_FAIL;
    }
    responseStr = malloc(sizeof(char) * contentLength);
    if (responseStr == NULL) {
        ESP_LOGE(TAG, "failed to allocate %lld bytes for http response", contentLength);
        if (esp_http_client_close(client) != ESP_OK) {
            ESP_LOGE(TAG, "failed to close client");
        }
        return ESP_FAIL;
    }
    int len = esp_http_client_read(client, responseStr, contentLength);
    while (len == -ESP_ERR_HTTP_EAGAIN) {
        len = esp_http_client_read(client, responseStr, contentLength);
    }
    if (esp_http_client_close(client) != ESP_OK) {
        ESP_LOGE(TAG, "failed to close client");
        free(responseStr);
        return ESP_FAIL;
    }
    if (len == -1) {
        ESP_LOGE(TAG, "esp_http_client_read returned -1");
        free(responseStr);
        return ESP_FAIL;
    }
    for (int i = 0; i < contentLength && i < MAX_NUM_LEDS; i++) {
        speeds[i] = (uint8_t) responseStr[i];
    }
    free(responseStr);
    return ESP_OK;
}

void updateLED(QueueHandle_t I2CQueue, uint16_t ledNum, uint8_t percentFlow) {
    uint8_t red, green, blue;
    setColor(&red, &green, &blue, percentFlow);
    if (dotsSetColor(I2CQueue, ledNum, red, green, blue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK ||
        dotsSetScaling(I2CQueue, ledNum, 0xFF, 0xFF, 0xFF, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to change led %d color", ledNum);
    }
}

bool mustAbort(QueueHandle_t I2CQueue, QueueHandle_t dotQueue) {
    WorkerCommand command;
    if (xQueuePeek(dotQueue, &command, 0) == pdTRUE) {
        /* A new command has been issued, quick clear and abort command */
        ESP_LOGD(TAG, "Quick Clearing...");
        if (dotsReset(I2CQueue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK ||
            dotsSetGlobalCurrentControl(I2CQueue, CONFIG_GLOBAL_LED_CURRENT, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK ||
            dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK) 
        {
            ESP_LOGE(TAG, "failed to reset dot matrices");
        }
        return true;
    }
    return false;
}

esp_err_t handleRefresh(bool *aborted, Direction dir, uint8_t typicalSpeeds[], QueueHandle_t I2CQueue, QueueHandle_t dotQueue, esp_http_client_handle_t client, ErrorResources *errRes, bool prevConnError) {
    static uint8_t speeds[MAX_NUM_LEDS];
    esp_err_t ret = ESP_OK;
    *aborted = false;
    /* connect to API and query speeds */
    char *URL = (dir == NORTH) ? URL_DATA_CURRENT_NORTH : URL_DATA_CURRENT_SOUTH; 
    if (tomtomGetServerSpeeds(speeds, client, URL, API_RETRY_CONN_NUM) != ESP_OK)
    {
        /* failed to get typical north speeds from server, search nvs */
        ESP_LOGW(TAG, "failed to retrieve segment speeds from server");
        if (getSpeedsFromNvs(speeds, dir, true) != ESP_OK)
        {
            throwFatalError(errRes, false);
        }
        if (!prevConnError) {
            throwNoConnError(errRes, false);
        }
        ret = ESP_FAIL;
    } else {
        /* successfully retrieved current segment speeds from server */
        if (prevConnError) {
            resolveNoConnError(errRes, false, false);
        }
        ESP_LOGD(TAG, "updating segment speeds in non-volatile storage");
        if (setSpeedsToNvs(speeds, dir, true) != ESP_OK) {
            ESP_LOGW(TAG, "failed to update segment speeds in non-volatile storage");
        }
    }

    switch (dir) {
        case NORTH:
            for (int ndx = MAX_NUM_LEDS - 1; ndx > 0; ndx--) {
                if (ndx > MAX_NUM_LEDS || typicalSpeeds[ndx] == 0) {
                    ESP_LOGW(TAG, "skipping LED %d update due to lack of typical speed", ndx);
                    continue;
                }
                uint8_t percentFlow = (100 * speeds[ndx]) / typicalSpeeds[ndx];
                updateLED(I2CQueue, ndx, percentFlow);
                if (mustAbort(I2CQueue, dotQueue)) {
                    *aborted = true;
                    return ret;
                }
                vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_UPDATE_PERIOD));
            }
            break;
        case SOUTH:
            for (int ndx = 1; ndx < MAX_NUM_LEDS; ndx++) {
                if (ndx > MAX_NUM_LEDS || typicalSpeeds[ndx] == 0) {
                    ESP_LOGW(TAG, "skipping LED %d update due to lack of typical speed", ndx);
                    continue;
                }
                uint8_t percentFlow = (100 * speeds[ndx]) / typicalSpeeds[ndx];
                updateLED(I2CQueue, ndx, percentFlow);
                if (mustAbort(I2CQueue, dotQueue)) {
                    *aborted = true;
                    return ret;
                }
                vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_UPDATE_PERIOD));
            }
            break;
    }
    return ret;
}

/**
 * @brief Initializes the worker task, which is implemented by vWorkerTask.
 * 
 * @param handle A pointer to a handle which will refer to the created task
 *               if successful.
 * @param dotQueue A handle to a queue that holds struct DotCommand 
 *                 objects. This task retrieves commands from this 
 *                 queue and performs work to fulfill them.
 * @param I2CQueue A handle to a queue that holds struct I2CCommand 
 *                 objects. This task issues commands to this queue 
 *                 to be handled by the I2C gatekeeper, implemented 
 *                 by vI2CGatekeeperTask.
 * @param errRes A pointer to global error handling resources.
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t createWorkerTask(TaskHandle_t *handle, QueueHandle_t dotQueue, QueueHandle_t I2CQueue, ErrorResources *errRes) {
  static WorkerTaskResources taskResources;
  BaseType_t success;
  /* input guards */
  if (dotQueue == NULL ||
      I2CQueue == NULL ||
      errRes == NULL ||
      errRes->errMutex == NULL)
  {
    return ESP_FAIL;
  }
  /* copy resources */
  taskResources.dotQueue = dotQueue;
  taskResources.I2CQueue = I2CQueue;
  taskResources.errRes = errRes;
  /* create task */
  success = xTaskCreate(vWorkerTask, "worker", CONFIG_WORKER_STACK, 
                        &taskResources, CONFIG_WORKER_PRIO, handle);
  return (success == pdPASS) ? ESP_OK : ESP_FAIL;
}

esp_err_t removeExtraWorkerNvsEntries(void) {
  esp_err_t ret;
  nvs_iterator_t nvs_iter;
  nvs_handle_t nvsHandle;
  if (nvs_open(WORKER_NVS_NAMESPACE, NVS_READWRITE, &nvsHandle) != ESP_OK) {
    return ESP_FAIL;
  }
  esp_err_t err = nvs_entry_find_in_handle(nvsHandle, NVS_TYPE_ANY, &nvs_iter);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    return ESP_OK; // no entries to remove
  } else if (err != ESP_OK) {
    return ESP_FAIL;
  }
  ret = nvs_entry_next(&nvs_iter);
  while (ret != ESP_OK) {
    nvs_entry_info_t info;
    if (nvs_entry_info(nvs_iter, &info) != ESP_OK) {
      return ESP_FAIL;
    }
    if (strcmp(info.namespace_name, WORKER_NVS_NAMESPACE) == 0 &&
            (strcmp(info.key, CURRENT_NORTH_NVS_KEY) == 0 ||
             strcmp(info.key, CURRENT_SOUTH_NVS_KEY) == 0 ||
             strcmp(info.key, TYPICAL_NORTH_NVS_KEY) == 0 ||
             strcmp(info.key, TYPICAL_SOUTH_NVS_KEY) == 0))
    {
      ret = nvs_entry_next(&nvs_iter);
      continue;
    }
    ESP_LOGD(TAG, "removing nvs entry: %s", info.key);
    if (nvs_erase_key(nvsHandle, info.key) != ESP_OK) {
      return ESP_FAIL;
    }
    ret = nvs_entry_next(&nvs_iter);
  }
  if (nvs_commit(nvsHandle) != ESP_OK) {
    return ESP_FAIL;
  }
  if (ret == ESP_ERR_INVALID_ARG) {
    return ESP_FAIL;
  }
  return ESP_OK;
}


/**
 * @brief Implements the worker task, which is responsible for handling
 *        commands of type WorkerCommand sent from the main task.
 * 
 * @note The worker task receives commands from the main task. It is the task 
 *       that does the most 'business logic' of the application. It relieves 
 *       the main task of these duties so that it can quickly respond to user 
 *       input.
 * 
 * @param pvParameters A pointer to a WorkerTaskResources object which
 *                     should remain valid through the lifetime of the task.
 */
void vWorkerTask(void *pvParameters) {
    esp_http_client_config_t httpConfig = {
        .host = CONFIG_DATA_SERVER,
        .path = "/",
        .auth_type = API_AUTH_TYPE,
        .method = API_METHOD,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = NULL,
        .user_data = NULL,
    };

    ESP_LOGD(TAG, "worker task created");

    WorkerTaskResources *res = (WorkerTaskResources *) pvParameters;
    esp_http_client_handle_t client;
    WorkerCommand dot;

    client = esp_http_client_init(&httpConfig);
    if (client == NULL) {
        throwFatalError(res->errRes, false);
    }

    /* remove extra non-volatile storage entries from partition */
    if (removeExtraWorkerNvsEntries() != ESP_OK) {
        throwFatalError(res->errRes, false);
    }

    /* retrieve typical speeds from the server */
    static uint8_t typicalSpeedsNorth[MAX_NUM_LEDS];
    static uint8_t typicalSpeedsSouth[MAX_NUM_LEDS];
    for (int i = 0; i < MAX_NUM_LEDS; i++) {
        typicalSpeedsNorth[i] = DEFAULT_TYPICAL_SPEED;
        typicalSpeedsSouth[i] = DEFAULT_TYPICAL_SPEED;
    }
    if (tomtomGetServerSpeeds(typicalSpeedsNorth, client, 
                              URL_DATA_TYPICAL_NORTH, 
                              API_RETRY_CONN_NUM) != ESP_OK) 
    {
        /* failed to get typical north speeds from server, search nvs */
        ESP_LOGW(TAG, "failed to retrieve typical northbound speeds from server, searching non-volatile storage");
        if (esp_http_client_cleanup(client) != ESP_OK ||
            (client = esp_http_client_init(&httpConfig)) == NULL)
        {
            throwFatalError(res->errRes, false);
        }
        getSpeedsFromNvs(typicalSpeedsNorth, NORTH, false); // don't care if this fails
    } else {
        ESP_LOGD(TAG, "setting typical north speeds in non-volatile storage");
        if (setSpeedsToNvs(typicalSpeedsNorth, NORTH, false) != ESP_OK) {
            ESP_LOGW(TAG, "failed to set typical speeds in non-volatile storage");
        }
    }
    if (tomtomGetServerSpeeds(typicalSpeedsSouth, client,
                              URL_DATA_TYPICAL_SOUTH,
                              API_RETRY_CONN_NUM) != ESP_OK) 
    {
        /* failed to get typical north speeds from server, search nvs */
        ESP_LOGW(TAG, "failed to retrieve typical southbound speeds from server, searching non-volatile storage");
        if (esp_http_client_cleanup(client) != ESP_OK ||
            (client = esp_http_client_init(&httpConfig)) == NULL)
        {
            throwFatalError(res->errRes, false);
        }
        getSpeedsFromNvs(typicalSpeedsSouth, SOUTH, false); // don't care if this fails
    } else {
        ESP_LOGD(TAG, "setting typical south speeds in non-volatile storage");
        if (setSpeedsToNvs(typicalSpeedsNorth, SOUTH, false) != ESP_OK) {
            ESP_LOGW(TAG, "failed to set typical speeds in non-volatile storage");
        }
    }

    /* Wait for commands and execute them forever */
    bool prevCommandAborted = false;
    bool connError = false;
    for (;;) {  // This task should never end
        if (ulTaskNotifyTake(pdTRUE, 0) == 1) {
            /* recieved an error from I2C gatekeeper */
            ESP_LOGW(TAG, "received an error from the I2C gatekeeper");
        }
        /* wait for a command on the queue */
        while (xQueueReceive(res->dotQueue, &dot, INT_MAX) == pdFALSE) {}
        /* update led colors */
        switch (dot.type) {
            case REFRESH_NORTH:
                ESP_LOGD(TAG, "Refreshing North...");
                if (handleRefresh(&prevCommandAborted, NORTH, typicalSpeedsNorth, res->I2CQueue, res->dotQueue, client, res->errRes, connError) != ESP_OK) {
                    esp_http_client_cleanup(client);
                    connError = true;
                    client = esp_http_client_init(&httpConfig);
                } else {
                    connError = false;
                }
                break;
            case REFRESH_SOUTH:
                ESP_LOGD(TAG, "Refreshing South...");
                if (handleRefresh(&prevCommandAborted, SOUTH, typicalSpeedsSouth, res->I2CQueue, res->dotQueue, client, res->errRes, connError) != ESP_OK) {
                    esp_http_client_cleanup(client);
                    connError = true;
                    client = esp_http_client_init(&httpConfig);
                } else {
                    connError = false;
                }
                break;
            case CLEAR_NORTH:
                if (prevCommandAborted) {
                    break;
                }
                ESP_LOGD(TAG, "Clearing North...");
                for (int ndx = MAX_NUM_LEDS; ndx > 0; ndx--) {
                    if (dotsSetColor(res->I2CQueue, ndx, 0x00, 0x00, 0x00, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK) {
                        ESP_LOGE(TAG, "failed to change led %d color", ndx);
                    }
                    vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_CLEAR_PERIOD));
                }
                prevCommandAborted = false;
                break;
            case CLEAR_SOUTH:
                if (prevCommandAborted) {
                    break;
                }
                ESP_LOGD(TAG, "Clearing South...");
                for (int ndx = 1; ndx < MAX_NUM_LEDS + 1; ndx++) {
                    if (dotsSetColor(res->I2CQueue, ndx, 0x00, 0x00, 0x00, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK) {
                        ESP_LOGE(TAG, "failed to change led %d color", ndx);
                    }
                    vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_CLEAR_PERIOD));
                }
                prevCommandAborted = false;
                break;
            case QUICK_CLEAR:
                ESP_LOGD(TAG, "Quick Clearing...");
                if (dotsReset(res->I2CQueue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK ||
                    dotsSetGlobalCurrentControl(res->I2CQueue, CONFIG_GLOBAL_LED_CURRENT, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK ||
                    dotsSetOperatingMode(res->I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK) 
                {
                    ESP_LOGE(TAG, "failed to reset dot matrices");
                }
                prevCommandAborted = false;
                break;
            default:
                break;
        }
        
    }
    ESP_LOGE(TAG, "dot worker task is exiting! This should be impossible!");
    esp_http_client_cleanup(client);
    vTaskDelete(NULL); // exit safely (should never happen)
}

/**
 * @brief Initializes the over-the-air (OTA) task, which is implemented by
 *        vOTATask.
 * 
 * @note This function creates shallow copies of parameters that will be 
 *       provided to the task in static memory. It assumes that only one of 
 *       this type of task will be created; any additional tasks will have 
 *       pointers to the same location in static memory.
 * 
 * @param handle A pointer to a handle which will refer to the created task
 *               if successful.
 * @param errorResources A pointer to an ErrorResources object. A deep copy
 *                       of the object will be created in static memory.
 *                       
 * @returns ESP_OK if the task was created successfully, otherwise ESP_FAIL.
 */
esp_err_t createOTATask(TaskHandle_t *handle, const ErrorResources *errorResources) {
    static ErrorResources taskErrorResources;
    BaseType_t success;
    /* input guards */
    if (errorResources == NULL ||
        errorResources->errMutex == NULL)
    {
        return ESP_FAIL;
    }
    /* copy parameters */
    taskErrorResources.err = errorResources->err;
    taskErrorResources.errMutex = errorResources->errMutex;
    taskErrorResources.errTimer = errorResources->errTimer;
    /* create OTA task */
    success = xTaskCreate(vOTATask, "OTATask", CONFIG_OTA_STACK,
                          &taskErrorResources, CONFIG_OTA_PRIO, handle);
    return (success == pdPASS) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Implements the over-the-air (OTA) task, which is responsible for
 *        handling user requests to update to the latest version of firmware.
 * 
 * @note To avoid runtime errors, the OTA task should only be created by the
 *       createOTATask function.
 * 
 * @param pvParameters A pointer to an ErrorResources object which should
 *                     remain valid through the lifetime of the task.
 */
void vOTATask(void* pvParameters) {
    ErrorResources *errRes = (ErrorResources *) pvParameters;
    while (true) {
        if (ulTaskNotifyTake(pdTRUE, INT_MAX) == 0) {
            continue; // block on notification timed out
        }
        // received a task notification indicating update firmware
        ESP_LOGD(TAG, "OTA update in progress...");
        gpio_set_direction(LED_NORTH_PIN, GPIO_MODE_OUTPUT);
        gpio_set_direction(LED_EAST_PIN, GPIO_MODE_OUTPUT);
        gpio_set_direction(LED_SOUTH_PIN, GPIO_MODE_OUTPUT);
        gpio_set_direction(LED_WEST_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(LED_NORTH_PIN, 1);
        gpio_set_level(LED_EAST_PIN, 1);
        gpio_set_level(LED_SOUTH_PIN, 1);
        gpio_set_level(LED_WEST_PIN, 1);
        esp_http_client_config_t https_config = {
            .url = FIRMWARE_UPGRADE_URL,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
        esp_https_ota_config_t ota_config = {
            .http_config = &https_config,
        };
        esp_err_t ret = esp_https_ota(&ota_config);
        if (ret == ESP_OK) {
            ESP_LOGD(TAG, "completed OTA update successfully!");
            unregisterWifiHandler();
            esp_restart();
        }
        
        ESP_LOGD(TAG, "did not complete OTA update successfully!");
        throwHandleableError(errRes, false);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_OTA_LEFT_ON_MS)); // leave LEDs on for a bit
        gpio_set_level(LED_NORTH_PIN, 0);
        gpio_set_level(LED_EAST_PIN, 0);
        gpio_set_level(LED_SOUTH_PIN, 0);
        gpio_set_level(LED_WEST_PIN, 0);
        resolveHandleableError(errRes, false, false);
    }
}
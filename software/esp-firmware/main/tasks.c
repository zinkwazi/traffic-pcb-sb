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
#include "led_registers.h"
#include "wifi.h"
#include "main_types.h"

#define DOTS_GLOBAL_CURRENT 0x25 // TODO: move this back to main

/* The URL of server data (to be appended with version) */
#define URL_DATA_SERVER_NORTH (CONFIG_DATA_SERVER "/current_data/data_north_")
#define URL_DATA_SERVER_SOUTH (CONFIG_DATA_SERVER "/current_data/data_south_")
#define URL_DATA_FILE_TYPE (".dat")

#define API_METHOD HTTP_METHOD_GET
#define API_AUTH_TYPE HTTP_AUTH_TYPE_NONE

void setColor(uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t speed) {
    if (speed < 30) {
        *red = 0xFF;
        *green = 0x00;
        *blue = 0x00;
    } else if (speed < 60) {
        *red = 0x15;
        *green = 0x09;
        *blue = 0x00;
    } else {
        *red = 0x00;
        *green = 0x00;
        *blue = 0x09;
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
    strcat(urlStr, ".dat");
    ESP_LOGI(TAG, "%s", urlStr);
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
    responseStr = malloc(sizeof(char) * (contentLength + 100));
    if (responseStr == NULL) {
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
        return ESP_FAIL;
    }
    if (len == -1) {
        ESP_LOGE(TAG, "esp_http_client_read returned -1");
        return ESP_FAIL;
    }
    for (int i = 0; i < contentLength && i < speedsSize; i++) {
        speeds[i] = (uint8_t) responseStr[i];
    }
    return ESP_OK;
}

void updateLED(QueueHandle_t I2CQueue, uint16_t ledNum, uint8_t speed) {
    uint8_t red, green, blue;
    setColor(&red, &green, &blue, speed);
    if (dotsSetColor(I2CQueue, ledNum, red, green, blue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK ||
        dotsSetScaling(I2CQueue, ledNum, 0xFF, 0xFF, 0xFF, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to change led %d color", ledNum);
    }
}

bool mustAbort(QueueHandle_t I2CQueue, QueueHandle_t dotQueue) {
    DotCommand command;
    if (xQueuePeek(dotQueue, &command, 0) == pdTRUE) {
        /* A new command has been issued, quick clear and abort command */
        ESP_LOGI(TAG, "Quick Clearing...");
        if (dotsReset(I2CQueue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK ||
            dotsSetGlobalCurrentControl(I2CQueue, DOTS_GLOBAL_CURRENT, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK ||
            dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK) 
        {
            ESP_LOGE(TAG, "failed to reset dot matrices");
        }
        return true;
    }
    return false;
}

esp_err_t handleRefresh(bool *aborted, Direction dir, QueueHandle_t I2CQueue, QueueHandle_t dotQueue, esp_http_client_handle_t client) {
    static const int speedsSize = MAX_NUM_LEDS + 1;
    static uint8_t speeds[MAX_NUM_LEDS + 1]; // +1 because 0 is not an led number
    *aborted = false;
    /* connect to API and query speeds */
    if (tomtomGetServerSpeeds(speeds, speedsSize, dir, client, CONFIG_HARDWARE_VERSION CONFIG_SERVER_FIRMWARE_VERSION, 1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to retrieve segment speeds from server");
        return ESP_FAIL;
    }
    switch (dir) {
        case NORTH:
            for (int ndx = speedsSize - 1; ndx > 0; ndx--) {
                updateLED(I2CQueue, ndx, speeds[ndx]);
                if (mustAbort(I2CQueue, dotQueue)) {
                    *aborted = true;
                    return ESP_OK;
                }
                vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_UPDATE_PERIOD));
            }
            break;
        case SOUTH:
            for (int ndx = 1; ndx < speedsSize - 1; ndx++) {
                updateLED(I2CQueue, ndx, speeds[ndx]);
                if (mustAbort(I2CQueue, dotQueue)) {
                    *aborted = true;
                    return ESP_OK;
                }
                vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_UPDATE_PERIOD));
            }
            break;
    }
    for (int i = 0; i < speedsSize; i++) {
        speeds[i] = 0;
    }
    return ESP_OK;
}

esp_err_t createDotWorkerTask(DotWorkerTaskResources *resources) {
  BaseType_t success;
  /* input guards */
  if (resources->dotQueue == NULL ||
      resources->I2CQueue == NULL ||
      resources->errorOccurred == NULL ||
      resources->errorOccurredMutex == NULL)
  {
    return ESP_FAIL;
  }
  /* create task */
  success = xTaskCreate(vDotWorkerTask, "worker", DOTS_WORKER_STACK, 
                        resources, DOTS_WORKER_PRIO, NULL);
  if (success != pdPASS) {
    ESP_LOGE(TAG, "failed to create dot worker task");
    return ESP_FAIL;
  }
  return ESP_OK;
}

void tomtomErrorTimerCallback(void *params) {
    static int currentOutput = 0;
    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
    currentOutput = (currentOutput == 0) ? 1 : 0;
    gpio_set_level(ERR_LED_PIN, currentOutput);
}

/**
 * Accepts requests for dot updates off of
 * a queue, retrieves the dot's current speed,
 * then sends a command to the I2C gatekeeper
 * to update the color of the dot.
 */
void vDotWorkerTask(void *pvParameters) {
    esp_http_client_config_t httpConfig = {
        .host = CONFIG_DATA_SERVER,
        .path = "/",
        .auth_type = API_AUTH_TYPE,
        .method = API_METHOD,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = NULL,
        .user_data = NULL,
    };

    DotWorkerTaskResources *res = (DotWorkerTaskResources *) pvParameters;
    esp_http_client_handle_t client;
    DotCommand dot;

    client = esp_http_client_init(&httpConfig);
    if (client == NULL) {
        INDICATE_ERR(res->errorOccurred, res->errorOccurredMutex);
        for (;;) {}
    }
    
    /* Wait for commands and execute them forever */
    bool prevCommandAborted = false;
    for (;;) {  // This task should never end
        if (ulTaskNotifyTake(pdTRUE, 0) == 1) {
            /* recieved an error from I2C gatekeeper */
            INDICATE_ERR(res->errorOccurred, res->errorOccurredMutex);
        }
        /* wait for a command on the queue */
        while (xQueueReceive(res->dotQueue, &dot, INT_MAX) == pdFALSE) {}
        /* update led colors */
        switch (dot.type) {
            case REFRESH_NORTH:
                if (handleRefresh(&prevCommandAborted, NORTH, res->I2CQueue, res->dotQueue, client) != ESP_OK) {
                    esp_http_client_cleanup(client);
                    client = esp_http_client_init(&httpConfig);
                }
                break;
            case REFRESH_SOUTH:
                if (handleRefresh(&prevCommandAborted, SOUTH, res->I2CQueue, res->dotQueue, client) != ESP_OK) {
                    esp_http_client_cleanup(client);
                    client = esp_http_client_init(&httpConfig);
                }
                break;
            case CLEAR_NORTH:
                if (prevCommandAborted) {
                    break;
                }
                ESP_LOGI(TAG, "Clearing North...");
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
                ESP_LOGI(TAG, "Clearing South...");
                for (int ndx = 1; ndx < MAX_NUM_LEDS + 1; ndx++) {
                    if (dotsSetColor(res->I2CQueue, ndx, 0x00, 0x00, 0x00, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK) {
                        ESP_LOGE(TAG, "failed to change led %d color", ndx);
                    }
                    vTaskDelay(pdMS_TO_TICKS(CONFIG_LED_CLEAR_PERIOD));
                }
                prevCommandAborted = false;
                break;
            case QUICK_CLEAR:
                ESP_LOGI(TAG, "Quick Clearing...");
                if (dotsReset(res->I2CQueue, DOTS_NOTIFY, DOTS_ASYNC) != ESP_OK ||
                    dotsSetGlobalCurrentControl(res->I2CQueue, DOTS_GLOBAL_CURRENT, DOTS_NOTIFY, DOTS_BLOCKING) != ESP_OK ||
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

void vOTATask(void* pvParameters) {
    while (true) {
        if (ulTaskNotifyTake(pdTRUE, INT_MAX) == 0) {
            continue; // block on notification timed out
        }
        // received a task notification indicating update firmware
        ESP_LOGI(TAG, "OTA update in progress...");
        gpio_set_direction(LED_NORTH_PIN, GPIO_MODE_OUTPUT);
        gpio_set_direction(LED_EAST_PIN, GPIO_MODE_OUTPUT);
        gpio_set_direction(LED_SOUTH_PIN, GPIO_MODE_OUTPUT);
        gpio_set_direction(LED_WEST_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(LED_NORTH_PIN, 1);
        gpio_set_level(LED_EAST_PIN, 1);
        gpio_set_level(LED_SOUTH_PIN, 1);
        gpio_set_level(LED_WEST_PIN, 1);
        esp_http_client_config_t https_config = {
            .url = CONFIG_FIRMWARE_UPGRADE_SERVER "/firmware/firmware" CONFIG_HARDWARE_VERSION ".bin",
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
        esp_https_ota_config_t ota_config = {
            .http_config = &https_config,
        };
        esp_err_t ret = esp_https_ota(&ota_config);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "completed OTA update successfully!");
            unregisterWifiHandler();
            esp_restart();
        }
        ESP_LOGI(TAG, "did not complete OTA update successfully!");
    }
}
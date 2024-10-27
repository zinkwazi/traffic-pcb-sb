/**
 * main.c
 * 
 * This file contains the entrypoint task
 * for the application (app_main) and
 * configuration including function hooks.
 */

/* IDF component includes */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_tls.h"
#include "cJSON.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

/* Main component includes */
#include "pinout.h"

/* Component includes */
#include "api_config.h"
#include "tomtom.h"
#include "led_locations.h"
#include "dots_commands.h"

#define TAG "app_main"

#define I2C_GATEKEEPER_STACK ESP_TASK_MAIN_STACK // TODO: determine minimum stack size for i2c gatekeeper
#define I2C_GATEKEEPER_PRIO (ESP_TASK_MAIN_PRIO + 1) // always start an I2C command if possible
#define I2C_QUEUE_SIZE 20

#define NUM_LEDS 326

/**
 * The entrypoint task of the application, which initializes 
 * other tasks, then handles TomTom API calls.
 */
void app_main(void)
{
    /* initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    /* initialize tcp/ip stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    /* Establish wifi connection */
    wifi_init_config_t default_wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&default_wifi_cfg));
    ESP_ERROR_CHECK(establishWifiConnection());
    esp_tls_t *tls = esp_tls_init();
    ESP_GOTO_ON_FALSE(
      (tls != NULL), ESP_FAIL, spin_forever,
      TAG, "failed to allocate esp_tls handle"
    );
    /* Create I2C gatekeeper */
    static I2CGatekeeperTaskParameters i2cGatekeeperTaskParams;
    TaskHandle_t I2CGatekeeperHandle;
    QueueHandle_t I2CQueue = xQueueCreate(I2C_QUEUE_SIZE, sizeof(I2CCommand));
    i2cGatekeeperTaskParams.I2CQueueHandle = I2CQueue;
    if (i2cGatekeeperTaskParams.I2CQueueHandle == NULL) {
      ESP_LOGE(TAG, "Failed to create I2C command queue");
      goto spin_forever;
    }
    if (pdPASS != xTaskCreate(vI2CGatekeeperTask, "I2CGatekeeper", I2C_GATEKEEPER_STACK, 
                              &i2cGatekeeperTaskParams, I2C_GATEKEEPER_PRIO, &I2CGatekeeperHandle))
    {
      ESP_LOGE(TAG, "Failed to create I2C Gatekeeper");
      goto spin_forever;
    }
    if (I2CGatekeeperHandle == NULL) {
      ESP_LOGE(TAG, "Failed to retrieve I2C Gatekeeper handle");
      goto spin_forever;
    }
    /* Configure DOTS matrices */
    ESP_LOGI(TAG, "resetting matrices");
    ESP_ERROR_CHECK(dotsReset(I2CQueue));
    ESP_LOGI(TAG, "changing global current control");
    ESP_ERROR_CHECK(dotsSetGlobalCurrentControl(I2CQueue, 0x08));
    ESP_LOGI(TAG, "setting operating mode to normal");
    ESP_ERROR_CHECK(dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION));
    /* request all LED speeds and display results */
    esp_http_client_handle_t tomtomHandle = tomtomCreateHttpHandle();
    ESP_GOTO_ON_FALSE(
      (tomtomHandle != NULL), ESP_FAIL, spin_forever,
      TAG, "failed to allocate tomtom http handle"
    );
    for (int i = 1; i < NUM_LEDS; i++) {
      uint result = 0;
      tomtomRequestSpeed(&result, tomtomHandle, i, NORTH);
      ESP_LOGI(TAG, "North LED %d speed: %d", i, result);
      if (result < 30) {
        ESP_ERROR_CHECK(dotsSetScaling(I2CQueue, i, 0xFF, 0xFF, 0xFF));
        ESP_ERROR_CHECK(dotsSetColor(I2CQueue, i, 0xFF, 0x00, 0x00));
      } else if (result < 60) {
        ESP_ERROR_CHECK(dotsSetScaling(I2CQueue, i, 0xFF, 0xFF, 0xFF));
        ESP_ERROR_CHECK(dotsSetColor(I2CQueue, i, 0xFF, 0x55, 0x00));
      } else {
        ESP_ERROR_CHECK(dotsSetScaling(I2CQueue, i, 0xFF, 0xFF, 0xFF));
        ESP_ERROR_CHECK(dotsSetColor(I2CQueue, i, 0x00, 0xFF, 0x00));
      }
    }
    // for (int i = 1; i < NUM_LEDS; i++) {
    //   uint result = 0;
    //   tomtomRequestSpeed(&result, tomtomHandle, i, SOUTH);
    //   ESP_LOGI(TAG, "North LED %d speed: %d", i, result);
    //   if (result < 40) {
    //     ESP_ERROR_CHECK(dotsSetScaling(I2CQueue, i, 0xFF, 0xFF, 0xFF));
    //     ESP_ERROR_CHECK(dotsSetColor(I2CQueue, i, 0xFF, 0x00, 0x00));
    //   } else if (result < 65) {
    //     ESP_ERROR_CHECK(dotsSetScaling(I2CQueue, i, 0xFF, 0xFF, 0xFF));
    //     ESP_ERROR_CHECK(dotsSetColor(I2CQueue, i, 0xFF, 0x55, 0x00));
    //   } else {
    //     ESP_ERROR_CHECK(dotsSetScaling(I2CQueue, i, 0xFF, 0xFF, 0xFF));
    //     ESP_ERROR_CHECK(dotsSetColor(I2CQueue, i, 0x00, 0xFF, 0x00));
    //   }
    // }

    /* This task has nothing left to do, but should not exit */
spin_forever:
    for (;;) {
      vTaskDelay(INT_MAX);
    }
}
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
    if (tls == NULL) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle");
        goto spin_forever;
    }
    /* Create I2C gatekeeper */
    static I2CGatekeeperTaskParameters i2cGatekeeperTaskParams;
    TaskHandle_t I2CGatekeeperHandle;
    i2cGatekeeperTaskParams.I2CQueueHandle = xQueueCreate(I2C_QUEUE_SIZE, sizeof(I2CCommand));
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
    /* Request speed from TomTom API */
    uint result = 0;
    ESP_ERROR_CHECK(tomtomRequestSpeed(&result, 3, SOUTH));
    printf("speed: %d\n", result);
    fflush(stdout);
    /* This task has nothing left to do, but should not exit */
spin_forever:
    for (;;) {
      vTaskDelay(INT_MAX);
    }
}
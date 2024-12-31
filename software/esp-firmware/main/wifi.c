/**
 * wifi.c
 * 
 * Contains functions that manage wifi events.
 */

#include "wifi.h"

#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "lwip/sys.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_event_base.h"

#define TAG "wifi"

#define WAIT_CONNECTED_MS (100) /* wait time to establish a wifi connection */

/* Indicator that the app is connected to the AP */
static bool wifiConnected;
static EventGroupHandle_t wifiEvents;
static esp_event_handler_instance_t instanceAnyID;
static esp_event_handler_instance_t instanceAnyIP;
static char *sWifiSSID;
static char *sWifiPass;
static gpio_num_t sWifiLED;

/**
 * A handler that recieves wifi events before connection with the AP is made. See establishWifiConnection.
 */
void connectHandler(void *arg, esp_event_base_t eventBase,
                            int32_t eventId, void* eventData)
{
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        wifiConnected = false;
        ESP_LOGI(TAG, "disconnect event!");
        gpio_set_level(sWifiLED, 0);
        xEventGroupSetBits(wifiEvents, WIFI_DISCONNECTED_BIT);
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "wifi connected event!");
        wifiConnected = true;
        gpio_set_level(sWifiLED, 1);
        xEventGroupSetBits(wifiEvents, WIFI_CONNECTED_BIT);
    }
}

/**
 * A handler that recieves wifi events after connection with the AP is made. See establishWifiConnection.
 */
void wifiEventHandler(void *arg, esp_event_base_t eventBase,
                            int32_t eventId, void *eventData)
{
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        wifiConnected = false;
        ESP_LOGI(TAG, "disconnect event! AP connected");
        gpio_set_level(sWifiLED, 0);
        esp_wifi_connect();
        vTaskDelay(500);
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "wifi connected event! AP connected");
        wifiConnected = true;
        gpio_set_level(sWifiLED, 1);
    }
}

/**
 * Initializes wifi synchronization primitives and
 * stores a pointer to wifiSSID and wifiPass strings,
 * which must point to memory that is always available.
 */
esp_err_t initWifi(char *wifiSSID, char *wifiPass, gpio_num_t wifiLED) {
    wifiConnected = false;
    instanceAnyID = NULL;
    instanceAnyIP = NULL;
    sWifiSSID = wifiSSID;
    sWifiPass = wifiPass;
    sWifiLED = wifiLED;
    wifiEvents = xEventGroupCreate();
    return ESP_OK;
}

bool isWifiConnected(void) {
    return wifiConnected;
}

esp_err_t registerWifiHandler(esp_event_handler_t handler, void *handler_arg) {
    esp_err_t ret;
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            handler,
                                            handler_arg,
                                            &instanceAnyIP);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            handler,
                                            handler_arg,
                                            &instanceAnyID);
    if (ret != ESP_OK) {
        if (ESP_OK != esp_event_handler_instance_unregister(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        instanceAnyIP))
        {
            return ESP_FAIL;
        }
        return ret;
    }
    return ret;
}

esp_err_t unregisterWifiHandler(void) {
    esp_err_t ret;
    ret = esp_event_handler_instance_unregister(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            instanceAnyIP);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "FAIL");
        return ret;
    }
    ret = esp_event_handler_instance_unregister(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            instanceAnyID);
    ESP_LOGI(TAG, "unregistered wifi handler");
    return ret;
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
esp_err_t establishWifiConnection(void) 
{
    esp_err_t ret;
    EventBits_t bits;
    wifi_config_t wifi_cfg = {
        .sta = {
            .scan_method = WIFI_SCAN_METHOD,
            .threshold.authmode = WIFI_AUTH_MODE,
        },
    };
    /* copy STA information */
    ESP_LOGI(TAG, "copying wifi information");
    const unsigned int staSSIDLen = sizeof(wifi_cfg.sta.ssid) / sizeof(wifi_cfg.sta.ssid[0]);
    const unsigned int staPassLen = sizeof(wifi_cfg.sta.password) / sizeof(wifi_cfg.sta.password[0]);
    for (unsigned int i = 0; i < staSSIDLen && i < 32; i++) {
        wifi_cfg.sta.ssid[i] = ((uint8_t *) sWifiSSID)[i];
    }
    for (unsigned int i = 0; i < staPassLen && i < 64; i++) {
        wifi_cfg.sta.password[i] = ((uint8_t *) sWifiPass)[i];
    }
    ESP_LOGI(TAG, "wifi ssid: %s", wifi_cfg.sta.ssid);
    ESP_LOGI(TAG, "wifi pass: %s", wifi_cfg.sta.password);
    /* register wifi handler */
    ESP_LOGI(TAG, "registering handler");
    ret = registerWifiHandler(connectHandler, NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    /* attempt to connect to AP */
    ESP_LOGI(TAG, "connecting to AP");
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        unregisterWifiHandler();
        return ret;
    }
    ESP_LOGI(TAG, "setting config");
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (ret != ESP_OK) {
        unregisterWifiHandler();
        return ret;
    }
    ESP_LOGI(TAG, "starting wifi");
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        unregisterWifiHandler();
        return ret;
    }
    ESP_LOGI(TAG, "connecting to wifi");
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        unregisterWifiHandler();
        return ret;
    }
    ESP_LOGI(TAG, "waiting for connection");
    bits = xEventGroupWaitBits(wifiEvents,
                               WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT,
                               pdFALSE,
                               pdFALSE,
                               portMAX_DELAY);
    if (bits & ~WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "did not connect to wifi AP");
        unregisterWifiHandler();
        xEventGroupClearBits(wifiEvents, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "connected to wifi AP");
    unregisterWifiHandler();
    xEventGroupClearBits(wifiEvents, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);
    ret = registerWifiHandler(wifiEventHandler, NULL);
    return ret;
}
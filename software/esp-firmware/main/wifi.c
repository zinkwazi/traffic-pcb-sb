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

#define TAG "wifi"

#define WAIT_CONNECTED_MS (100) /* wait time to establish a wifi connection */

/* Semaphore that gaurds wifi reconnection */
static SemaphoreHandle_t wifiMutex;
/* Indicator that the app is connected to the AP */
static bool wifiConnected;
static EventGroupHandle_t wifiEvents;
static esp_event_handler_instance_t instanceAnyID;
static esp_event_handler_instance_t instanceAnyIP;
static char *sWifiSSID;
static char *sWifiPass;
static gpio_num_t sWifiLED;

/**
 * A handler that recieves wifi connection events. See establishWifiConnection.
 */
static void connectHandler(void *arg, esp_event_base_t eventBase,
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
 * Initializes wifi synchronization primitives and
 * stores a pointer to wifiSSID and wifiPass strings,
 * which must point to memory that is always available.
 */
esp_err_t initWifi(char *wifiSSID, char *wifiPass, gpio_num_t wifiLED) {
    wifiMutex = xSemaphoreCreateMutex();
    wifiConnected = false;
    instanceAnyID = NULL;
    instanceAnyIP = NULL;
    sWifiSSID = wifiSSID;
    sWifiPass = wifiPass;
    sWifiLED = wifiLED;
    wifiEvents = xEventGroupCreate();
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        connectHandler,
                                        NULL,
                                        &instanceAnyIP);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        connectHandler,
                                        NULL,
                                        &instanceAnyID);
    return ESP_OK;
}

bool isWifiConnected(void) {
    return wifiConnected;
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
esp_err_t establishWifiConnection()
{
    esp_err_t ret;
    EventBits_t bits;
    wifi_config_t wifi_cfg = {
        .sta = {
            .scan_method = WIFI_SCAN_METHOD,
            .threshold.authmode = WIFI_AUTH_MODE,
        },
    };
    /* acquire wifi mutex */
    while (xSemaphoreTake(wifiMutex, INT_MAX) == pdFALSE) {}
    /* check that wifi was not connected while waiting */
    if (wifiConnected) {
        xSemaphoreGive(wifiMutex);
        return ESP_OK;
    }
    /* copy STA information */
    const unsigned int staSSIDLen = sizeof(wifi_cfg.sta.ssid) / sizeof(wifi_cfg.sta.ssid[0]);
    const unsigned int staPassLen = sizeof(wifi_cfg.sta.password) / sizeof(wifi_cfg.sta.password[0]);
    for (unsigned int i = 0; i < staSSIDLen; i++) {
        wifi_cfg.sta.ssid[i] = ((uint8_t *) sWifiSSID)[i];
    }
    for (unsigned int i = 0; i < staPassLen; i++) {
        wifi_cfg.sta.password[i] = ((uint8_t *) sWifiPass)[i];
    }
    /* attempt to connect to AP */
    ESP_LOGI(TAG, "connecting to AP");
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        xSemaphoreGive(wifiMutex);
        return ret;
    }
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (ret != ESP_OK) {
        xSemaphoreGive(wifiMutex);
        return ret;
    }
    ESP_LOGI(TAG, "starting wifi");
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        xSemaphoreGive(wifiMutex);
        return ret;
    }
    ESP_LOGI(TAG, "connecting to wifi");
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        xSemaphoreGive(wifiMutex);
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
        xEventGroupClearBits(wifiEvents, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);
        xSemaphoreGive(wifiMutex);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "connected to wifi AP");
    xEventGroupClearBits(wifiEvents, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);
    xSemaphoreGive(wifiMutex);
    return ret;
}
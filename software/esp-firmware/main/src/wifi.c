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

/* wait time to establish a wifi connection */
#define WAIT_CONNECTED_MS (100) 

#define WIFI_SCAN_METHOD WIFI_FAST_SCAN
#define WIFI_AUTH_MODE WIFI_AUTH_WPA2_PSK

/* Connection method configuration */
#define WIFI_CONNECTED_BIT (BIT0) /* wifi event group bit */
#define WIFI_DISCONNECTED_BIT (BIT1) /* wifi event group bit */

/* The internal buffer space for the wifi ssid in wifi_config_t */
#define WIFI_SSID_LEN 32
/* The internal buffer space for the wifi password in wifi_config_t */
#define WIFI_PASS_LEN 64

/* Indicator that the app is connected to the AP */
static bool sWifiConnected;
static EventGroupHandle_t sWifiEvents;
static esp_event_handler_instance_t sInstanceAnyID;
static esp_event_handler_instance_t sInstanceAnyIP;
static char *sWifiSSID;
static char *sWifiPass;
static gpio_num_t sWifiLED;

/**
 * A handler that recieves wifi events BEFORE connection with the AP is made. See establishWifiConnection.
 */
void connectHandler(void *arg, esp_event_base_t eventBase,
                            int32_t eventId, void* eventData)
{
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        sWifiConnected = false;
        ESP_LOGD(TAG, "disconnect event!");
        gpio_set_level(sWifiLED, 0);
        xEventGroupSetBits(sWifiEvents, WIFI_DISCONNECTED_BIT);
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        ESP_LOGD(TAG, "wifi connected event!");
        sWifiConnected = true;
        gpio_set_level(sWifiLED, 1);
        xEventGroupSetBits(sWifiEvents, WIFI_CONNECTED_BIT);
    }
}

/**
 * A handler that recieves wifi events AFTER connection with the AP is made. See establishWifiConnection.
 */
void wifiEventHandler(void *arg, esp_event_base_t eventBase,
                            int32_t eventId, void *eventData)
{
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        sWifiConnected = false;
        ESP_LOGD(TAG, "disconnect event! AP connected");
        gpio_set_level(sWifiLED, 0);
        esp_wifi_connect();
        vTaskDelay(CONFIG_RETRY_RECONNECT_PERIOD);
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        ESP_LOGD(TAG, "wifi connected event! AP connected");
        sWifiConnected = true;
        gpio_set_level(sWifiLED, 1);
    }
}

/**
 * Initializes wifi synchronization primitives and
 * stores a pointer to wifiSSID and wifiPass strings,
 * which must point to memory that is always available.
 */
esp_err_t initWifi(char *wifiSSID, char *wifiPass, gpio_num_t wifiLED) {
    sWifiConnected = false;
    sInstanceAnyID = NULL;
    sInstanceAnyIP = NULL;
    sWifiSSID = wifiSSID;
    sWifiPass = wifiPass;
    sWifiLED = wifiLED;
    sWifiEvents = xEventGroupCreate();
    return ESP_OK;
}

bool isWifiConnected(void) {
    return sWifiConnected;
}

esp_err_t registerWifiHandler(esp_event_handler_t handler, void *handler_arg) {
    esp_err_t ret;
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            handler,
                                            handler_arg,
                                            &sInstanceAnyIP);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            handler,
                                            handler_arg,
                                            &sInstanceAnyID);
    if (ret != ESP_OK) {
        if (ESP_OK != esp_event_handler_instance_unregister(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        sInstanceAnyIP))
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
                                            sInstanceAnyIP);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_event_handler_instance_unregister(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            sInstanceAnyID);
    ESP_LOGD(TAG, "unregistered wifi handler");
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
    ESP_LOGD(TAG, "copying wifi information");
    const unsigned int staSSIDLen = sizeof(wifi_cfg.sta.ssid) / sizeof(wifi_cfg.sta.ssid[0]);
    const unsigned int staPassLen = sizeof(wifi_cfg.sta.password) / sizeof(wifi_cfg.sta.password[0]);
    for (unsigned int i = 0; i < staSSIDLen && i < WIFI_SSID_LEN; i++) {
        wifi_cfg.sta.ssid[i] = ((uint8_t *) sWifiSSID)[i];
    }
    for (unsigned int i = 0; i < staPassLen && i < WIFI_PASS_LEN; i++) {
        wifi_cfg.sta.password[i] = ((uint8_t *) sWifiPass)[i];
    }
    ESP_LOGD(TAG, "wifi ssid: %s", wifi_cfg.sta.ssid);
    ESP_LOGD(TAG, "wifi pass: %s", wifi_cfg.sta.password);
    /* register wifi handler */
    ESP_LOGD(TAG, "registering handler");
    ret = registerWifiHandler(connectHandler, NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    /* attempt to connect to AP */
    ESP_LOGD(TAG, "connecting to AP");
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        unregisterWifiHandler();
        return ret;
    }
    ESP_LOGD(TAG, "setting config");
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (ret != ESP_OK) {
        unregisterWifiHandler();
        return ret;
    }
    ESP_LOGD(TAG, "starting wifi");
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        unregisterWifiHandler();
        return ret;
    }
    ESP_LOGD(TAG, "connecting to wifi");
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        unregisterWifiHandler();
        return ret;
    }
    ESP_LOGD(TAG, "waiting for connection");
    bits = xEventGroupWaitBits(sWifiEvents,
                               WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT,
                               pdFALSE,
                               pdFALSE,
                               portMAX_DELAY);
    if (bits & ~WIFI_CONNECTED_BIT) {
        ESP_LOGD(TAG, "did not connect to wifi AP");
        unregisterWifiHandler();
        xEventGroupClearBits(sWifiEvents, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);
        registerWifiHandler(wifiEventHandler, NULL);
        esp_wifi_connect(); // start handler loop
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "connected to wifi AP");
    unregisterWifiHandler();
    xEventGroupClearBits(sWifiEvents, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);
    ret = registerWifiHandler(wifiEventHandler, NULL);
    return ret;
}
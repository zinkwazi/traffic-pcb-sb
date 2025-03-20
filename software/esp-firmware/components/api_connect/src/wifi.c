/**
 * wifi.c
 * 
 * Contains functions that manage wifi events.
 */

#include "wifi.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include "esp_netif_types.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "indicators.h"

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

static void connectHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void* eventData);
static void wifiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData);
static esp_err_t registerWifiHandler(esp_event_handler_t handler, void *handler_arg);

bool isWifiConnected(void) {
    return sWifiConnected;
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
    return ret;
}

/**
 * @brief Initializes wifi static variables.
 * 
 * @param[in] wifiSSID A pointer to the wifi SSID, which must remain
 *        valid for the lifetime of the application.
 * @param[in] wifiPass A pointer to the wifi password, which must remain
 *        valid for the lifetime of the application.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid argument.
 *          ESP_ERR_NO_MEM if insufficient FreeRTOS heap space.
 */
esp_err_t initWifi(char *wifiSSID, char *wifiPass) {
    /* input guards */
    if (wifiSSID == NULL) return ESP_ERR_INVALID_ARG;
    if (wifiPass == NULL) return ESP_ERR_INVALID_ARG;
    /* initialize static variables */
    sWifiEvents = xEventGroupCreate();
    if (sWifiEvents == NULL) return ESP_ERR_NO_MEM;
    sWifiConnected = false;
    sInstanceAnyID = NULL;
    sInstanceAnyIP = NULL;
    sWifiSSID = wifiSSID;
    sWifiPass = wifiPass;
    return ESP_OK;
}

/**
 * @brief Deinitializes wifi static variables to allow initWifi to be called
 *        again.
 * 
 * @note This function is useful for unit test teardowns.
 * 
 * @returns ESP_OK if successful, otherwise wifi handlers may have been removed.
 */
esp_err_t deinitWifi(void)
{
    esp_err_t err;
    err = unregisterWifiHandler();
    if (err != ESP_OK) return err;
    err = esp_wifi_stop();
    if (err != ESP_OK) return err;
    vEventGroupDelete(sWifiEvents);
    sWifiEvents = NULL;
    sWifiConnected = false;
    sInstanceAnyID = NULL;
    sInstanceAnyIP = NULL;
    sWifiSSID = NULL;
    sWifiPass = NULL;
    return ESP_OK;
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
    const unsigned int staSSIDLen = sizeof(wifi_cfg.sta.ssid) / sizeof(wifi_cfg.sta.ssid[0]);
    const unsigned int staPassLen = sizeof(wifi_cfg.sta.password) / sizeof(wifi_cfg.sta.password[0]);
    for (unsigned int i = 0; i < staSSIDLen && i < WIFI_SSID_LEN; i++) {
        wifi_cfg.sta.ssid[i] = ((uint8_t *) sWifiSSID)[i];
    }
    for (unsigned int i = 0; i < staPassLen && i < WIFI_PASS_LEN; i++) {
        wifi_cfg.sta.password[i] = ((uint8_t *) sWifiPass)[i];
    }
    ESP_LOGI(TAG, "wifi ssid: %s", wifi_cfg.sta.ssid);
    /* register wifi handler */
    ret = registerWifiHandler(connectHandler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "registerWifiHandler");
        return ret;
    }
    /* attempt to connect to AP */
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode");
        unregisterWifiHandler();
        return ret;
    }
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config");
        unregisterWifiHandler();
        return ret;
    }
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start");
        unregisterWifiHandler();
        return ret;
    }
    ESP_LOGI(TAG, "connecting to wifi");
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect. err: %d", ret);
        unregisterWifiHandler();
        return ret;
    }
    bits = xEventGroupWaitBits(sWifiEvents,
                            WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT,
                            pdFALSE,
                            pdFALSE,
                            portMAX_DELAY);
    if (bits & ~WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "did not connect to wifi");
        unregisterWifiHandler();
        xEventGroupClearBits(sWifiEvents, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);
        registerWifiHandler(wifiEventHandler, NULL);
        esp_wifi_connect(); // start handler loop
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "connected to wifi");
    unregisterWifiHandler();
    xEventGroupClearBits(sWifiEvents, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);
    ret = registerWifiHandler(wifiEventHandler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "registerWifiHandler failed");
    }
    return ret;
}

/**
 * A handler that recieves wifi events BEFORE connection with the AP is made. 
 * See establishWifiConnection.
 */
static void connectHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void* eventData)
{
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        sWifiConnected = false;
        (void) indicateWifiNotConnected();
        xEventGroupSetBits(sWifiEvents, WIFI_DISCONNECTED_BIT);
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        sWifiConnected = true;
        (void) indicateWifiConnected();
        xEventGroupSetBits(sWifiEvents, WIFI_CONNECTED_BIT);
    }
}

/**
 * A handler that recieves wifi events AFTER connection with the AP is made. 
 * See establishWifiConnection.
 */
static void wifiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        sWifiConnected = false;
        (void) indicateWifiNotConnected();
        esp_wifi_connect();
        vTaskDelay(CONFIG_RETRY_RECONNECT_PERIOD);
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        sWifiConnected = true;
        (void) indicateWifiConnected();
    }
}

static esp_err_t registerWifiHandler(esp_event_handler_t handler, void *handler_arg) {
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
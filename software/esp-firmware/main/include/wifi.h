/**
 * wifi.h
 * 
 * Contains functions that manage wifi events.
 */

#ifndef WIFI_H_
#define WIFI_H_

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdbool.h>

#define WIFI_SCAN_METHOD WIFI_FAST_SCAN
#define WIFI_AUTH_MODE WIFI_AUTH_WPA2_PSK

/* Connection method configuration */
#define WIFI_CONNECTED_BIT (BIT0) /* wifi event group bit */
#define WIFI_DISCONNECTED_BIT (BIT1) /* wifi event group bit */

/**
 * Initializes wifi synchronization primitives and
 * stores a pointer to wifiSSID and wifiPass strings,
 * which must point to memory that is always available.
 */
esp_err_t initWifi(char *wifiSSID, char* wifiPass, gpio_num_t wifiLED);

bool isWifiConnected(void);

esp_err_t unregisterWifiHandler(void);

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
esp_err_t establishWifiConnection(void);

esp_err_t initWifiEvents(void);

#endif /* WIFI_H_ */
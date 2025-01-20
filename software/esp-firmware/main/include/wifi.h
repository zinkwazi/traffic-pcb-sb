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

esp_err_t initWifi(char *wifiSSID, char* wifiPass, gpio_num_t wifiLED);
bool isWifiConnected(void);
esp_err_t establishWifiConnection(void);
esp_err_t unregisterWifiHandler(void); // used in OTA task before restart

#endif /* WIFI_H_ */
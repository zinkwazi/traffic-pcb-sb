/**
 * utilities.h
 * 
 * This file contains functions that may be useful to tasks
 * contained in various other header files.
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* IDF component includes */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_vfs_dev.h"
#include "driver/uart_vfs.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_tls.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_timer.h"
#include "nvs.h"
#include "esp_https_ota.h"

/* Main component includes */
#include "pinout.h"
#include "tasks.h"
#include "utilities.h"
#include "wifi.h"
#include "main_types.h"
#include "app_errors.h"

/* Component includes */
#include "dots_commands.h"
#include "led_registers.h"

#define STR_HELPER(x) #x      /* STR(x) adds "" around any text expression */
#define STR(x) STR_HELPER(x)

#define HARDWARE_VERSION_STR "V" STR(CONFIG_HARDWARE_VERSION) "_" STR(CONFIG_HARDWARE_REVISION)
#define VERSION_STR HARDWARE_VERSION_STR "_" STR(CONFIG_FIRMWARE_VERSION)
#define VERBOSE_VERSION_STR VERSION_STR CONFIG_FIRMWARE_CONF
#define SERVER_VERSION_STR HARDWARE_VERSION_STR "_" STR(CONFIG_SERVER_FIRMWARE_VERSION)

#define FIRMWARE_UPGRADE_URL CONFIG_FIRMWARE_UPGRADE_SERVER "/firmware/firmware" HARDWARE_VERSION_STR ".bin"


/**
 * @brief Calls spinForever if x is not ESP_OK.
 * 
 * @param x A value of type esp_err_t.
 * @param errResources A pointer to an ErrorResources holding global error
 *                     handling resources. If NULL, immediately spins.
 */
#define SPIN_IF_ERR(x, errResources) \
      if (x != ESP_OK) { throwFatalError(errResources, false); }

/**
 * @brief Calls spinForever if x is not true.
 * 
 * @param x A bool.
 * @param errResources A pointer to an ErrorResources holding global error
 *                     handling resources. If NULL, immediately spins.
 */
#define SPIN_IF_FALSE(x, errResources) \
      if (!x) { throwFatalError(errResources, false); } 

/**
 * @brief Calls updateSettingsAndRestart if x is not ESP_OK.
 * 
 * @param x A value of type esp_err_t.
 * @param handle The non-volatile storage handle to store user settings in.
 * @param errResources A pointer to an ErrorResources holding global error
 *                     handling resources. If NULL, immediately spins.
 */
#define UPDATE_SETTINGS_IF_ERR(x, handle, errResources) \
      if (x != ESP_OK) { updateNvsSettings(handle, errResources); }

/**
 * @brief Calls updateSettingsAndRestart if x is not true.
 * 
 * @param x A bool.
 * @param handle The non-volatile storage handle to store user settings in.
 * @param occurred A pointer to a bool that indicates whether an error has 
 *                 occurred at any point in the program or not. This bool is 
 *                 shared by all  tasks and should only be accessed after 
 *                 errMutex has been obtained.
 * @param errResources A pointer to an ErrorResources holding global error
 *                     handling resources. If NULL, immediately spins.
 */
#define UPDATE_SETTINGS_IF_FALSE(x, handle, errResources) \
      if (!x) { updateNvsSettings(handle, errResources); }

esp_err_t nvsEntriesExist(nvs_handle_t nvsHandle);
esp_err_t removeExtraMainNvsEntries(nvs_handle_t nvsHandle);
esp_err_t getNvsEntriesFromUser(nvs_handle_t nvsHandle);
esp_err_t initDotMatrices(QueueHandle_t I2CQueue);
esp_err_t updateLEDs(QueueHandle_t dotQueue, Direction dir);
esp_err_t retrieveNvsEntries(nvs_handle_t nvsHandle, struct UserSettings *settings);
esp_err_t quickClearLEDs(QueueHandle_t dotQueue);
esp_err_t clearLEDs(QueueHandle_t dotQueue, Direction currDir);
void updateNvsSettings(nvs_handle_t nvsHandle, ErrorResources *errResources);

#endif /* UTILITIES_H_ */
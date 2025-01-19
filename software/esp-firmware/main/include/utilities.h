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
#include "error.h"

/* Component includes */
#include "dots_commands.h"
#include "led_registers.h"

/** @brief The name of the non-volatile storage entry for the wifi SSID. */
#define WIFI_SSID_NVS_NAME "wifi_ssid"

/** @brief The name of the non-volatile storage entry for the wifi password. */
#define WIFI_PASS_NVS_NAME "wifi_pass"

/** @brief The name of the non-volatile storage entry for the most recent
 *         road segment speed data retrieved from the server.
*/
#define SPEED_DATA_NVS_NAME "speed_data"

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

/**
 * @brief Determines whether user settings currently exist in non-volatile
 *        storage.
 * 
 * @note User settings should not exist in storage on the first powerup of the
 *       system, however they should exist during subsequent reboots.
 * 
 * @param nvsHandle The non-volatile storage handle where the settings would
 *                  exist.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t nvsEntriesExist(nvs_handle_t nvsHandle);

/**
 * @brief Removes any entries in non-volatile storage that are unnecessary for
 *        device operation.
 * 
 * @note Unnecessary NVS entries may exist if a firmware update has been
 *       performed and previously necessary entries have been made obsolete.
 *       All entries that are deemed necessary are those searched for in
 *       the nvsEntriesExist function.
 * 
 * @param nvsHandle The non-volatile storage handle where user settings exist.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t removeExtraMainNvsEntries(nvs_handle_t nvsHandle);

/**
 * @brief Queries the user for settings and writes responses in non-volatile
 *        storage.
 * 
 * @note Uses UART0 to query settings.
 * 
 * @param nvsHandle The non-volatile storage handle to store settings in.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t getNvsEntriesFromUser(nvs_handle_t nvsHandle);

esp_err_t getNvsSpeedData(nvs_handle_t);

esp_err_t initDotMatrices(QueueHandle_t I2CQueue);
esp_err_t updateLEDs(QueueHandle_t dotQueue, Direction dir);

/**
 * @brief Retrieves user settings from non-volatile storage.
 * 
 * Retrieves user settings from non-volatile storage and places results in
 * the provided settings, with space allocated from the heap.
 * 
 * @note It is necessary for the calling function to free pointers in 
 *       settings if settings is to be destroyed.
 * 
 * @param nvsHandle The non-volatile storage handle where settings exist.
 * @param[out] settings A pointer to struct UserSettings that will be
 *                      populated with the retrieved user settings.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t retrieveNvsEntries(nvs_handle_t nvsHandle, struct UserSettings *settings);

/**
 * @brief Enables the direction button interrupt, which is handled by
 *        dirButtonISR.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t enableDirectionButtonIntr(void);

/**
 * @brief Disables the direction button interrupt, which is handled by
 *        dirButtonISR.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t disableDirectionButtonIntr(void);

/**
 * @brief Sends a command to the worker task to quickly clear all LEDs.
 * 
 * @note The worker task, implemented by vDotWorkerTask, quickly clears all
 *       of the LEDs by resetting all dot matrices.
 * 
 * @param dotQueue The queue that the worker task receives commands from, which
 *                 holds elements of type DotCommand.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t quickClearLEDs(QueueHandle_t dotQueue);

/**
 * @brief Sends a command to the worker task to clear all LEDs sequentially in
 *        a particular direction.
 * 
 * @note This is distinct from quickClearLEDs as the worker task, implemented
 *       by vDotWorkerTask, does not reset the dot matrices to fulfill the 
 *       command.
 * 
 * @param dotQueue The queue that the worker task receives commands from, which
 *                 holds elements of type DotCommand.
 * @param currDir  The direction that the LEDs will be cleared toward.
 *
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t clearLEDs(QueueHandle_t dotQueue, Direction currDir);

/**
 * @brief Handles errors that are due to a user settings issue by setting the
 *        error LED high, querying the user for new settings, then restarting 
 *        the application.
 * 
 * @note Errors that occur while attempting to query the user cause the
 *       spinForever function to be called.
 * 
 * @param nvsHandle The non-volatile storage handle to store settings in.
 * @param errorOccurred A pointer to a bool that indicates whether an error
 *                      has occurred at any point in the program or not. This
 *                      bool is shared by all tasks and should only be accessed
 *                      after errorOccurredMutex has been obtained, ideally
 *                      through the use of the boolWithTestSet function.
 * @param errorOccurredMutex A handle to a mutex that guards access to the bool
 *                           pointed to by errorOccurred.
 */
void updateNvsSettings(nvs_handle_t nvsHandle, ErrorResources *errResources);

#endif /* UTILITIES_H_ */
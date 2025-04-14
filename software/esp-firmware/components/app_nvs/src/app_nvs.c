/**
 * nvs_settings.c
 * 
 * This file contains functions that interact with non-volatile storage,
 * particularly those that deal with persistent user settings.
 */

#include "app_nvs.h"

#include <malloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/usb_serial_jtag.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "app_err.h"
#include "app_errors.h"
#include "led_registers.h"
#include "main_types.h"
#include "routines.h"

#define TAG "nvs_settings"


#define NVS_MAIN_NAMESPACE "main"
/** @brief The name of the non-volatile storage entry for the wifi SSID. */
#define WIFI_SSID_NVS_NAME "wifi_ssid"
/** @brief The name of the non-volatile storage entry for the wifi password. */
#define WIFI_PASS_NVS_NAME "wifi_pass"

/* NVS namespace and keys */
#define NVS_WORKER_NAMESPACE "worker"
#define CURRENT_NORTH_NVS_KEY "current_north"
#define CURRENT_SOUTH_NVS_KEY "current_south"
#define TYPICAL_NORTH_NVS_KEY "typical_north"
#define TYPICAL_SOUTH_NVS_KEY "typical_south"

/**
* @brief Obtains a handle to the main NVS namespace in read/write mode.
* 
* @returns A handle to the main NVS namespace if successful, otherwise NULL.
*/
nvs_handle_t openMainNvs(void)
{
    esp_err_t err;
    nvs_handle_t handle;

    err = nvs_open(NVS_MAIN_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to open main nvs. err: %d", err);
        return (nvs_handle_t) NULL;
    }
    return handle;
}

/**
* @brief Obtains a handle to the worker NVS namespace in read/write mode.
* 
* @returns A handle to the worker NVS namespace if successful, otherwise NULL.
*/
nvs_handle_t openWorkerNvs(void)
{
    esp_err_t err;
    nvs_handle_t handle;

    err = nvs_open(NVS_WORKER_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open worker nvs. err: %d", err);
        return (nvs_handle_t) NULL;
    }
    return handle;
}

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
 * @returns ESP_OK if entries are found successfully.
 * ESP_ERR_NOT_FOUND if any user setting is not found in NVS.
 * ESP_ERR_INVALID_STATE if any user setting exists in NVS but is not a string.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t nvsEntriesExist(nvs_handle_t nvsHandle)
{
    esp_err_t err;
    nvs_type_t nvsType;
    err = nvs_find_key(nvsHandle, WIFI_SSID_NVS_NAME, &nvsType);
    if (err == ESP_ERR_NOT_FOUND) return ESP_ERR_NOT_FOUND; // don't throw bc could be intentional
    if (err != ESP_OK) THROW_ERR(ESP_FAIL);
    if (nvsType != NVS_TYPE_STR) THROW_ERR(ESP_ERR_INVALID_STATE);

    err = nvs_find_key(nvsHandle, WIFI_PASS_NVS_NAME, &nvsType);
    if (err == ESP_ERR_NOT_FOUND) return ESP_ERR_NOT_FOUND; // don't throw bc could be intentional
    if (err != ESP_OK) THROW_ERR(ESP_FAIL);
    if (nvsType != NVS_TYPE_STR) THROW_ERR(ESP_ERR_INVALID_STATE);
    return ESP_OK;
}

/**
 * @brief Removes any entries in the non-volatile storage main namespace that 
 *        are unnecessary for device operation.
 * 
 * @note Unnecessary NVS entries may exist if a firmware update has been
 *       performed and previously necessary entries have been made obsolete.
 *       All entries that are deemed necessary are those searched for in
 *       the nvsEntriesExist function.
 * 
 * @param nvsHandle A read/write handle to the main NVS namespace.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t removeExtraMainNvsEntries(nvs_handle_t nvsHandle) {
    esp_err_t err;
    nvs_entry_info_t info;
    nvs_iterator_t nvs_iter;

    err = nvs_entry_find_in_handle(nvsHandle, NVS_TYPE_ANY, &nvs_iter);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK; // no elements to remove
    if (err != ESP_OK) return err;
    if (nvs_iter == NULL) return ESP_OK;

    while (err == ESP_OK)
    {
        err = nvs_entry_info(nvs_iter, &info);
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL; // should not occur

        ESP_LOGI(TAG, "key: %s", info.key);
        if (strcmp(info.namespace_name, NVS_MAIN_NAMESPACE) == 0 &&
              (strcmp(info.key, WIFI_SSID_NVS_NAME) == 0 ||
               strcmp(info.key, WIFI_PASS_NVS_NAME) == 0))
        {
            err = nvs_entry_next(&nvs_iter);
            if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
            continue;
        }
  
        ESP_LOGW(TAG, "erasing key: %s", info.key);
        err = nvs_erase_key(nvsHandle, info.key);
        if (err != ESP_OK) {
            return err;
        }
    }

    nvs_release_iterator(nvs_iter);
    err = nvs_commit(nvsHandle);
    return err;
}

/**
 * @brief Removes any entries in the non-volatile storage worker namespace that 
 *        are unnecessary for ddevice operation.
 * 
 * @note Unnecessary NVS entries may exist if a firmware update has been
 *       performed and previously necessary entries have been made obsolete.
 *       All entries that are deemed necessary are those searched for in
 *       the nvsEntriesExist function.
 * 
 * @param nvsHandle A read/write handle to the worker NVS namespace.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t removeExtraWorkerNvsEntries(nvs_handle_t nvsHandle) {
    nvs_entry_info_t info;
    nvs_iterator_t nvs_iter;
    esp_err_t err;

    err = nvs_entry_find_in_handle(nvsHandle, NVS_TYPE_ANY, &nvs_iter);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK; // no entries to remove
    if (err != ESP_OK) return err;
    if (nvs_iter == NULL) return ESP_OK;

    while (err == ESP_OK)
    {
        err = nvs_entry_info(nvs_iter, &info);
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL; // should not occur

        ESP_LOGI(TAG, "key: %s", info.key);
        if (strcmp(info.namespace_name, NVS_WORKER_NAMESPACE) == 0 &&
              (strcmp(info.key, CURRENT_NORTH_NVS_KEY) == 0 ||
               strcmp(info.key, CURRENT_SOUTH_NVS_KEY) == 0 ||
               strcmp(info.key, TYPICAL_NORTH_NVS_KEY) == 0 ||
               strcmp(info.key, TYPICAL_SOUTH_NVS_KEY) == 0))
        {
            err = nvs_entry_next(&nvs_iter);
            if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
            continue;
        }

        ESP_LOGW(TAG, "erasing key: %s", info.key);
        err = nvs_erase_key(nvsHandle, info.key);
        if (err != ESP_OK) {
            return err;
        }
    }

    nvs_release_iterator(nvs_iter);
    err = nvs_commit(nvsHandle);
    return err;
}

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
esp_err_t retrieveNvsEntries(nvs_handle_t nvsHandle, UserSettings *settings)
{
    /* retrieve wifi ssid */
    if (nvs_get_str(nvsHandle, WIFI_SSID_NVS_NAME, NULL, &(settings->wifiSSIDLen)) != ESP_OK) {
        return ESP_FAIL;
    }
    if ((settings->wifiSSID = malloc(settings->wifiSSIDLen)) == NULL) {
        return ESP_FAIL;
    }
    if (nvs_get_str(nvsHandle, WIFI_SSID_NVS_NAME, settings->wifiSSID, &(settings->wifiSSIDLen)) != ESP_OK) {
        free(settings->wifiSSID);
        return ESP_FAIL;
    }
    /* retrieve wifi password */
    if (nvs_get_str(nvsHandle, WIFI_PASS_NVS_NAME, NULL, &(settings->wifiPassLen)) != ESP_OK) {
        return ESP_FAIL;
    }
    if ((settings->wifiPass = malloc(settings->wifiPassLen)) == NULL) {
        free(settings->wifiSSID);
        return ESP_FAIL;
    }
    if (nvs_get_str(nvsHandle, WIFI_PASS_NVS_NAME, settings->wifiPass, &(settings->wifiPassLen)) != ESP_OK) {
        free(settings->wifiSSID);
        free(settings->wifiPass);
        return ESP_FAIL;
    }
    /* dynamically allocated SSID and password will exist for the duration of the program */
    return ESP_OK;
}

/**
 * @brief Stores user settings to non-volatile storage
 * 
 * @param nvsHandle The non-volatile storage handle where settings exist.
 * @param settings The user settings to store in non-volatile storage.
 */
esp_err_t storeNvsSettings(nvs_handle_t nvsHandle, UserSettings settings)
{
    esp_err_t err;
    err = nvs_set_str(nvsHandle, WIFI_SSID_NVS_NAME, settings.wifiSSID);
    if (err != ESP_OK) return err;

    err = nvs_set_str(nvsHandle, WIFI_PASS_NVS_NAME, settings.wifiPass);
    if (err != ESP_OK) return err;

    err = nvs_commit(nvsHandle);

    return err;
}

/**
 * @brief Handles errors that are due to a user settings issue by setting the
 *        error LED high, querying the user for new settings, then restarting 
 *        the application.
 * 
 * @note Errors that occur while attempting to query the user cause the
 *       spinForever function to be called.
 * 
 * @param nvsHandle The non-volatile storage handle to store settings in.
 */
void updateNvsSettings(nvs_handle_t nvsHandle) {
    throwHandleableError(); // turns on error LED
    /* flash direction LEDs to indicate settings update is requested */
    esp_timer_handle_t flashDirTimer = createDirectionFlashTimer();
    if (flashDirTimer == NULL)
    {
        throwFatalError();
    }
    if (esp_timer_start_periodic(flashDirTimer, CONFIG_ERROR_PERIOD * 1000) != ESP_OK)
    {
        throwFatalError();
    }
    /* request settings update from user */
    if (getNvsEntriesFromUser(nvsHandle) != ESP_OK)
    {
        throwFatalError();
    }

    if (esp_timer_stop(flashDirTimer) != ESP_OK ||
        esp_timer_delete(flashDirTimer) != ESP_OK)
    {
        throwFatalError();
    }

    resolveHandleableError(false); // returns error LED to previous state
}

/**
 * @brief Updates the data stored in the provided array by querying it from
 *        non-volatile storage.
 * 
 * @param[out] data The destination of the retrieved data.
 * @param[in] dir The direction of data to retrieve.
 * @param[in] category The category of data to retrieve.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid argument.
 *          ESP_ERR_INVALID_SIZE if retrieved data is incorrect size.
 *          Various error codes passed from NVS functions.
 *          ESP_FAIL if something unexpected occurred.
 */
esp_err_t refreshSpeedsFromNVS(LEDData data[static MAX_NUM_LEDS_REG], Direction dir, SpeedCategory category)
{
    esp_err_t err;
    nvs_handle_t nvsHandle;
    size_t size;
    char *key;
    /* open nvs */
    err = nvs_open(NVS_WORKER_NAMESPACE, NVS_READONLY, &nvsHandle);
    if (err != ESP_OK) return err;

    /* determine proper nvs key */
    key = NULL;
    switch (dir)
    {
        case NORTH:
            switch(category)
            {
              case LIVE:
                  key = CURRENT_NORTH_NVS_KEY;
                  break;
              case TYPICAL:
                  key = TYPICAL_NORTH_NVS_KEY;
                  break;
              default:
                  return ESP_ERR_INVALID_ARG;
            }
            break;
        case SOUTH:
            switch(category)
            {
                case LIVE:
                    key = CURRENT_SOUTH_NVS_KEY;
                    break;
                case TYPICAL:
                    key = TYPICAL_SOUTH_NVS_KEY;
                    break;
                default:
                    return ESP_ERR_INVALID_ARG;
            }
            break;
    }
    if (key == NULL) return ESP_FAIL;

    /* retrieve NVS data */
    size = MAX_NUM_LEDS_REG * sizeof(LEDData);
    err = nvs_get_blob(nvsHandle, key, data, &size);
    if (err != ESP_OK) {
        return err;
    }
    if (size == 0 || size / sizeof(uint8_t) != MAX_NUM_LEDS_REG * sizeof(LEDData))
    {
        return ESP_ERR_INVALID_SIZE;
    }
    nvs_close(nvsHandle);
    return ESP_OK;
}

esp_err_t storeSpeedsToNVS(LEDData data[static MAX_NUM_LEDS_REG], Direction dir, SpeedCategory category)
{
    esp_err_t err;
    nvs_handle_t nvsHandle;
    size_t size;
    char *key;
    /* open nvs */
    err = nvs_open(NVS_WORKER_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) return err;

    /* determine proper nvs key */
    key = NULL;
    switch (dir)
    {
        case NORTH:
            switch(category)
            {
                case LIVE:
                    key = CURRENT_NORTH_NVS_KEY;
                    break;
                case TYPICAL:
                    key = TYPICAL_NORTH_NVS_KEY;
                    break;
                default:
                    return ESP_ERR_INVALID_ARG;
            }
            break;
        case SOUTH:
            switch(category)
            {
                case LIVE:
                    key = CURRENT_SOUTH_NVS_KEY;
                    break;
                case TYPICAL:
                    key = TYPICAL_SOUTH_NVS_KEY;
                    break;
                default:
                    return ESP_ERR_INVALID_ARG;
            }
        break;
    }
    if (key == NULL) return ESP_FAIL;

    /* store data to NVS */
    size = MAX_NUM_LEDS_REG * sizeof(LEDData);
    err = nvs_set_blob(nvsHandle, key, data, size);
    if (err != ESP_OK)
    {
        err = nvs_erase_key(nvsHandle, key);
        if (err != ESP_OK) return err;
        err = nvs_set_blob(nvsHandle, key, data, size);
        if (err != ESP_OK) return err;
    }    
    err = nvs_commit(nvsHandle);
    if (err != ESP_OK) return err;
    nvs_close(nvsHandle);
    return ESP_OK;
}

#if CONFIG_HARDWARE_VERSION == 1

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
esp_err_t getNvsEntriesFromUser(nvs_handle_t nvsHandle) {
    const unsigned short bufLen = CONFIG_NVS_ENTRY_BUFFER_LENGTH;
    char c;
    char buf[bufLen];
    esp_err_t err;
    printf("\nWifi SSID: ");
    fflush(stdout);
    for (int i = 0; i < bufLen; i++)
    {
        buf[i] = getchar();
        if (buf[i] == '\n')
        {
            buf[i] = '\0';
            break;
        }
        printf("%c", buf[i]);
        fflush(stdout);
    }
    while ((c = getchar()) != '\n') {}
    buf[bufLen - 1] = '\0'; // in case the user writes too much
    fflush(stdout);
    ESP_RETURN_ON_ERROR(
        nvs_set_str(nvsHandle, WIFI_SSID_NVS_NAME, buf),
        TAG, "failed to write wifi SSID to non-volatile storage"
    );
    printf("\nWifi Password: ");
    fflush(stdout);
    for (int i = 0; i < bufLen; i++)
    {
        buf[i] = getchar();
        if (buf[i] == '\n')
        {
            buf[i] = '\0';
            break;
        }
        printf("%c", buf[i]);
        fflush(stdout);
    }
    while ((c = getchar()) != '\n') {}
    buf[bufLen] = '\0'; // in case the user writes too much
    fflush(stdout);
    err = nvs_set_str(nvsHandle, WIFI_PASS_NVS_NAME, buf);
    if (err != ESP_OK) THROW_ERR(err);
    err = nvs_commit(nvsHandle);
    if (err != ESP_OK) THROW_ERR(ESP_FAIL);
    return ESP_OK;
}

#elif CONFIG_HARDWARE_VERSION == 2

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
esp_err_t getNvsEntriesFromUser(nvs_handle_t nvsHandle) {
    const unsigned short bufLen = CONFIG_NVS_ENTRY_BUFFER_LENGTH;
    char c;
    char *str;
    char buf[bufLen];
    int numBytes;
    esp_err_t err;
    str = "\nWifi SSID: ";
    do {
        numBytes = usb_serial_jtag_write_bytes(str, strlen(str), INT_MAX);
    } while (numBytes == 0);
    if (numBytes != strlen(str)) return ESP_FAIL;

    /* read user input into buffer */
    for (int i = 0; i < bufLen; i++) {
        do {
            numBytes = usb_serial_jtag_read_bytes(&buf[i], 1, INT_MAX);
        } while (numBytes == 0);
        if (numBytes != 1) return ESP_FAIL;
    
        if (buf[i] == '\n' || buf[i] == '\r') {
            c = buf[i];
            do {
                numBytes = usb_serial_jtag_write_bytes("\r", 1, INT_MAX);
            } while (numBytes == 0);
            if (numBytes != 1) return ESP_FAIL;
            buf[i] = '\0';
            break;
        }
        do {
            numBytes = usb_serial_jtag_write_bytes(&buf[i], 1, INT_MAX);
        } while (numBytes == 0);
        if (numBytes != 1) return ESP_FAIL;

        c = buf[i];
    }
    /* ignore the rest of what the user is typing */
    while (c != '\n' && c != '\r')
    {
        do {
            numBytes = usb_serial_jtag_read_bytes(&c, 1, INT_MAX);
        } while (numBytes == 0);
        if (numBytes != 1) return ESP_FAIL;
    }
    buf[bufLen - 1] = '\0'; // in case the user writes too much
    ESP_RETURN_ON_ERROR(
        nvs_set_str(nvsHandle, WIFI_SSID_NVS_NAME, buf),
        TAG, "failed to write wifi SSID to non-volatile storage"
    );

    str = "\nWifi Password: ";
    do {
        numBytes = usb_serial_jtag_write_bytes(str, strlen(str), INT_MAX);
    } while (numBytes == 0);
    if (numBytes != strlen(str)) return ESP_FAIL;

    /* read user input into buffer */
    for (int i = 0; i < bufLen; i++) {
        do {
            numBytes = usb_serial_jtag_read_bytes(&buf[i], 1, INT_MAX);
        } while (numBytes == 0);
        if (numBytes != 1) return ESP_FAIL;
    
        if (buf[i] == '\n'|| buf[i] == '\r') {
            c = buf[i];
            do {
                numBytes = usb_serial_jtag_write_bytes("\r", 1, INT_MAX);
            } while (numBytes == 0);
            if (numBytes != 1) return ESP_FAIL;
            buf[i] = '\0';
            break;
        }
        do {
            numBytes = usb_serial_jtag_write_bytes(&buf[i], 1, INT_MAX);
        } while (numBytes == 0);
        if (numBytes != 1) return ESP_FAIL;
        c = buf[i];
    }
    /* ignore the rest of what the user is typing */
    while (c != '\n' && c != '\r')
    {
        do {
            numBytes = usb_serial_jtag_read_bytes(&c, 1, INT_MAX);
        } while (numBytes == 0);
        if (numBytes != 1) return ESP_FAIL;
    }
    buf[bufLen - 1] = '\0'; // in case the user writes too much
    err = nvs_set_str(nvsHandle, WIFI_PASS_NVS_NAME, buf);
    if (err != ESP_OK) THROW_ERR(err);
    err = nvs_commit(nvsHandle);
    if (err != ESP_OK) THROW_ERR(ESP_FAIL);
    return ESP_OK;
}

#else
#error "Unsupported hardware version!"
#endif
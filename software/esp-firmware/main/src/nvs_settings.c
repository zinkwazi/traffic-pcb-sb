/**
 * nvs_settings.c
 * 
 * This file contains functions that interact with non-volatile storage,
 * particularly those that deal with persistent user settings.
 */

#include "nvs_settings.h"

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

#include "app_errors.h"
#include "api_connect.h"
#include "led_registers.h"

#include "main_types.h"
#include "routines.h"

#define TAG "nvs_settings"

/** @brief The name of the non-volatile storage entry for the wifi SSID. */
#define WIFI_SSID_NVS_NAME "wifi_ssid"

/** @brief The name of the non-volatile storage entry for the wifi password. */
#define WIFI_PASS_NVS_NAME "wifi_pass"

#define NVS_MAIN_NAMESPACE "main"

/* NVS namespace and keys */
#define WORKER_NVS_NAMESPACE "worker"
#define CURRENT_NORTH_NVS_KEY "current_north"
#define CURRENT_SOUTH_NVS_KEY "current_south"
#define TYPICAL_NORTH_NVS_KEY "typical_north"
#define TYPICAL_SOUTH_NVS_KEY "typical_south"

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
esp_err_t nvsEntriesExist(nvs_handle_t nvsHandle) {
  esp_err_t ret;
  nvs_type_t nvsType;
  ret = nvs_find_key(nvsHandle, WIFI_SSID_NVS_NAME, &nvsType);
  ESP_RETURN_ON_FALSE(
    (ret == ESP_OK && nvsType == NVS_TYPE_STR), ret,
    TAG, "failed to lookup wifi ssid in non-volatile storage"
  );
  ret = nvs_find_key(nvsHandle, WIFI_PASS_NVS_NAME, &nvsType);
  ESP_RETURN_ON_FALSE(
    (ret == ESP_OK && nvsType == NVS_TYPE_STR), ret,
    TAG, "failed to lookup wifi password in non-volatile storage"
  );
  return ret;
}

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
esp_err_t removeExtraMainNvsEntries(nvs_handle_t nvsHandle) {
  esp_err_t ret;
  nvs_iterator_t nvs_iter;

  ret = nvs_entry_find_in_handle(nvsHandle, NVS_TYPE_ANY, &nvs_iter);
  if (ret == ESP_ERR_NVS_NOT_FOUND)
  {
    return ESP_OK;
  } else if (ret != ESP_OK)
  {
    return ESP_FAIL;
  }
  ret = nvs_entry_next(&nvs_iter);
  while (ret != ESP_OK) {
    nvs_entry_info_t info;
    if (nvs_entry_info(nvs_iter, &info) != ESP_OK) {
      return ESP_FAIL;
    }
    if (strcmp(info.namespace_name, NVS_MAIN_NAMESPACE) == 0 &&
            (strcmp(info.key, WIFI_SSID_NVS_NAME) == 0 ||
             strcmp(info.key, WIFI_PASS_NVS_NAME) == 0))
    {
      ret = nvs_entry_next(&nvs_iter);
      continue;
    }
    if (nvs_erase_key(nvsHandle, info.key) != ESP_OK) {
      return ESP_FAIL;
    }
    ret = nvs_entry_next(&nvs_iter);
  }
  if (nvs_commit(nvsHandle) != ESP_OK) {
    return ESP_FAIL;
  }
  if (ret == ESP_ERR_INVALID_ARG) {
    return ESP_FAIL;
  }
  return ESP_OK;
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
    ESP_LOGI(TAG, "failed to retrive wifi ssid length");
    return ESP_FAIL;
  }
  if ((settings->wifiSSID = malloc(settings->wifiSSIDLen)) == NULL) {
    ESP_LOGI(TAG, "failed to allocate mem1");
    return ESP_FAIL;
  }
  if (nvs_get_str(nvsHandle, WIFI_SSID_NVS_NAME, settings->wifiSSID, &(settings->wifiSSIDLen)) != ESP_OK) {
    free(settings->wifiSSID);
    ESP_LOGI(TAG, "failed to retrieve wifi ssid");
    return ESP_FAIL;
  }
  /* retrieve wifi password */
  if (nvs_get_str(nvsHandle, WIFI_PASS_NVS_NAME, NULL, &(settings->wifiPassLen)) != ESP_OK) {
    ESP_LOGI(TAG, "failed to retrieve password length");
    return ESP_FAIL;
  }
  if ((settings->wifiPass = malloc(settings->wifiPassLen)) == NULL) {
    free(settings->wifiSSID);
    ESP_LOGI(TAG, "failed to allocate mem");
    return ESP_FAIL;
  }
  if (nvs_get_str(nvsHandle, WIFI_PASS_NVS_NAME, settings->wifiPass, &(settings->wifiPassLen)) != ESP_OK) {
    free(settings->wifiSSID);
    free(settings->wifiPass);
    ESP_LOGI(TAG, "failed to retrieve password");
    return ESP_FAIL;
  }
  /* dynamically allocated SSID and password will exist for the duration of the program */
  return ESP_OK;
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
 * @param errorOccurred A pointer to a bool that indicates whether an error
 *                      has occurred at any point in the program or not. This
 *                      bool is shared by all tasks and should only be accessed
 *                      after errorOccurredMutex has been obtained, ideally
 *                      through the use of the boolWithTestSet function.
 * @param errorOccurredMutex A handle to a mutex that guards access to the bool
 *                           pointed to by errorOccurred.
 */
void updateNvsSettings(nvs_handle_t nvsHandle, ErrorResources *errRes) {
  throwHandleableError(errRes, false); // turns on error LED
  /* flash direction LEDs to indicate settings update is requested */
  esp_timer_handle_t flashDirTimer = createDirectionFlashTimer();
  if (flashDirTimer == NULL) {
    throwFatalError(errRes, false);
  }
  if (esp_timer_start_periodic(flashDirTimer, CONFIG_ERROR_PERIOD * 1000) != ESP_OK) {
    throwFatalError(errRes, false);
  }
  /* request settings update from user */
  if (getNvsEntriesFromUser(nvsHandle) != ESP_OK) {
    throwFatalError(errRes, false);
  }

  if (esp_timer_stop(flashDirTimer) != ESP_OK ||
      esp_timer_delete(flashDirTimer) != ESP_OK)
  {
    throwFatalError(errRes, false);
  }

  resolveHandleableError(errRes, false, false); // returns error LED to previous state
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
    /* input guards */
    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    /* open nvs */
    err = nvs_open(WORKER_NVS_NAMESPACE, NVS_READONLY, &nvsHandle);
    if (err != ESP_OK) {
      return err;
    }
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
    if (key == NULL) {
      return ESP_FAIL;
    }
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
  /* input guards */
  if (data == NULL)
  {
      return ESP_ERR_INVALID_ARG;
  }
  /* open nvs */
  err = nvs_open(WORKER_NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
  if (err != ESP_OK) {
    return err;
  }
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
  if (key == NULL) {
    return ESP_FAIL;
  }
  /* store data to NVS */
  size = MAX_NUM_LEDS_REG * sizeof(LEDData);
  err = nvs_set_blob(nvsHandle, key, data, size);
  if (err != ESP_OK) {
      err = nvs_erase_key(nvsHandle, key);
      if (err != ESP_OK) {
        return err;
      }
      err = nvs_set_blob(nvsHandle, key, data, size);
      if (err != ESP_OK) {
        return err;
      }
  }    
  err = nvs_commit(nvsHandle);
  if (err != ESP_OK) {
    return err;
  }
  nvs_close(nvsHandle);
  return ESP_OK;
}

esp_err_t removeExtraWorkerNvsEntries(void) {
  esp_err_t ret;
  nvs_iterator_t nvs_iter;
  nvs_handle_t nvsHandle;
  esp_err_t err;
  err = nvs_open(WORKER_NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
  if (err != ESP_OK) {
    return ESP_FAIL;
  }
  err = nvs_entry_find_in_handle(nvsHandle, NVS_TYPE_ANY, &nvs_iter);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    return ESP_OK; // no entries to remove
  } else if (err != ESP_OK) {
    return ESP_FAIL;
  }
  if (nvs_iter == NULL) {
    return ESP_OK;
  }
  ret = nvs_entry_next(&nvs_iter);
  while (ret != ESP_OK) {
    nvs_entry_info_t info;
    if (nvs_iter == NULL) {
        return ESP_OK;
    }
    err = nvs_entry_info(nvs_iter, &info);
    if (err != ESP_OK) {
      return ESP_FAIL;
    }
    if (strcmp(info.namespace_name, WORKER_NVS_NAMESPACE) == 0 &&
            (strcmp(info.key, CURRENT_NORTH_NVS_KEY) == 0 ||
             strcmp(info.key, CURRENT_SOUTH_NVS_KEY) == 0 ||
             strcmp(info.key, TYPICAL_NORTH_NVS_KEY) == 0 ||
             strcmp(info.key, TYPICAL_SOUTH_NVS_KEY) == 0))
    {
      ret = nvs_entry_next(&nvs_iter);
      continue;
    }
    err = nvs_erase_key(nvsHandle, info.key);
    if (err != ESP_OK) {
      return ESP_FAIL;
    }
    ret = nvs_entry_next(&nvs_iter);
  }
  if (nvs_commit(nvsHandle) != ESP_OK) {
    return ESP_FAIL;
  }
  if (ret == ESP_ERR_INVALID_ARG) {
    return ESP_FAIL;
  }
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
  printf("\nWifi SSID: ");
  fflush(stdout);
  for (int i = 0; i < bufLen; i++) {
    buf[i] = getchar();
    if (buf[i] == '\n') {
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
  for (int i = 0; i < bufLen; i++) {
    buf[i] = getchar();
    if (buf[i] == '\n') {
      buf[i] = '\0';
      break;
    }
    printf("%c", buf[i]);
    fflush(stdout);
  }
  while ((c = getchar()) != '\n') {}
  buf[bufLen] = '\0'; // in case the user writes too much
  fflush(stdout);
  ESP_RETURN_ON_ERROR(
    nvs_set_str(nvsHandle, WIFI_PASS_NVS_NAME, buf),
    TAG, "failed to write wifi password to non-volatile storage"
  );
  ESP_RETURN_ON_ERROR(
    nvs_commit(nvsHandle),
    TAG, "failed to commit NVS changes"
  );
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
  str = "\nWifi SSID: ";
  do {
    numBytes = usb_serial_jtag_write_bytes(str, strlen(str), INT_MAX);
  } while (numBytes == 0);
  if (numBytes != strlen(str))
  {
    return ESP_FAIL;
  }

  /* read user input into buffer */
  for (int i = 0; i < bufLen; i++) {
    
    do {
      numBytes = usb_serial_jtag_read_bytes(&buf[i], 1, INT_MAX);
    } while (numBytes == 0);
    if (numBytes != 1)
    {
      return ESP_FAIL;
    }
    
    if (buf[i] == '\n' || buf[i] == '\r') {
      c = buf[i];
      do {
        numBytes = usb_serial_jtag_write_bytes("\r", 1, INT_MAX);
      } while (numBytes == 0);
      if (numBytes != 1)
      {
        return ESP_FAIL;
      }
      buf[i] = '\0';
      break;
    }
    do {
      numBytes = usb_serial_jtag_write_bytes(&buf[i], 1, INT_MAX);
    } while (numBytes == 0);
    if (numBytes != 1)
    {
      return ESP_FAIL;
    }

    c = buf[i];
  }
  /* ignore the rest of what the user is typing */
  while (c != '\n' && c != '\r')
  {
    do {
      numBytes = usb_serial_jtag_read_bytes(&c, 1, INT_MAX);
    } while (numBytes == 0);
    if (numBytes != 1)
    {
      return ESP_FAIL;
    }
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
  if (numBytes != strlen(str))
  {
    return ESP_FAIL;
  }

  /* read user input into buffer */
  for (int i = 0; i < bufLen; i++) {
    
    do {
      numBytes = usb_serial_jtag_read_bytes(&buf[i], 1, INT_MAX);
    } while (numBytes == 0);
    if (numBytes != 1)
    {
      return ESP_FAIL;
    }
    
    if (buf[i] == '\n'|| buf[i] == '\r') {
      c = buf[i];
      do {
        numBytes = usb_serial_jtag_write_bytes("\r", 1, INT_MAX);
      } while (numBytes == 0);
      if (numBytes != 1)
      {
        return ESP_FAIL;
      }
      buf[i] = '\0';
      break;
    }
    do {
      numBytes = usb_serial_jtag_write_bytes(&buf[i], 1, INT_MAX);
    } while (numBytes == 0);
    if (numBytes != 1)
    {
      return ESP_FAIL;
    }
    c = buf[i];
  }
  /* ignore the rest of what the user is typing */
  while (c != '\n' && c != '\r')
  {
    do {
      numBytes = usb_serial_jtag_read_bytes(&c, 1, INT_MAX);
    } while (numBytes == 0);
    if (numBytes != 1)
    {
      return ESP_FAIL;
    }
  }
  buf[bufLen - 1] = '\0'; // in case the user writes too much
  ESP_RETURN_ON_ERROR(
    nvs_set_str(nvsHandle, WIFI_PASS_NVS_NAME, buf),
    TAG, "failed to write wifi password to non-volatile storage"
  );

  ESP_RETURN_ON_ERROR(
    nvs_commit(nvsHandle),
    TAG, "failed to commit NVS changes"
  );
  return ESP_OK;
}

#else
#error "Unsupported hardware version!"
#endif
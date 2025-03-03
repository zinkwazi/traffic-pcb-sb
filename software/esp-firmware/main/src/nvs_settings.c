
#include "nvs_settings.h"

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "esp_check.h"

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
  if (nvs_entry_find_in_handle(nvsHandle, NVS_TYPE_ANY, &nvs_iter) != ESP_OK) {
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
    ESP_LOGD(TAG, "removing nvs entry: %s", info.key);
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
  ESP_LOGD(TAG, "Querying settings from user...");
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
  buf[bufLen] = '\0'; // in case the user writes too much
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

esp_err_t getSpeedsFromNvs(LEDData *speeds, uint32_t speedsLen, Direction dir, bool currentSpeeds) {
  nvs_handle_t nvsHandle;
  size_t size = MAX_NUM_LEDS_REG;
  if (nvs_open(WORKER_NVS_NAMESPACE, NVS_READONLY, &nvsHandle) != ESP_OK) {
      return ESP_FAIL;
  }
  char *key = NULL;
  if (currentSpeeds) {
      key = (dir == NORTH) ? CURRENT_NORTH_NVS_KEY : CURRENT_SOUTH_NVS_KEY;
  } else {
      key = (dir == NORTH) ? TYPICAL_NORTH_NVS_KEY : TYPICAL_SOUTH_NVS_KEY;
  }
  if (nvs_get_blob(nvsHandle, key, speeds, &size) != ESP_OK) {
      return ESP_FAIL;
  }
  if (size == 0 || size / sizeof(uint8_t) != speedsLen * sizeof(LEDData)) {
      return ESP_FAIL;
  }
  nvs_close(nvsHandle);
  return ESP_OK;
}

esp_err_t setSpeedsToNvs(LEDData *speeds, uint32_t speedsLen, Direction dir, bool currentSpeeds) {
  nvs_handle_t nvsHandle;
  size_t size = speedsLen * sizeof(LEDData);
  esp_err_t err = ESP_OK;
  if ((err = nvs_open(WORKER_NVS_NAMESPACE, NVS_READWRITE, &nvsHandle)) != ESP_OK) {
      return ESP_FAIL;
  }
  char *key = NULL;
  if (currentSpeeds) {
      key = (dir == NORTH) ? CURRENT_NORTH_NVS_KEY : CURRENT_SOUTH_NVS_KEY;
  } else {
      key = (dir == NORTH) ? TYPICAL_NORTH_NVS_KEY : TYPICAL_SOUTH_NVS_KEY;
  }
  err = nvs_set_blob(nvsHandle, key, speeds, size);
  if (err != ESP_OK) {
      err = nvs_erase_key(nvsHandle, key);
      if (err != ESP_OK) {
          return ESP_FAIL;
      }
      err = nvs_set_blob(nvsHandle, key, speeds, size);
      if (err != ESP_OK) {
          return ESP_FAIL;
      }
  }    
  if (nvs_commit(nvsHandle) != ESP_OK) {
      return ESP_FAIL;
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
    ESP_LOGI(TAG, "ret is invalid arg");
    return ESP_FAIL;
  }
  return ESP_OK;
}
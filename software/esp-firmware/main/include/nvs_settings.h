/**
 * nvs_settings.h
 * 
 * This file contains functions that interact with non-volatile storage,
 * particularly those that deal with persistent user settings.
 */

#ifndef NVS_H_
#define NVS_H_

#include "nvs.h"
#include "esp_err.h"

#include "app_errors.h"
#include "api_connect.h"

#include "main_types.h"

esp_err_t nvsEntriesExist(nvs_handle_t nvsHandle);
esp_err_t retrieveNvsEntries(nvs_handle_t nvsHandle, UserSettings *settings);
esp_err_t removeExtraMainNvsEntries(nvs_handle_t nvsHandle);
esp_err_t getNvsEntriesFromUser(nvs_handle_t nvsHandle);
void updateNvsSettings(nvs_handle_t nvsHandle, ErrorResources *errResources);
esp_err_t getSpeedsFromNvs(LEDData *speeds, uint32_t speedsLen, Direction dir, bool currentSpeeds);
esp_err_t setSpeedsToNvs(LEDData *speeds, uint32_t speedsLen, Direction dir, bool currentSpeeds);
esp_err_t removeExtraWorkerNvsEntries(void);

#endif /* NVS_H_ */
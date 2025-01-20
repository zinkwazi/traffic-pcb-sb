/**
 * nvs_settings.h
 * 
 * This file contains functions that interact with non-volatile storage,
 * particularly those that deal with persistent user settings.
 */

#ifndef NVS_H_
#define NVS_H_

#include "nvs.h"
#include "app_errors.h"

esp_err_t nvsEntriesExist(nvs_handle_t nvsHandle);
esp_err_t removeExtraMainNvsEntries(nvs_handle_t nvsHandle);
esp_err_t getNvsEntriesFromUser(nvs_handle_t nvsHandle);
void updateNvsSettings(nvs_handle_t nvsHandle, ErrorResources *errResources);

#endif /* NVS_H_ */
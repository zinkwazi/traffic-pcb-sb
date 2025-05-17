/**
 * app_nvs.h
 * 
 * This file contains functions that interact with non-volatile storage,
 * particularly those that deal with persistent user settings.
 */

#ifndef APP_NVS_H_4_9_25
#define APP_NVS_H_4_9_25

#include "esp_err.h"
#include "nvs.h"

#include "app_errors.h"
#include "led_registers.h"

#include "main_types.h"

nvs_handle_t openMainNvs(void);
nvs_handle_t openWorkerNvs(void);
esp_err_t nvsEntriesExist(nvs_handle_t nvsHandle);
esp_err_t retrieveNvsEntries(nvs_handle_t nvsHandle, UserSettings *settings);
esp_err_t storeNvsSettings(nvs_handle_t nvsHandle, UserSettings settings);
esp_err_t removeExtraMainNvsEntries(nvs_handle_t nvsHandle);
esp_err_t removeExtraWorkerNvsEntries(nvs_handle_t nvsHandle);
esp_err_t getNvsEntriesFromUser(nvs_handle_t nvsHandle);
void updateNvsSettings(nvs_handle_t nvsHandle);
esp_err_t refreshSpeedsFromNVS(LEDData data[], Direction dir, SpeedCategory category);
esp_err_t storeSpeedsToNVS(LEDData data[], Direction dir, SpeedCategory category);


#endif /* APP_NVS_H_4_9_25 */
/**
 * ota_pi.h
 * 
 * Private interface of ota.c. This is not written directly in the source file
 * to facilitate white-box unit testing and function mocking.
 */

#ifndef OTA_PI_H_3_27_25
#define OTA_PI_H_3_27_25

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_http_client.h"

#include "ota_types.h"
#include "utilities.h"

/* private interface */

STATIC_IF_NOT_TEST void vOTATask(void* pvParameters);
STATIC_IF_NOT_TEST esp_err_t versionFromKey(VersionType *verType, const char str[], int strLen);
STATIC_IF_NOT_TEST UpdateType compareVersions(VersionInfo serverVer);
STATIC_IF_NOT_TEST esp_err_t processOTAAvailableFile(bool *available, bool *patch, esp_http_client_handle_t client);

#ifndef CONFIG_DISABLE_TESTING_FEATURES

SemaphoreHandle_t getPerformedUpdateSema(void);
esp_err_t initPerformedUpdateSema(void);
void setUpdateFails(bool fails);

#endif /* CONFIG_DISABLE_TESTING_FEATURES */

#endif /* OTA_PI_H_3_27_25 */
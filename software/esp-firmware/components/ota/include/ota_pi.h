/**
 * ota_pi.h
 * 
 * Private interface of ota.c for white-box unit testing.
 */

#ifndef OTA_PI_H_3_27_25
#define OTA_PI_H_3_27_25
#ifndef CONFIG_DISABLE_TESTING_FEATURES

#include <stdbool.h>

#include "esp_err.h"
#include "esp_http_client.h"

enum VersionType {
    HARDWARE = 1,
    REVISION = 2,
    MAJOR = 3,
    MINOR = 4,
    PATCH = 5,
    VER_TYPE_UNKNOWN = 6, // end of enum bounds
};

typedef enum VersionType VersionType;

void vOTATask(void* pvParameters);
esp_err_t versionFromKey(VersionType *verType, const char str[], int strLen);
bool compareVersions(uint hardVer, uint revVer, uint majorVer, uint minorVer, uint patchVer);
esp_err_t processOTAAvailableFile(bool *available, esp_http_client_handle_t client);
esp_err_t queryOTAUpdateAvailable(bool *available);

/* These getters and setters override versioning macros. They must
   be set before calling compareVersions. */

void setHardwareVersion(uint version);
void setHardwareRevision(uint version);
void setFirmwareMajorVersion(uint version);
void setFirmwareMinorVersion(uint version);
void setFirmwarePatchVersion(uint version);

uint getHardwareVersion(void);
uint getHardwareRevision(void);
uint getFirmwareMajorVersion(void);
uint getFirmwareMinorVersion(void);
uint getFirmwarePatchVersion(void);

#endif /* CONFIG_DISABLE_TESTING_FEATURES */
#endif /* OTA_PI_H_3_27_25 */
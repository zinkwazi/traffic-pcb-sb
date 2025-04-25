/**
 * ota_types.h
 * 
 * Types necessary for ota.c, in this file for testing purposes.
 */

#ifndef OTA_TYPES_H_4_18_25
#define OTA_TYPES_H_4_18_25

#include <stdint.h>

enum VersionType {
    HARDWARE = 1,
    REVISION = 2,
    MAJOR = 3,
    MINOR = 4,
    PATCH = 5,
    VER_TYPE_UNKNOWN = 6, // end of enum bounds
};

typedef enum VersionType VersionType;

enum UpdateType {
    UPDATE_NONE,
    UPDATE_MAJOR,
    UPDATE_MINOR,
    UPDATE_PATCH,
};

typedef enum UpdateType UpdateType;

struct VersionInfo {
    uint32_t hardwareVer;
    uint32_t revisionVer;
    uint32_t majorVer;
    uint32_t minorVer;
    uint32_t patchVer;
};

typedef struct VersionInfo VersionInfo;

#endif /* OTA_TYPES_H_4_28_25 */
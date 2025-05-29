/**
 * ota/test/resources/createOTATaskResources.h
 * 
 * Contains extern definitions for createOTATask testing files.
 */

#ifndef CREATE_OTA_TASK_RESOURCES_H_5_29_25
#define CREATE_OTA_TASK_RESOURCES_H_5_29_25

#include "mock_esp_http_client.h"

#define MOCK_ENDPOINT(name) \
    const MockHttpEndpoint name = { \
        .url = "https://bearanvil.com/createOTATask_" #name ".json", \
        .responseCode = 200, \
        .response = name##_start, \
        .contentLen = name##_end - name##_start + 1, \
    };

extern const char version_start[] asm("_binary_createOTATask_version_json_start");
extern const char version_end[] asm("_binary_createOTATask_version_json_end");

extern const char indicatesCorrectly_start[] asm("_binary_createOTATask_indicatesCorrectly_json_start");
extern const char indicatesCorrectly_end[] asm("_binary_createOTATask_indicatesCorrectly_json_end");

#endif /* CREATE_OTA_TASK_RESOURCES_H_5_29_25 */
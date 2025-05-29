/**
 * resources/queryOTAResources.h
 * 
 * Contains resources for queryOTA unit tests.
 */

#ifndef QUERY_OTA_RESOURCES_H_5_29_25
#define QUERY_OTA_RESOURCES_H_5_29_25

#include "mock_esp_http_client.h"

#define MOCK_ENDPOINT(name) \
    const MockHttpEndpoint name = { \
        .url = "https://bearanvil.com/queryOTA_" #name ".json", \
        .responseCode = 200, \
        .response = name##_start, \
        .contentLen = name##_end - name##_start + 1, \
    };

extern const char version1_start[] asm("_binary_queryOTA_version1_json_start");
extern const char version1_end[] asm("_binary_queryOTA_version1_json_end");

#endif /* QUERY_OTA_RESOURCES_H_5_29_25 */
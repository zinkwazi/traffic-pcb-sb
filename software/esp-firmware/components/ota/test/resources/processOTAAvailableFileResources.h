/**
 * ota/test/resources/processOTAAvailableFileResources.h
 * 
 * Contains extern definitions for processOTAAvailable testing files.
 */

#include "mock_esp_http_client.h"

#define MOCK_ENDPOINT(name) \
    const MockHttpEndpoint name = { \
        .url = "https://bearanvil.com/processOTAAvailableFile_" #name ".json", \
        .responseCode = 200, \
        .response = name##_start, \
        .contentLen = name##_end - name##_start - 1, \
    };

extern const char comments0_start[] asm("_binary_processOTAAvailableFile_comments0_json_start");
extern const char comments0_end[] asm("_binary_processOTAAvailableFile_comments0_json_end");

extern const char comments1_start[] asm("_binary_processOTAAvailableFile_comments1_json_start");
extern const char comments1_end[] asm("_binary_processOTAAvailableFile_comments1_json_end");

extern const char comments2_start[] asm("_binary_processOTAAvailableFile_comments2_json_start");
extern const char comments2_end[] asm("_binary_processOTAAvailableFile_comments2_json_end");

extern const char comments3_start[] asm("_binary_processOTAAvailableFile_comments3_json_start");
extern const char comments3_end[] asm("_binary_processOTAAvailableFile_comments3_json_end");

extern const char ignore1_start[] asm("_binary_processOTAAvailableFile_ignore1_json_start");
extern const char ignore1_end[] asm("_binary_processOTAAvailableFile_ignore1_json_end");

extern const char invalid1_start[] asm("_binary_processOTAAvailableFile_invalid1_json_start");
extern const char invalid1_end[] asm("_binary_processOTAAvailableFile_invalid1_json_end");

extern const char invalid2_start[] asm("_binary_processOTAAvailableFile_invalid2_json_start");
extern const char invalid2_end[] asm("_binary_processOTAAvailableFile_invalid2_json_end");

extern const char invalid3_start[] asm("_binary_processOTAAvailableFile_invalid3_json_start");
extern const char invalid3_end[] asm("_binary_processOTAAvailableFile_invalid3_json_end");

extern const char invalid4_start[] asm("_binary_processOTAAvailableFile_invalid4_json_start");
extern const char invalid4_end[] asm("_binary_processOTAAvailableFile_invalid4_json_end");

extern const char invalid5_start[] asm("_binary_processOTAAvailableFile_invalid5_json_start");
extern const char invalid5_end[] asm("_binary_processOTAAvailableFile_invalid5_json_end");

extern const char invalid6_start[] asm("_binary_processOTAAvailableFile_invalid6_json_start");
extern const char invalid6_end[] asm("_binary_processOTAAvailableFile_invalid6_json_end");

extern const char string1_start[] asm("_binary_processOTAAvailableFile_string1_json_start");
extern const char string1_end[] asm("_binary_processOTAAvailableFile_string1_json_end");

extern const char typical1_start[] asm("_binary_processOTAAvailableFile_typical1_json_start");
extern const char typical1_end[] asm("_binary_processOTAAvailableFile_typical1_json_end");

extern const char typical2_start[] asm("_binary_processOTAAvailableFile_typical2_json_start");
extern const char typical2_end[] asm("_binary_processOTAAvailableFile_typical2_json_end");

extern const char typical3_start[] asm("_binary_processOTAAvailableFile_typical3_json_start");
extern const char typical3_end[] asm("_binary_processOTAAvailableFile_typical3_json_end");

extern const char unordered1_start[] asm("_binary_processOTAAvailableFile_unordered1_json_start");
extern const char unordered1_end[] asm("_binary_processOTAAvailableFile_unordered1_json_end");
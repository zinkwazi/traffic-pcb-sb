/**
 * resources/testApiConnectResources.h
 * 
 * Contains resources for api_connect component unit tests.
 */

#ifndef TEST_API_CONNECT_RESOURCES_H_5_29_25
#define TEST_API_CONNECT_RESOURCES_H_5_29_25

#include "mock_esp_http_client.h"

// - 1 on content lengths are to remove null terminators

#define DEFINE_DATA_NORTH_V1_0_5_ENDPOINT(varName) \
    const MockHttpEndpoint varName = { \
        .url = "https://bearanvil.com/current_data/data_north_V1_0_5.csv", \
        .responseCode = 200, \
        .response = data_north_V1_0_5_start, \
        .contentLen = data_north_V1_0_5_end - data_north_V1_0_5_start - 1, \
    }

#define DEFINE_DATA_NORTH_V1_0_3_ENDPOINT(varName) \
    const MockHttpEndpoint varName = { \
        .url = "https://bearanvil.com/current_data/data_north_V1_0_3.dat", \
        .responseCode = 200, \
        .response = data_north_V1_0_3_start, \
        .contentLen = data_north_V1_0_3_end - data_north_V1_0_3_start - 1, \
    }

#define DEFINE_DATA_NORTH_ADD_V1_0_5_ENDPOINT(varName) \
    const MockHttpEndpoint varName = { \
        .url = "https://bearanvil.com/current_data/data_north_V1_0_5.csv_add/V1_0_5.add", \
        .responseCode = 200, \
        .response = data_north_add_V1_0_5_start, \
        .contentLen = data_north_add_V1_0_5_end - data_north_add_V1_0_5_start - 1, \
    }

#define DEFINE_DATA_NORTH_ADD_V2_0_0_ENDPOINT(varName) \
    const MockHttpEndpoint varName = { \
        .url = "https://bearanvil.com/current_data/data_north_V1_0_5.csv_add/V2_0_0.add", \
        .responseCode = 200, \
        .response = data_north_add_V2_0_0_start, \
        .contentLen = data_north_add_V2_0_0_end - data_north_add_V2_0_0_start - 1, \
    }

#define DEFINE_APPENDS_NEWLINE_1_ENDPOINT(varName) \
    const MockHttpEndpoint varName = { \
        .url = "https://bearanvil.com/current_data/getNextResponseBlock_appendsNewline1", \
        .responseCode = 200, \
        .response = getNextResponseBlock_appendsNewline1_start, \
        .contentLen = getNextResponseBlock_appendsNewline1_end - getNextResponseBlock_appendsNewline1_start - 1, \
    }

#define DEFINE_OPEN_SERVER_FILE_TYPICAL_1_ENDPOINT(varName) \
    const MockHttpEndpoint varName = { \
        .url = "https://bearanvil.com/current_data/openServerFile_typical1", \
        .responseCode = 200, \
        .response = openServerFile_typical1_start, \
        .contentLen = openServerFile_typical1_end - openServerFile_typical1_start - 1, \
    }

#define DEFINE_OPEN_SERVER_FILE_ZERO_LENGTH_1_ENDPOINT(varName) \
    const MockHttpEndpoint varName = { \
        .url = "https://bearanvil.com/current_data/openServerFile_zeroContentLength1", \
        .responseCode = 200, \
        .response = openServerFile_zeroContentLength1_start, \
        .contentLen = openServerFile_zeroContentLength1_end - openServerFile_zeroContentLength1_start - 1, \
    }

#define DEFINE_SPEED_DATA_PREINIT_SMALL_FILE_ENDPOINT(varName) \
    const MockHttpEndpoint varName = { \
        .url = "https://bearanvil.com/current_data/speedDataPreinit_smallFile", \
        .responseCode = 200, \
        .response = readServerSpeedDataPreinit_smallFile_start, \
        .contentLen = readServerSpeedDataPreinit_smallFile_end - readServerSpeedDataPreinit_smallFile_start - 1, \
    }

extern const char data_north_V1_0_5_start[] asm("_binary_data_north_V1_0_5_csv_start");
extern const char data_north_V1_0_5_end[] asm("_binary_data_north_V1_0_5_csv_end");

extern const char data_north_V1_0_3_start[] asm("_binary_data_north_V1_0_3_dat_start");
extern const char data_north_V1_0_3_end[] asm("_binary_data_north_V1_0_3_dat_end");

extern const char data_north_add_V1_0_5_start[] asm("_binary_data_north_add_V1_0_5_csv_start");
extern const char data_north_add_V1_0_5_end[] asm("_binary_data_north_add_V1_0_5_csv_end");

extern const char data_north_add_V2_0_0_start[] asm("_binary_data_north_add_V2_0_0_csv_start");
extern const char data_north_add_V2_0_0_end[] asm("_binary_data_north_add_V2_0_0_csv_end");

extern const char getNextResponseBlock_appendsNewline1_start[] asm("_binary_getNextResponseBlock_appendsNewline1_txt_start");
extern const char getNextResponseBlock_appendsNewline1_end[] asm("_binary_getNextResponseBlock_appendsNewline1_txt_end");

extern const char openServerFile_typical1_start[] asm("_binary_openServerFile_typical1_txt_start");
extern const char openServerFile_typical1_end[] asm("_binary_openServerFile_typical1_txt_end");

extern const char openServerFile_zeroContentLength1_start[] asm("_binary_openServerFile_zeroContentLength1_txt_start");
extern const char openServerFile_zeroContentLength1_end[] asm("_binary_openServerFile_zeroContentLength1_txt_end");

extern const char readServerSpeedDataPreinit_smallFile_start[] asm("_binary_readServerSpeedDataPreinit_smallFile_csv_start");
extern const char readServerSpeedDataPreinit_smallFile_end[] asm("_binary_readServerSpeedDataPreinit_smallFile_csv_end");

#endif /* TEST_API_CONNECT_RESOURCES_H_5_29_25 */
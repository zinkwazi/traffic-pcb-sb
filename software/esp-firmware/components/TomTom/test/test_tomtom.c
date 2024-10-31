/**
 * test_tomtom.c
 * 
 * This file includes unit tests for tomtom.c.
 */

#include <stdint.h>
#include "unity.h"
#include "test_tomtom_data.h"
#include "tomtom.c"

TEST_CASE("parseSpeed", "[tomtom]")
{
    esp_err_t retval;
    unsigned int expectedSpeed, actualSpeed, prevSpeed;
    char* typicalChunks[10] = {
        TYPICAL_CHUNK_1, // contains speed 56
        TYPICAL_CHUNK_2,
        TYPICAL_CHUNK_3,
        TYPICAL_CHUNK_4,
        TYPICAL_CHUNK_5,
        TYPICAL_CHUNK_6,
        TYPICAL_CHUNK_7,
        TYPICAL_CHUNK_8,
        TYPICAL_CHUNK_9,
        TYPICAL_CHUNK_10,
    };
    /* Test typical target chunk */
    ESP_LOGI(TAG, "Testing typical target chunk...");
    expectedSpeed = 56;
    actualSpeed = 0;
    retval = tomtomParseSpeed(&actualSpeed, typicalChunks[0], strlen(typicalChunks[0]));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, retval,
        "Expected ESP_OK return value when speed is entirely in chunk");
    TEST_ASSERT_EQUAL_MESSAGE(expectedSpeed, actualSpeed,
        "Expected actual speed to equal speed provided in chunk");

    /* Test typical non-target chunks */
    ESP_LOGI(TAG, "Testing typical non-target chunks...");
    prevSpeed = 15;
    actualSpeed = 15;
    for (int i = 1; i < 10; i++) {
        retval = tomtomParseSpeed(&actualSpeed, typicalChunks[i], strlen(typicalChunks[i]));
        TEST_ASSERT_EQUAL_MESSAGE(TOMTOM_NO_SPEED, retval,
            "Expected TOMTOM_NO_SPEED return value when speed is not in chunk");
        TEST_ASSERT_EQUAL_MESSAGE(prevSpeed, actualSpeed,
            "Expected result to be unmodified when speed is not in chunk");
    }

    /* Test NULL result pointer */
    ESP_LOGI(TAG, "Testing NULL result pointer...");
    retval = tomtomParseSpeed(NULL, typicalChunks[1], strlen(typicalChunks[1]));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_FAIL, retval,
        "Expected ESP_FAIL return value when given NULL result pointer");

    /* Test indicate new message (NULL chunk) */
    ESP_LOGI(TAG, "Testing indicate new message functionality (NULL chunk)...");
    retval = tomtomParseSpeed(NULL, NULL, 0);
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, retval,
        "Expected ESP_OK return value when given NULL chunk, indicating start of a new http response");

    /* Test empty string chunk */
    ESP_LOGI(TAG, "Testing empty string chunk...");
    prevSpeed = 15;
    actualSpeed = 15;
    retval = tomtomParseSpeed(&actualSpeed, "", 0);
    TEST_ASSERT_EQUAL_MESSAGE(TOMTOM_NO_SPEED, retval,
        "Expected TOMTOM_NO_SPEED return value when given empty chunk");
    TEST_ASSERT_EQUAL_MESSAGE(prevSpeed, actualSpeed,
        "Expected result to be unmodified when given empty chunk");

    /* Test single character chunk */
    ESP_LOGI(TAG, "Testing single character chunk...");
    prevSpeed = 15;
    actualSpeed = 15;
    retval = tomtomParseSpeed(&actualSpeed, "a", 1);
    TEST_ASSERT_EQUAL_MESSAGE(TOMTOM_NO_SPEED, retval,
        "Expected TOMTOM_NO_SPEED return value when given single character chunk");
    TEST_ASSERT_EQUAL_MESSAGE(prevSpeed, actualSpeed,
        "Expected result to be unmodified when given single character chunk");
    
    /* Test target across chunk boundary */
    ESP_LOGI(TAG, "Testing target across chunk boundary...");
    prevSpeed = 15;
    actualSpeed = 15;
    retval = tomtomParseSpeed(&actualSpeed, TARGET_ACROSS_CHUNKS_1, strlen(TARGET_ACROSS_CHUNKS_1));
    TEST_ASSERT_EQUAL_MESSAGE(TOMTOM_NO_SPEED, retval,
        "Expected TOMTOM_NO_SPEED return value when given partial target chunk");
    TEST_ASSERT_EQUAL_MESSAGE(prevSpeed, actualSpeed,
        "Expected result to be unmodified when given partial target chunk");
    retval = tomtomParseSpeed(&actualSpeed, TARGET_ACROSS_CHUNKS_2, strlen(TARGET_ACROSS_CHUNKS_2));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, retval,
        "Expected ESP_OK return value when given target across a chunk boundary");
    TEST_ASSERT_EQUAL_MESSAGE(expectedSpeed, actualSpeed,
        "Expected actual speed to equal speed provided across chunk boundary");
}
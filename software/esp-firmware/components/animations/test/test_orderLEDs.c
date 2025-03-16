/**
 * test_orderLEDs.c
 * 
 * Black box unit tests for animations.c:orderLEDs.
 */

#include "unity.h"


#include "animations.h"
#include "animation_types.h"
#include "led_coordinates.h"
#include "animation_config.h"

/**
 * Tests that ledOrder = NULL returns ESP_ERR_INVALID_ARG.
 */
TEST_CASE("orderLEDs_nullOrderArr", "[animations]")
{
    esp_err_t err;
    err = orderLEDs(NULL, 10, DIAG_LINE, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

/**
 * Tests that coords = NULL returns ESP_ERR_INVALID_ARG.
 */
TEST_CASE("orderLEDs_nullCoordsArr", "[animations]")
{
    esp_err_t err;
    int32_t ledOrder[ANIM_STANDARD_ARRAY_SIZE];

    err = orderLEDs(ledOrder, ANIM_STANDARD_ARRAY_SIZE, DIAG_LINE, NULL, ANIM_STANDARD_ARRAY_SIZE);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

/**
 * Tests that ledOrderLen != coordsLen returns ESP_ERR_INVALID_SIZE.
 */
TEST_CASE("orderLEDs_mismatchSizeArrs", "[animations]")
{
    esp_err_t err;
    int32_t ledOrder[ANIM_STANDARD_ARRAY_SIZE - 1];

    err = orderLEDs(ledOrder, ANIM_STANDARD_ARRAY_SIZE - 1, DIAG_LINE, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
}

/**
 * Tests that ledOrderLen > ANIM_STANDARD_ARRAY_SIZE returns ESP_ERR_INVALID_SIZE.
 */
TEST_CASE("orderLEDs_largeSizeOrderArr", "[animations]")
{
    esp_err_t err;
    int32_t ledOrder[ANIM_STANDARD_ARRAY_SIZE + 1];

    err = orderLEDs(ledOrder, ANIM_STANDARD_ARRAY_SIZE + 1, DIAG_LINE, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
}

/**
 * Tests that coordsLen > ANIM_STANDARD_ARRAY_SIZE returns ESP_ERR_INVALID_SIZE.
 */
TEST_CASE("orderLEDs_largeSizeCoordsArr", "[animations]")
{
    esp_err_t err;
    int32_t ledOrder[ANIM_STANDARD_ARRAY_SIZE];
    LEDCoord numToCoord[ANIM_STANDARD_ARRAY_SIZE + 1];

    for (int i = 0; i < ANIM_STANDARD_ARRAY_SIZE + 1; i++)
    {
        numToCoord[i].x = 0;
        numToCoord[i].y = 0;
    }

    err = orderLEDs(ledOrder, ANIM_STANDARD_ARRAY_SIZE, DIAG_LINE, numToCoord, ANIM_STANDARD_ARRAY_SIZE + 1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
}

/**
 * Tests that anim >= ANIM_MAXIMUM returns ESP_ERR_INVALID_ARG.
 */
TEST_CASE("orderLEDs_invalidAnim", "[animations]")
{
    esp_err_t err;
    int32_t ledOrder[ANIM_STANDARD_ARRAY_SIZE];

    err = orderLEDs(ledOrder, ANIM_STANDARD_ARRAY_SIZE, ANIM_MAXIMUM, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = orderLEDs(ledOrder, ANIM_STANDARD_ARRAY_SIZE, INT_MAX, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

/**
 * Tests that diagonal animations are ordered correctly. This test should
 * pass for every possible configuration of diagonal line.
 */
TEST_CASE("orderLEDs_diagAnim", "[animations]")
{
    esp_err_t err;
    int32_t ledOrder[7];
    LEDCoord coords[7] = { {.x = 0, .y = 0},
                              {.x = 1, .y = -1},
                              {.x = 4, .y = -4},
                              {.x = 2, .y = -2},
                              {.x = 3, .y = -3},
                              {.x = -1, .y = 1},
                              {.x = -2, .y = 2},
    };

    err = orderLEDs(ledOrder, 7, DIAG_LINE, coords, 7);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(6, ledOrder[0]);
    TEST_ASSERT_EQUAL(5, ledOrder[1]);
    TEST_ASSERT_EQUAL(0, ledOrder[2]);
    TEST_ASSERT_EQUAL(1, ledOrder[3]);
    TEST_ASSERT_EQUAL(3, ledOrder[4]);
    TEST_ASSERT_EQUAL(4, ledOrder[5]);
    TEST_ASSERT_EQUAL(2, ledOrder[6]);
}
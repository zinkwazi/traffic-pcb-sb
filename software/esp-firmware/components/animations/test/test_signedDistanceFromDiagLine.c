/**
 * test_signedDistanceFromDiagLine.c
 * 
 * Unit tests for animations.c:signedDistanceFromDiagLine.
 */

#include <math.h>

#include "unity.h"

#include "animations_pi.h"
#include "animation_types.h"

/* The maximum allowable amount of error 
between expected and actual double values */
#define MAX_DOUBLE_ERROR (0.001)

/**
 * Dependencies: None.
 * 
 * Potential Errors:
 *  - zero input edge case.
 *  - incorrect zero distance calculation.
 */
TEST_CASE("signedDistanceFromDiagLine_origin", "[animations]")
{
    const LEDCoord input = {
        .x = 0,
        .y = 0,
    };
    const double angle = M_PI_4;
    const double expected = 0;
    double actual;
    
    actual = signedDistanceFromDiagLine(input, angle);

    TEST_ASSERT_DOUBLE_WITHIN(MAX_DOUBLE_ERROR, expected, actual);
}

/**
 * Dependencies: None.
 * 
 * Potential Errors:
 *  - incorrect zero distance calculation.
 */
TEST_CASE("signedDistanceFromDiagLine_zero_distance", "[animations]")
{
    const LEDCoord input = {
        .x = 1,
        .y = 1,
    };
    const double angle = M_PI_4;
    const double expected = 0;
    double actual;
    
    actual = signedDistanceFromDiagLine(input, angle);

    TEST_ASSERT_DOUBLE_WITHIN(MAX_DOUBLE_ERROR, expected, actual);
}

/**
 * Dependencies: None.
 * 
 * Potential Errors:
 *  - incorrect positive distance calculation.
 */
TEST_CASE("signedDistanceFromDiagLine_typical_point", "[animations]")
{
    const LEDCoord input = {
        .x = 4,
        .y = 1,
    };
    const double angle = M_PI_4;
    const double expected = 2.12132034356;
    double actual;
    
    actual = signedDistanceFromDiagLine(input, angle);

    TEST_ASSERT_DOUBLE_WITHIN(MAX_DOUBLE_ERROR, expected, actual);
}

/**
 * Dependencies: None.
 * 
 * Potential Errors:
 *  - incorrect negative distance calculation.
 */
TEST_CASE("signedDistanceFromDiagLine_negative_distance", "[animations]")
{
    const LEDCoord input = {
        .x = -3,
        .y = -2,
    };
    const double angle = M_PI_4;
    const double expected = -sqrt(2) / 2;
    double actual;
    
    actual = signedDistanceFromDiagLine(input, angle);

    TEST_ASSERT_DOUBLE_WITHIN(MAX_DOUBLE_ERROR, expected, actual);
}

/**
 * Dependencies: None.
 * 
 * Potential Errors:
 *  - x/y swapped.
 *  - zero angle edge case.
 */
TEST_CASE("signedDistanceFromDiagLine_zero_angle", "[animations]")
{
    const LEDCoord input = {
        .x = 6,
        .y = 5,
    };
    const double angle = 0;
    const double expected = -5;
    double actual;
    
    actual = signedDistanceFromDiagLine(input, angle);

    TEST_ASSERT_DOUBLE_WITHIN(MAX_DOUBLE_ERROR, expected, actual);
}

/**
 * Dependencies: None.
 * 
 * Potential Errors:
 *  - x/y swapped.
 *  - pi/2 angle edge case.
 */
TEST_CASE("signedDistanceFromDiagLine_pi_2_angle", "[animations]")
{
    const LEDCoord input = {
        .x = 7,
        .y = 10,
    };
    const double angle = M_PI_2;
    const double expected = 7;
    double actual;
    
    actual = signedDistanceFromDiagLine(input, angle);

    TEST_ASSERT_DOUBLE_WITHIN(MAX_DOUBLE_ERROR, expected, actual);
}

/**
 * Dependencies: None.
 * 
 * Potential Errors:
 *  - atypical angle.
 *  - incorrect negative distance calculation.
 */
TEST_CASE("signedDistanceFromDiagLine_atypical_angle", "[animations]")
{
    const LEDCoord input = {
        .x = 0,
        .y = 12,
    };
    const double angle = 0.553;
    const double expected = -10.211429147;
    double actual;
    
    actual = signedDistanceFromDiagLine(input, angle);

    TEST_ASSERT_DOUBLE_WITHIN(MAX_DOUBLE_ERROR, expected, actual);
}
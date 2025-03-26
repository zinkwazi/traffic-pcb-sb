/**
 * animations.c
 * 
 * Contains functionality for determining the proper ordering of LEDs in order
 * to animate board refreshes in some way.
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_assert.h"

#include "animations.h"
#include "animation_types.h"
#include "led_coordinates.h"
#include "animation_config.h"

#define TAG "animations"

struct LEDCoordPair {
    int32_t ledNum;
    LEDCoord coord;
};

/* the function declarations below are not static to allow for white-box unit testing */

esp_err_t orderLEDs(int32_t ledOrder[], int32_t ledOrderLen, Animation anim, const LEDCoord coords[], int32_t coordsLen);
double signedDistanceFromDiagLine(LEDCoord coords, double angle);
double signedDistanceFromCurvedLineNorth(LEDCoord coords, double angle);
double signedDistanceFromCurvedLineSouth(LEDCoord coords, double angle);
int compDistFromCurvedLineNorth(const void *c1, const void *c2);
int compDistFromCurvedLineSouth(const void *c1, const void *c2);
esp_err_t sortLEDsByDistanceFromDiagLine(int32_t ledArr[], int32_t ledArrLen, const LEDCoord coords[], int32_t coordsLen);
esp_err_t sortLEDsByDistanceFromCurvedLineNorth(int32_t ledArr[], int32_t ledArrLen, const LEDCoord coords[], int32_t coordsLen);
esp_err_t sortLEDsByDistanceFromCurvedLineSouth(int32_t ledArr[], int32_t ledArrLen, const LEDCoord coords[], int32_t coordsLen);


/**
 * @brief Generates an ordering of LEDs corresponding to the chosen animation.
 * 
 * @param[out] ledOrder The location where the order of LEDs will be placed,
 *        where the first LED in the animation is stored at index 0.
 * @param[in] ledOrderLen The length of ledOrder.
 * @param[in] anim The animation that will be generated.
 * @param[in] coords An array containing LED board coordinates, where index 'i'
 *        corresponds to LED number 'i' + 1.
 * @param[in] coordsLen The length of coords.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid arguments are provided.
 *          ESP_ERR_INVALID_SIZE if either ledArrLen or coordsLen is greater
 *          than STANDARD_ARR_SIZE, or ledArrLen does not equal coordsLen.
 *          ESP_FAIL if an unexpected error occurred.
 */
esp_err_t orderLEDs(int32_t ledOrder[],
                    int32_t ledOrderLen,
                    Animation anim,
                    const LEDCoord coords[],
                    int32_t coordsLen)
{
    esp_err_t err;
    /* input guards */
    if (ledOrder == NULL) return ESP_ERR_INVALID_ARG;
    if (coords == NULL) return ESP_ERR_INVALID_ARG;
    if (anim >= ANIM_MAXIMUM) return ESP_ERR_INVALID_ARG;
    if (ledOrderLen > ANIM_STANDARD_ARRAY_SIZE) return ESP_ERR_INVALID_SIZE;
    if (coordsLen > ANIM_STANDARD_ARRAY_SIZE) return ESP_ERR_INVALID_SIZE;
    if (ledOrderLen != coordsLen) return ESP_ERR_INVALID_SIZE;
    /* select ordering function */
    err = ESP_FAIL;
    switch (anim)
    {
        case DIAG_LINE:
            err = sortLEDsByDistanceFromDiagLine(ledOrder, ledOrderLen, coords, coordsLen);
            break;
        case DIAG_LINE_REVERSE:
            err = sortLEDsByDistanceFromDiagLine(ledOrder, ledOrderLen, coords, coordsLen);
            for (int32_t i = 0; i < ledOrderLen / 2; i++)
            {
                int32_t temp = ledOrder[i];
                ledOrder[i] = ledOrder[ledOrderLen - i - 1];
                ledOrder[ledOrderLen - i - 1] = temp;
            }
            break;
        case CURVED_LINE_NORTH:
            err = sortLEDsByDistanceFromCurvedLineNorth(ledOrder, ledOrderLen, coords, coordsLen);
            break;
        case CURVED_LINE_NORTH_REVERSE:
            err = sortLEDsByDistanceFromCurvedLineNorth(ledOrder, ledOrderLen, coords, coordsLen);
            for (int32_t i = 0; i < (ledOrderLen) / 2; i++)
            {
                int32_t temp = ledOrder[i];
                ledOrder[i] = ledOrder[ledOrderLen - i - 1];
                ledOrder[ledOrderLen - i - 1] = temp;
            }
            break;
        case CURVED_LINE_SOUTH:
            err = sortLEDsByDistanceFromCurvedLineSouth(ledOrder, ledOrderLen, coords, coordsLen);
            break;
        case CURVED_LINE_SOUTH_REVERSE:
            err = sortLEDsByDistanceFromCurvedLineSouth(ledOrder, ledOrderLen, coords, coordsLen);
            for (int32_t i = 0; i < (ledOrderLen) / 2; i++)
            {
                int32_t temp = ledOrder[i];
                ledOrder[i] = ledOrder[ledOrderLen - i - 1];
                ledOrder[ledOrderLen - i - 1] = temp;
            }
            break;
        default:
            err = ESP_FAIL;
            break;
    }
    return err;
}

/**
 * @brief Calculates the distance of point (x, y) to the line 
 *        y = tan(angle) * x.
 * 
 * @note Requires that 0 <= angle <= pi / 2.
 * 
 * @param[in] coords The coordinates of the point.
 * @param[in] angle The angle of the diagonal line.
 * 
 * @returns The calculated distance from the line, with negative distances
 *          to the left of the line.
 * 
 */
double signedDistanceFromDiagLine(LEDCoord coords, double angle)
{
    return sin(angle) * coords.x - (cos(angle) * coords.y);
}

/**
 * @brief Calculates the distance of point (x, y) to a curved diagonal line.
 * 
 * @note The curved line resembles a parabola opening to the southeast.
 * 
 * @param[in] coords The coordinates of the point.
 * @param[in] angle The angle of the diagonal line.
 * 
 * @returns The calculated distance from the line, with negative distances
 *          to the left of the line.
 */
double signedDistanceFromCurvedLineNorth(LEDCoord coords, double angle)
{
    double xPrime = cos(-angle) * coords.x - sin(-angle) * coords.y;
    double yPrime = sin(-angle) * coords.x + cos(-angle) * coords.y;
    xPrime *= NORTH_GROWTH_FACTOR;
    yPrime *= NORTH_GROWTH_FACTOR;
    xPrime -= CURVED_NORTH_TANGENTIAL_OFFSET;
    yPrime += CURVED_NORTH_OFFSET;
    return (NORTH_OVAL_FACTOR * xPrime * xPrime) + (yPrime * yPrime);
}

/**
 * @brief Calculates the distance of point (x, y) to a curved diagonal line.
 * 
 * @note The curved line resembles a parabola opening to the northwest.
 * 
 * @param[in] coords The coordinates of the point.
 * @param[in] angle The angle of the diagonal line.
 * 
 * @returns The calculated distance from the line, with negative distances
 *          to the left of the line.
 */
double signedDistanceFromCurvedLineSouth(LEDCoord coords, double angle)
{
    double xPrime = cos(-angle) * coords.x - sin(-angle) * coords.y;
    double yPrime = sin(-angle) * coords.x + cos(-angle) * coords.y;
    xPrime *= SOUTH_GROWTH_FACTOR;
    yPrime *= SOUTH_GROWTH_FACTOR;
    xPrime -= CURVED_SOUTH_TANGENTIAL_OFFSET;
    yPrime -= CURVED_SOUTH_OFFSET;
    return (SOUTH_OVAL_FACTOR * xPrime * xPrime) + (yPrime * yPrime);
}

/**
 * @brief Compares the signed distance of two LEDCoordPairs to the line defined
 *        by signedDistanceFromDiagLine.
 * 
 * @param[in] c1 A pointer to an LEDCoordPair.
 * @param[in] c2 A pointer to an LEDCoordPair.
 * 
 * @returns Less than 0 if the signed distance of c1 from the line is less than
 *          that of c2, 0 if they are equal, otherwise greater than 0.
 */
int compDistFromDiagLine(const void *c1, const void *c2)
{
    LEDCoord coord1 = ((struct LEDCoordPair *) c1)->coord;
    LEDCoord coord2 = ((struct LEDCoordPair *) c2)->coord;
    double dist1 = signedDistanceFromDiagLine(coord1, DIAG_LINE_ANGLE);
    double dist2 = signedDistanceFromDiagLine(coord2, DIAG_LINE_ANGLE);
    return (dist1 > dist2) - (dist1 < dist2);
}

/**
 * @brief Compares the signed distance of two LEDCoordPairs to the line defined
 *        by signedDistanceFromCurvedLineNorth.
 * 
 * @note The curved line resembles a parabola opening to the southwest.
 * 
 * @param[in] c1 A pointer to an LEDCoordPair.
 * @param[in] c2 A pointer to an LEDCoordPair.
 * 
 * @returns Less than 0 if the signed distance of c1 from the line is less than
 *          that of c2, 0 if they are equal, otherwise greater than 0.
 */
int compDistFromCurvedLineNorth(const void *c1, const void *c2)
{
    LEDCoord coord1 = ((struct LEDCoordPair *) c1)->coord;
    LEDCoord coord2 = ((struct LEDCoordPair *) c2)->coord;
    double dist1 = signedDistanceFromCurvedLineNorth(coord1, DIAG_LINE_ANGLE);
    double dist2 = signedDistanceFromCurvedLineNorth(coord2, DIAG_LINE_ANGLE);
    return (dist1 > dist2) - (dist1 < dist2);
}

/**
 * @brief Compares the signed distance of two LEDCoordPairs to the line defined
 *        by signedDistanceFromCurvedLineSouth.
 * 
 * @note The curved line resembles a parabola opening to the northeast.
 * 
 * @param[in] c1 A pointer to an LEDCoordPair.
 * @param[in] c2 A pointer to an LEDCoordPair.
 * 
 * @returns Less than 0 if the signed distance of c1 from the line is less than
 *          that of c2, 0 if they are equal, otherwise greater than 0.
 */
int compDistFromCurvedLineSouth(const void *c1, const void *c2)
{
    LEDCoord coord1 = ((struct LEDCoordPair *) c1)->coord;
    LEDCoord coord2 = ((struct LEDCoordPair *) c2)->coord;
    double dist1 = signedDistanceFromCurvedLineSouth(coord1, DIAG_LINE_ANGLE);
    double dist2 = signedDistanceFromCurvedLineSouth(coord2, DIAG_LINE_ANGLE);
    return (dist1 > dist2) - (dist1 < dist2);
}


/**
 * @brief Fills ledArr with a list of LEDs, whose coordinates are stored in
 *        led_coordinates.h, sorted by the distance calculated by
 *        distanceFromDiagLine.
 * 
 * @param[out] ledArr The location to place the list of LEDs sorted in ascending
 *        order of distance from the diagonal line.
 * @param[in] ledArrLen The length of ledArr.
 * @param[in] coords An array containing LED board coordinates, where index 'i'
 *        corresponds to LED number 'i' + 1.
 * @param[in] coordsLen The length of coords.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if ledArr is NULL.
 *          ESP_ERR_INVALID_SIZE if either ledArrLen or coordsLen is greater
 *          than STANDARD_ARR_SIZE, or ledArrLen does not equal coordsLen.
 */
esp_err_t sortLEDsByDistanceFromDiagLine(int32_t ledArr[], 
                                         int32_t ledArrLen, 
                                         const LEDCoord coords[], 
                                         int32_t coordsLen)
{
    struct LEDCoordPair coordPairs[ANIM_STANDARD_ARRAY_SIZE];
    /* input guards */
    if (ledArr == NULL) return ESP_ERR_INVALID_ARG;
    if (coords == NULL) return ESP_ERR_INVALID_ARG;
    if (ledArrLen > ANIM_STANDARD_ARRAY_SIZE) return ESP_ERR_INVALID_SIZE;
    if (coordsLen > ANIM_STANDARD_ARRAY_SIZE) return ESP_ERR_INVALID_SIZE;
    if (ledArrLen != coordsLen) return ESP_ERR_INVALID_SIZE;
    /* copy coordinate pairs */
    for (int32_t i = 0; i < coordsLen; i++)
    {
        coordPairs[i].ledNum = i + 1;
        coordPairs[i].coord = coords[i];
    }
    /* sort based on distances */
    qsort(coordPairs, coordsLen, sizeof(struct LEDCoordPair), compDistFromDiagLine);
    /* copy results */
    for (int32_t i = 0; i < coordsLen; i++)
    {
        ledArr[i] = coordPairs[i].ledNum;
    }
    return ESP_OK;
}

/**
 * @brief Fills ledArr with a list of LEDs, whose coordinates are stored in
 *        led_coordinates.h, sorted by the distance calculated by
 *        distanceFromCurvedLineNorth.
 * 
 * @note The animation has a curve resembling a parabola opening to the southwest.
 * 
 * @param[out] ledArr The location to place the list of LEDs sorted in ascending
 *        order of distance from the curved line.
 * @param[in] ledArrLen The length of ledArr.
 * @param[in] coords An array containing LED board coordinates, where index 'i'
 *        corresponds to LED number 'i' + 1.
 * @param[in] coordsLen The length of coords.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if ledArr or coords is NULL.
 *          ESP_ERR_INVALID_SIZE if either ledArrLen or coordsLen is greater
 *          than STANDARD_ARR_SIZE, or ledArrLen does not equal coordsLen.
 */
esp_err_t sortLEDsByDistanceFromCurvedLineNorth(int32_t ledArr[], 
                                           int32_t ledArrLen, 
                                           const LEDCoord coords[], 
                                           int32_t coordsLen)
{
    struct LEDCoordPair coordPairs[ANIM_STANDARD_ARRAY_SIZE];
    /* input guards */
    if (ledArr == NULL) return ESP_ERR_INVALID_ARG;
    if (coords == NULL) return ESP_ERR_INVALID_ARG;
    if (ledArrLen > ANIM_STANDARD_ARRAY_SIZE) return ESP_ERR_INVALID_SIZE;
    if (coordsLen > ANIM_STANDARD_ARRAY_SIZE) return ESP_ERR_INVALID_SIZE;
    if (ledArrLen != coordsLen) return ESP_ERR_INVALID_SIZE;
    /* copy coordinate pairs */
    for (int32_t i = 0; i < coordsLen; i++)
    {
        coordPairs[i].ledNum = i + 1;
        coordPairs[i].coord = coords[i];
    }
    /* sort based on distances */
    qsort(coordPairs, coordsLen, sizeof(struct LEDCoordPair), compDistFromCurvedLineNorth);
    /* copy results */
    for (int32_t i = 0; i < coordsLen; i++)
    {
        ledArr[i] = coordPairs[i].ledNum;
    }
    return ESP_OK;
}

/**
 * @brief Fills ledArr with a list of LEDs, whose coordinates are stored in
 *        led_coordinates.h, sorted by the distance calculated by
 *        distanceFromCurvedLineSouth.
 * 
 * @note The animation has a curve resembling a parabola opening to the northeast.
 * 
 * @param[out] ledArr The location to place the list of LEDs sorted in ascending
 *        order of distance from the curved line.
 * @param[in] ledArrLen The length of ledArr.
 * @param[in] coords An array containing LED board coordinates, where index 'i'
 *        corresponds to LED number 'i' + 1.
 * @param[in] coordsLen The length of coords.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if ledArr or coords is NULL.
 *          ESP_ERR_INVALID_SIZE if either ledArrLen or coordsLen is greater
 *          than STANDARD_ARR_SIZE, or ledArrLen does not equal coordsLen.
 */
esp_err_t sortLEDsByDistanceFromCurvedLineSouth(int32_t ledArr[], 
                                                int32_t ledArrLen, 
                                                const LEDCoord coords[], 
                                                int32_t coordsLen)
{
struct LEDCoordPair coordPairs[ANIM_STANDARD_ARRAY_SIZE];
/* input guards */
if (ledArr == NULL) return ESP_ERR_INVALID_ARG;
if (coords == NULL) return ESP_ERR_INVALID_ARG;
if (ledArrLen > ANIM_STANDARD_ARRAY_SIZE) return ESP_ERR_INVALID_SIZE;
if (coordsLen > ANIM_STANDARD_ARRAY_SIZE) return ESP_ERR_INVALID_SIZE;
if (ledArrLen != coordsLen) return ESP_ERR_INVALID_SIZE;
/* copy coordinate pairs */
for (int32_t i = 0; i < coordsLen; i++)
{
coordPairs[i].ledNum = i + 1;
coordPairs[i].coord = coords[i];
}
/* sort based on distances */
qsort(coordPairs, coordsLen, sizeof(struct LEDCoordPair), compDistFromCurvedLineSouth);
/* copy results */
for (int32_t i = 0; i < coordsLen; i++)
{
ledArr[i] = coordPairs[i].ledNum;
}
return ESP_OK;
}
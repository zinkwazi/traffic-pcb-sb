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
#include "animation_config.h"
#include "app_err.h"
#include "led_coordinates.h"

#define TAG "animations"

struct LEDCoordPair {
    int32_t ledNum;
    LEDCoord coord;
};

/* sequences that are calculated during initialization to avoid delays during refreshes */
static bool sDiagLineSaved = false;
static int32_t sDiagLineSequence[ANIM_STANDARD_ARRAY_SIZE];
static bool sCurvedNorthSaved = false;
static int32_t sCurvedLineNorthSequence[ANIM_STANDARD_ARRAY_SIZE];
static bool sCurvedSouthSaved = false;
static int32_t sCurvedLineSouthSequence[ANIM_STANDARD_ARRAY_SIZE];

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
 * @brief Calculates LED sequences for each animation and stores them for
 * quick loading during calls to sort functions. This reduces delays during
 * refreshes.
 * 
 * @returns ESP_OK if calculated and stored sequences successfully.
 */
esp_err_t calculateLEDSequences(void)
{
    esp_err_t err;
    if (!sDiagLineSaved)
    {
        err = sortLEDsByDistanceFromDiagLine(sDiagLineSequence, ANIM_STANDARD_ARRAY_SIZE, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
        if (err == ESP_OK)
        {
            sDiagLineSaved = true;
        } else
        {
            sDiagLineSaved = false;
            THROW_ERR(err);
        }
    }
    if (!sCurvedNorthSaved)
    {
        err = sortLEDsByDistanceFromCurvedLineNorth(sCurvedLineNorthSequence, ANIM_STANDARD_ARRAY_SIZE, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
        if (err == ESP_OK)
        {
            sCurvedNorthSaved = true;
        } else
        {
            sCurvedNorthSaved = false;
            THROW_ERR(err);
        }
    }
    if (!sCurvedSouthSaved)
    {
        err = sortLEDsByDistanceFromCurvedLineSouth(sCurvedLineSouthSequence, ANIM_STANDARD_ARRAY_SIZE, LEDNumToCoord, ANIM_STANDARD_ARRAY_SIZE);
        if (err == ESP_OK)
        {
            sCurvedSouthSaved = true;
        } else
        {
            sCurvedSouthSaved = false;
            THROW_ERR(err);
        }
    }
    return ESP_OK;
}

/**
 * @brief Generates an ordering of LEDs corresponding to the chosen animation.
 * 
 * @see calculateLEDSequences, which pre-calculates animation sequences so
 * that this function does not manually calculate orderings every time it is
 * called.
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
    if (ledOrder == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (coords == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (anim >= ANIM_MAXIMUM) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (ledOrderLen > ANIM_STANDARD_ARRAY_SIZE) THROW_ERR(ESP_ERR_INVALID_SIZE);
    if (coordsLen > ANIM_STANDARD_ARRAY_SIZE) THROW_ERR(ESP_ERR_INVALID_SIZE);
    if (ledOrderLen != coordsLen) THROW_ERR(ESP_ERR_INVALID_SIZE);
    /* select ordering function */
    err = ESP_FAIL;
    switch (anim)
    {
        case DIAG_LINE:
            if (sDiagLineSaved)
            {
                for (int32_t i = 0; i < ledOrderLen; i++)
                {
                    ledOrder[i] = sDiagLineSequence[i];
                }
            } else
            {
                err = sortLEDsByDistanceFromDiagLine(ledOrder, ledOrderLen, coords, coordsLen);
                if (err != ESP_OK) return err;
            }
            
            break;
        case DIAG_LINE_REVERSE:
            if (sDiagLineSaved)
            {
                for (int32_t i = 0; i < ledOrderLen; i++)
                {
                    ledOrder[i] = sDiagLineSequence[ANIM_STANDARD_ARRAY_SIZE - i - 1];
                }
            } else
            {
                err = sortLEDsByDistanceFromDiagLine(ledOrder, ledOrderLen, coords, coordsLen);
                if (err != ESP_OK) return err;
                for (int32_t i = 0; i < ledOrderLen / 2; i++)
                {
                    int32_t temp = ledOrder[i];
                    ledOrder[i] = ledOrder[ledOrderLen - i - 1];
                    ledOrder[ledOrderLen - i - 1] = temp;
                }
            }
            break;
        case CURVED_LINE_NORTH:
            if (sCurvedNorthSaved)
            {
                for (int32_t i = 0; i < ledOrderLen; i++)
                {
                    ledOrder[i] = sCurvedLineNorthSequence[i];
                }
            } else
            {
                err = sortLEDsByDistanceFromCurvedLineNorth(ledOrder, ledOrderLen, coords, coordsLen);
                if (err != ESP_OK) return err;
            }
            
            break;
        case CURVED_LINE_NORTH_REVERSE:
            if (sCurvedNorthSaved)
            {
                for (int32_t i = 0; i < ledOrderLen; i++)
                {
                    ledOrder[i] = sCurvedLineNorthSequence[ANIM_STANDARD_ARRAY_SIZE - i - 1];
                }
            } else
            {
                err = sortLEDsByDistanceFromCurvedLineNorth(ledOrder, ledOrderLen, coords, coordsLen);
                if (err != ESP_OK) return err;
                for (int32_t i = 0; i < (ledOrderLen) / 2; i++)
                {
                    int32_t temp = ledOrder[i];
                    ledOrder[i] = ledOrder[ledOrderLen - i - 1];
                    ledOrder[ledOrderLen - i - 1] = temp;
                }
            }
            break;
        case CURVED_LINE_SOUTH:
            if (sCurvedSouthSaved)
            {
                for (int i = 0; i < ledOrderLen; i++)
                {
                    ledOrder[i] = sCurvedLineSouthSequence[i];
                }
            } else
            {
                err = sortLEDsByDistanceFromCurvedLineSouth(ledOrder, ledOrderLen, coords, coordsLen);
                if (err != ESP_OK) return err;
            }
            break;
        case CURVED_LINE_SOUTH_REVERSE:
            if (sCurvedSouthSaved)
            {
                for (int32_t i = 0; i < ledOrderLen; i++)
                {
                    ledOrder[i] = sCurvedLineSouthSequence[ANIM_STANDARD_ARRAY_SIZE - i - 1];
                }
            } else
            {
                err = sortLEDsByDistanceFromCurvedLineSouth(ledOrder, ledOrderLen, coords, coordsLen);
                if (err != ESP_OK) return err;
                for (int32_t i = 0; i < (ledOrderLen) / 2; i++)
                {
                    int32_t temp = ledOrder[i];
                    ledOrder[i] = ledOrder[ledOrderLen - i - 1];
                    ledOrder[ledOrderLen - i - 1] = temp;
                }
            }
            break;
        default:
            THROW_ERR(ESP_FAIL);
    }
    return ESP_OK;
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
    if (ledArr == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (coords == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (ledArrLen > ANIM_STANDARD_ARRAY_SIZE) THROW_ERR(ESP_ERR_INVALID_SIZE);
    if (coordsLen > ANIM_STANDARD_ARRAY_SIZE) THROW_ERR(ESP_ERR_INVALID_SIZE);
    if (ledArrLen != coordsLen) THROW_ERR(ESP_ERR_INVALID_SIZE);
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
    if (ledArr == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (coords == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (ledArrLen > ANIM_STANDARD_ARRAY_SIZE) THROW_ERR(ESP_ERR_INVALID_SIZE);
    if (coordsLen > ANIM_STANDARD_ARRAY_SIZE) THROW_ERR(ESP_ERR_INVALID_SIZE);
    if (ledArrLen != coordsLen) THROW_ERR(ESP_ERR_INVALID_SIZE);
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
    if (ledArr == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (coords == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (ledArrLen > ANIM_STANDARD_ARRAY_SIZE) THROW_ERR(ESP_ERR_INVALID_SIZE);
    if (coordsLen > ANIM_STANDARD_ARRAY_SIZE) THROW_ERR(ESP_ERR_INVALID_SIZE);
    if (ledArrLen != coordsLen) THROW_ERR(ESP_ERR_INVALID_SIZE);
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
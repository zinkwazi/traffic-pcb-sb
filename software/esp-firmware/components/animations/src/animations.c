/**
 * animations.c
 * 
 * Contains functions that create iterators to determine proper ordering
 * of LEDs during a refresh to display a particular animation.
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_err.h"
#include "esp_log.h"

#include "animations.h"
#include "led_coordinates.h"

#define TAG "animations"

#define DIAG_LINE_ANGLE (M_PI / 3)
#define PARABOLIC_MAP_FACTOR (0.12)
#define MAP_OFFSET_X (-0)
#define MAP_OFFSET_Y (-30)

struct LEDCoordPair {
    int32_t ledNum;
    LEDCoord coord;
};

double signedDistanceFromDiagLine(LEDCoord coords);
double signedDistanceFromDiagLineLifted(LEDCoord coords);
int compDistFromDiagLine(const void *c1, const void *c2);
int compDistFromParabolicMapLine(const void *c1, const void *c2);
esp_err_t sortLEDsByDistanceFromDiagLine(int32_t ledArr[], int32_t ledArrLen);
esp_err_t sortLEDsByDistParabolicMap(int32_t ledArr[], int32_t ledArrLen);

/**
 * @brief Generates an ordering of LEDs corresponding to the chosen animation.
 * 
 * @param[out] ledOrder The location where the order of LEDs will be placed,
 *        where the first LED in the animation is stored at index 0.
 * @param[in] anim The animation that will be generated.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid arguments are provided.
 *          ESP_FAIL if an unexpected error occurred.
 */
esp_err_t orderLEDs(int32_t ledOrder[static MAX_NUM_LEDS_COORDS + 1], Animation anim)
{
    esp_err_t err;
    /* input guards */
    if (ledOrder == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    /* select ordering function */
    err = ESP_FAIL;
    switch (anim)
    {
        case DIAG_LINE:
            err = sortLEDsByDistanceFromDiagLine(ledOrder, MAX_NUM_LEDS_COORDS + 1);
            break;
        case DIAG_LINE_REVERSE:
            err = sortLEDsByDistanceFromDiagLine(ledOrder, MAX_NUM_LEDS_COORDS + 1);
            for (int32_t i = 0; i < (MAX_NUM_LEDS_COORDS + 1) / 2; i++) {
                int32_t temp = ledOrder[i];
                ledOrder[i] = ledOrder[MAX_NUM_LEDS_COORDS - i - 1];
                ledOrder[MAX_NUM_LEDS_COORDS - i - 1] = temp;
            }
            break;
        case CURVED_LINE:
            err = sortLEDsByDistParabolicMap(ledOrder, MAX_NUM_LEDS_COORDS + 1);
            break;
        case CURVED_LINE_REVERSE:
            err = sortLEDsByDistParabolicMap(ledOrder, MAX_NUM_LEDS_COORDS + 1);
            for (int32_t i = 0; i < (MAX_NUM_LEDS_COORDS + 1) / 2; i++) {
                int32_t temp = ledOrder[i];
                ledOrder[i] = ledOrder[MAX_NUM_LEDS_COORDS - i - 1];
                ledOrder[MAX_NUM_LEDS_COORDS - i - 1] = temp;
            }
            break;
        default:
            ESP_LOGE(TAG, "Encountered invalid animation: %d", anim);
            err = ESP_FAIL;
            break;
    }
    return err;
}

/**
 * @brief Calculates the distance of point (x, y) to the line y = tan(pi/4) * x.
 * 
 * @param[in] coords The coordinates of the point.
 * 
 * @returns The calculated distance from the line, with negative distances
 *          to the left of the line.
 */
double signedDistanceFromDiagLine(LEDCoord coords) {
    return sin(DIAG_LINE_ANGLE) * coords.x - (cos(DIAG_LINE_ANGLE) * coords.y);
}

double signedDistanceFromDiagLineLifted(LEDCoord coords) {
    double x1 = coords.x + MAP_OFFSET_X;
    double y1 = coords.y + MAP_OFFSET_Y;
    double x2 = cos(-DIAG_LINE_ANGLE) * x1 - sin(-DIAG_LINE_ANGLE) * y1;
    double y2 = sin(-DIAG_LINE_ANGLE) * x1 + cos(-DIAG_LINE_ANGLE) * y1;
    return (PARABOLIC_MAP_FACTOR * x2) * (PARABOLIC_MAP_FACTOR * x2) - y2;
}

int compDistFromDiagLine(const void *c1, const void *c2) {
    LEDCoord coord1 = ((struct LEDCoordPair *) c1)->coord;
    LEDCoord coord2 = ((struct LEDCoordPair *) c2)->coord;
    double dist1 = signedDistanceFromDiagLine(coord1);
    double dist2 = signedDistanceFromDiagLine(coord2);
    return (dist1 > dist2) - (dist1 < dist2);
}

int compDistFromParabolicMapLine(const void *c1, const void *c2) {
    LEDCoord coord1 = ((struct LEDCoordPair *) c1)->coord;
    LEDCoord coord2 = ((struct LEDCoordPair *) c2)->coord;
    double dist1 = signedDistanceFromDiagLineLifted(coord1);
    double dist2 = signedDistanceFromDiagLineLifted(coord2);
    return (dist1 > dist2) - (dist1 < dist2);
}

/**
 * @brief Fills ledArr with a list of LEDs, whose coordinates are stored in
 *        led_coordinates.h, sorted by the distance calculated by
 *        distanceFromDiagLine.
 * 
 * @param[out] ledArr The location to place the sorted list of LEDs.
 * @param[in] ledArrLen The length of the array, which will be checked against
 *        the number of LEDs with coordinates in led_coordinates.h. 
 * @param[in] theta The angle of the defined line.
 */
esp_err_t sortLEDsByDistanceFromDiagLine(int32_t ledArr[], int32_t ledArrLen) {
    struct LEDCoordPair sortedCoords[MAX_NUM_LEDS_COORDS + 1];
    /* input guards */
    if (ledArr == NULL || ledArrLen != MAX_NUM_LEDS_COORDS + 1) {
        return ESP_FAIL;
    }
    /* copy coordinates */
    for (int32_t i = 0; i < MAX_NUM_LEDS_COORDS + 1; i++) {
        sortedCoords[i].ledNum = i;
        sortedCoords[i].coord = LEDNumToCoord[i];
    }
    /* sort based on distances */
    qsort(sortedCoords, MAX_NUM_LEDS_COORDS + 1, sizeof(struct LEDCoordPair), compDistFromDiagLine);
    /* copy results */
    for (int32_t i = 0 ; i < MAX_NUM_LEDS_COORDS + 1; i++) {
        ledArr[i] = sortedCoords[i].ledNum;
    }
    return ESP_OK;
}

esp_err_t sortLEDsByDistParabolicMap(int32_t ledArr[], int32_t ledArrLen) {
    struct LEDCoordPair sortedCoords[MAX_NUM_LEDS_COORDS + 1];
    /* input guards */
    if (ledArr == NULL || ledArrLen != MAX_NUM_LEDS_COORDS + 1) {
        ESP_LOGI("sort", "failed. %ld != %d", ledArrLen, MAX_NUM_LEDS_COORDS);
        return ESP_FAIL;
    }
    /* copy coordinates */
    for (int32_t i = 0; i < MAX_NUM_LEDS_COORDS + 1; i++) {
        sortedCoords[i].ledNum = i;
        sortedCoords[i].coord = LEDNumToCoord[i];
    }
    /* sort based on distances */
    qsort(sortedCoords, MAX_NUM_LEDS_COORDS + 1, sizeof(struct LEDCoordPair), compDistFromParabolicMapLine);
    /* copy results */
    for (int32_t i = 0 ; i < MAX_NUM_LEDS_COORDS + 1; i++) {
        ledArr[i] = sortedCoords[i].ledNum;
    }
    return ESP_OK;
}
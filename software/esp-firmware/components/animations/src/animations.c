/**
 * animations.c
 * 
 * Contains functions that create iterators to determine proper ordering
 * of LEDs during a refresh to display a particular animation.
 */

#include "animations.h"
#include "led_coordinates.h"

#include "esp_err.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

struct LEDCoordPair {
    int32_t ledNum;
    LEDCoord coord;
};

/**
 * @brief Calculates the distance of point (x, y) to the line y = tan(pi/4) * x.
 * 
 * @param[in] coords The coordinates of the point.
 * 
 * @returns The calculated distance from the line, with negative distances
 *          to the left of the line.
 */
double signedDistanceFromDiagLine(LEDCoord coords) {
    return sin(M_PI_4) * coords.x - (cos(M_PI_4) * coords.y);
}

int compDistFromDiagLine(const void *c1, const void *c2) {
    LEDCoord coord1 = ((struct LEDCoordPair *) c1)->coord;
    LEDCoord coord2 = ((struct LEDCoordPair *) c2)->coord;
    double dist1 = signedDistanceFromDiagLine(coord1);
    double dist2 = signedDistanceFromDiagLine(coord2);
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
    struct LEDCoordPair sortedCoords[MAX_NUM_LEDS_COORDS];
    /* input guards */
    if (ledArr == NULL || ledArrLen != MAX_NUM_LEDS_COORDS) {
        return ESP_FAIL;
    }
    /* copy coordinates */
    for (int32_t i = 0; i < MAX_NUM_LEDS_COORDS; i++) {
        sortedCoords[i].ledNum = i;
        sortedCoords[i].coord = LEDNumToCoord[i];
    }
    /* sort based on distances */
    qsort(sortedCoords, MAX_NUM_LEDS_COORDS, sizeof(struct LEDCoordPair), compDistFromDiagLine);
    /* copy results */
    for (int32_t i = 0 ; i < MAX_NUM_LEDS_COORDS; i++) {
        ledArr[i] = sortedCoords[i].ledNum;
    }
    return ESP_OK;
}
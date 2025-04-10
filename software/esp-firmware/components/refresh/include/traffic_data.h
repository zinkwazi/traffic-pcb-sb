/**
 * traffic_data.h
 * 
 * Contains thread-safe getters and setters for the current traffic data. In
 * this context, thread-safe means that only one task has ownership of all
 * traffic data at a time. Ownership allows the task to use the functions below 
 * and nothing more.
 */

#ifndef TRAFFIC_DATA_H_4_9_25
#define TRAFFIC_DATA_H_4_9_25

#include <stddef.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"

#include "main_types.h"

enum DataSource {
    SOURCE_SERVER,
    SOURCE_NVS,
};

typedef enum DataSource DataSource;

esp_err_t initTrafficData(void);

esp_err_t borrowTrafficData(SpeedCategory category, TickType_t xTicksToWait);
esp_err_t releaseTrafficData(SpeedCategory category);

esp_err_t updateTrafficData(LEDData data[], size_t dataSize, Direction dir, SpeedCategory category);
esp_err_t copyTrafficData(LEDData out[], size_t outLen, Direction dir, SpeedCategory category);

#endif /* TRAFFIC_DATA_H_4_9_25 */
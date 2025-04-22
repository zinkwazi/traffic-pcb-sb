/**
 * mock_indicators.h
 * 
 * Contains functionality for controlling the output of mocked indicator.h
 * function behavior.
 */

#ifdef CONFIG_DISABLE_TESTING_FEATURES
#error "Mock indicators component included in non-testing build!"
#endif

#include <stddef.h>
#include <stdbool.h>

#include "esp_err.h"

enum MockIndicatorCall {
    INDICATE_WIFI_CONNECTED,
    INDICATE_WIFI_NOT_CONNECTED,
    INDICATE_OTA_AVAILABLE,
    INDICATE_OTA_UPDATE,
    INDICATE_OTA_FAILURE,
    INDICATE_OTA_SUCCESS,
    INDICATE_NORTHBOUND,
    INDICATE_SOUTHBOUND,
    INDICATE_DIRECTION,
    INDICATE_RECORDING_END,
    INDICATE_RECORDING_OOB, // out of bounds
};

typedef enum MockIndicatorCall MockIndicatorCall;

esp_err_t mockIndicatorsStartRecording(size_t len);
MockIndicatorCall mockIndicatorsGetRecording(size_t ndx);
void mockIndicatorsDestroyRecording(void);
bool mockIndicatorsRecordingOverflowed(void);
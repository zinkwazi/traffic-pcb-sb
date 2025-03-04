/**
 * refresh.h
 * 
 * Contains function that handle refreshes of the LEDs.
 */

#ifndef REFRESH_H_
#define REFRESH_H_

#include "esp_err.h"
#include "esp_http_client.h"

#include "app_errors.h"
#include "api_connect.h"
#include "led_types.h"
#include "led_registers.h"
#include "animations.h"

#include "main_types.h"

#define REFRESH_ABORT (0x3578)
#define CONNECT_ERROR (0x3569)

void clearBoard(Direction dir);
esp_err_t quickClearBoard(void);
esp_err_t refreshData(LEDData data[static MAX_NUM_LEDS_REG + 1], esp_http_client_handle_t client, Direction dir, SpeedCategory category, ErrorResources *errRes);
esp_err_t refreshBoard(LEDData currSpeeds[static MAX_NUM_LEDS_REG + 1], LEDData typicalSpeeds[static MAX_NUM_LEDS_REG + 1], Animation anim);

#endif /* REFRESH_H_ */
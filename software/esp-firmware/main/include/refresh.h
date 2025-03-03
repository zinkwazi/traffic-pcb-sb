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

#include "main_types.h"

#define REFRESH_ABORT (0x3578)
#define CONNECT_ERROR (0x3569)

void clearLEDs(Direction dir);
esp_err_t refreshTypicalSpeeds(esp_http_client_handle_t client, LEDData typicalSouth[static MAX_NUM_LEDS_REG], LEDData typicalNorth[MAX_NUM_LEDS_REG]);
esp_err_t handleRefresh(Direction dir, LEDData typicalSpeeds[static MAX_NUM_LEDS_REG], esp_http_client_handle_t client, ErrorResources *errRes);

#endif /* REFRESH_H_ */
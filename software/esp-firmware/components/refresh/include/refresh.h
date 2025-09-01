/**
 * refresh.h
 * 
 * Contains functionality for refreshing all of the traffic and direction LEDs
 * on the board. Note that the main task has ownership of this resource and
 * it is designed with that in mind.
 */

#ifndef REFRESH_H_4_9_25
#define REFRESH_H_4_9_25

#include "esp_err.h"
#include "esp_http_client.h"

#include "animations.h"
#include "app_errors.h"
#include "led_registers.h"
#include "main_types.h"

#define REFRESH_ABORT (0x3578)
#define REFRESH_ABORT_NO_CLEAR (0x3592)
#define CONNECT_ERROR (0x3569)

esp_err_t initRefresh(void);
esp_http_client_handle_t initHttpClient(void);
void lockBoardRefresh(void);
void unlockBoardRefresh(void);
esp_err_t clearBoard(Direction dir, bool quick);
esp_err_t quickClearBoard(Direction dir);
esp_err_t refreshData(LEDData data[], esp_http_client_handle_t client, Direction dir, SpeedCategory category);
esp_err_t refreshBoard(Direction dir, Animation anim);

#endif /* REFRESH_H_4_9_25 */
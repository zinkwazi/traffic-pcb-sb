/**
 * input.h
 * 
 * Contains button input functionality, which
 * is complicated by differences in meaning
 * between quick and long button presses.
 */

#ifndef INPUT_H_6_21_25
#define INPUT_H_6_21_25

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t initInput(TaskHandle_t otaTask, TaskHandle_t mainTask, bool *toggle);

/**
 * Quick direction button press.
 * 
 * This button press causes a refresh of the board
 * and switches direction or quickly clears the board 
 * if a refresh is already underway.
 * 
 * @note Due to the presence of the "hold direction press",
 * this button registers when the button is released after
 * a debouncing period.
 */

esp_err_t enableQuickDirButton(void);
esp_err_t disableQuickDirButton(void);

/**
 * Hold direction button press.
 * 
 * A long hold of the direction button toggles nighttime mode.
 * 
 * @note This button registers after a duration of time has
 * passed with the direction button being held. Nothing happens
 * when the button is initially released afterward.
 */

esp_err_t enableHoldDirButton(void);
esp_err_t disableHoldDirButton(void);

/**
 * OTA button press.
 * 
 * An OTA button press initiates an OTA update.
 * 
 * @note This button registers when the button is pressed,
 * not released, after a debouncing period.
 */

esp_err_t enableOTAButton(void);
esp_err_t disableOTAButton(void);

#endif /* INPUT_H_6_21_25 */
/**
 * refresh_task.h
 * 
 * Contains the refresh task, which is a task that controls traffic LEDs
 * and handles high level commands to update them. It will sleep in
 * between updating each LED and wake up immediately if a new command
 * is received.
 */

#ifndef REFRESH_TASK_H_
#define REFRESH_TASK_H_

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "main_types.h"
#include "led_types.h"

#include "refresh_fsm.h"

esp_err_t createRefreshTask(TaskHandle_t *handle, const UBaseType_t prio);
esp_err_t refreshLEDs(LEDSpeed *data, uint32_t dataLen, Direction dir, TickType_t blockTime);
esp_err_t refreshEnableNightMode(TickType_t blockTime);
esp_err_t refreshDisableNightMode(TickType_t blockTime);

#ifdef CONFIG_TEST_REFRESH

void refreshResetTaskResources(void);

#endif /* CONFIG_TEST_REFRESH */
#endif /* REFRESH_TASK_H_ */
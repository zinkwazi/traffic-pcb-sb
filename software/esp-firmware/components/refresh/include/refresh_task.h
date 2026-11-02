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

#include "refresh_types.h"

esp_err_t createRefreshTask(TaskHandle_t *handle, const UBaseType_t prio);
esp_err_t refreshTaskReserveFrame(LEDSpeed (**frame)[MAX_FRAME_SIZE], uint32_t *frameNdx, TickType_t ticksToWait);
esp_err_t refreshTaskReleaseFrame(uint32_t frameNdx, TickType_t ticksToWait);
esp_err_t refreshTaskRefreshLEDs(uint32_t frameNdx, uint32_t frameLen, Direction dir, volatile bool *received, volatile bool *cleared, TickType_t ticksToWait);
esp_err_t refreshTaskUpdateTypical(uint32_t frameNdx, uint32_t frameLen, Direction dir, volatile bool *received, TickType_t ticksToWait);
esp_err_t refreshTaskEnableNightMode(volatile bool *received, TickType_t ticksToWait);
esp_err_t refreshTaskDisableNightMode(volatile bool *received, TickType_t ticksToWait);

#ifdef CONFIG_TEST_REFRESH

#include "refresh_fsm.h"

void refreshTaskResetResources(void);
esp_err_t refreshTaskCreateResourcesForTest(void);
bool refreshTaskInit(void);
void refreshTaskHandleCommand(RefreshFSMCommand *cmd, RefreshFSMOutput *out);
uint32_t refreshTaskGetAvailableFrameCount(void);
bool refreshTaskIsFrameReserved(uint32_t frameNdx);

#endif /* CONFIG_TEST_REFRESH */
#endif /* REFRESH_TASK_H_ */
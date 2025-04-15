/**
 * strobe_task.h
 * 
 * Contains functionality to create the strobe task. Separate from strobe.h
 * in order to minimize the risk of modifying the strobe command queue handle.
 */

#ifndef STROBE_TASK_H_4_5_25
#define STROBE_TASK_H_4_5_25

#include "sdkconfig.h"

#ifdef CONFIG_SUPPORT_STROBING

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"

#include "app_errors.h"

/* indicates that the strobe command queue has been fully processed. This is
0x00 almost always, with 0x01 occurring momentarily after the queue is confirmed
to be empty. Thus, it acts as a synchronization point for other tasks to know
that the strobe task will no longer modify an LED that was just unregistered. */
#define STROBE_QUEUE_PROCESSED_EVENT_BIT (0x01)

QueueHandle_t getStrobeQueue(void);
esp_err_t strobeWaitEvent(EventBits_t bits, bool allBits, TickType_t blockTime);
esp_err_t acquireStrobeQueueMutex(TickType_t blockTime);
esp_err_t releaseStrobeQueueMutex(void);
esp_err_t createStrobeTask(TaskHandle_t *handle);

#endif /* CONFIG_SUPPORT_STROBING */
#endif /* STROBE_TASK_H_4_5_25 */
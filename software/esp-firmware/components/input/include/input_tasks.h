/**
 * input_tasks.h
 * 
 * Created On: 9/17/2026
 * Author: Jaden Baptista
 * 
 * Contains input tasks that handle deferred processing
 * of button press IRQs to avoid increasing system jitter.
 */

#ifndef INC_INPUT_TASKS_H_9172026
#define INC_INPUT_TASKS_H_9172026

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/**
 * An event posted to a button task's event queue, either by the button's
 * ISR (an edge) or by its debounce timer callback (a verification window
 * elapsing with no edge). The pin level is captured by whichever of those
 * posts the event, so the task never has to read it late.
 */
typedef struct ButtonEvent
{
    bool edgeDetected; // true if this event is an edge; false if a debounce timeout
    bool pinHigh;      // the GPIO pin level at the time this event was generated
} ButtonEvent;

extern QueueHandle_t otaButtonEventQueue;
extern QueueHandle_t toggleButtonEventQueue;

esp_err_t createOTAButtonTask(TaskHandle_t *handle, const UBaseType_t prio, TaskHandle_t otaTaskHandle);
esp_err_t createToggleButtonTask(TaskHandle_t *handle, const UBaseType_t prio, QueueHandle_t mainInputQueue);

#endif /* INC_INPUT_TASKS_H_9172026 */
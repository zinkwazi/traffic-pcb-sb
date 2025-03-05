/**
 * app_errors.c
 * 
 * Contains functions for raising error states to the user.
 */

#include <stdbool.h>

#include "sdkconfig.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"

#include "app_errors.h"
#include "pinout.h"
#include "led_matrix.h"

#define TAG "app_error"

#define ERROR_COLOR_RED (0xFF)
#define ERROR_COLOR_GREEN (0x00)
#define ERROR_COLOR_BLUE (0x00)


#if CONFIG_HARDWARE_VERSION == 1
static inline void indicateError(void) {
  /* intentionally ignoring error codes because 
     this is a best effort function */
  (void) gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
  (void) gpio_set_level(ERR_LED_PIN, 1);
}

static inline void indicateNoError(void) {
  /* intentionally ignoring error codes because 
     this is a best effort function */
  (void) gpio_set_level(ERR_LED_PIN, 0);
}
#elif CONFIG_HARDWARE_VERSION == 2
static inline void indicateError(void) {
  /* intentionally ignoring error codes because 
     this is a best effort function */
  (void) matSetColor(ERROR_LED_NUM, ERROR_COLOR_RED, ERROR_COLOR_GREEN, ERROR_COLOR_BLUE);
}

static inline void indicateNoError(void) {
  /* intentionally ignoring error codes because 
     this is a best effort function */
  (void) matSetColor(ERROR_LED_NUM, 0x00, 0x00, 0x00);
}
#else
#error "Unsupported hardware version!"
#endif


void throwNoConnError(ErrorResources *errRes, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }
  
  switch (errRes->err) {
    case NO_ERR:
      errRes->err = NO_SERVER_CONNECT_ERR;
      /* falls through */
    case NO_SERVER_CONNECT_ERR:
      if (errRes->errTimer == NULL) {
        startErrorFlashing(errRes, true);
      }
      break;
    case HANDLEABLE_ERR:
      errRes->err = HANDLEABLE_AND_NO_SERVER_CONNECT_ERR;
      // do not flash because solid takes priority
      break;
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      break;
    case FATAL_ERR:
      throwFatalError(errRes, true);
      break;
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
}

void throwHandleableError(ErrorResources *errRes, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  if (errRes->errTimer != NULL) {
    stopErrorFlashing(errRes, true);
  }
  indicateError();
  switch (errRes->err) {
    case NO_ERR:
      errRes->err = HANDLEABLE_ERR;
      break;
    case NO_SERVER_CONNECT_ERR:
      errRes->err = HANDLEABLE_AND_NO_SERVER_CONNECT_ERR;
      break;
    case HANDLEABLE_ERR:
      // cannot have multiple handleable errors at once
      /* falls through */
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      // cannot have multiple handleable errors at once
      ESP_LOGE(TAG, "multiple HANDLEABLE_ERR thrown!");
      /* falls through */
    case FATAL_ERR:
      throwFatalError(errRes, true);
      break;
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    ESP_LOGD(TAG, "releasing error semaphore");
    xSemaphoreGive(errRes->errMutex);
  }
}

void throwFatalError(ErrorResources *errRes, bool callerHasErrMutex) {
  ESP_LOGE(TAG, "FATAL_ERR thrown!");
  if (errRes == NULL) {
    indicateError();
    for (;;) {
      vTaskDelay(INT_MAX);
    }
  }

  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }
  if (errRes->errTimer != NULL) {
    stopErrorFlashing(errRes, true);
  }
  indicateError();
  errRes->err = FATAL_ERR;

#ifdef CONFIG_FATAL_CAUSES_REBOOT
  vTaskDelay(pdMS_TO_TICKS(CONFIG_ERROR_PERIOD)); // let the error LED shine for a short time
  gpio_set_level(ERR_LED_PIN, 0);
  esp_restart();
#endif /* CONFIG_FATAL_CAUSES_REBOOT == true */  

  xSemaphoreGive(errRes->errMutex); // give up mutex in caller's name
  /* calling task should not return */
  for (;;) {
    vTaskDelay(INT_MAX);
  }
}

void resolveNoConnError(ErrorResources *errRes, bool resolveNone, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  ESP_LOGW(TAG, "resolving NO_SERVER_CONNECT_ERR");
  if (errRes->errTimer != NULL) {
    stopErrorFlashing(errRes, true);
    indicateNoError();
  }
  switch (errRes->err) {
    case NO_SERVER_CONNECT_ERR:
      errRes->err = NO_ERR;
      break;
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      errRes->err = HANDLEABLE_ERR;
      break;
    case NO_ERR:
      /* falls through */
    case HANDLEABLE_ERR:
      if (resolveNone) {
        break;
      }
      ESP_LOGE(TAG, "resolving NO_SERVER_CONNECT_ERR without its error state");
      /* falls through */
    case FATAL_ERR:
      /* falls through */
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
}

void resolveHandleableError(ErrorResources *errRes, bool resolveNone, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  ESP_LOGW(TAG, "resolving HANDLEABLE_ERR");
  switch (errRes->err) {
    case HANDLEABLE_ERR:
      errRes->err = NO_ERR;
      indicateNoError();
      break;
    case HANDLEABLE_AND_NO_SERVER_CONNECT_ERR:
      errRes->err = NO_SERVER_CONNECT_ERR;
      if (errRes->errTimer == NULL) {
        startErrorFlashing(errRes, true);
      }
      break;
    case NO_ERR:
      /* falls through */
    case NO_SERVER_CONNECT_ERR:
      if (resolveNone) {
        break;
      }
      ESP_LOGE(TAG, "resolving HANDLEABLE_ERR without its error state");
      /* falls through */
    case FATAL_ERR:
      /* falls through */
    default:
      throwFatalError(errRes, true);
      break;
  }

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
}

void startErrorFlashing(ErrorResources *errRes, bool callerHasErrMutex) {
    const esp_timer_create_args_t timerArgs = {
      .callback = timerFlashErrCallback,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "errorTimer",
    };

    if (!callerHasErrMutex) {
      while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
    }
    if (esp_timer_create(&timerArgs, &(errRes->errTimer)) != ESP_OK) {
      throwFatalError(errRes, true);
    }
    if (esp_timer_start_periodic(errRes->errTimer, CONFIG_ERROR_PERIOD * 1000) != ESP_OK) {
      esp_timer_delete(errRes->errTimer);
      errRes->errTimer = NULL;
      throwFatalError(errRes, true);
    }

    if (!callerHasErrMutex) {
      xSemaphoreGive(errRes->errMutex);
    }
}

void stopErrorFlashing(ErrorResources *errRes, bool callerHasErrMutex) {
  if (!callerHasErrMutex) {
    while (xSemaphoreTake(errRes->errMutex, INT_MAX) != pdTRUE) {}
  }

  esp_timer_stop(errRes->errTimer);
  esp_timer_delete(errRes->errTimer);
  errRes->errTimer = NULL;

  if (!callerHasErrMutex) {
    xSemaphoreGive(errRes->errMutex);
  }
}

void timerFlashErrCallback(void *params) {
    static bool currentOutput = false;
    currentOutput = (currentOutput) ? false : true;
    if (currentOutput)
    {
      indicateError();
    } else 
    {
      indicateNoError();
    }
}
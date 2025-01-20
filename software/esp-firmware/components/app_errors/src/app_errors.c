#include "app_errors.h"
#include "pinout.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include <stdbool.h>

#define TAG "app_error"

static inline void indicateError(void) {
    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(ERR_LED_PIN, 1);
}

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
    ESP_LOGI(TAG, "releasing error semaphore");
    xSemaphoreGive(errRes->errMutex);
  }
}

void throwFatalError(ErrorResources *errRes, bool callerHasErrMutex) {
  ESP_LOGE(TAG, "FATAL_ERR thrown!");
  gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
  if (errRes == NULL) {
    gpio_set_level(ERR_LED_PIN, 1);
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
  gpio_set_level(ERR_LED_PIN, 1);
  errRes->err = FATAL_ERR;
  
  xSemaphoreGive(errRes->errMutex); // give up mutex in caller's name
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
    gpio_set_level(ERR_LED_PIN, 0);
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
      gpio_set_level(ERR_LED_PIN, 0);
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

    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
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
    static int currentOutput = 0;
    currentOutput = (currentOutput == 1) ? 0 : 1;
    gpio_set_level(ERR_LED_PIN, currentOutput);
}
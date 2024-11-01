/**
 * main.c
 * 
 * This file contains the entrypoint task
 * for the application (app_main) and
 * configuration including function hooks.
 */

/* IDF component includes */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_tls.h"
#include "cJSON.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

/* Main component includes */
#include "pinout.h"
#include "worker.h"

/* Component includes */
#include "api_config.h"
#include "tomtom.h"
#include "led_locations.h"
#include "dots_commands.h"

#define TAG "app_main"

#define I2C_GATEKEEPER_STACK ESP_TASK_MAIN_STACK // TODO: determine minimum stack size for i2c gatekeeper
#define I2C_GATEKEEPER_PRIO (ESP_TASK_MAIN_PRIO + 1) // always start an I2C command if possible
#define I2C_QUEUE_SIZE 20

#define NUM_DOT_WORKERS 5
#define DOTS_WORKER_STACK ESP_TASK_MAIN_STACK
#define DOTS_WORKER_PRIO (ESP_TASK_MAIN_PRIO - 1)
#define DOTS_QUEUE_SIZE 10

#define NUM_LEDS 326
#define DOTS_GLOBAL_CURRENT 0x08

void dirButtonISR(void *params) {
  TaskHandle_t mainTask = (TaskHandle_t) params;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(mainTask, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

esp_err_t initDotMatrices(QueueHandle_t I2CQueue) {
    ESP_LOGI(TAG, "initializing dot matrices");
    ESP_RETURN_ON_ERROR(
      dotsReset(I2CQueue),
      TAG, "failed to reset dot matrices"
    );
    ESP_RETURN_ON_ERROR(
      dotsSetGlobalCurrentControl(I2CQueue, DOTS_GLOBAL_CURRENT),
      TAG, "failed to change dot matrices global current control"
    );
    for (int i = 1; i <= NUM_LEDS; i++) {
      ESP_RETURN_ON_ERROR(
        dotsSetScaling(I2CQueue, i, 0xFF, 0xFF, 0xFF),
        TAG, "failed to set dot scaling"
      );
    }
    ESP_RETURN_ON_ERROR(
      dotsSetOperatingMode(I2CQueue, NORMAL_OPERATION),
      TAG, "failed to set dot matrices into normal operation mode"
    );
    return ESP_OK;
}

esp_err_t createI2CGatekeeperTask(QueueHandle_t I2CQueue) {
  BaseType_t success;
  I2CGatekeeperTaskParams *params;
  /* input guards */
  ESP_RETURN_ON_FALSE(
    (I2CQueue != NULL), ESP_FAIL,
    TAG, "cannot create I2C gatekeeper task with NULL I2C command queue"
  );
  /* create parameters */
  params = malloc(sizeof(I2CGatekeeperTaskParams));
  ESP_RETURN_ON_FALSE(
    (params != NULL), ESP_FAIL,
    TAG, "failed to allocate memory"
  );
  params->I2CQueue = I2CQueue;
  params->port = I2C_PORT;
  params->sdaPin = SDA_PIN;
  params->sclPin = SCL_PIN;
  /* create task */
  success = xTaskCreate(vI2CGatekeeperTask, "I2CGatekeeper", I2C_GATEKEEPER_STACK,
                        params, I2C_GATEKEEPER_PRIO, NULL);
  ESP_RETURN_ON_FALSE(
    (success == pdPASS), ESP_FAIL,
    TAG, "failed to create I2C gatekeeper"
  );
  return ESP_OK;
}

esp_err_t createDotWorkerTask(char *name, QueueHandle_t dotQueue, QueueHandle_t I2CQueue) {
  BaseType_t success;
  struct dotWorkerTaskParams *params;
  /* input guards */
  ESP_RETURN_ON_FALSE(
    (dotQueue != NULL), ESP_FAIL,
    TAG, "cannot create dot worker task with NULL dot command queue"
  );
  ESP_RETURN_ON_FALSE(
    (I2CQueue != NULL), ESP_FAIL,
    TAG, "cannot create dot worker task with NULL I2C command queue"
  );
  if (name == NULL) {
    name = "worker";
  }
  /* allocate parameters */
  params = malloc(sizeof(struct dotWorkerTaskParams)); // task parameters are given via reference, not copy
  ESP_RETURN_ON_FALSE(
    (params != NULL), ESP_FAIL,
    TAG, "failed to allocate memory for dot worker task parameters"
  );
  params->dotQueue = dotQueue;
  params->I2CQueue = I2CQueue;
  /* create task */
  success = xTaskCreate(vDotWorkerTask, name, DOTS_WORKER_STACK, 
                        params, DOTS_WORKER_PRIO, NULL);
  if (success != pdPASS) {
    ESP_LOGE(TAG, "failed to create dot worker task");
    free(params);
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t initDirectionLEDs(void) {
    /* Set GPIO directions */
    ESP_RETURN_ON_ERROR(
      gpio_set_direction(LED_NORTH_PIN, GPIO_MODE_OUTPUT),
      TAG, "failed to set north led pin to output"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_direction(LED_EAST_PIN, GPIO_MODE_OUTPUT),
      TAG, "failed to set east led pin to output"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_direction(LED_SOUTH_PIN, GPIO_MODE_OUTPUT),
      TAG, "failed to set south led pin to output"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_direction(LED_WEST_PIN, GPIO_MODE_OUTPUT),
      TAG, "failed to set west led pin to output"
    );
    /* Set GPIO levels */
    ESP_RETURN_ON_ERROR(
      gpio_set_level(LED_NORTH_PIN, 0),
      TAG, "failed to turn off direction led"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_level(LED_EAST_PIN, 0),
      TAG, "failed to turn off direction led"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_level(LED_SOUTH_PIN, 0),
      TAG, "failed to turn off direction led"
    );
    ESP_RETURN_ON_ERROR(
      gpio_set_level(LED_WEST_PIN, 0),
      TAG, "failed to turn off direction led"
    );
    return ESP_OK;
}

esp_err_t initDirectionButton(void) {
  ESP_RETURN_ON_ERROR(
    gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT), // pin has an external pullup
    TAG, "failed to set gpio direction of direction button"
  );
  TaskHandle_t mainTask = xTaskGetCurrentTaskHandle(); // ISR will send event notifications to this task
  ESP_RETURN_ON_ERROR(
    gpio_set_intr_type(T_SW_PIN, GPIO_INTR_NEGEDGE),
    TAG, "failed to set direction button interrupt type"
  );
  ESP_RETURN_ON_ERROR(
    gpio_isr_handler_add(T_SW_PIN, dirButtonISR, (void *) mainTask),
    TAG, "failed to setup direction button ISR"
  );
  ESP_RETURN_ON_ERROR(
    gpio_intr_enable(T_SW_PIN),
    TAG, "failed to enable interrupts for direction button"
  );
  return ESP_OK;
}

esp_err_t enableDirectionButtonIntr(void) {
  ESP_RETURN_ON_ERROR(
    gpio_intr_enable(T_SW_PIN),
    TAG, "failed to enable interrupts for direction button"
  );
  return ESP_OK;
}

esp_err_t disableDirectionButtonIntr(void) {
  ESP_RETURN_ON_ERROR(
    gpio_intr_disable(T_SW_PIN),
    TAG, "failed to disable interrupts for direction button"
  );
  return ESP_OK;
}

esp_err_t clearLEDs(QueueHandle_t I2CQueue) {
  for(int i = 1; i <= NUM_LEDS; i++) {
    ESP_RETURN_ON_ERROR(
      dotsSetColor(I2CQueue, i, 0x00, 0x00, 0x00),
      TAG, "failed to clear led"
    );
  }
  return ESP_OK;
}

esp_err_t updateLEDs(QueueHandle_t dotQueue, Direction dir) {
  esp_err_t ret = ESP_OK;
  DotCommand dot;
  int startNdx, endNdx, northLvl, southLvl, eastLvl, westLvl;
  /* input guards */
  ESP_RETURN_ON_FALSE(
    (dotQueue != NULL), ESP_FAIL,
    TAG, "updateLEDs received NULL dot queue handle"
  );
  /* update direction LEDs */
  switch (dir) {
    case NORTH:
      startNdx = NUM_LEDS;
      endNdx = 1;
      northLvl = 1;
      eastLvl = 0;
      southLvl = 0;
      westLvl = 1;
      break;
    case SOUTH:
      startNdx = 1;
      endNdx = NUM_LEDS;
      northLvl = 0;
      eastLvl = 1;
      southLvl = 1;
      westLvl = 0;
      break;
    default:
      ESP_LOGE(TAG, "updateLEDs received unknown direction");
      goto error_with_dir_leds_off;
      break;
  }
  ESP_GOTO_ON_ERROR(
    gpio_set_level(LED_NORTH_PIN, northLvl), error_with_dir_leds_off,
    TAG, "failed to turn change north led"
  );
  ESP_GOTO_ON_ERROR(
    gpio_set_level(LED_EAST_PIN, eastLvl), error_with_dir_leds_off,
    TAG, "failed to turn change east led"
  );
  ESP_GOTO_ON_ERROR(
    gpio_set_level(LED_SOUTH_PIN, southLvl), error_with_dir_leds_off,
    TAG, "failed to turn change south led"
  );
  ESP_GOTO_ON_ERROR(
    gpio_set_level(LED_WEST_PIN, westLvl), error_with_dir_leds_off,
    TAG, "failed to turn change west led"
  );
  /* send commands to update all dot LEDs */
  dot.dir = dir;
  if (startNdx <= endNdx) {
    for (int i = startNdx; i <= endNdx; i++) {
      dot.ledNum = i;
      while (pdPASS != xQueueSendToBack(dotQueue, &dot, INT_MAX)) {
        ESP_LOGI(TAG, "failed to add dot to queue, retrying...");
      }
    }
  } else {
    for (int i = startNdx; i >= endNdx; i--) {
      dot.ledNum = i;
      while (pdPASS != xQueueSendToBack(dotQueue, &dot, INT_MAX)) {
        ESP_LOGI(TAG, "failed to add dot to queue, retrying...");
      }
    }
  }
  /* wait for all LEDs to be updated */
  while (uxQueueMessagesWaiting(dotQueue) > 0) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  return ret;
error_with_dir_leds_off:
  /* wait for all LEDs to be updated */
  while (uxQueueMessagesWaiting(dotQueue) > 0) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  ESP_ERROR_CHECK(gpio_set_level(LED_NORTH_PIN, 0));
  ESP_ERROR_CHECK(gpio_set_level(LED_EAST_PIN, 0));
  ESP_ERROR_CHECK(gpio_set_level(LED_SOUTH_PIN, 0));
  ESP_ERROR_CHECK(gpio_set_level(LED_WEST_PIN, 0));
  return ret;
}

/**
 * The entrypoint task of the application, which initializes 
 * other tasks, then handles TomTom API calls.
 */
void app_main(void)
{
    esp_err_t ret;
    QueueHandle_t I2CQueue, dotQueue;
    int i;

    /* Create queues */
    I2CQueue = xQueueCreate(I2C_QUEUE_SIZE, sizeof(I2CCommand));
    ESP_GOTO_ON_FALSE(
      (I2CQueue != NULL), ESP_FAIL, spin_forever,
      TAG, "failed to allocate I2C queue"
    );
    dotQueue = xQueueCreate(DOTS_QUEUE_SIZE, sizeof(DotCommand));
    ESP_GOTO_ON_FALSE(
      (dotQueue != NULL), ESP_FAIL, spin_forever,
      TAG, "failed to allocate dots queue"
    );
    /* create tasks */
    ESP_GOTO_ON_ERROR(
      createI2CGatekeeperTask(I2CQueue), spin_forever,
      TAG, "failed to create I2C gatekeeper task"
    );
    for (i = 0; i < NUM_DOT_WORKERS; i++) {
      ESP_GOTO_ON_ERROR(
        createDotWorkerTask("dotWorker", dotQueue, I2CQueue), spin_forever,
        TAG, "failed to create dot worker task"
      );
    }
    /* initialize pins */
    ESP_GOTO_ON_ERROR(
      initDirectionLEDs(), spin_forever,
      TAG, "failed to initialize direction leds"
    );
    /* initialize matrices */
    ESP_GOTO_ON_ERROR(
      initDotMatrices(I2CQueue), spin_forever,
      TAG, "failed to initialize dot matrices"
    );
    ESP_GOTO_ON_ERROR(
      clearLEDs(I2CQueue), spin_forever,
      TAG, "failed to clear dots"
    );
    /* initialize NVS */
    ESP_LOGI(TAG, "initializing nvs");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    /* initialize tcp/ip stack */
    ESP_LOGI(TAG, "initializing TCP/IP stack");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    /* Establish wifi connection */
    ESP_LOGI(TAG, "establishing wifi connection");
    wifi_init_config_t default_wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&default_wifi_cfg));
    ESP_ERROR_CHECK(establishWifiConnection());
    esp_tls_t *tls = esp_tls_init();
    ESP_GOTO_ON_FALSE(
      (tls != NULL), ESP_FAIL, spin_forever,
      TAG, "failed to allocate esp_tls handle"
    );
    /* initialize direction button */
    ESP_GOTO_ON_ERROR(
      gpio_install_isr_service(0), spin_forever,
      TAG, "failed to install gpio ISR service"
    );
    ESP_GOTO_ON_ERROR(
      initDirectionButton(), spin_forever,
      TAG, "failed to initialize direction button"
    );
    ESP_LOGI(TAG, "initialization complete, handling toggle button presses...");
    /* handle requests to update all LEDs */
    Direction currDirection = NORTH;
    for (;;) {
      ESP_GOTO_ON_ERROR(
        enableDirectionButtonIntr(), spin_forever,
        TAG, "failed to enable direction button interrupts"
      );
      uint32_t ulNotificationValue = ulTaskNotifyTake(0, INT_MAX);
      while (ulNotificationValue != 1) {
        continue;
      }
      ESP_GOTO_ON_ERROR(
        disableDirectionButtonIntr(), spin_forever,
        TAG, "failed to disable direction button interrupts"
      );
      switch (currDirection) {
        case NORTH:
          currDirection = SOUTH;
          break;
        case SOUTH:
          currDirection = NORTH;
          break;
        default:
          currDirection = NORTH;
          ESP_LOGW(TAG, "current direction is not NORTH nor SOUTH, setting NORTH...");
          break;
      }
      if (clearLEDs(I2CQueue) != ESP_OK) {
        ESP_LOGE(TAG, "failed to clear LEDs");
        continue;
      }
      if (updateLEDs(dotQueue, currDirection) != ESP_OK) {
        ESP_LOGE(TAG, "failed to update LEDs");
        continue;
      }
    }
    /* This task has nothing left to do, but should not exit */
spin_forever:
    for (;;) {
      vTaskDelay(INT_MAX);
    }
}
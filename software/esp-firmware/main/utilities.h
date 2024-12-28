/**
 * utilities.h
 * 
 * This file contains functions that may be useful to tasks
 * contained in various other header files.
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* IDF component includes */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_vfs_dev.h"
#include "driver/uart_vfs.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_tls.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_timer.h"
#include "nvs.h"
#include "esp_https_ota.h"

/* Main component includes */
#include "pinout.h"
#include "worker.h"
#include "utilities.h"
#include "wifi.h"

/* Component includes */
#include "api_config.h"
#include "tomtom.h"
#include "led_locations.h"
#include "dots_commands.h"
#include "led_registers.h"

/** @brief The name of the non-volatile storage entry for the wifi SSID. */
#define WIFI_SSID_NVS_NAME "wifi_ssid"

/** @brief The name of the non-volatile storage entry for the wifi password. */
#define WIFI_PASS_NVS_NAME "wifi_pass"

/** @brief The name of the non-volatile storage entry for the api key. */
#define API_KEY_NVS_NAME "api_key"

/** @brief The task priority of the main task. */
#define MAIN_TASK_PRIO (3)

/** @brief The stack size allocated for the I2C gatekeeper task. */
#define I2C_GATEKEEPER_STACK (ESP_TASK_MAIN_STACK - 1400)

/** @brief The task priority of the I2C gatekeeper task. */
#define I2C_GATEKEEPER_PRIO (2)

/** @brief The queue size in elements of the I2C command queue. */
#define I2C_QUEUE_SIZE 20

/* UPDATE DOT_WORKER_BITS IF CHANGING NUM_DOT_WORKERS!!!! */
#define NUM_DOT_WORKERS 1 // maximum 8 due to the necessity of a synchronizing 
                          // event group with a bit for each task
#define DOT_WORKER_BITS (0x01) // event group bits for worker tasks, one for each task

/** @brief The stack size allocated for the dot worker task. */
#define DOTS_WORKER_STACK (ESP_TASK_MAIN_STACK + 1000)

/** @brief The task priority of the dot worker task. */
#define DOTS_WORKER_PRIO (1)

/** @brief The queue size of the dot command queue. */
#define DOTS_QUEUE_SIZE 1

/** @brief The stack size allocated for the OTA task. */
#define OTA_TASK_STACK (ESP_TASK_MAIN_STACK)

/** @brief The task priority of the OTA task. */
#define OTA_TASK_PRIO (4)

/** @brief The number of LEDs present on the device. */
#define NUM_LEDS sizeof(LEDNumToReg) / sizeof(LEDNumToReg[0])

/**
 * @brief Calls spinForever if x is not ESP_OK.
 * 
 * @param x A value of type esp_err_t.
 * @param occurred A pointer to a bool that indicates whether an error has 
 *                 occurred at any point in the program or not. This bool is 
 *                 shared by all  tasks and should only be accessed after 
 *                 errMutex has been obtained.
 * @param errMutex A handle to a mutex that guards access to the bool pointed to
 *                 by occurred.
 */
#define SPIN_IF_ERR(x, occurred, errMutex) if (x != ESP_OK) { spinForever(occurred, errMutex); }

/**
 * @brief Calls spinForever if x is not true.
 * 
 * @param x A bool.
 * @param occurred A pointer to a bool that indicates whether an error has 
 *                 occurred at any point in the program or not. This bool is 
 *                 shared by all  tasks and should only be accessed after 
 *                 errMutex has been obtained.
 * @param errMutex A handle to a mutex that guards access to the bool pointed to
 *                 by occurred.
 */
#define SPIN_IF_FALSE(x, occurred, errMutex) if (!x) { spinForever(occurred, errMutex); } 

/**
 * @brief Calls updateSettingsAndRestart if x is not ESP_OK.
 * 
 * @param x A value of type esp_err_t.
 * @param handle The non-volatile storage handle to store user settings in.
 * @param occurred A pointer to a bool that indicates whether an error has 
 *                 occurred at any point in the program or not. This bool is 
 *                 shared by all  tasks and should only be accessed after 
 *                 errMutex has been obtained.
 * @param errMutex A handle to a mutex that guards access to the bool pointed to
 *                 by occurred.
 */
#define UPDATE_SETTINGS_IF_ERR(x, handle, occurred, errMutex) if (x != ESP_OK) { updateSettingsAndRestart(handle, occurred, errMutex); }

/**
 * @brief Calls updateSettingsAndRestart if x is not true.
 * 
 * @param x A bool.
 * @param handle The non-volatile storage handle to store user settings in.
 * @param occurred A pointer to a bool that indicates whether an error has 
 *                 occurred at any point in the program or not. This bool is 
 *                 shared by all  tasks and should only be accessed after 
 *                 errMutex has been obtained.
 * @param errMutex A handle to a mutex that guards access to the bool pointed to
 *                 by occurred.
 */
#define UPDATE_SETTINGS_IF_FALSE(x, handle, occurred, errMutex) if (!x) { updateSettingsAndRestart(handle, occurred, errMutex); }

/**
 * @brief User non-volatile storage settings.
 * 
 * @note This struct is populated when user non-volatile storage settings
 *       are retrieved with retrieveNvsEntries.
 */
struct userSettings {
  char *wifiSSID; /*!< A string containing the wifi SSID. */
  size_t wifiSSIDLen; /*!< The length of the wifiSSID string. */
  char *wifiPass; /*!< A string containing the wifi password. */
  size_t wifiPassLen; /*!< The length of the wifiPass string. */
  char *apiKey; /*!< NO LONGER IN USE */
  size_t apiKeyLen; /*!< NO LONGER IN USE */
};

/**
 * @brief The input parameters to dirButtonISR, which gives the routine
 * pointers to the main task's objects.
 */
struct dirButtonISRParams {
  TaskHandle_t mainTask; /*!< A handle to the main task used to send a 
                              notification. */
  bool *toggle; /*!< Indicates to the main task that the LED direction should
                     change from North to South or vice versa. The bool should
                     remain in-scope for the duration of use of this struct. */ 
};

/**
 * @brief Interrupt service routine that handles direction button presses.
 * 
 * Handles direction button presses once the main task is 
 * ready to refresh LEDs. A button press is only acted upon
 * once the main task has refreshed all LEDs because the ISR
 * sends a task notification to the main task, which the task
 * only checks once it has finished handling a previous press.
 * 
 * @param params A pointer to a struct dirButtonISRParams that
 *               contains references to the main task's objects.
 */
void dirButtonISR(void *params);

/**
 * @brief Interrupt service routine that handles OTA button presses.
 * 
 * Handles OTA button presses to tell the main task to trigger an over-the-air
 * firmware upgrade.
 * 
 * @param params A TaskHandle_t that is the handle of the main task.
 */
void otaButtonISR(void *params);

/**
 * @brief Callback that periodically sends a task notification to the main task.
 * 
 * Callback that periodically tells the main task to refresh all LEDs if the 
 * direction button has not been pressed. The timer that calls this function 
 * restarts if the direction button is pressed.
 * 
 * @param params A TaskHandle_t that is the handle of the main task.
 */
void timerCallback(void *params);

/**
 * @brief Callback that toggles all the direction LEDs.
 * 
 * Callback that is called from a timer that is active when the main task
 * requests a settings update from the user. This periodically toggles all
 * the direction LEDs, causing them to flash.
 * 
 * @param params An int* used to store the current output value of the LEDs.
 *               This object should not be destroyed or modified while the
 *               timer using this callback is active.
 */
void timerFlashDirCallback(void *params);

/**
 * @brief Determines whether user settings currently exist in non-volatile
 *        storage.
 * 
 * @note User settings should not exist in storage on the first powerup of the
 *       system, however they should exist during subsequent reboots.
 * 
 * @param nvsHandle The non-volatile storage handle where the settings would
 *                  exist.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t nvsEntriesExist(nvs_handle_t nvsHandle);

/**
 * @brief Queries the user for settings and writes responses in non-volatile
 *        storage.
 * 
 * @note Uses UART0 to query settings.
 * 
 * @param nvsHandle The non-volatile storage handle to store settings in.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t getNvsEntriesFromUser(nvs_handle_t nvsHandle);

esp_err_t initDotMatrices(QueueHandle_t I2CQueue);
esp_err_t updateLEDs(QueueHandle_t dotQueue, Direction dir);

/**
 * @brief Retrieves user settings from non-volatile storage.
 * 
 * Retrieves user settings from non-volatile storage and places results in
 * the provided settings, with space allocated from the heap.
 * 
 * @note It is necessary for the calling function to free pointers in 
 *       settings if settings is to be destroyed.
 * 
 * @param nvsHandle The non-volatile storage handle where settings exist.
 * @param[out] settings A pointer to struct userSettings that will be
 *                      populated with the retrieved user settings.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t retrieveNvsEntries(nvs_handle_t nvsHandle, struct userSettings *settings);

/**
 * @brief Initializes the I2C gatekeeper task, which is implemented by 
 *        vI2CGatekeeperTask.
 * 
 * @note The gatekeeper is intended to be the only task that interacts with the
 *       I2C peripheral in order to keep dot matrices in known states.
 * 
 * @param I2CQueue A handle to a queue that holds struct I2CCommand
 *                 objects. This task retrieves commands from this queue and
 *                 performs I2C transactions to fulfill them.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t createI2CGatekeeperTask(QueueHandle_t I2CQueue);

/**
 * @brief Initializes the dot worker task, which is implemented by 
 *        vDotWorkerTask.
 * 
 * @note The dot worker task receives commands from the main task. It is the
 *       task that does the most 'business logic' of the application. It
 *       relieves the main task of these duties so that it can quickly respond
 *       to user input.
 * 
 * @param name A string that is copied and used as the name of this task for
 *             debugging purposes.
 * @param workerNum NO LONGER USED.
 * @param dotQueue A handle to a queue that holds struct DotCommand
 *                 objects. This task retrieves commands from this queue and
 *                 performs work to fulfill them.
 * @param I2CQueue A handle to a queue that holds struct I2CCommand objects.
 *                 This task issues commands to this queue to be handled by the
 *                 I2C gatekeeper, implemented by vI2CGatekeeperTask.
 * @param workerEvents NO LONGER USED.
 * @param apiKey NO LONGER USED.
 * @param errorOccurred A pointer to a bool that indicates whether an error
 *                      has occurred at any point in the program or not. This
 *                      bool is shared by all tasks and should only be accessed
 *                      after errorOccurredMutex has been obtained, ideally
 *                      through the use of the boolWithTestSet function.
 * @param errorOccurredMutex A handle to a mutex that guards access to the bool
 *                           pointed to by errorOccurred.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t createDotWorkerTask(char *name, int workerNum, QueueHandle_t dotQueue, QueueHandle_t I2CQueue, EventGroupHandle_t workerEvents, char *apiKey, bool *errorOccurred, SemaphoreHandle_t errorOccurredMutex);

/**
 * @brief Configures and sets initial levels of the direction LEDs.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t initDirectionLEDs(void);

/**
 * @brief Initializes the direction button and attaches dirButtonISR to a 
 *        negative edge of the GPIO pin.
 * 
 * @param toggle A pointer to a bool that is passed to dirButtonISR. The bool
 *               should be in-scope for the duration of use of dirButtonISR.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t initDirectionButton(bool *toggle);

/**
 * @brief Initializes the OTA button (IO0) and attaches otaButtonISR to a 
 *        negative edge of the GPIO pin.
 * 
 * @param otaTask A handle to the OTA task, which is implemented by vOTATask.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t initIOButton(TaskHandle_t otaTask);

/**
 * @brief Enables the direction button interrupt, which is handled by
 *        dirButtonISR.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t enableDirectionButtonIntr(void);

/**
 * @brief Disables the direction button interrupt, which is handled by
 *        dirButtonISR.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t disableDirectionButtonIntr(void);

/**
 * @brief Sends a command to the worker task to quickly clear all LEDs.
 * 
 * @note The worker task, implemented by vDotWorkerTask, quickly clears all
 *       of the LEDs by resetting all dot matrices.
 * 
 * @param dotQueue The queue that the worker task receives commands from, which
 *                 holds elements of type DotCommand.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t quickClearLEDs(QueueHandle_t dotQueue);

/**
 * @brief Sends a command to the worker task to clear all LEDs sequentially in
 *        a particular direction.
 * 
 * @note This is distinct from quickClearLEDs as the worker task, implemented
 *       by vDotWorkerTask, does not reset the dot matrices to fulfill the 
 *       command.
 * 
 * @param dotQueue The queue that the worker task receives commands from, which
 *                 holds elements of type DotCommand.
 * @param currDir  The direction that the LEDs will be cleared toward.
 *
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t clearLEDs(QueueHandle_t dotQueue, Direction currDir);

/**
 * @brief Handles errors that are not due to a user settings issue by trapping
 *        the task in a delay forever loop after setting the error LED high.
 * 
 * @note This function requires a full system restart from the user and is
 *       intended to give the user time to retrieve error logs.
 * 
 * @param errorOccurred A pointer to a bool that indicates whether an error
 *                      has occurred at any point in the program or not. This
 *                      bool is shared by all tasks and should only be accessed
 *                      after errorOccurredMutex has been obtained, ideally
 *                      through the use of the boolWithTestSet function.
 * @param errorOccurredMutex A handle to a mutex that guards access to the bool
 *                           pointed to by errorOccurred.
 */
void spinForever(bool *errorOccurred, SemaphoreHandle_t errorOccurredMutex);

/**
 * @brief Handles errors that are due to a user settings issue by setting the
 *        error LED high, querying the user for new settings, then restarting 
 *        the application.
 * 
 * @note Errors that occur while attempting to query the user cause the
 *       spinForever function to be called.
 * 
 * @param nvsHandle The non-volatile storage handle to store settings in.
 * @param errorOccurred A pointer to a bool that indicates whether an error
 *                      has occurred at any point in the program or not. This
 *                      bool is shared by all tasks and should only be accessed
 *                      after errorOccurredMutex has been obtained, ideally
 *                      through the use of the boolWithTestSet function.
 * @param errorOccurredMutex A handle to a mutex that guards access to the bool
 *                           pointed to by errorOccurred.
 */
void updateSettingsAndRestart(nvs_handle_t nvsHandle, bool *errorOccurred, SemaphoreHandle_t errorOccurredMutex);

/**
 * Atomically tests and sets val to true. Returns whether
 * val was already true.
 */

/**
 * @brief Atomically tests and sets val to true.
 * 
 * @param val A pointer to a bool that indicates whether an error has occurred 
 *            at any point in the program or not. This bool is shared by all 
 *            tasks and should only be accessed after mutex has been obtained.
 * @param mutex A handle to a mutex that guards access to the bool pointed to by
 *              val.
 * 
 * @returns True if val was true before this function was called, otherwise 
 *          false.
 */
bool boolWithTestSet(bool *val, SemaphoreHandle_t mutex);

#endif /* UTILITIES_H_ */
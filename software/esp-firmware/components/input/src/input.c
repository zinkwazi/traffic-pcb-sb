/**
 * input.c
 * 
 * Contains button input functionality, which
 * is complicated by differences in meaning
 * between quick and long button presses.
 */

#include "input.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "pinout.h"
#include "app_err.h"

#define TAG "input"

#define TIMEOUT 5000

/**
 * @brief The input parameters to dirButtonISR, which gives the routine
 * pointers to the main task's objects.
 */
struct DirButtonISRParams {
    TaskHandle_t mainTask; /*!< A handle to the main task used to send a 
                              notification. */
    bool *toggle; /*!< Indicates to the main task that the LED direction should
                     change from North to South or vice versa. The bool should
                     remain in-scope for the duration of use of this struct. */
    bool *timerExpired; /*!< A pointer to a boolean shared between the direction
                             button interrupt and timer for internal use. */
};

typedef struct DirButtonISRParams DirButtonISRParams;

static esp_timer_handle_t dirButtonTimer = NULL;
static bool dirButtonShortEnable = false;
static bool dirButtonLongEnable = false;

static void shortDirButtonPress(TaskHandle_t mainTask, bool *toggle);
static void longDirButtonPress(TaskHandle_t mainTask, bool *toggle);
static esp_err_t initDirectionButton(TaskHandle_t mainTask, bool *toggle);
static esp_err_t initIOButton(TaskHandle_t otaTask);
static void otaButtonISR(void *params);
static void dirButtonISR(void *params);
static void timerDirButtonCallback(void *params);

esp_err_t initInput(TaskHandle_t otaTask, TaskHandle_t mainTask, bool *toggle)
{
    esp_err_t err;

    err = initDirectionButton(mainTask, toggle);
    if (err != ESP_OK) return err;

    err = initOTAButton(otaTask);
    if (err != ESP_OK) return err;
    
    return ESP_OK;
}


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

esp_err_t enableQuickDirButton(void)
{
    esp_err_t err = ESP_OK;
    if ((!dirButtonShortEnable) && (!dirButtonLongEnable))
    {
        /* interrupt is disabled, thus enable it */
        dirButtonShortEnable = true;
        err = gpio_intr_enable(T_SW_PIN);
    } else
    {
        dirButtonShortEnable = true;
    }
    return err;
}

esp_err_t disableQuickDirButton(void)
{
    esp_err_t err = ESP_OK;
    dirButtonShortEnable = false;
    if (!dirButtonLongEnable)
    {
        /* both button types disabled, thus disable interrupt */
        err = gpio_intr_disable(T_SW_PIN);    
    }
    return err;
}


/**
 * Hold direction button press.
 * 
 * A long hold of the direction button toggles nighttime mode.
 * 
 * @note This button registers after a duration of time has
 * passed with the direction button being held. Nothing happens
 * when the button is initially released afterward.
 */

esp_err_t enableHoldDirButton(void)
{
    esp_err_t err = ESP_OK;
    if ((!dirButtonShortEnable) && (!dirButtonLongEnable))
    {
        /* interrupt is disabled, thus enable it */
        dirButtonLongEnable = true;
        err = gpio_intr_enable(T_SW_PIN);
    } else
    {
        dirButtonLongEnable = true;
    }
    return err;
}

esp_err_t disableHoldDirButton(void)
{
    esp_err_t err = ESP_OK;
    dirButtonLongEnable = false;
    if (!dirButtonShortEnable)
    {
        /* both button types disabled, thus disable interrupt */
        err = gpio_intr_disable(T_SW_PIN);    
    }
    return err;
}


/**
 * OTA button press.
 * 
 * An OTA button press initiates an OTA update.
 * 
 * @note This button registers when the button is pressed,
 * not released, after a debouncing period.
 */

esp_err_t enableOTAButton(void)
{
    return gpio_intr_enable(IO_SW_PIN);
}

esp_err_t disableOTAButton(void)
{
    return gpio_intr_disable(IO_SW_PIN);
}

static esp_err_t initDirectionButton(TaskHandle_t mainTask, bool *toggle)
{
    static DirButtonISRParams dirButtonISRParams;
    static bool timerExpired = false; // state variable for dirButtonISR & dirButtonTimer communication
    const esp_timer_create_args_t timerArgs = {
        .name = "dirButtonTimer",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = timerDirButtonCallback,
        .arg = &timerExpired,
    };
    esp_err_t err;

    /* input guards */
    if (mainTask == NULL || toggle == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);

    /* initialize params */
    dirButtonISRParams.mainTask = mainTask;
    dirButtonISRParams.toggle = toggle;
    dirButtonISRParams.timerExpired = &timerExpired;

    /* initialize timer */
    err = esp_timer_create(&timerArgs, &dirButtonISRParams);
    if (err != ESP_OK) THROW_ERR(err);

    /* initialize direction button ISR */
    err = gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_set_intr_type(T_SW_PIN, GPIO_INTR_ANYEDGE);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_isr_handler_add(T_SW_PIN, dirButtonISR, &dirButtonISRParams);
    if (err != ESP_OK) THROW_ERR(err);

    return ESP_OK;
}

static esp_err_t initIOButton(TaskHandle_t otaTask)
{
    esp_err_t err;

    err = gpio_set_pull_mode(IO_SW_PIN, GPIO_PULLUP_ONLY);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_pullup_en(IO_SW_PIN);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_set_direction(IO_SW_PIN, GPIO_MODE_INPUT);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_set_intr_type(IO_SW_PIN, GPIO_INTR_NEGEDGE);
    if (err != ESP_OK) THROW_ERR(err);
    err = gpio_isr_handler_add(IO_SW_PIN, otaButtonISR, otaTask);
    if (err != ESP_OK) THROW_ERR(err);

    return ESP_OK;
}

/**
 * @brief Defines what should occur when a short direction button
 * press is detected.
 * 
 * @note This is called within an ISR context, thus should be short
 * and non-blocking.
 */
static void shortDirButtonPress(TaskHandle_t mainTask, bool *toggle)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    *toggle = true;
    vTaskNotifyGiveFromISR(mainTask, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Defines what should occur when a long direction button
 * press is detected.
 */
static void longDirButtonPress(TaskHandle_t mainTask, bool *toggle)
{

}

/**
 * @brief Interrupt service routine that handles OTA button presses.
 * 
 * Handles OTA button presses to tell the main task to trigger an over-the-air
 * firmware upgrade.
 * 
 * @param params A TaskHandle_t that is the handle of the main task.
 */
static void otaButtonISR(void *params) {
    static TickType_t lastISRTick = 0;
    TaskHandle_t otaTask = (TaskHandle_t) params;
    TickType_t currentTick = xTaskGetTickCountFromISR();

    /* debounce button */
    if (currentTick - lastISRTick < pdMS_TO_TICKS(CONFIG_DEBOUNCE_PERIOD)) return;
    lastISRTick = currentTick;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(otaTask, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Interrupt service routine that handles direction button presses.
 * 
 * @note The pin is active high, meaning a falling edge is expected before
 * a rising edge.
 */
static void dirButtonISR(void *params)
{
    static TickType_t lastISRTick = 0;

    DirButtonISRParams *dirButtonISRParams = (DirButtonISRParams *) params;

    TickType_t currentTick = xTaskGetTickCountFromISR();
    bool rising = (gpio_get_level(T_SW_PIN) == 1) ? true : false;

    /* debounce button */
    if (currentTick - lastISRTick < pdMS_TO_TICKS(CONFIG_DEBOUNCE_PERIOD)) return;
    lastISRTick = currentTick;

    if (rising)
    { // rising edge
        if (*(dirButtonISRParams->timerExpired)) return; // long button press, already handled
        /* stop timer */
        (void) esp_timer_stop(dirButtonTimer); // just need it to stop
        shortDirButtonPress(dirButtonISRParams->mainTask, dirButtonISRParams->toggle); // yields from ISR if higher priority task woken
    } else
    { // falling edge
        /* set timer */
        if (dirButtonTimer != NULL) return; // can't report the error
        (void) esp_timer_start_once(dirButtonTimer, TIMEOUT); // nowhere for error to go
        *(dirButtonISRParams->timerExpired) = false;
    }
}

static void timerDirButtonCallback(void *params)
{
    DirButtonISRParams *dirButtonISRParams = (DirButtonISRParams *) params;

    *(dirButtonISRParams->timerExpired) = true;
    longDirButtonPress(dirButtonISRParams->mainTask, dirButtonISRParams->toggle);
}
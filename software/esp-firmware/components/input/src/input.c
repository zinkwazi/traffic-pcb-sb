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

#include "app_err.h"
#include "pinout.h"
#include "refresh.h"

#define TAG "input"

#define DIR_BUTTON_LONG_PRESS_TIME_US 500000

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
};

typedef struct DirButtonISRParams DirButtonISRParams;

static esp_timer_handle_t dirButtonTimer = NULL;
static bool dirButtonShortEnable = false;
static bool dirButtonLongEnable = false;

static void shortDirButtonPress(TaskHandle_t mainTask, bool *toggle);
static void longDirButtonPress(TaskHandle_t mainTask, bool *toggle);
static esp_err_t initDirectionButton(TaskHandle_t mainTask, bool *toggle);
static esp_err_t initOTAButton(TaskHandle_t otaTask);
static void otaButtonISR(void *params);
static void dirButtonISR(void *params);
static void timerDirButtonCallback(void *params);

/**
 * Initializes all input buttons on the board.
 * 
 * @param otaTask A handle to the OTA task, which will be used
 * to send it task notifications.
 * @param mainTask A handle to the main task, which will be used
 * to send it task notifications.
 * @param toggle A pointer to a boolean which the main task uses
 * to determine whether to switch the direction of traffic when
 * a task notification is received.
 */
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
    const esp_timer_create_args_t timerArgs = {
        .name = "dirButtonTimer",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = timerDirButtonCallback,
        .arg = &dirButtonISRParams,
    };
    esp_err_t err;

    /* input guards */
    if (mainTask == NULL || toggle == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);

    /* initialize params */
    dirButtonISRParams.mainTask = mainTask;
    dirButtonISRParams.toggle = toggle;

    /* initialize timer */
    err = esp_timer_create(&timerArgs, &dirButtonTimer); // lives forever
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

static esp_err_t initOTAButton(TaskHandle_t otaTask)
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
 * 
 * @param mainTask A handle to the main task, which will receive
 * a task notification from this function.
 * @param toggle A pointer to the main task's 'toggle' variable,
 * which is modified when the task notification is sent in order
 * to let the main task know if it should switch direction
 * of traffic.
 */
static void shortDirButtonPress(TaskHandle_t mainTask, bool *toggle)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // ESP_LOGI(TAG, "short button press");
    ESP_EARLY_LOGI(TAG, "1");
    if (!dirButtonShortEnable) return;

    /* send a notification to the main task telling it to
    switch the direction of traffic. */
    *toggle = true;
    ESP_EARLY_LOGI(TAG, "here");
    vTaskNotifyGiveFromISR(mainTask, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Defines what should occur when a long direction button
 * press is detected.
 * 
 * @note This is called from the direction task timer.
 * 
 * @param mainTask A handle to the main task, which will receive
 * a task notification from this function.
 * @param toggle A pointer to the main task's 'toggle' variable,
 * which is modified when the task notification is sent in order
 * to let the main task know if it should switch direction
 * of traffic.
 */
static void longDirButtonPress(TaskHandle_t mainTask, bool *toggle)
{
    if (!dirButtonLongEnable) return;

    // ESP_LOGI(TAG, "long button press");

    // TODO: check if it is nighttime and indicate nighttime mode is activated
    lockBoardRefresh();

    /* force a refresh of the board, which will be blank because the
    board is locked. */
    *toggle = false;
    xTaskNotifyGive(mainTask);
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
    bool rising = (gpio_get_level(T_SW_PIN) == 0) ? true : false;

    /* debounce button */
    if (currentTick - lastISRTick < pdMS_TO_TICKS(100)) return;
    lastISRTick = currentTick; // invert this so that reading the gpio level doesn't happen in bounce period

    if (rising)
    { // rising edge
        ESP_EARLY_LOGI(TAG, "rising");
         /* set timer */
        (void) esp_timer_start_once(dirButtonTimer, DIR_BUTTON_LONG_PRESS_TIME_US); // nowhere for error to go
        // shortDirButtonPress(dirButtonISRParams->mainTask, dirButtonISRParams->toggle); // yields from ISR if higher priority task woken
    } else
    { // falling edge
        ESP_EARLY_LOGI(TAG, "falling");
        /* stop timer */
        (void) esp_timer_stop(dirButtonTimer); // just need it to stop
    }
}

/**
 * Callback for when the direction button timer expires. This
 * indicates that the button has been held long enough for the
 * input to be considered a long/hold button press.
 * 
 * @note If hold dir button presses are disabled, this is
 * interpreted as a short button press.
 * 
 * @param params A pointer to a DirButtonISRParams struct.
 */
static void timerDirButtonCallback(void *params)
{
    DirButtonISRParams *dirButtonISRParams = (DirButtonISRParams *) params;

    ESP_LOGI(TAG, "dirbuttonTimer");
    if (dirButtonLongEnable)
    {
        // longDirButtonPress(dirButtonISRParams->mainTask, dirButtonISRParams->toggle);
    } else
    {
        // shortDirButtonPress(dirButtonISRParams->mainTask, dirButtonISRParams->toggle);
    }
}
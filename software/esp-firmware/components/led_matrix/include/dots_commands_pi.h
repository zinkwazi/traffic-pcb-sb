

#ifndef DOTS_COMMANDS_PI_H_
#define DOTS_COMMANDS_PI_H_
#ifndef CONFIG_DISABLE_TESTING_FEATURES // this is inverted for the esp-idf vscode extension

/* Include the public interface */
#include "dots_commands.h"

/*******************************************/
/*            INTERNAL FUNCTIONS           */
/*******************************************/

esp_err_t addCommandToI2CQueue(QueueHandle_t queue, enum I2CCommandFunc func, void *params, TaskHandle_t notifyTask, bool blocking);

/*******************************************/
/*            TESTING FEATURES             */
/*******************************************/

esp_err_t dotsReleaseBus(QueueHandle_t queue, bool notify, bool blocking);
esp_err_t dotsReaquireBus(QueueHandle_t queue, bool notify, bool blocking);

#endif /* CONFIG_DISABLE_TESTING_FEATURES */
#endif /* DOTS_COMMANDS_PI_H_ */
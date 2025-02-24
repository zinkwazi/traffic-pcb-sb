/**
 * api_connect_pi.h
 * 
 * test_api_connect.c testing features and access to api_connect.c 
 * internal functions for unit testing.
 */

#ifndef API_CONNECT_PI_H_
#define API_CONNECT_PI_H_
#ifndef CONFIG_DISABLE_TESTING_FEATURES

/* Include the public interface */
#include "api_connect.h"

/*******************************************/
/*            INTERNAL FUNCTIONS           */
/*******************************************/

esp_err_t nextCSVEntryFromMark(LEDData *data, CircularBuffer *circBuf, char *buf, uint32_t bufSize);

/*******************************************/
/*            TESTING FEATURES             */
/*******************************************/

int getResponseBlockSize(void);
int getCircBufSize(void);

#endif /* CONFIG_DISABLE_TESTING_FEATURES */
#endif /* API_CONNECT_PI_H_ */
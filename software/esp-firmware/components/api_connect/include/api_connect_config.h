/**
 * api_connect_config.h
 * 
 * Configuration options for api_connect.c, placed in a separate file to allow
 * white-box unit tests to access their values.
 */

#ifndef API_CONNECT_CONFIG_H_
#define API_CONNECT_CONFIG_H_

#include "sdkconfig.h"

/* The size in chars of the block size api http responses will be received in,
   which will be added to a circular buffer of double the size. This allows
   blocks that do not exactly align with blocks to be handled.  */
#define RESPONSE_BLOCK_SIZE (128)
#define CIRC_BUF_SIZE (2 * RESPONSE_BLOCK_SIZE)

/* the maximum number of characters necessary to fully contain an addendum
    filename and folder. */
#define MAX_ADDENDUM_FILEPATH (128)
#define ADDENDUM_FOLDER_ENDING "_add"

/* Explicitly define addendum filename for every version */
#define ADDENDUM_ENDING ".add"

#if CONFIG_HARDWARE_VERSION == 1

#define USE_ADDENDUMS true
#define FIRST_ADDENDUM_FILENAME "V1_0_5"

#elif CONFIG_HARDWARE_VERSION == 2

#define USE_ADDENDUMS true
#define FIRST_ADDENDUM_FILENAME "V2_0_0"

#else
#error "Unsupported hardware version!"
#endif

#endif /* API_CONNECT_CONFIG_H_ */
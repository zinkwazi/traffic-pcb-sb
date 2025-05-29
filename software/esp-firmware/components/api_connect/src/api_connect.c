/**
 * api_connect.c
 * 
 * Contains functions for connecting to and retrieving data from the server.
 */

#include "api_connect.h"
#include "api_connect_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_err.h"
#include "esp_assert.h"
#include "wrap_esp_http_client.h"

#include "circular_buffer.h"
#include "main_types.h"

#define TAG "api_connect"

esp_err_t getNextResponseBlock(char *output, int *outputLen, esp_http_client_handle_t client);
esp_err_t readServerSpeedDataPreinit(CircularBuffer *circBuf, LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client);
esp_err_t nextCSVEntryFromMark(LEDData *data, CircularBuffer *circBuf, char *buf, uint32_t bufSize);

#if USE_ADDENDUMS == true
esp_err_t getServerSpeedsWithAddendums(LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client, char *fileURL, int retryNum);
esp_err_t parseMetadata(char **dataStart, char *block, int blockLen, char *metadata, int *metadataLen);
#else
esp_err_t getServerSpeedsNoAddendums(LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client, char *URL, int retryNum);
esp_err_t readServerSpeedData(LEDData ledSpeeds[], uint32_t ledSpeedsLen, esp_http_client_handle_t client);
#endif

/**
 * @brief Retrieves the current speeds from a CSV file located at URL, including
 *        any addendums that are present for the current hardware version.
 * 
 * @param[out] ledSpeeds An output array where retrieved data will be stored.
 *        Specifically, LED number 'x' will be stored at index x - 1. If an
 *        index already has an LED number that is not 0, the index will be
 *        skipped--this allows addendums to take priority over base data.
 * @param[in] ledSpeedsLen The size of ledSpeeds, which should be no less than
 *        the maximum LED number present on the device. LED numbers retrieved
 *        that are greater than this length will be ignored.
 * @param[in] client The http client to make the request through.
 * @param[in] URL A null-terminated string of the URL to make a request to.
 * @param[in] retryNum The number of times to retry connecting to the server.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 */
esp_err_t getServerSpeeds(LEDData ledSpeeds[], 
                          uint32_t ledSpeedsLen, 
                          esp_http_client_handle_t client, 
                          char *URL, 
                          int retryNum)
{
#if USE_ADDENDUMS == true
    return getServerSpeedsWithAddendums(ledSpeeds, ledSpeedsLen, client, URL, retryNum);
#else
    return getServerSpeedsNoAddendums(ledSpeeds, ledSpeedsLen, client, URL, retryNum);
#endif
}

/**
 * @brief Retrieves and preprocesses the next block of response data from client.
 * 
 * @note This function reserves the last two elements of the output for potential
 *       preprocessing, meaning the maximum length of output is outputLen - 2.
 * 
 * @note Preprocessing includes:
 *           1. appending a newline character to the end of the file.
 *           2. appending a null-terminator to the end of the block.
 * 
 * @note Requires client to be an open connection such that esp_http_client_read
 *       can be called on it.
 * 
 * @param[out] output The location to place the next block in.
 * @param[out] outputLen The size of the output buffer. After execution, it is
 *        updated to the length of the retrieved and preprocessed block. Must be
 *        at least 3 to include null-terminator and potential newline character.
 * @param[in] client The client to retrieve data from, using esp_http_client_read.
 * 
 * @returns ESP_OK if successful and outputLen is equal to the length of the
 *          preprocessed block.
 *          ESP_ERR_INVALID_ARG if invalid arguments.
 *          ESP_ERR_NOT_FOUND if esp_http_client_read returns 0.
 *          ESP_FAIL if failed to read HTTP response.
 */
esp_err_t getNextResponseBlock(char *output, 
                               int *outputLen, 
                               esp_http_client_handle_t client)
{
    int numBytesToRead;
    esp_err_t ret = ESP_OK;
    int numBytesRead;

    /* input guards */
    if (output == NULL) return ESP_ERR_INVALID_ARG;
    if (outputLen == NULL) return ESP_ERR_INVALID_ARG;
    if (*outputLen < 3) return ESP_ERR_INVALID_ARG;
    if (client == NULL) return ESP_ERR_INVALID_ARG;

    /* read block */
    numBytesToRead = ((int32_t) *outputLen) - 2; // preprocessing includes a null-terminator, 
                                                 // and potentially a newline character
    numBytesRead = ESP_HTTP_CLIENT_READ(client, output, numBytesToRead);
    if (numBytesRead < 0) return ESP_FAIL;
    if (numBytesRead == 0) {
        ret = ESP_ERR_NOT_FOUND;
    }

    /* preprocess block */
    if (numBytesRead < numBytesToRead)
    {
        /* this block includes EOF, add newline character */
        output[numBytesRead] = '\n';
        numBytesRead++;
    }
    output[numBytesRead] = '\0'; // ensure output is c-string
    *outputLen = numBytesRead;
    return ret;
}

/**
 * @brief Parses the next CSV entry from the mark in circBuf and remark the next
 *        newline character in the buffer. The end of a CSV entry is denoted by
 *        a newline character.
 * 
 * @note If the mark is on a '\n' in the circBuf, the character will be skipped.
 *       This prevents the caller from needing to remember to move the mark
 *       generated by this function, which marks the first found '\n'.
 * 
 * @note The end of a CSV file may not contain a newline character; in this
 *       case, a newline character should manually be appended.
 * 
 * @param[out] data The location to which retrieved data will be stored.
 * @param[in] circBuf The circular buffer to parse the next entry from.
 * @param[in] buf An array of length bufSize, used internally by the function.
 *        Assume that the data inside buf is scrambled after use.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid arguments.
 *          APP_ERR_UNINITIALIZED if circBuf is uninitialized.
 *          ESP_ERR_NOT_FOUND if no data was found.
 *          ESP_ERR_INVALID_RESPONSE if the server returned -1 speed.
 *          ESP_FAIL otherwise and the circular buffer mark is unmodified,
 *          however buf is potentially modified.
 */
esp_err_t nextCSVEntryFromMark(LEDData *data, 
                               CircularBuffer *circBuf, 
                               char *buf, 
                               uint32_t bufSize)
{
    esp_err_t err;
    int bufferLen = 0;
    int bytesRead = 0;
    int commaNdx = 0;
    int64_t entryLEDNum = -INT64_MAX;
    int64_t entrySpeed = -INT64_MAX;

    /* input guards */
    if (data == NULL) return ESP_ERR_INVALID_ARG;
    if (circBuf == NULL) return ESP_ERR_INVALID_ARG;
    if (buf == NULL) return ESP_ERR_INVALID_ARG;
    if (bufSize == 0) return ESP_ERR_INVALID_ARG;

    /* retrieve data from circular buffer */
    bufferLen = circularBufferReadFromMark(circBuf, buf, bufSize - 1);
    if (bufferLen < 0) return (esp_err_t) -bufferLen; // -bufferLen is an error 
                                                      // code if faiure
    /* parse CSV data from linear buffer */
    int i = 0;
    if (buf[i] == '\n')
    {
        i++; // the user of the func will prob forget to move the mark manually.
    }
    for (; i < bufferLen; i++)
    {
        if (buf[i] == '\0') return ESP_FAIL; // did not finish the entry

        /* parse single CSV row */
        if (buf[i] == ',' && entryLEDNum == -INT64_MAX)
        {
            commaNdx = i;
            entryLEDNum = strtol(&buf[0], NULL, 10);
            if (errno != 0)
            {
                ESP_LOGI(TAG, "strtol failure parsing CSV");
                return ESP_FAIL;
            }
        } else if (buf[i] == ',' && entryLEDNum != -INT64_MAX)
        {
            return ESP_FAIL;
        }
        if (buf[i] == '\n')
        {
            bytesRead = i; // don't include this newline character
            entrySpeed = strtol(&buf[commaNdx + 1], NULL, 10);
            if (errno != 0)
            {
                ESP_LOGI(TAG, "strtol failure parsing CSV");
                return ESP_FAIL;
            }
            break;
        }
    }
    if (entryLEDNum == -INT64_MAX) return ESP_ERR_NOT_FOUND;
    if (entrySpeed == -INT64_MAX) return ESP_ERR_NOT_FOUND;

    /* remark circular buffer and return data */
    err = circularBufferMark(circBuf, bytesRead, FROM_PREV_MARK);
    if (err != ESP_OK) return ESP_FAIL;

    data->ledNum = entryLEDNum;
    data->speed = entrySpeed;
    if (entrySpeed == -1) return ESP_ERR_INVALID_RESPONSE; // -1 indicates 'Error' LED type
    if (entrySpeed == -2) return ESP_OK; // -2 indicates 'Special' LED type

    return ESP_OK;
}

/**
 * @brief Parses CSV data from the response in client, using a preinitialized
 *        circular buffer with data already stored in it. The buffer must be
 *        marked at the beginning of CSV data.
 * 
 * @note This is useful when part of a file includes non-csv data, such as
 *       addendums, which contain metadata before csv rows.
 * 
 * @param[in] circBuf A circular buffer that has already been initialized,
 *        filled with some data, and marked at the beginning of CSV data. Must
 *        be initialized with a buffer of at least 2 * RESPONSE_BLOCK_SIZE.
 * @param[out] ledSpeeds An output array where retrieved data will be stored.
 *        Specifically, LED number 'x' will be stored at index x - 1. If an
 *        index already has an LED number that is not 0, the index will be
 *        skipped--this allows addendums to take priority over base data.
 * @param[in] ledSpeedsLen The size of ledSpeeds, which should be no less than
 *        the maximum LED number present on the device. LED numbers retrieved
 *        that are greater than this length will be ignored.
 * @param[in] client The http client the request was made through. This function
 *        repeatedly calls esp_http_client_read.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid arguments.
 *          ESP_FAIL otherwise.
 */
esp_err_t readServerSpeedDataPreinit(CircularBuffer *circBuf, 
                                     LEDData ledSpeeds[], 
                                     uint32_t ledSpeedsLen, 
                                     esp_http_client_handle_t client)
{
    esp_err_t err;
    char buffer[RESPONSE_BLOCK_SIZE];
    int len;
    LEDData result;

    /* input guards */
    if (circBuf == NULL) return ESP_ERR_INVALID_ARG;
    if (ledSpeeds == NULL) return ESP_ERR_INVALID_ARG;
    if (ledSpeedsLen == 0) return ESP_ERR_INVALID_ARG;
    if (client == NULL) return ESP_ERR_INVALID_ARG;

    /* parse CSV rows from circular buffer, then read new data into it */
    do
    {
        /* parse rows while available */
        do {
            err = nextCSVEntryFromMark(&result, circBuf, buffer, RESPONSE_BLOCK_SIZE);
            /* assert that ledNum is within indices */
            if (err == ESP_OK && result.ledNum > ledSpeedsLen)
            {
                ESP_LOGW(TAG, "found LED %u in file, which is out of bounds", result.ledNum);
                continue;
            }
            /* set data point if not present */
            if (err == ESP_OK && ledSpeeds[result.ledNum - 1].ledNum == 0)
            {
                ledSpeeds[result.ledNum - 1].ledNum = result.ledNum;
                ledSpeeds[result.ledNum - 1].speed = result.speed;
            }
            if (err != ESP_OK && err != ESP_ERR_NOT_FOUND && err != ESP_ERR_INVALID_RESPONSE) return err;
        } while (err != ESP_ERR_NOT_FOUND);

        /* read new data from response */
        len = RESPONSE_BLOCK_SIZE - 1;
        err = getNextResponseBlock(buffer, &len, client);
        if (err == ESP_ERR_NOT_FOUND) return ESP_OK; // only success exit case
        if (err != ESP_OK) return ESP_FAIL;

        err = circularBufferStore(circBuf, buffer, (uint32_t) len);
        if (err != ESP_OK) return ESP_FAIL;

    } while (len > 0);

    return ESP_FAIL; // should not exit here
}

/**
 * @brief Initiates an HTTPS request to the provided URL, leaving the client in
 *        a state to begin reading the response using esp_http_client_read.
 * 
 * @note Requires that a wifi connection is present and client has been
 *       initialized with esp_http_client_init.
 * 
 * @param[out] contentLength The location where the response content length will
 *        be placed if successful. Will be modified even if the function fails.
 * @param[in] client The HTTP client to make the request through, which should
 *        be configured before use.
 * @param[in] URL A null-terminated string of the URL to make a request to.
 * @param[in] retryNum The number of time to retry connecting to the URL.
 * 
 * @returns ESP_OK if successful, with contentLength equal to the contentLength
 *          returned by the HTTPS response and client open.
 *          ESP_ERR_INVALID_ARG if invalid arguments.
 *          ESP_ERR_NOT_FOUND if the content length was 0.
 *          ESP_FAIL if unable to close client or flush response.
 *          Other error codes if an unexpected error occurs.
 */
esp_err_t openServerFile(int64_t *contentLength, 
                         esp_http_client_handle_t client, 
                         const char *URL, 
                         int retryNum)
{
    int bytesFlushed;
    const int flushBufSize = 128;
    char buf[flushBufSize];
    esp_err_t err;
    /* input guards */
    if (contentLength == NULL) return ESP_ERR_INVALID_ARG;
    if (client == NULL) return ESP_ERR_INVALID_ARG;
    if (URL == NULL) return ESP_ERR_INVALID_ARG;
    if (retryNum <= 0) return ESP_ERR_INVALID_ARG;

    /* clear http buffer if not clear */
    do {
        bytesFlushed = ESP_HTTP_CLIENT_READ(client, buf, flushBufSize);
    } while (bytesFlushed != 0);

    /* establish connection and open URL */
    ESP_LOGI(TAG, "retrieving: %s", URL);
    while (retryNum != 0)
    {
        err = ESP_HTTP_CLIENT_SET_URL(client, URL);
        if (err != ESP_OK) return err; // should always be able to do this

        err = ESP_HTTP_CLIENT_OPEN(client, 0);
        if (err != ESP_OK) return err; // should always be able to do this

        *contentLength = ESP_HTTP_CLIENT_GET_CONTENT_LENGTH(client);
        while (*contentLength == -ESP_ERR_HTTP_EAGAIN)
        {
            *contentLength = ESP_HTTP_CLIENT_GET_CONTENT_LENGTH(client);
        }
        if (*contentLength <= 0)
        {
            ESP_LOGW(TAG, "contentLength <= 0");
            if (ESP_HTTP_CLIENT_CLOSE(client) != ESP_OK)
            {
                ESP_LOGE(TAG, "failed to close client");
                return ESP_FAIL;
            }
            retryNum--;
            return ESP_ERR_NOT_FOUND;
        }
        int status = ESP_HTTP_CLIENT_GET_STATUS_CODE(client);
        if (status != 200)
        {
            ESP_LOGE(TAG, "status code is %d", status);
            /* flush internal response buffer and close client */
            err = ESP_HTTP_CLIENT_FLUSH_RESPONSE(client, &bytesFlushed);
            ESP_LOGW(TAG, "flushed %d bytes", bytesFlushed);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "failed to flush response");
                return ESP_FAIL;
            }

            err = ESP_HTTP_CLIENT_CLOSE(client);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "failed to close client");
                return ESP_FAIL;
            }
            retryNum--;
            return ESP_ERR_NOT_FOUND;
        }
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND; // retried too many times
}

#if USE_ADDENDUMS == true

/**
 * @brief Retrieves the current speeds from a CSV file located at URL, while
 *        first retrieving and processing addendums.
 * 
 * @note The entrypoint addendum is defined by FIRST_ADDENDUM_FILENAME and is
 *       specified for each hardware version.
 * 
 * @param[out] ledSpeeds An output array where retrieved data will be stored.
 *        Specifically, LED number 'x' will be stored at index x - 1. If an
 *        index already has an LED number that is not 0, the index will be
 *        skipped--this allows addendums to take priority over base data.
 * @param[in] ledSpeedsLen The size of ledSpeeds, which should be no less than
 *        the maximum LED number present on the device. LED numbers retrieved
 *        that are greater than this length will be ignored.
 * @param[in] client The http client to make the request through.
 * @param[in] URL A null-terminated string of the URL to make a request to.
 * @param[in] retryNum The number of times to retry connecting to the server.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid arguments.
 *          Various errors if failure and client may not be closed.
 */
esp_err_t getServerSpeedsWithAddendums(LEDData ledSpeeds[], 
                                      uint32_t ledSpeedsLen, 
                                      esp_http_client_handle_t client, 
                                      char *fileURL, 
                                      int retryNum)
{
    const char *addFolderEnding = ADDENDUM_FOLDER_ENDING "/";
    const int META_SIZE = MAX_ADDENDUM_FILEPATH + 2; // "{filepath}"
    char block[RESPONSE_BLOCK_SIZE];
    char metadata[META_SIZE];
    char circBufBacking[CIRC_BUF_SIZE];
    esp_err_t err;
    CircularBuffer circBuf;
    int64_t contentLen;
    int blockLen;
    int metaLen;
    char *dataStart;
    int len;

    /* input guards */
    if (ledSpeeds == NULL) return ESP_ERR_INVALID_ARG;
    if (ledSpeedsLen == 0) return ESP_ERR_INVALID_ARG;
    if (client == NULL) return ESP_ERR_INVALID_ARG;
    if (fileURL == NULL) return ESP_ERR_INVALID_ARG;
    if (retryNum == 0) return ESP_ERR_INVALID_ARG;

    /* clear ledSpeeds */
    for (uint32_t i = 0; i < ledSpeedsLen; i++) 
    {
        ledSpeeds[i].ledNum = 0;
        ledSpeeds[i].speed = 0;
    }
    
    /* determine correct starting addendum file path */
    strncpy(metadata, fileURL, MAX_ADDENDUM_FILEPATH);
    metadata[MAX_ADDENDUM_FILEPATH] = '\0'; // ensure null-terminated
    strncat(metadata, addFolderEnding, MAX_ADDENDUM_FILEPATH - strlen(metadata));
    metadata[MAX_ADDENDUM_FILEPATH] = '\0';
    strncat(metadata, FIRST_ADDENDUM_FILENAME ADDENDUM_ENDING, MAX_ADDENDUM_FILEPATH - strlen(metadata));
    metadata[MAX_ADDENDUM_FILEPATH] = '\0';

    /* retrieve and process addendum chain */
    metaLen = strlen(metadata);
    while (metaLen != 0)
    {
        /* there is another addendum/regular file to retrieve */
        /* open file and retrieve next addendum filename from metadata */
        err = openServerFile(&contentLen, client, metadata, retryNum);
        if (err != ESP_OK || contentLen < 0)
        {
            ESP_LOGW(TAG, "failed to retrieve %s", metadata);
            // client is closed by openServerFile if unsuccessful
            return ESP_FAIL;
        }

        blockLen = RESPONSE_BLOCK_SIZE;
        err = getNextResponseBlock(block, &blockLen, client);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "getNextResponseBlock failed: err: %d", err);
            return err;   
        }

        err = parseMetadata(&dataStart, block, blockLen, metadata, &metaLen);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "parseMetadata failed: err: %d", err);
            return err;   
        }

        /* load circular buffer with the rest of the data */
        err = circularBufferInit(&circBuf, circBufBacking, CIRC_BUF_SIZE);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "circularBufferInit failed: err: %d", err);
            return err;
        }

        len = blockLen - (dataStart - block);
        if (len > 0)
        {
            err = circularBufferStore(&circBuf, dataStart, len);
        } else
        {
            /* nothing leftover, load a '\n' into buffer */
            err = circularBufferStore(&circBuf, "\n", 1);
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "circularBufferStore failed: err: %d", err);
            return err;
        }

        err = circularBufferMark(&circBuf, 0, FROM_OLDEST_CHAR);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "circularBufferMark failed: err: %d", err);
            return err;
        }
        
        /* parse CSV data from preinitialized circular buffer */
        err = readServerSpeedDataPreinit(&circBuf, ledSpeeds, ledSpeedsLen, client);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "readServerSpeedDataPreinit failed: err: %d", err);
            return err;
        }

        /* close connection to allow next connection to work properly */
        if (ESP_HTTP_CLIENT_CLOSE(client) != ESP_OK)
        {
            ESP_LOGE(TAG, "failed to close client");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

/**
 * @brief Parses the addendum metadata from the block.
 * 
 * @param[out] dataStart The location to place a pointer to the beginning of
 *        the file's actual data, past the metadata, within the block. This is 
 *        changed to the beginning of the block if there is no metadata.
 * @param[in] block The location of the block of data to parse metadata from.
 * @param[in] blockLen The length of the block, not including a null-terminator.
 * @param[out] metadata The buffer to write the contents of the metadata to.
 *        Metadata will be null-terminated. If no metadata is found, then
 *        an empty, null-terminated string is written here.
 * @param[in] metadataLen The size of the metadata buffer. Thus, the maximum
 *        length of metadata that can be parsed is metadataLen - 1. When the
 *        function is finished, this is changed to the length of the metadata.
 * 
 * @returns ESP_OK if successful, as in the block was parsed. Metadata may or 
 *          may not have been found. 
 *          ESP_ERR_INVALID_ARG if invalid arguments.
 *          ESP_FAIL otherwise.
 */
esp_err_t parseMetadata(char **dataStart,
                        char *block, 
                        int blockLen, 
                        char *metadata, 
                        int *metadataLen)
{
    char *prev, *curr;

    /* input guards */
    if (dataStart == NULL) return ESP_ERR_INVALID_ARG;
    if (block == NULL) return ESP_ERR_INVALID_ARG;
    if (blockLen == 0) return ESP_ERR_INVALID_ARG;
    if (metadata == NULL) return ESP_ERR_INVALID_ARG;
    if (metadataLen == NULL) return ESP_ERR_INVALID_ARG;
    if (*metadataLen == 0) return ESP_ERR_INVALID_ARG;

    /* start reading data until '{' */
    curr = block;
    while (block + blockLen > curr && *curr != '{')
    {
        curr++;
    }
    if (block + blockLen == curr)
    {
        /* no metadata was found */
        *metadataLen = 0;
        metadata[0] = '\0';
        *dataStart = block;
        return ESP_OK;
    }

    /* continue reading until '}' */
    prev = curr;
    while (block + blockLen > curr && *curr != '}')
    {
        curr++;
    }
    if (block + blockLen == curr)
    {
        /* metadata is malformed, missing '}' */
        *metadataLen = 0;
        metadata[0] = '\0';
        *dataStart = block;
        return ESP_OK;
    }

    /* copy contents of metadata if it can fit */
    if (curr - prev - 1 >= *metadataLen - 1)
    {
        /* metadata is too long */
        ESP_LOGW(TAG, "metadata was longer than buffer");
        *dataStart = block;
        return ESP_FAIL;
    }
    prev++; // move off '{'
    *metadataLen = 0;
    while (prev != curr)
    {
        metadata[*metadataLen] = *prev;
        (*metadataLen)++;
        prev++;
    }
    metadata[*metadataLen] = '\0';

    /* find start of data, ignoring any whitespace */
    if (block + blockLen > curr)
    {
        curr++; // get off '}'
    }
    while ((block + blockLen > curr) && 
               (*curr == ' ' || *curr == '\n' || *curr == '\r'))
    {
        curr++;
    }
    *dataStart = curr;
    return ESP_OK;
}

#else /* of USE_ADDENDUMS == true */

/**
 * @brief Retrieves the current speeds from a CSV file located at URL.
 * 
 * @param[out] ledSpeeds An output array where retrieved data will be stored.
 *        Specifically, LED number 'x' will be stored at index x - 1. If an
 *        index already has an LED number that is not 0, the index will be
 *        skipped--this allows addendums to take priority over base data.
 * @param[in] ledSpeedsLen The size of ledSpeeds, which should be no less than
 *        the maximum LED number present on the device. LED numbers retrieved
 *        that are greater than this length will be ignored.
 * @param[in] client The http client to make the request through.
 * @param[in] URL A null-terminated string of the URL to make a request to.
 * @param[in] retryNum The number of times to retry connecting to the server.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid arguments.
 *          ESP_FAIL otherwise.
 */
esp_err_t getServerSpeedsNoAddendums(LEDData ledSpeeds[],
                                     uint32_t ledSpeedsLen,
                                     esp_http_client_handle_t client,
                                     char *URL,
                                     int retryNum)
{
    int64_t contentLength;
    esp_err_t err;

    /* input guards */
    ESP_STATIC_ASSERT(CIRC_BUF_SIZE >= (2 * RESPONSE_BLOCK_SIZE));
    if (ledSpeeds == NULL) return ESP_ERR_INVALID_ARG;
    if (ledSpeedsLen == 0) return ESP_ERR_INVALID_ARG;
    if (client == NULL) return ESP_ERR_INVALID_ARG;
    if (URL == NULL) return ESP_ERR_INVALID_ARG;
    if (retryNum == 0) return ESP_ERR_INVALID_ARG;

    /* open connection and retrieve headers */
    err = openServerFile(&contentLength, client, URL, retryNum);
    if (err != ESP_OK) return err; // openServerFile closes client if it fails

    err = readServerSpeedData(ledSpeeds, ledSpeedsLen, client);

    if (wrap_http_client_close(client) != ESP_OK) 
    {
        ESP_LOGE(TAG, "failed to close client");
        return ESP_FAIL;
    }
    return err;
}

/**
 * @brief es CSV data from the response in client.
 * 
 * @param[out] ledSpeeds An output array where retrieved data will be stored.
 *        Specifically, LED number 'x' will be stored at index x - 1. If an
 *        index already has an LED number that is not 0, the index will be
 *        skipped--this allows addendums to take priority over base data.
 * @param[in] ledSpeedsLen The size of ledSpeeds, which should be no less than
 *        the maximum LED number present on the device. LED numbers retrieved
 *        that are greater than this length will be ignored.
 * @param[in] client The http client the request was made through. This function
 *        repeatedly calls esp_http_client_read.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid arguments.
 *          ESP_FAIL otherwise.
 */
esp_err_t readServerSpeedData(LEDData ledSpeeds[],
                              uint32_t ledSpeedsLen,
                              esp_http_client_handle_t client)
{
    CircularBuffer circBuf;
    circ_err_t circ_err;
    esp_err_t err;
    char circBufBacking[CIRC_BUF_SIZE];
    char buffer[RESPONSE_BLOCK_SIZE];
    int len;

    /* input guards */
    if (ledSpeeds == NULL) return ESP_ERR_INVALID_ARG;
    if (ledSpeedsLen == 0) return ESP_ERR_INVALID_ARG;
    if (client == NULL) return ESP_ERR_INVALID_ARG;

    /* initialize circular buffer */
    circ_err = circularBufferInit(&circBuf, circBufBacking, CIRC_BUF_SIZE);
    if (circ_err != CIRC_OK) return ESP_FAIL;

    /* load initial data and mark beginning */
    len = RESPONSE_BLOCK_SIZE;
    err = getNextResponseBlock(buffer, &len, client);
    if (err != ESP_OK) return err;
    if (len <= 0) return ESP_FAIL; // expect to retrieve something

    circ_err = circularBufferStore(&circBuf, buffer, len);
    if (circ_err != CIRC_OK) return (esp_err_t) circ_err;

    circ_err = circularBufferMark(&circBuf, 0, FROM_OLDEST_CHAR);
    if (circ_err != CIRC_OK) return (esp_err_t) circ_err;

    return readServerSpeedDataPreinit(&circBuf, ledSpeeds, ledSpeedsLen, client);
}

#endif /* USE_ADDENDUMS == true */

#ifndef DISABLE_TESTING_FEATURES

int getResponseBlockSize(void)
{
    return RESPONSE_BLOCK_SIZE;
}

int getCircBufSize(void)
{
    return CIRC_BUF_SIZE;
}

#endif /* DISABLE_TESTING_FEATURES */
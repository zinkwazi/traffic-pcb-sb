/**
 * api_connect.c
 * 
 * Contains functions for connecting to and retrieving data from the server.
 */

#include "api_connect.h"

#include "circular_buffer.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_err.h"
#include "esp_assert.h"

#include <stdbool.h>
#include <stdint.h>

/* The size in chars of the block size api http responses will be received in,
   which will be added to a circular buffer of double the size. This allows
   blocks that do not exactly align with blocks to be handled.  */
#define RESPONSE_BLOCK_SIZE (128)
#define CIRC_BUF_SIZE (2 * RESPONSE_BLOCK_SIZE)

#define TAG "api_connect"

/**
 * Retrieves the current speeds from the server.
 * 
 * Parameters:
 *     speeds: Where the current speed data will be stored
 */
esp_err_t tomtomGetServerSpeeds(uint8_t ledSpeeds[], int ledSpeedsLen, esp_http_client_handle_t client, char *URL, int retryNum) {
    CircularBuffer circBuf;
    char circBufBacking[RESPONSE_BLOCK_SIZE * 2];
    char responseBuf[RESPONSE_BLOCK_SIZE];
    /* input guards */
    ESP_STATIC_ASSERT(CIRC_BUF_SIZE >= (2 * RESPONSE_BLOCK_SIZE));
    if (ledSpeeds == NULL ||
        ledSpeedsLen == 0 ||
        client == NULL ||
        URL == NULL ||
        retryNum == 0)
    {
        return ESP_FAIL;
    }
    /* open connection and retrieve headers */
    ESP_LOGI(TAG, "retrieving: %s", URL);
    if (esp_http_client_set_url(client, URL) != ESP_OK) {
        return ESP_FAIL;
    }
    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "failed to open connection");
        return ESP_FAIL;
    }
    int64_t contentLength = esp_http_client_fetch_headers(client);
    while (contentLength == -ESP_ERR_HTTP_EAGAIN) {
        contentLength = esp_http_client_fetch_headers(client);
    }
    if (contentLength <= 0) {
        ESP_LOGW(TAG, "contentLength <= 0");
        if (esp_http_client_close(client) != ESP_OK) {
            ESP_LOGE(TAG, "failed to close client");
        }
        return ESP_FAIL;
    }
    int status = esp_http_client_get_status_code(client);
    if (esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(TAG, "status code is %d", status);
        if (esp_http_client_close(client) != ESP_OK) {
            ESP_LOGE(TAG, "failed to close client");
        }
        return ESP_FAIL;
    }
    /* initialize circular buffer */
    (void) circularBufferInit(&circBuf, circBufBacking, CIRC_BUF_SIZE);
    /* read and process data blocks from https stream */
    int64_t currResponsePos = 0;
    int64_t currSpeedsPos = 0;
    while (currResponsePos < contentLength && currSpeedsPos < ledSpeedsLen) {
        /* read block and store in circular buffer */
        int64_t bytesRead = esp_http_client_read(client, responseBuf, RESPONSE_BLOCK_SIZE - 1);
        if (bytesRead == -1) {
            ESP_LOGE(TAG, "esp_http_client_read returned -1");
            return ESP_FAIL;
        } else if (bytesRead == -ESP_ERR_HTTP_EAGAIN) {
            continue; // data was not ready
        }
        currResponsePos += bytesRead;
        circ_err_t circ_err = circularBufferStore(&circBuf, responseBuf, bytesRead);
        if (circ_err != CIRC_OK) {
            return ESP_FAIL;
        }
        /* move previous mark or add mark at ndx 0 */
        circ_err = circularBufferMark(&circBuf, 1, FROM_PREV_MARK); // moves to start of new data
        if (circ_err == CIRC_LOST_MARK) {
            circ_err = circularBufferMark(&circBuf, 0, FROM_OLDEST_CHAR);
        }
        if (circ_err != CIRC_OK) {
            return ESP_FAIL;
        }
        /* process block starting from recent mark */
        bytesRead = circularBufferReadFromMark(&circBuf, responseBuf, RESPONSE_BLOCK_SIZE - 1); // reuse responsebuf & bytesRead
        if (bytesRead < 0) {
            return ESP_FAIL;
        }
        for (int64_t i = 0; i < bytesRead && currSpeedsPos + i < ledSpeedsLen; i++) {
            ledSpeeds[currSpeedsPos + i] = (uint8_t) responseBuf[i];
        }
        currSpeedsPos += bytesRead;
        /* mark end position in circular buffer */
        circ_err = circularBufferMark(&circBuf, 0, FROM_RECENT_CHAR);
        if (circ_err != CIRC_OK) {
            return ESP_FAIL;
        }
    }
    if (esp_http_client_close(client) != ESP_OK) {
        ESP_LOGE(TAG, "failed to close client");
        return ESP_FAIL;
    }
    return ESP_OK;
}
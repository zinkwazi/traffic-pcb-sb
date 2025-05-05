/**
 * http_wrap.c
 * 
 * Contains functions that wrap esp_http_client functions to allow mocking without
 * mocking the entire esp_http_client component separately.
 */

#include "http_wrap.h"

#include "esp_http_client.h"

/**
 * @brief Helps circularBufferStoreFromClient, particularly to reduce boilerplate
 * and allow quick 'mocking' of esp_http_client_read when testing.
 * 
 * @note This function does not check inputs and may cause a panic if requirements
 * are not respected!
 * 
 * @param[in] client The client to read data from. Must be initialized and
 * opened such that esp_http_client_read can be used on it.
 * @param[in] buffer The buffer to read data into, whose length must be greater
 * than or equal to len.
 * @param[in] len The maximum amount of data to read from client into buffer.
 * 
 * @returns The number of bytes read if successful.
 * ESP_FAIL otherwise.
 */
int readHttpClientWrapper(esp_http_client_handle_t client, char *buffer, int len)
{
    int bytesRead;
    do {
        bytesRead = esp_http_client_read(client, buffer, len);
    } while (bytesRead == -ESP_ERR_HTTP_EAGAIN);
    return bytesRead;
}
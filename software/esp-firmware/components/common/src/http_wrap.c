/**
 * http_wrap.c
 * 
 * Contains functions that wrap esp_http_client functions to allow mocking without
 * mocking the entire esp_http_client component separately.
 */

#include "http_wrap.h"

#include "esp_http_client.h"

/**
 * @brief Allows mocking of esp_http_client_init when testing.
 */
esp_http_client_handle_t wrap_http_client_init(const esp_http_client_config_t *config)
{
    return esp_http_client_init(config);
}

/**
 * @brief Allows mocking of esp_http_client_read when testing.
 * 
 * @note This function behaves exactly as esp_http_client_open behaves.
 */
esp_err_t wrap_http_client_open(esp_http_client_handle_t client, int write_len)
{
    return esp_http_client_open(client, write_len);
}

/**
 * @brief Helps reduce boilerplate and allows mocking of esp_http_client_read 
 * when testing.
 * 
 * @note This function does not check inputs and may cause a panic if
 * requirements are not respected!
 * 
 * @note This function retries forever if a timeout occurs.
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
int wrap_http_client_read(esp_http_client_handle_t client, char *buffer, int len)
{
    int bytesRead;
    do {
        bytesRead = esp_http_client_read(client, buffer, len);
    } while (bytesRead == -ESP_ERR_HTTP_EAGAIN);
    return bytesRead;
}

/**
 * @brief Calls esp_http_client_set_url and allows mocking of the function.
 */
esp_err_t wrap_http_client_set_url(esp_http_client_handle_t client, const char *url)
{
    return esp_http_client_set_url(client, url);
}

/**
 * @brief Calls esp_http_client_fetch_headers and allows mocking of the function.
 */
int64_t wrap_http_client_fetch_headers(esp_http_client_handle_t client)
{
    return esp_http_client_fetch_headers(client);
}

/**
 * @brief Calls esp_http_client_close and allows mocking of the function.
 */
esp_err_t wrap_http_client_close(esp_http_client_handle_t client)
{
    return esp_http_client_close(client);
}

/**
 * @brief Calls esp_http_client_get_status_code and allows mocking of the function.
 */
int wrap_http_client_get_status_code(esp_http_client_handle_t client)
{
    return esp_http_client_get_status_code(client);
}

/**
 * @brief Calls esp_http_client_flush_response and allows mocking of the function.
 */
esp_err_t wrap_http_client_flush_response(esp_http_client_handle_t client, int *len)
{
    return esp_http_client_flush_response(client, len);
}

/**
 * @brief Allows mocking of esp_http_client_cleanup.
 */
esp_err_t wrap_http_client_cleanup(esp_http_client_handle_t client)
{
    return esp_http_client_cleanup(client);
}


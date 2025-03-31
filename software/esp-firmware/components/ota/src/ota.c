/**
 * ota.c
 * 
 * Contains over-the-air update functionality, handled through an OTA task.
 */

#include "ota.h"

#include <stdbool.h>
#include <stddef.h>

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "app_errors.h"
#include "circular_buffer.h"
#include "indicators.h"
#include "utilities.h"
#include "wifi.h"

#include "ota_config.h"

#define TAG "tasks"

enum VersionType {
    HARDWARE = 1,
    REVISION = 2,
    MAJOR = 3,
    MINOR = 4,
    PATCH = 5,
    VER_TYPE_UNKNOWN = 6, // end of enum bounds
};

typedef enum VersionType VersionType;

void vOTATask(void* pvParameters);
esp_err_t versionFromKey(VersionType *verType, char *str, int strLen);
bool compareVersions(uint hardVer, uint revVer, uint majorVer, uint minorVer, uint patchVer);
esp_err_t processOTAAvailableFile(bool *available, esp_http_client_handle_t client);
esp_err_t queryOTAUpdateAvailable(bool *available);

/**
 * @brief Initializes the over-the-air (OTA) task, which is implemented by
 *        vOTATask.
 * 
 * @note This function creates shallow copies of parameters that will be 
 *       provided to the task in static memory. It assumes that only one of 
 *       this type of task will be created; any additional tasks will have 
 *       pointers to the same location in static memory.
 * 
 * @param handle A pointer to a handle which will refer to the created task
 *               if successful.
 * @param errorResources A pointer to an ErrorResources object. A deep copy
 *                       of the object will be created in static memory.
 *                       
 * @returns ESP_OK if the task was created successfully, otherwise ESP_FAIL.
 */
esp_err_t createOTATask(TaskHandle_t *handle, const ErrorResources *errorResources) {
    static ErrorResources taskErrorResources;
    BaseType_t success;
    /* input guards */
    if (errorResources == NULL) return ESP_FAIL;
    if (errorResources->errMutex == NULL) return ESP_FAIL;
    /* copy parameters */
    taskErrorResources.err = errorResources->err;
    taskErrorResources.errMutex = errorResources->errMutex;
    taskErrorResources.errTimer = errorResources->errTimer;
    /* create OTA task */
    success = xTaskCreate(vOTATask, "OTATask", CONFIG_OTA_STACK,
                          &taskErrorResources, CONFIG_OTA_PRIO, handle);
    return (success == pdPASS) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Implements the over-the-air (OTA) task, which is responsible for
 *        handling user requests to update to the latest version of firmware.
 * 
 * @note To avoid runtime errors, the OTA task should only be created by the
 *       createOTATask function.
 * 
 * @param pvParameters A pointer to an ErrorResources object which should
 *                     remain valid through the lifetime of the task.
 */
void vOTATask(void* pvParameters) {
    const esp_http_client_config_t https_config = {
        .url = FIRMWARE_UPGRADE_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    ErrorResources *errRes = (ErrorResources *) pvParameters;
    esp_err_t err;
    bool updateAvailable;

    /* query most recent server firmware version and indicate if an update is available */
#if CONFIG_HARDWARE_VERSION == 1
        /* feature unsupported */
#elif CONFIG_HARDWARE_VERSION == 2
    (void) queryOTAUpdateAvailable(&updateAvailable); // allow firmware updates even if this
                                                      // function fails in order to fix 
                                                      // potential issues in this function
#else
#error "Unsupported hardware version!"
#endif

    /* wait for Update/IO0 button press and execute OTA update */
    while (true) {
        if (ulTaskNotifyTake(pdTRUE, INT_MAX) == 0) 
        {
            continue; // block on notification timed out
        }
        // received a task notification indicating update firmware
        ESP_LOGI(TAG, "OTA update in progress...");
        (void) indicateOTAUpdate(); // allow update away from bad firmware
        
        esp_https_ota_config_t ota_config = {
            .http_config = &https_config,
        };
        err = esp_https_ota(&ota_config);
        if (err == ESP_OK) 
        {
            ESP_LOGI(TAG, "completed OTA update successfully!");
            (void) indicateOTASuccess(CONFIG_OTA_LEFT_ON_MS); // restart imminent anyway
            unregisterWifiHandler();
            esp_restart();
        }
        
        ESP_LOGI(TAG, "did not complete OTA update successfully!");
        err = indicateOTAFailure(errRes, CONFIG_OTA_LEFT_ON_MS);
        FATAL_IF_ERR(err, errRes);
        if (err != ESP_OK)
        {
            throwFatalError(errRes, false);
        }
    }
}

/**
 * @brief Determines the version type the JSON key corresponds to.
 * 
 * @param[out] verType The location to place the version type of str.
 * @param[in] str A buffer containing the JSON key, with '\"' marks. Modified.
 * @param[in] strLen The length of str.
 */
esp_err_t versionFromKey(VersionType *verType, 
                         char str[], 
                         int strLen)
{
    esp_err_t err;
    int currNdx = 0;
    int keyStartNdx = 0; // the beginning of the key, inclusive
    int keyEndNdx = 0; // the end of the key, exclusive

    /* input guards */
    if (verType == NULL) return ESP_ERR_INVALID_ARG;
    if (str == NULL) return ESP_ERR_INVALID_ARG;
    if (strLen == 0) return ESP_ERR_INVALID_ARG;

    /* find beginning of key */
    while (currNdx < strLen)
    {
        if (str[currNdx] == '\"')
        {
            keyStartNdx = currNdx + 1;
            currNdx++;
            break;
        }
        currNdx++;
    }

    /* find end of key */
    while (currNdx < strLen)
    {
        if (str[currNdx] == '\"')
        {
            keyEndNdx = currNdx - 1;
            break;
        }
        currNdx++;
    }

    if (keyEndNdx == 0) {
        /* impossible regularly, JSON is malformed */
        return ESP_ERR_NOT_FOUND;
    }

    str[keyEndNdx] = '\0'; // create c-string from key

    /* parse and match string */
    *verType = VER_TYPE_UNKNOWN;
    if (0 == strcmp(&str[keyStartNdx], HARDWARE_VERSION_KEY)) *verType = HARDWARE;
    if (0 == strcmp(&str[keyStartNdx], HARDWARE_REVISION_KEY)) *verType = REVISION;
    if (0 == strcmp(&str[keyStartNdx], HARDWARE_REVISION_KEY)) *verType = MAJOR;
    if (0 == strcmp(&str[keyStartNdx], HARDWARE_REVISION_KEY)) *verType = MINOR;
    if (0 == strcmp(&str[keyStartNdx], HARDWARE_REVISION_KEY)) *verType = PATCH;
    return ESP_OK;
}

/**
 * @brief Compares the provided firmware version with that of the currently 
 *        installed image.

 * @param[in] hardVer The hardware version of the image being compared.
 * @param[in] revVer The hardware revision version of the image being compared.
 * @param[in] majorVer The major firmware version of the image being compared.
 * @param[in] minorVer The minor firmware version of the image being compared.
 * @param[in] patchVer The patch firmware version of the image being compared.
 * 
 * @returns True if the image being compared is newer than the currently
 *          installed image.
 */
bool compareVersions(uint hardVer, 
                     uint revVer, 
                     uint majorVer, 
                     uint minorVer, 
                     uint patchVer)
{
    /* compare hadware version */
    if (hardVer != CONFIG_HARDWARE_VERSION) return false;
    if (revVer != CONFIG_HARDWARE_REVISION) return false;
    /* compare firmware version */
    if (majorVer > CONFIG_FIRMWARE_MAJOR_VERSION) return true;
    if (minorVer > CONFIG_FIRMWARE_MINOR_VERSION) return true;
    if (patchVer > CONFIG_FIRMWARE_PATCH_VERSION) return true;
    return false;
}

/**
 * @brief Parses the contents of buf for the latest available firmware version
 *        and compares that version to the current version installed on the device.
 * 
 * @note Expects buf contents to be formatted as a JSON file with all keys ignored
 *       except for the following, whose values are parsed as integers:
 *       hardware_version, hardware_revision, firmware_major_version,
 *       firmware_minor_version, firmware_patch_version. This function ignores
 *       brackets in the file.
 * 
 * @note The maximum distance between any '{' or '}' to any other '{' or '}'
 *       in the file must be less than OTA_RECV_BUF_SIZE due to limitations
 *       of the processing buffer.
 * 
 * @note Special characters that cannot be included in 
 * 
 * @param[out] available The location to place the output of the function,
 *        which is true when a firmware update is available that is a newer
 *        version than that which is currently installed.
 * @param[in] client The client which has been opened to the correct file, such
 *        that esp_http_client_read can be called repeatedly on it.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t processOTAAvailableFile(bool *available, 
                                  esp_http_client_handle_t client)
{
    char buf[OTA_RECV_BUF_SIZE];
    CircularBuffer circBuf;
    char circBacking[2 * OTA_RECV_BUF_SIZE];
    int bytesRead;
    int ndx;
    int startKey, endKey, startVal, endVal;
    VersionType verType;
    esp_err_t err;
    uint foundHardVer = 0;
    uint foundRevVer = 0;
    uint foundMajorVer = 0;
    uint foundMinorVer = 0;
    uint foundPatchVer = 0;

    /* input guards */
    if (available == NULL) return ESP_ERR_INVALID_ARG;
    if (client == NULL) return ESP_ERR_INVALID_ARG;

    /* load initial data into circular buffer */
    do {
        bytesRead = esp_http_client_read(client, buf, OTA_RECV_BUF_SIZE - 1);
    } while (bytesRead == -ESP_ERR_HTTP_EAGAIN);
    if (bytesRead <= 0) return ESP_ERR_NOT_FOUND;

    err = (esp_err_t) circularBufferInit(&circBuf, circBacking, 2 * OTA_RECV_BUF_SIZE);
    if (err != ESP_OK) return err;
    err = (esp_err_t) circularBufferStore(&circBuf, buf, bytesRead);
    if (err != ESP_OK) return err;
    err = (esp_err_t) circularBufferMark(&circBuf, 0, FROM_OLDEST_CHAR);
    if (err != ESP_OK) return err;

    /* mark initial bracket when found */
    bytesRead = circularBufferReadFromMark(&circBuf, buf, OTA_RECV_BUF_SIZE - 1);
    if (bytesRead <= 0) return bytesRead; // error code
    for (ndx = 0; ndx < bytesRead; ndx++)
    {
        if (buf[ndx] != '{') continue;
        err = (esp_err_t) circularBufferMark(&circBuf, ndx, FROM_PREV_MARK);
        if (err != ESP_OK) return err;
        break;
    }

    /* process file in chunks */
    while (circBuf.len > 0)
    {
        /* search for end of key, ':', and parse it */
        bytesRead = circularBufferReadFromMark(&circBuf, buf, OTA_RECV_BUF_SIZE - 1);
        if (bytesRead <= 0) return bytesRead; // error code
        for (ndx = 0; ndx < bytesRead; ndx++)
        {
            if (buf[ndx] != ':') continue;
            break;
        }
        if (buf[ndx] != ':')
        {
            ESP_LOGW(TAG, "JSON version file is malformed!");
            *available = false;
            return ESP_OK;
        }
        err = versionFromKey(&verType, buf, ndx);
        if (err == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGW(TAG, "JSON version file is malformed");
            *available = false;
            return ESP_OK;
        }

        /* search for end of value, ',', and parse it */
        for (; ndx < bytesRead; ndx++)
        {
            if (buf[ndx] == ',')
        }

        /* read more data from the file */
        do {
            bytesRead = esp_http_client_read(client, buf, OTA_RECV_BUF_SIZE - 1);
        } while (bytesRead == -ESP_ERR_HTTP_EAGAIN);
        if (bytesRead <= 0) return ESP_ERR_NOT_FOUND;
    
        err = (esp_err_t) circularBufferStore(&circBuf, buf, bytesRead);
        if (err != ESP_OK) return err;
    }
    /* compare versioning information against current version */
    *available = compareVersions(foundHardVer, foundRevVer, foundMajorVer, foundMinorVer, foundPatchVer);
    return ESP_OK;
}

/**
 * @brief Queries the server to ask if a firmware update is available. The
 *        queried file, FIRMWARE_UPGRADE_VERSION_URL, should correspond to
 *        the version of firmware at FIRMWARE_UPGRADE_URL.
 * 
 * @param[out] available The location to place the output of the function,
 *        which is true when a firmware update is available that is a newer
 *        version than that which is currently installed.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t queryOTAUpdateAvailable(bool *available)
{
    const esp_http_client_config_t https_config = {
        .url = FIRMWARE_UPGRADE_VERSION_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_err_t err;
    esp_http_client_handle_t client;
    int64_t contentLength;
    CircularBuffer circBuf;
    char circBufBacking[OTA_RECV_BUF_SIZE];
    int numBytesRead;

    if (available == NULL) return ESP_ERR_INVALID_ARG;

    // for (int i = 0; i < RETRY_CONNECT_OTA_AVAILABLE; i++)
    // {
    //     /* connect to server and query file */
    //     client = esp_http_client_init(&https_config);
    //     if (client == NULL)
    //     {
    //         ESP_LOGE(TAG, "queryOTAUpdateAvailable esp_http_client_init error");
    //         return ESP_FAIL;
    //     }
    
    //     err = esp_http_client_open(client, 0);
    //     if (err != ESP_OK)
    //     {
    //         ESP_LOGE(TAG, "queryOTAUpdateAvailable esp_http_client_open err: %d", err);
    //         return err;
    //     }
    
    //     contentLength = esp_http_client_fetch_headers(client);
    //     while (contentLength == -ESP_ERR_HTTP_EAGAIN)
    //     {
    //         contentLength = esp_http_client_fetch_headers(client);
    //     }
    //     if (contentLength <= 0 || contentLength >= AVAILABLE_BUFFER_SIZE) // null-terminator
    //     {
    //         ESP_LOGE(TAG, "queryOTAUpdateAvailable contentLength: %lld", contentLength);
    //         return ESP_FAIL;
    //     }

    //     int status = esp_http_client_get_status_code(client);
    //     if (esp_http_client_get_status_code(client) != 200)
    //     {
    //         ESP_LOGE(TAG, "queryOTAUpdateAvailable status code is %d", status);
    //         err = esp_http_client_cleanup(client);
    //         if (err != ESP_OK) {
    //             ESP_LOGE(TAG, "queryOTAUpdateAvailable esp_http_client_cleanup err: %d", err);
    //             return ESP_FAIL;
    //         }
    //     }

    //     /* read file */
    //     do
    //     {
    //         numBytesRead = esp_http_client_read(client, buffer, AVAILABLE_BUFFER_SIZE - 1);
    //     } while (numBytesRead == -ESP_ERR_HTTP_EAGAIN);
    //     if (numBytesRead <= 0) {
    //         ESP_LOGE(TAG, "queryOTAUpdateAvailable read %d bytes", numBytesRead);
    //         return ESP_FAIL;
    //     }


    // }
    ESP_LOGW(TAG, "queryOTAUpdateAvailable max retries exceeded");
    return ESP_FAIL; // max num retries exceeded
}
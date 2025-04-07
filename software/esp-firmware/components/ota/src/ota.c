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

#define TAG "ota"

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
esp_err_t versionFromKey(VersionType *verType, const char *str, int strLen);
bool compareVersions(uint hardVer, uint revVer, uint majorVer, uint minorVer, uint patchVer);
esp_err_t processOTAAvailableFile(bool *available, esp_http_client_handle_t client);
esp_err_t queryOTAUpdateAvailable(bool *available);

#ifndef CONFIG_DISABLE_TESTING_FEATURES

static uint hardVer;
static uint hardRev;
static uint majorVer;
static uint minorVer;
static uint patchVer;

void setHardwareVersion(uint version);
void setHardwareRevision(uint version);
void setFirmwareMajorVersion(uint version);
void setFirmwareMinorVersion(uint version);
void setFirmwarePatchVersion(uint version);

uint getHardwareVersion(void);
uint getHardwareRevision(void);
uint getFirmwareMajorVersion(void);
uint getFirmwareMinorVersion(void);
uint getFirmwarePatchVersion(void);

#else

static uint getHardwareVersion(void);
static uint getHardwareRevision(void);
static uint getFirmwareMajorVersion(void);
static uint getFirmwareMinorVersion(void);
static uint getFirmwarePatchVersion(void);

#endif /* CONFIG_DISABLE_TESTING_FEATURES */

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

    /* query most recent server firmware version and indicate if an update is available */
#if CONFIG_HARDWARE_VERSION == 1
        /* feature unsupported */
#elif CONFIG_HARDWARE_VERSION == 2
    bool updateAvailable;

    (void) queryOTAUpdateAvailable(&updateAvailable); // allow firmware updates even if this
                                                      // function fails in order to fix 
                                                      // potential issues in this function
    if (updateAvailable)
    {
        indicateOTAAvailable();
    }
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
 * @note The function will set verType to VER_TYPE_UNKNOWN if the
 *       key does not match a versioning key, even when ESP_OK is returned.
 * 
 * @param[out] verType The location to place the version type of str.
 * @param[in] str A buffer containing the JSON key. Contents are
 *        unmodified.
 * @param[in] strLen The length of str.
 * 
 * @returns ESP_OK if successful.
 *          ESP_ERR_INVALID_ARG if invalid argument,
 *          and verType is unchanged.
 *          ESP_ERR_NOT_FOUND if quotation marks are not found,
 *          and verType is unchanged.
 */
esp_err_t versionFromKey(VersionType *verType, 
                         const char str[], 
                         int strLen)
{
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
            keyEndNdx = currNdx;
            break;
        }
        currNdx++;
    }

    if (keyEndNdx == 0) {
        /* impossible regularly, JSON is malformed */
        return ESP_ERR_NOT_FOUND;
    }

    /* parse and match string */
    *verType = VER_TYPE_UNKNOWN;
    if (keyEndNdx - keyStartNdx == sizeof(HARDWARE_VERSION_KEY) - 1 &&
        0 == strncmp(&str[keyStartNdx], HARDWARE_VERSION_KEY, keyEndNdx - keyStartNdx))
    {
        *verType = HARDWARE;
    }

    if (keyEndNdx - keyStartNdx == sizeof(HARDWARE_REVISION_KEY) - 1 &&
        0 == strncmp(&str[keyStartNdx], HARDWARE_REVISION_KEY, keyEndNdx - keyStartNdx))
    {
        *verType = REVISION;
    }

    if (keyEndNdx - keyStartNdx == sizeof(FIRMWARE_MAJOR_KEY) - 1 &&
        0 == strncmp(&str[keyStartNdx], FIRMWARE_MAJOR_KEY, keyEndNdx - keyStartNdx))
    {
        *verType = MAJOR;
    }

    if (keyEndNdx - keyStartNdx == sizeof(FIRMWARE_MINOR_KEY) - 1 &&
        0 == strncmp(&str[keyStartNdx], FIRMWARE_MINOR_KEY, keyEndNdx - keyStartNdx))
    {
        *verType = MINOR;
    }

    if (keyEndNdx - keyStartNdx == sizeof(FIRMWARE_PATCH_KEY) - 1 &&
        0 == strncmp(&str[keyStartNdx], FIRMWARE_PATCH_KEY, keyEndNdx - keyStartNdx))
    {
        *verType = PATCH;
    }

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

    ESP_LOGI(TAG, "server firmware image is V%d_%d v%d.%d.%d", hardVer, revVer, majorVer, minorVer, patchVer);
    ESP_LOGI(TAG, "device firmware image is V%d_%d v%d.%d.%d", getHardwareVersion(), getHardwareRevision(), getFirmwareMajorVersion(), getFirmwareMinorVersion(), getFirmwarePatchVersion());

    /* compare hadware version */
    if (hardVer != getHardwareVersion()) return false;
    if (revVer != getHardwareRevision()) return false;
    /* compare firmware version */
    if (majorVer > getFirmwareMajorVersion()) return true;
    if (majorVer < getFirmwareMajorVersion()) return false;

    if (minorVer > getFirmwareMinorVersion()) return true;
    if (minorVer < getFirmwareMinorVersion()) return false;

    if (patchVer > getFirmwarePatchVersion()) return true;
    if (patchVer < getFirmwarePatchVersion()) return false;
    return false;
}

/**
 * @brief Parses the contents of buf for the latest available firmware version
 *        and compares that version to the current version installed on the device.
 * 
 * @note Expects buf contents to be formatted as a JSON file with all keys ignored
 *       except for the following, whose VALUES ARE PARSED AS INTEGERS--
 *       hardware_version, hardware_revision, firmware_major_version,
 *       firmware_minor_version, firmware_patch_version. This function ignores
 *       brackets in the file.
 * 
 * @note This function does not implement a full JSON parser. It expects a single
 *       flat JSON object with only key value pairs, where the value is an integer.
 * 
 * @param[out] available The location to place the output of the function,
 *        which is true when a firmware update is available that is a newer
 *        version than that which is currently installed.
 * @param[in] client The client which has been opened to the correct file, such
 *        that esp_http_client_read can be called repeatedly on it.
 * 
 * @returns ESP_OK if successful and available contains true or false.
 *          ESP_ERR_INVALID_ARG if invalid argument and available is unchanged.
 *          ESP_FAIL or other codes if an error occurs and available is false.
 */
esp_err_t processOTAAvailableFile(bool *available, 
                                  esp_http_client_handle_t client)
{
    char buf[OTA_RECV_BUF_SIZE];
    CircularBuffer circBuf;
    char circBacking[2 * OTA_RECV_BUF_SIZE];
    int bytesRead;
    int ndx;
    int value;
    VersionType verType;
    esp_err_t err;
    uint foundHardVer = 0;
    uint foundRevVer = 0;
    uint foundMajorVer = 0;
    uint foundMinorVer = 0;
    uint foundPatchVer = 0;

    /* parsing state variables */
    bool inKey = false;
    bool inValue = false;
    bool inJSON = false;
    bool inComment = false;
    bool inString = false;
    bool JSONParsed = false;

    /* input guards */
    if (available == NULL) return ESP_ERR_INVALID_ARG;
    if (client == NULL) return ESP_ERR_INVALID_ARG;

    *available = false;

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

    // handle edge case of formatting character at buf[0]. The loop below skips
    // the previous formatting character at buf[0], so this character must be
    // handled manually here.
    if (buf[0] == '{')
    {
        inJSON = true;
        inKey = true;
    } else if (buf[0] == '#')
    {
        inComment = true;
    } else if (buf[0] == ':' || 
               buf[0] == ',' || 
               buf[0] == '}' || 
               buf[0] == '\"')
    {
        ESP_LOGW(TAG, "JSON contains stray formatting character, %c", buf[0]);
        return ESP_FAIL;
    }

    /* continuously process, mark file, and read data in as necessary */
    // the mark will always be on a formatting character, which should be skipped in processing
    bool foundFormattingChar = true;
    while (bytesRead > 0)
    {
        if (!foundFormattingChar)
        {
            /* circular buffer is missing next formatting char, retrieve more data */
            do {
                bytesRead = esp_http_client_read(client, buf, OTA_RECV_BUF_SIZE - 1);
            } while (bytesRead == -ESP_ERR_HTTP_EAGAIN);
            if (bytesRead < 0) {
                ESP_LOGE(TAG, "processOTAAvailableFile esp_http_client_read err: %d", err);
                return err;
            }
            if (bytesRead == 0) break; // circ buf is empty and nothing else to read
        
            err = (esp_err_t) circularBufferStore(&circBuf, buf, bytesRead);
            if (err == (esp_err_t) CIRC_LOST_MARK)
            {
                ESP_LOGW(TAG, "JSON contains fields that are too large to parse");
                return ESP_FAIL;
            }
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "processOTAAvailableFile circularBufferStore err: %d", err);
                return err;
            }

            /* reset 'skip' states because characters mutating the state
               will be reread, and must not be read incorrectly. This has
               the effect of increasing the effective size between marks,
               meaning comments add to the field length. */
            inComment = false;
            inString = false;
        }
        bytesRead = circularBufferReadFromMark(&circBuf, buf, OTA_RECV_BUF_SIZE - 1);
        if (bytesRead < 0) return bytesRead; // error code

        /* search for formatting character */
        foundFormattingChar = false;
        for (ndx = 1; ndx < bytesRead; ndx++) // ndx = 1: skip prev formatting char
        {
            /* handle comments */
            if (buf[ndx] == '#' && !inString)
            {
                inComment = true;
                continue;
            } else if (buf[ndx] == '\n' && inComment)
            {
                inComment = false;
                continue;
            }

            if (inComment)
            {
                continue;
            }

            /* handle 'inString' state, which allows formatting characters in strings */
            if (buf[ndx] == '\"')
            {
                if (!inKey && inJSON)
                {
                    ESP_LOGW(TAG, "found invalid \" in JSON. String values are not supported!");
                    return ESP_FAIL;
                } else if (!inKey && !inJSON)
                {
                    ESP_LOGW(TAG, "missing '{' in JSON, or stray \" exists before JSON object!");
                    return ESP_FAIL;
                }

                inString = !inString;
                continue;
            }

            if (inString)
            {
                continue;
            }

            /* valid JSON formatting */
            if (buf[ndx] == '{' ||
                buf[ndx] == ':' ||
                buf[ndx] == ',' ||
                buf[ndx] == '}')
            {
                /* a formatting character that is not in
                a comment or string has been found */
                foundFormattingChar = true;
                err = (esp_err_t) circularBufferMark(&circBuf, ndx, FROM_PREV_MARK);
                if (err != (esp_err_t) CIRC_OK) 
                {
                    return err;
                }
                break;
            }
        }

        /* at this point, a formatting char has been found and marked, 
           with buf[0] denoting the previous formatting char or start of file */
        if (buf[ndx] == '{')
        {
            if (inJSON)
            {
                ESP_LOGW(TAG, "misplaced \'{\' found in JSON");
                return ESP_FAIL;
            }

            inJSON = true;
            inKey = true;
        }

        if (buf[ndx] == ':')
        {
            if (!inKey)
            {
                ESP_LOGW(TAG, "misplaced \':\' found in JSON");
                return ESP_FAIL;
            }

            inValue = true;
            inKey = false;
            
            err = versionFromKey(&verType, &buf[1], ndx - 1);
            if (err != ESP_OK)
            {
                ESP_LOGW(TAG, "processOTAavailableFile versionFromKey err: %d", err);
                return ESP_FAIL;
            }
        }

        if (buf[ndx] == ',' || buf[ndx] == '}')
        {
            if (!inValue)
            {
                ESP_LOGW(TAG, "misplaced \'%c\' found in JSON", buf[ndx]);
                return ESP_FAIL;
            }

            inValue = false;
            if (buf[ndx] == '}')
            {
                inJSON = false;
                JSONParsed = true;
            } else 
            {
                inKey = true;
            }

            if (verType < VER_TYPE_UNKNOWN) // short-circuit don't care keys
            {
                /* determine value */
                buf[ndx] = '\0'; // create a c-string from value, necessary for strtol
                value = (int) strtol(&buf[1], NULL, 10); // this is not a thread safe function,
                                                         // because errno is not thread safe.
                                                         // However, this func returns 0 if it fails,
                                                         // which for this purpose is ok because
                                                         // 0 will always be the smallest value possible
                if (value < 0) value = 0; // clamp value. 0 is a safe number b/c no version is smaller.
                buf[ndx] = ','; // avoid potential issues from null terminator...

                /* record key/value pair */
                switch (verType)
                {
                    case HARDWARE:
                        foundHardVer = value;
                        break;
                    case REVISION:
                        foundRevVer = value;
                        break;
                    case MAJOR:
                        foundMajorVer = value;
                        break;
                    case MINOR:
                        foundMinorVer = value;
                        break;
                    case PATCH:
                        foundPatchVer = value;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    if (!JSONParsed)
    {
        ESP_LOGW(TAG, "Did not find \'}\' in JSON");
        return ESP_FAIL;
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

    if (available == NULL) return ESP_ERR_INVALID_ARG;

    for (int i = 0; i < RETRY_CONNECT_OTA_AVAILABLE; i++)
    {
        /* connect to server and query file */
        client = esp_http_client_init(&https_config);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "queryOTAUpdateAvailable esp_http_client_init error");
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "Checking server firmware version: %s", FIRMWARE_UPGRADE_VERSION_URL);
        err = esp_http_client_open(client, 0);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "queryOTAUpdateAvailable esp_http_client_open err: %d", err);
            return err;
        }

        do {
            contentLength = esp_http_client_fetch_headers(client);
        } while (contentLength == -ESP_ERR_HTTP_EAGAIN);
        if (contentLength <= 0) // null-terminator
        {
            ESP_LOGE(TAG, "queryOTAUpdateAvailable contentLength: %lld", contentLength);
            return ESP_FAIL;
        }

        int status = esp_http_client_get_status_code(client);
        if (esp_http_client_get_status_code(client) != 200)
        {
            ESP_LOGE(TAG, "queryOTAUpdateAvailable status code is %d", status);
            err = esp_http_client_cleanup(client);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "queryOTAUpdateAvailable esp_http_client_cleanup err: %d", err);
                return ESP_FAIL;
            }
        }

        err = processOTAAvailableFile(available, client);
        if (err == ESP_OK) return ESP_OK;
        if (err != ESP_OK)
        {
            *available = false;
            ESP_LOGE(TAG, "failed to process OTA available file. err: %d", err);
        }
    }
    ESP_LOGW(TAG, "queryOTAUpdateAvailable max retries exceeded");
    return ESP_FAIL; // max num retries exceeded
}

#ifndef CONFIG_DISABLE_TESTING_FEATURES

void setHardwareVersion(uint version) { hardVer = version; }
void setHardwareRevision(uint version) { hardRev = version; }
void setFirmwareMajorVersion(uint version) { majorVer = version; }
void setFirmwareMinorVersion(uint version) { minorVer = version; }
void setFirmwarePatchVersion(uint version) { patchVer = version; }

/* Mocking getter functions that replace __attribute__((weak)) functions */

uint getHardwareVersion(void) { return hardVer; }
uint getHardwareRevision(void) { return hardRev; }
uint getFirmwareMajorVersion(void) { return majorVer; }
uint getFirmwareMinorVersion(void) { return minorVer; }
uint getFirmwarePatchVersion(void) { return patchVer; }

#else

static uint getHardwareVersion() { return CONFIG_HARDWARE_VERSION; }
static uint getHardwareRevision() { return CONFIG_HARDWARE_REVISION; }
static uint getFirmwareMajorVersion() { return CONFIG_FIRMWARE_MAJOR_VERSION; }
static uint getFirmwareMinorVersion() { return CONFIG_FIRMWARE_MINOR_VERSION; }
static uint getFirmwarePatchVersion() { return CONFIG_FIRMWARE_PATCH_VERSION; }

#endif /* CONFIG_DISABLE_TESTING_FEATURES */
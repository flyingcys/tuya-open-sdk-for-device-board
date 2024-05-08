/**
 * @file tkl_output.c
 * @brief this file was auto-generated by tuyaos v&v tools, developer can add implements between BEGIN and END
 * 
 * @warning: changes between user 'BEGIN' and 'END' will be keeped when run tuyaos v&v tools
 *           changes in other place will be overwrited and lost
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 * 
 */

// --- BEGIN: user defines and implements ---
#include "tkl_output.h"
#include "tuya_error_code.h"

#include "esp_err.h"
#include "esp_log.h"
// --- END: user defines and implements ---

#if defined(ENABLE_LOG_OUTPUT_FORMAT) && (ENABLE_LOG_OUTPUT_FORMAT == 1)
#ifndef MAX_SIZE_OF_DEBUG_BUF
#define MAX_SIZE_OF_DEBUG_BUF (1024)
#endif

STATIC CHAR_T s_output_buf[MAX_SIZE_OF_DEBUG_BUF] = {0};
#endif

/**
* @brief Output log information
*
* @param[in] format: log information
*
* @note This API is used for outputing log information
*
* @return 
*/
VOID_T tkl_log_output(CONST CHAR_T *format, ...)
{
    // --- BEGIN: user implements ---
    if (format == NULL) {
        return;
    }

#if defined(ENABLE_LOG_OUTPUT_FORMAT) && (ENABLE_LOG_OUTPUT_FORMAT == 1)
    va_list ap;

    va_start(ap, format);
    vsnprintf(s_output_buf, MAX_SIZE_OF_DEBUG_BUF,format,ap);
    va_end(ap);
    ESP_LOGI("TKL_LOG", "%s", format);
#else
    ESP_LOGI("TKL_LOG", "%s", format);
#endif
    // --- END: user implements ---
}

/**
* @brief Close log port
*
* @param VOID
*
* @note This API is used for closing log port.
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tkl_log_close(VOID_T)
{
    // --- BEGIN: user implements ---
    return OPRT_OK;
    // --- END: user implements ---
}

/**
* @brief Open log port
*
* @param VOID
*
* @note This API is used for openning log port.
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tkl_log_open(VOID_T)
{
    // --- BEGIN: user implements ---
    return OPRT_OK;
    // --- END: user implements ---
}

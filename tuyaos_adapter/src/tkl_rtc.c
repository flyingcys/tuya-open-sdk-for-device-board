/**
* @file tkl_rtc.c
* @brief Common process - adapter the rtc api default weak implement
* @version 0.1
* @date 2021-08-06
*
* @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
*
*/
#include "tkl_rtc.h"
#include   <time.h> 

/**
 * @brief rtc init
 * 
 * @param[in] none
 *
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_rtc_init(VOID_T)
{
    return OPRT_OK;
}

/**
 * @brief rtc deinit
 * @param[in] none
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_rtc_deinit(VOID_T)
{
    return OPRT_OK;
}

/**
 * @brief rtc time set
 * 
 * @param[in] time_sec: rtc time seconds
 *
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_rtc_time_set(TIME_T time_sec)
{
    time_t   timep = time_sec;

    printf( "clock -> %s \r\n",ctime(&timep));

    return OPRT_OK;
}

/**
 * @brief rtc time get
 * 
 * @param[in] time_sec:rtc time seconds
 *
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_rtc_time_get(TIME_T *time_sec)
{
    time_t   timep;

    time(&timep);
    
    *time_sec = timep;

    return OPRT_OK;
}



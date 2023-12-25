/**
 * @file tkl_system.c
 * @brief the default weak implements of tuya os system api, this implement only used when OS=linux
 * @version 0.1
 * @date 2019-08-15
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_system.h"
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

/**
* @brief Get system ticket count
*
* @param VOID
*
* @note This API is used to get system ticket count.
*
* @return system ticket count
*/
TUYA_WEAK_ATTRIBUTE SYS_TICK_T tkl_system_get_tick_count(VOID_T)
{
    struct timespec time1 = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &time1);
    return 1000*((ULONG_T)time1.tv_sec) + ((ULONG_T)time1.tv_nsec)/1000000;
}

/**
* @brief Get system millisecond
*
* @param none
*
* @return system millisecond
*/
TUYA_WEAK_ATTRIBUTE SYS_TIME_T tkl_system_get_millisecond(VOID_T)
{
    struct timespec time1 = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &time1);
    return 1000*((ULONG_T)time1.tv_sec) + ((ULONG_T)time1.tv_nsec)/1000000;
}

/**
* @brief System sleep
*
* @param[in] msTime: time in MS
*
* @note This API is used for system sleep.
*
* @return VOID
*/
TUYA_WEAK_ATTRIBUTE VOID_T tkl_system_sleep(CONST UINT_T num_ms)
{
    TIME_S sTmpTime;
    TIME_MS msTmpTime;

    sTmpTime = num_ms / 1000;
    msTmpTime = num_ms % 1000;

    if(sTmpTime)
        sleep(sTmpTime);

    if(msTmpTime)
        usleep(msTmpTime*1000);

    return;
}

/**
* @brief System reset
*
* @param VOID
*
* @note This API is used for system reset.
*
* @return VOID
*/
TUYA_WEAK_ATTRIBUTE VOID_T tkl_system_reset(VOID_T)
{
    printf("Rev Restart Req\r\n");
    exit(0);
    return;
}

/**
* @brief Get system reset reason
*
* @param VOID
*
* @note This API is used for getting system reset reason.
*
* @return reset reason of system
*/
TUYA_RESET_REASON_E tkl_system_get_reset_reason(CHAR_T** describe)
{
    return TUYA_RESET_REASON_UNKNOWN;
}

/**
* @brief Get a random number in the specified range
*
* @param[in] range: range
*
* @note This API is used for getting a random number in the specified range
*
* @return a random number in the specified range
*/
TUYA_WEAK_ATTRIBUTE INT_T tkl_system_get_random(CONST UINT_T range)
{
    return rand() % range;
}

/**
* @brief Set the low power mode of CPU
*
* @param[in] enable: enable switch
* @param[in] mode:   cpu sleep mode
*
* @note This API is used for setting the low power mode of CPU.
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tkl_cpu_sleep_mode_set(BOOL_T enable, TUYA_CPU_SLEEP_MODE_E mode)
{
    return OPRT_OK;
}


OPERATE_RET tkl_system_get_cpu_info(TUYA_CPU_INFO_T **cpu_ary, INT_T *cpu_cnt)
{
    return OPRT_NOT_FOUND;
}
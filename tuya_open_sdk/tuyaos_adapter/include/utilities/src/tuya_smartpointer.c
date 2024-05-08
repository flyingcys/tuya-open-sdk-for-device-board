/**
 * @file tuya_smartpointer.h
 * @brief tuya reference data module
 * @version 1.0
 * @date 2019-10-30
 * 
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 * 
 */
 
#include "tuya_smartpointer.h"
#include "tkl_memory.h"
#include <string.h>

#if defined(OPERATING_SYSTEM) && (SYSTEM_NON_OS == OPERATING_SYSTEM)
#define SP_CREATE_LOCK(sp_data)    (sp_data)
#define SP_RELEASE_LOCK(sp_data)   (sp_data)
#define SP_LOCK(sp_data)   TKL_ENTER_CRITICAL()
#define SP_UNLOCK(sp_data) TKL_EXIT_CRITICAL()
#else
#define SP_CREATE_LOCK(sp_data)  tkl_mutex_create_init(&sp_data->mutex)
#define SP_RELEASE_LOCK(sp_data) tkl_mutex_release(sp_data->mutex)
#define SP_LOCK(sp_data)   tkl_mutex_lock(sp_data->mutex)
#define SP_UNLOCK(sp_data) tkl_mutex_unlock(sp_data->mutex)
#endif

/**
 * @brief create a reference data
 * 
 * @param[in] data the data buffer
 * @param[in] data_len the date length
 * @param[in] malk need malloc memory for the data
 * @param[in] cnt the Initial value of the reference
 * @return the reference data address
 */
SMARTPOINTER_T *tuya_smartpointer_create(VOID *data, CONST UINT_T data_len, CONST BOOL_T malk, CONST UINT_T cnt)
{
    if (0 == data_len || NULL == data) {
        return NULL;
    }

    SMARTPOINTER_T *sp_data = NULL;

    if (TRUE == malk) {
        sp_data = (SMARTPOINTER_T *)tkl_system_malloc(SIZEOF(SMARTPOINTER_T) + data_len);
    } else {
        sp_data = (SMARTPOINTER_T *)tkl_system_malloc(SIZEOF(SMARTPOINTER_T));
    }
    
    if (NULL == sp_data) {
        return NULL;
    }
    memset(sp_data,0,SIZEOF(SMARTPOINTER_T));

    SP_CREATE_LOCK(sp_data);

    if (TRUE == malk) {
        sp_data->data = (BYTE_T *)sp_data + SIZEOF(SMARTPOINTER_T);
        memcpy(sp_data->data,data,data_len);
    } else {
        sp_data->data = data;
    }
    sp_data->data_len = data_len;
    sp_data->rfc = cnt;

    return sp_data;
}


/**
 * @brief get the reference data, increase the reference
 * 
 * @param[inout] sp_data the reference data
 * @return VOID 
 */
VOID_T tuya_smartpointer_get(SMARTPOINTER_T *sp_data)
{
    if (NULL == sp_data) {
        return;
    }

    SP_LOCK(sp_data);
    sp_data->rfc++;
    SP_UNLOCK(sp_data);

    return;
}

/**
 * @brief put the reference data, decrease the reference
 * 
 * @param[inout] sp_data the reference data 
 * @return VOID 
 * 
 * @note the reference data will be released when reference is 0
 */
VOID_T tuya_smartpointer_put(SMARTPOINTER_T *sp_data)
{
    if (NULL == sp_data) {
        return;
    }

    UINT_T rfc = 0;
    SP_LOCK(sp_data);
    rfc = --sp_data->rfc;
    SP_UNLOCK(sp_data);
    if (0 == rfc) {
        SP_RELEASE_LOCK(sp_data);
        if(FALSE == sp_data->malk) {
            tkl_system_free(sp_data->data);
        }
        tkl_system_free(sp_data);
    }

    return;
}

/**
 * @brief delete the reference data, ignore the reference
 * 
 * @param[inout] sp_data the reference data 
 * @return VOID 
 */
VOID_T tuya_smartpointer_del(SMARTPOINTER_T *sp_data)
{
    SP_RELEASE_LOCK(sp_data);

    if(FALSE == sp_data->malk) {
        tkl_system_free(sp_data->data);
    }
    
    tkl_system_free(sp_data);
    return;
}



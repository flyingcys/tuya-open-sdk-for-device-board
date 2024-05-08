/**
 * @file tkl_memory.c
 * @brief this file was auto-generated by tuyaos v&v tools, developer can add implements between BEGIN and END
 * 
 * @warning: changes between user 'BEGIN' and 'END' will be keeped when run tuyaos v&v tools
 *           changes in other place will be overwrited and lost
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 * 
 */

// --- BEGIN: user defines and implements ---
#include "tkl_memory.h"
#include "freertos/FreeRTOS.h"
#include "tuya_error_code.h"
// --- END: user defines and implements ---

/**
* @brief Alloc memory of system
*
* @param[in] size: memory size
*
* @note This API is used to alloc memory of system.
*
* @return the memory address malloced
*/
VOID_T *tkl_system_malloc(SIZE_T size)
{
    // --- BEGIN: user implements ---
    return pvPortMalloc(size);
    // --- END: user implements ---
}

/**
* @brief Free memory of system
*
* @param[in] ptr: memory point
*
* @note This API is used to free memory of system.
*
* @return VOID_T
*/
VOID_T tkl_system_free(VOID_T* ptr)
{
    // --- BEGIN: user implements ---
    vPortFree(ptr);
    // --- END: user implements ---
}

/**
 * @brief Allocate and clear the memory
 * 
 * @param[in]       nitems      the numbers of memory block
 * @param[in]       size        the size of the memory block
 *
 * @return the memory address calloced
 */
VOID_T *tkl_system_calloc(size_t nitems, size_t size)
{
    // --- BEGIN: user implements ---
    void *addr;

    addr = tkl_system_malloc(nitems * size);
    if (addr == NULL) {
        return addr;
    }

    memset(addr, 0, nitems * size);
    return addr;
    // --- END: user implements ---
}

/**
 * @brief Re-allocate the memory
 * 
 * @param[in]       nitems      source memory address
 * @param[in]       size        the size after re-allocate
 *
 * @return VOID_T
 */
VOID_T *tkl_system_realloc(VOID_T* ptr, size_t size)
{
    // --- BEGIN: user implements ---
    void *new;

    if (size == 0) {
        tkl_system_free(ptr);
        return NULL;
    }

    if (ptr == NULL) {
        return tkl_system_malloc(size);
    }

    new = tkl_system_malloc(size);
    if (new == NULL) {
        return NULL;
    }

    memcpy(new, ptr, size);
    tkl_system_free(ptr);
    return new;
    // --- END: user implements ---
}

/**
* @brief Get system free heap size
*
* @param none
*
* @return heap size
*/
INT_T tkl_system_get_free_heap_size(VOID_T)
{
    /*
     * ble assert 时会通过bk_ble_mem_assert_type_cb返回对应的asser reason， 通过这个回调判断当前是否出现ble assert。
     * 通过 tkl_system_get_free_heap_size 返回小于 8k 内存，触发sdk对内存监控小于8k时通知应用，决定是否需要重启。
    */

    // --- BEGIN: user implements ---
    return  (int)xPortGetFreeHeapSize();
    // --- END: user implements ---
}


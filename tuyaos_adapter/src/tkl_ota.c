/**
 * @file tkl_ota.c
 * @brief default weak implements of tuya ota, it only used when OS=linux
 * 
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 * 
 */
#include "tkl_ota.h"
#include "tuya_error_code.h"

STATIC FILE *s_upgrade_fd = NULL;


/**
* @brief get ota ability
*
* @param[out] image_size:  max image size
* @param[out] type:        ota type
*
* @note This API is used for get chip ota ability
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_ota_get_ability(UINT_T *image_size, UINT32_T *type)
{
    *image_size = -1;
    *type = TUYA_OTA_FULL|TUYA_OTA_DIFF;
    
    return OPRT_OK;
}

/**
 * @brief ota packet download start inform
 * 
 * @param[in] file_size the size of the ota packet
 * 
 * @return OPRT_OK on success, others on failed
 *
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_ota_start_notify(UINT_T image_size, TUYA_OTA_TYPE_E type, TUYA_OTA_PATH_E path)
{

    FILE *p_upgrade_fd = (FILE *)s_upgrade_fd;

    if (p_upgrade_fd) {
        fclose(p_upgrade_fd);
    }

    char ota_path[255];

    sprintf(ota_path, "./tuyadb/ota");
    s_upgrade_fd = fopen(ota_path, "w+b");

    if(NULL == s_upgrade_fd){
        printf("open upgrade file fail. upgrade fail %s\r\n", ota_path);
        return OPRT_COM_ERROR;
    }


    return OPRT_OK;
}

/**
 * @brief ota packet data process
 * 
 * @param[in] total_len ota the total length of the ota packet
 * @param[in] offset the data offset in ota packet
 * @param[in] data ota the data
 * @param[in] len the data length
 * @param[out] remain_len the remain length of the data
 * @param[in] pri_data reserved
 *
 * @return OPRT_OK on success, others on failed
 *
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_ota_data_process(TUYA_OTA_DATA_T *pack, UINT_T* remain_len)
{
    printf("Rev File Data Total_len %d, Offset:%u Len:%u\r\n", pack->total_len, pack->offset, pack->len);
    FILE *p_upgrade_fd = (FILE *)s_upgrade_fd;
    if (pack->offset + pack->len != pack->total_len) {
        fwrite(pack->data, 1, pack->len - 100, p_upgrade_fd);
        *remain_len = 100;
    } else {
        fwrite(pack->data, 1, pack->len, p_upgrade_fd);
        *remain_len = 0;
    }

    return OPRT_OK;
}

/**
 * @brief firmware ota packet transform success inform, can do verify in this funcion
 * 
 * @param[in]        reset       need reset or not
 * 
 * @return OPRT_OK on success, others on failed
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_ota_end_notify(BOOL_T reset)
{
    FILE *p_upgrade_fd = (FILE *)s_upgrade_fd;
    fclose(p_upgrade_fd);

    if (reset) {
        printf("SOC Upgrade File Download Success\r\n");
        // UserTODO
        exit(1);
    }else {
        printf("SOC Upgrade File Download Fail.ret = %d\r\n", reset);
    }


    return OPRT_OK;
}


/**
 * @file tkl_wired.c
 * @brief the default weak implements of wired driver, this implement only used when OS=linux
 * @version 0.1
 * @date 2019-08-15
 * 
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 * 
 */
#include "tuya_cloud_types.h"
#include "tkl_wired.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

TKL_WIRED_STATUS_CHANGE_CB event_cb;
pthread_t  wired_event_thread = 0;

STATIC OPERATE_RET __tkl_wired_get_status_by_name(CONST CHAR_T *if_name, TKL_WIRED_STAT_E *status)
{
    int sockfd;
    struct ifreq ifr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return OPRT_SOCK_ERR;
    }

    strncpy(ifr.ifr_name, if_name, strlen(if_name));
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        perror("ioctl");
        close(sockfd);
        return OPRT_COM_ERROR;
    }

    *status = (ifr.ifr_flags & IFF_UP)?TKL_WIRED_LINK_UP:TKL_WIRED_LINK_DOWN;
    close(sockfd);

    return OPRT_OK;       
}

STATIC VOID *wifi_status_event_cb(VOID *arg)
{
    TKL_WIRED_STAT_E last_stat = -1;
    TKL_WIRED_STAT_E stat = TKL_WIRED_LINK_DOWN;
     
    pthread_detach (pthread_self());
    while (1) {
        tkl_wired_get_status(&stat);
        if (stat != last_stat) {
            event_cb(stat);
            last_stat = stat;
        }

        sleep(3);
    }

    return NULL;
}

/**
 * @brief  get the link status of wired link
 *
 * @param[out]  is_up: the wired link status is up or not
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_wired_get_status(TKL_WIRED_STAT_E *status)
{
    *status = TKL_WIRED_LINK_DOWN;
    struct if_nameindex *name_list;
    int i;

    name_list = if_nameindex();
    if (NULL == name_list) {
        return OPRT_NOT_FOUND;
    }

    for (i=0; name_list[i].if_index != 0; ++i) {
        if (strcmp(name_list[i].if_name, "lo") == 0)
            continue;

        if (!__tkl_wired_get_status_by_name(name_list[i].if_name, status))
            break;
    }

    if_freenameindex(name_list);
    return OPRT_OK;
}

/**
 * @brief  set the status change callback
 *
 * @param[in]   cb: the callback when link status changed
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_wired_set_status_cb(TKL_WIRED_STATUS_CHANGE_CB cb)
{
    if (cb) {
        TKL_WIRED_STAT_E status;
        struct if_nameindex *name_list;
        int i;

        name_list = if_nameindex();
        if (NULL == name_list) {
            return OPRT_NOT_FOUND;
        }

        for (i=0; name_list[i].if_index != 0; ++i) {
            if (strcmp(name_list[i].if_name, "lo") == 0)
                continue;

            if (!__tkl_wired_get_status_by_name(name_list[i].if_name, &status))
                break;
        }

        if_freenameindex(name_list);

        event_cb = cb;
        if (!wired_event_thread) {
            int ret = pthread_create(&wired_event_thread, NULL, wifi_status_event_cb,NULL);
            if (OPRT_OK != ret) {
                return ret;
            }            
        }

        event_cb(status);
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __tkl_wired_get_ip_by_name(CONST CHAR_T *if_name, NW_IP_S *ip)
{
    int sock;
    struct ifreq ifr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("socket create failse...GetLocalIp!\n");
        return OPRT_COM_ERROR;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name) - 1);
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        close(sock);
        return OPRT_COM_ERROR;
    }
    strncpy(ip->ip, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), SIZEOF(ip->ip));

    if (ioctl(sock, SIOCGIFNETMASK, &ifr) < 0) {
        close(sock);
        return OPRT_COM_ERROR;
    }
    strncpy(ip->mask, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr), SIZEOF(ip->mask));

    close(sock);
    return OPRT_OK;
}

/**
 * @brief  get the ip address of the wired link
 * 
 * @param[in]   ip: the ip address
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_wired_get_ip(NW_IP_S *ip)
{
    OPERATE_RET rt = OPRT_OK;
    struct if_nameindex *name_list;
    int i;

    name_list = if_nameindex();
    if (NULL == name_list) {
        return OPRT_NOT_FOUND;
    }

    for (i=0; name_list[i].if_index != 0; ++i) {
        if (strcmp(name_list[i].if_name, "lo") == 0)
            continue;

        if (!__tkl_wired_get_ip_by_name(name_list[i].if_name, ip))
            break;
    }

    if (name_list[i].if_name == NULL) {
        rt = OPRT_NOT_FOUND;
    }

    if_freenameindex(name_list);

    return rt;
}

/**
 * @brief  get the mac address of the wired link
 * 
 * @param[in]   mac: the mac address
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_wired_get_mac(NW_MAC_S *mac)
{
    mac->mac[0] = 0xc8;
    mac->mac[1] = 0x5b;
    mac->mac[2] = 0x76;
    mac->mac[3] = 0x4d;
    mac->mac[4] = 0x75;
    mac->mac[5] = 0xcd;
    return OPRT_OK;
}

/**
 * @brief  set the mac address of the wired link
 * 
 * @param[in]   mac: the mac address
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_wired_set_mac(CONST NW_MAC_S *mac)
{
    return OPRT_OK;
}


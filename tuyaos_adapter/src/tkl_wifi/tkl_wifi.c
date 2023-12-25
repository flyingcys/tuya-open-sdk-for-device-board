#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/if_ether.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include "linux_wifi.h"
#include "tkl_memory.h"
#include "wpa_command.h"

typedef struct {
    WIFI_EVENT_CB           event_cb;
    pthread_t               event_thread;
    bool                    event_flag;
    bool                    sta_conn_flag;
    //! wifi sniffer
    pthread_t               snifffer;
    SNIFFER_CALLBACK        snifffer_cb;
    bool                    snifffer_flag;
    //! wifi management frame
    pthread_t               mgnt;
    WIFI_REV_MGNT_CB        mgnt_cb;
    bool                    mgnt_flag;
    //! ifname
    char                   *sta;
    char                   *ap;
    WF_AP_CFG_IF_S          apcfg;
} tkl_wifi_t;

tkl_wifi_t s_tkl_wifi;


static void *wifi_status_event_cb(void *arg)
{
    WF_EVENT_E wf_event;
    WF_STATION_STAT_E stat = 0;
    WF_STATION_STAT_E last_stat = 0;
    int last_state = -1;

    pthread_detach (pthread_self());

    s_tkl_wifi.event_flag = true;

    while (s_tkl_wifi.event_flag) {
        if (!s_tkl_wifi.sta_conn_flag) {
            //ap switch sta, waiting
            sleep(1);
            continue;
            s_tkl_wifi.sta_conn_flag = 0;
        } 

        tkl_wifi_station_get_status(&stat);
        if ( stat == WSS_IDLE ) {
            sleep(1);
            continue;
        }

        if (stat != WSS_CONN_SUCCESS && stat != WSS_GOT_IP) {
            wf_event = WFE_DISCONNECTED;
        }
        if (stat == WSS_CONN_FAIL) {
            wf_event = WFE_CONNECT_FAILED;
        } else if (stat == WSS_GOT_IP || stat == WSS_CONN_SUCCESS) {
            wf_event = WFE_CONNECTED;
            if (last_stat != stat) {
                last_stat = stat;
                last_state = wf_event;
                s_tkl_wifi.event_cb(wf_event, NULL);
                continue;
            }
        }

        if (last_state == wf_event) {
            sleep(1);
            continue;
        }

        last_state = wf_event;
        s_tkl_wifi.event_cb(wf_event,NULL);

        sleep(3);
    }

    return NULL;
}

OPERATE_RET tkl_wifi_init(WIFI_EVENT_CB cb)
{
    if (NULL == cb) {
        return OPRT_INVALID_PARM;
    }

    s_tkl_wifi.event_cb = cb;
    s_tkl_wifi.sta = WLAN_DEV;
    s_tkl_wifi.ap  = WLAN_AP;

    int ret = pthread_create(&s_tkl_wifi.event_thread, NULL, wifi_status_event_cb,NULL);
    if (OPRT_OK != ret) {
        TKL_LOGE("create status_cs thread failed");
        return ret;
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_set_cur_channel(UCHAR_T chan)
{
    int ret = 0;

    char *ifname = s_tkl_wifi.sta;

    ret =  wifi_set_channel(ifname, chan);
    if (0 != ret) {
        TKL_LOGE("wifi set channel failed %d", ret);
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_get_cur_channel(UCHAR_T *chan)
{
    int ret = 0;
    unsigned char channel = 0;

    if (NULL == chan) {
        return OPRT_INVALID_PARM;
    }

    char *ifname = s_tkl_wifi.sta;

    ret = wifi_get_channel(ifname, &channel);
    if (0 != ret) {
         return OPRT_COM_ERROR;
    }

    *chan = channel;

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_scan_ap(CONST SCHAR_T *ssid, AP_IF_S **ap_ary, UINT_T *num)
{
    int ret = 0;
    int index;
    char *ifname = s_tkl_wifi.sta;

    AP_IF_S *s_aps = (AP_IF_S *)tkl_system_malloc(MAX_AP_SEARCH * sizeof(AP_IF_S));
    memset(s_aps, 0, MAX_AP_SEARCH * sizeof(AP_IF_S));

    wifi_enable(ifname, 1);

    ret = wifi_scan_ap(ifname, s_aps, MAX_AP_SEARCH);
    if (ret == 0) {
        TKL_LOGE(" ap failed");
        return OPRT_COM_ERROR;
    }
    if (ssid == NULL) {
        *num = ret;
        *ap_ary = s_aps;
        /* --- print all scanned aps' information --- */
        for(index = 0; index < *num; index++) {
            TKL_LOGD("channel:%2d bssid:%02X-%02X-%02X-%02X-%02X-%02X ssid[%d]:%s",
                    s_aps[index].channel,
                    s_aps[index].bssid[0],s_aps[index].bssid[1],s_aps[index].bssid[2],
                    s_aps[index].bssid[3],s_aps[index].bssid[4],s_aps[index].bssid[5],
                    index, s_aps[index].ssid);
        }
    } else {
        AP_IF_S *ap_s = (AP_IF_S *)tkl_system_malloc(sizeof(AP_IF_S));
        memset(ap_s, 0, sizeof(AP_IF_S));
        *ap_ary = ap_s;
        *num = 1;
        for(index = 0; index < ret; index++) {
            if (strcmp((char*)ssid, (char*)s_aps[index].ssid) == 0) {
                memcpy(ap_s, &s_aps[index], sizeof(AP_IF_S));
                break;
            }
        }
        tkl_system_free(s_aps);
        s_aps = NULL;
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_release_ap(AP_IF_S *ap)
{
    if (ap == NULL) {
        return OPRT_INVALID_PARM;
    }
    tkl_system_free(ap);
    ap = NULL;

    return OPRT_OK;
}


OPERATE_RET tkl_wifi_get_ip(WF_IF_E wf, NW_IP_S *ip)
{
    int ret = 0;

    if (NULL == ip) {
        return OPRT_COM_ERROR;
    }

    //! TODO:
    if (WF_AP == wf) {
        *ip = s_tkl_wifi.apcfg.ip;
        return OPRT_OK;
    }

    char *ifname = s_tkl_wifi.sta;

    /* get ip */
    ret = wifi_get_ip(ifname, ip->ip);
    if (ret != 0){
        // TKL_LOGE("wifi get ip failed %d", ret);
        return OPRT_COM_ERROR;
    }
    /* get mask */
    ret = wifi_get_ip_mask(ifname, ip->mask);
    if (ret != 0){
        TKL_LOGE("wifi get mask failed %d", ret);
        return OPRT_COM_ERROR;
    }
    /* TODO: get gateway */
    strcpy(ip->gw, ip->ip);
    char *dot = strrchr(ip->gw, '.');
    if (dot) {
        *dot = 0;
        strcat(ip->gw, ".1");
    } else {
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_set_mac(WF_IF_E wf, CONST NW_MAC_S *mac)
{
    int   ret    = 0;

    char *ifname = s_tkl_wifi.sta;

    ret = wifi_set_mac(ifname, mac->mac);
    if (ret < 0) {
        return OPRT_COM_ERROR;
    } 
    TKL_LOGD("wifi set mac:%02x:%02x:%02x:%02x:%02x:%02x",
        mac->mac[0],mac->mac[1],mac->mac[2],mac->mac[3],mac->mac[4],mac->mac[5]);

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_get_mac(CONST WF_IF_E wf, NW_MAC_S *mac)
{
    int   ret    = 0;

    char *ifname = s_tkl_wifi.sta;

    ret = wifi_get_mac(ifname, mac->mac);
    if (ret < 0) {
        return OPRT_COM_ERROR;
    }
    TKL_LOGD("GET MAC success, MAC:%02x:%02x:%02x:%02x:%02x:%02x",
        mac->mac[0],mac->mac[1],mac->mac[2],mac->mac[3],mac->mac[4],mac->mac[5]);

    //! FIXME:
    if (wf == WF_AP) {
        mac->mac[4] = 
        mac->mac[4] = 0x4A;
        mac->mac[5] = 0x12;
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_set_work_mode(CONST WF_WK_MD_E mode)
{
    int ret = 0;

    switch (mode) {
        case WWM_POWERDOWN:
            ret = wifi_enable(s_tkl_wifi.sta, 0);
            break;
        case WWM_SNIFFER:
            ret = wifi_set_mode(s_tkl_wifi.sta, "Monitor");
            break;
        case WWM_STATION:
            ret = wifi_set_mode(s_tkl_wifi.sta, "Managed");
            break;
        case WWM_SOFTAP:
            ret = wifi_enable(s_tkl_wifi.ap, 0);
            ret = wifi_set_mode(s_tkl_wifi.ap, "Master");
            ret = wifi_enable(s_tkl_wifi.ap, 1);
            break;
        case WWM_STATIONAP:     /*sta + ap mode*/
            ret  = wifi_set_mode(s_tkl_wifi.sta, "Managed");
            ret |= wifi_set_mode(s_tkl_wifi.ap,  "Master");
            break;
        default:
            break;
    }

    if (0 == ret) {
        TKL_LOGD("WIFI SET work mode success, set mode %d", mode);
    } else {
        TKL_LOGE("WIFI SET work mode failed");
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_get_work_mode(WF_WK_MD_E *mode)
{
    int ret = 0;

    int  wf_mode;

    ret = wifi_get_mode(s_tkl_wifi.sta, &wf_mode);
    if (ret < 0) {
        TKL_LOGE("get work mode failed");
        return OPRT_COM_ERROR;
    }

    if (wf_mode == WIFI_MODE_MANAGED) {
        //! TODO:
        int iret2 = 0;
        WIFI_MODE_E wf2_mode;
        ret = wifi_get_mode(s_tkl_wifi.ap, (int*)&wf2_mode);

        if (iret2 == 0 && wf2_mode == WIFI_MODE_MASTER) {
            *mode = WWM_STATIONAP;
        } else {
            *mode = WWM_STATION;
        }
    } else if(wf_mode == WIFI_MODE_MASTER) {
        *mode = WWM_SOFTAP;
    } else if(wf_mode == WIFI_MODE_MONITOR) {
        *mode = WWM_SNIFFER;
    } else {
        return -1;
    }


    return OPRT_OK;
}

OPERATE_RET tkl_wifi_start_ap(CONST WF_AP_CFG_IF_S *cfg)
{
    int ret = 0;
    
    if (cfg == NULL) {
        TKL_LOGE("invalid parm");
        return OPRT_INVALID_PARM;
    }
    TKL_LOGD("Start AP SSID:%s ,CHANNEL:%d", cfg->ssid,cfg->chan);

    WF_AP_CFG_IF_S tmp_cfg;

    wifi_enable(s_tkl_wifi.ap, 1);

    memcpy(&tmp_cfg, cfg, sizeof(WF_AP_CFG_IF_S));
    if (cfg->chan <= 0 || cfg->chan > 14) {
        TKL_LOGE("WIFI : Set a wrong channel out of range");
        return -1;
    } else if (cfg->ip.ip[0] == 0x0 || strlen(cfg->ip.ip) == 0) {
        TKL_LOGE("WIFI:Set a wrong ip address"); 	
        strcpy(tmp_cfg.ip.ip, WIFI_AP_DEFAULT_IP);
    }
    ret = wifi_ap_start(s_tkl_wifi.ap, (WF_AP_CFG_IF_S *)cfg);
    if (ret < 0){
        TKL_LOGE(" WIFI start ap failed");
        return OPRT_COM_ERROR;
    }

    memcpy(&s_tkl_wifi.apcfg, &tmp_cfg, sizeof(WF_AP_CFG_IF_S));

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_stop_ap(VOID_T)
{
    int ret = 0;

    ret = wifi_ap_stop();
    if (ret < 0) {
        TKL_LOGE("WIFI STOP softap failed");
    }

    wifi_enable(s_tkl_wifi.ap, 0);

    return 0 == ret ? OPRT_OK : OPRT_COM_ERROR;
}

OPERATE_RET tkl_wifi_get_connected_ap_info(FAST_WF_CONNECTED_AP_INFO_T **fast_ap_info)
{
    int ret = 0;

    if (!fast_ap_info) {
        TKL_LOGE("WiFi : illegal parameter");
        return OPRT_INVALID_PARM;
    }

    FAST_WF_CONNECTED_AP_INFO_T *ap_info_t = NULL;
    FAST_CONNECTED_AP_INFO_S    *ap_info_s;

    ap_info_t = (FAST_WF_CONNECTED_AP_INFO_T *)tkl_system_malloc(sizeof(FAST_WF_CONNECTED_AP_INFO_T) + sizeof(FAST_CONNECTED_AP_INFO_S));
    if(NULL == ap_info_t) {
        TKL_LOGE("WiFi : malloc memory failed");
        return -1;
    }

    ap_info_t->len = sizeof(FAST_CONNECTED_AP_INFO_S);
    ap_info_s = (FAST_CONNECTED_AP_INFO_S *)&(ap_info_t->data[0]);
    ret = wifi_get_channel(s_tkl_wifi.sta, &(ap_info_s->chan));
    if (ret < 0) {
        TKL_LOGE("WiFi : get current work channel failed\n");
        return -1;
    }
    ret = wifi_get_bssid(&(ap_info_s->bssid[0]));
    if(ret < 0) {
        TKL_LOGE("WiFi : get bssid failed\n");
        return -1;
    }

    ret = wifi_get_connected_ap_info((char*)ap_info_s->ssid, (char*)ap_info_s->passwd);
    if (ret < 0) {
        TKL_LOGE("TKL WIFI get connect ap info failed");
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_station_fast_connect(CONST FAST_WF_CONNECTED_AP_INFO_T *fast_ap_info)
{
    int ret = 0;
    FAST_CONNECTED_AP_INFO_S *ap_info_s = NULL;
    ap_info_s = (FAST_CONNECTED_AP_INFO_S *)&(fast_ap_info->data[0]);

    ret = wifi_station_connect((char*)ap_info_s->ssid, (char*)ap_info_s->passwd);
    if (ret < 0) {
        TKL_LOGE("WIFI fast connect failed");
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_station_connect(CONST SCHAR_T *ssid, CONST SCHAR_T *passwd)
{
    int ret = 0;

    s_tkl_wifi.sta_conn_flag = 1;
    TKL_LOGD("WiFi : connect to ap \n");

    ret = wifi_station_connect(ssid, passwd);
    if (ret < 0) {
        TKL_LOGE("WIFI Station connect ap failed");
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_station_disconnect(VOID_T)
{
    int ret = 0;

    s_tkl_wifi.sta_conn_flag = 0;

    ret = wifi_disconnect(s_tkl_wifi.sta);
    if (ret < 0) {
        TKL_LOGE("TKL WIFI sta disconnect failed");
        return OPRT_COM_ERROR;
    }

    wifi_enable(s_tkl_wifi.sta, 0);
    usleep(100);
    wifi_enable(s_tkl_wifi.sta, 1);


    return OPRT_OK;
}

OPERATE_RET tkl_wifi_station_get_conn_ap_rssi(SCHAR_T *rssi)
{
    int ret = 0;

    char *ifname = s_tkl_wifi.sta;

    ret = wifi_get_curr_rssi(ifname, (char*)rssi);
    if(ret < 0){
        TKL_LOGE("get connect ap's rssi failed");
        return -1;
    } else {
        TKL_LOGD("get connect ap's rssi : %d",*rssi);
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_get_bssid(UCHAR_T *mac)
{
    int ret = 0;

    ret = wifi_get_bssid(mac);
    if(ret < 0){
        TKL_LOGE("WIFI GET BSSID failed");
        return -1;
    } else {
        TKL_LOGD("WiFi Get AP BSSID %02x:%02x:%02x:%02x:%02x:%02x",
            mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    }

    return OPRT_OK;
}

static int is_valid_mac(unsigned char* mac)
{
    unsigned char cmp_mac[6] = {0}; 

    if (mac == NULL) {
        TKL_LOGE("invalid param\n"); 
        return 0;
    }
    if (mac[0] &0x1 ) {
        // TKL_LOGE("mac is multi broadcast mac\n");
        return 0;
    }
    memset(cmp_mac, 0x0, sizeof( cmp_mac ) );
    if (memcmp(cmp_mac, mac, 6) == 0) {
        // TKL_LOGE("mac is all zero,not valid\n");
        return 0;
    }
    memset(cmp_mac, 0xff, 6);
    if (memcpy(cmp_mac, mac, 6) == 0) {
        // TKL_LOGE("mac is all 0xff,not valid\n");
        return 0;
    }

    return 1;
}

OPERATE_RET tkl_wifi_station_get_status(WF_STATION_STAT_E *stat)
{
    int ret = 0;
    char *ifname = WLAN_DEV;    /* default: wlan0*/
    unsigned char mac[6] = {0x0};
    char ip[16] = {0x0};
    int f_get_ap_mac = 0, f_get_ip = 0, f_essid_on = 0;
    WIFI_MODE_E mode;

    wifi_get_mode(ifname, (int *)&mode);

    if(mode != WIFI_MODE_MANAGED) {
        TKL_LOGE("get wifi work status only in station mode");
        *stat = WSS_IDLE;
        return 0;
    }

    ret = wifi_get_bssid(mac);
    if(!ret && is_valid_mac(mac)) {
        f_get_ap_mac = 1;
    }

    ret = wifi_get_ip(ifname,ip);
    if(!ret && strlen(ip) != 0) {
        f_get_ip = 1;
    }

    f_essid_on = wifi_check_essid_on(ifname);

    if (f_get_ap_mac == 0 || f_essid_on == 0) {
        *stat = WSS_CONN_FAIL;
    }

    if (f_get_ap_mac == 1 && f_essid_on == 1 && f_get_ip == 0) {
        *stat = WSS_CONN_SUCCESS;
    }

    if (f_get_ap_mac == 1 && f_essid_on == 1 && f_get_ip == 1) {
        *stat = WSS_GOT_IP;
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_set_country_code(CONST COUNTRY_CODE_E ccode)
{
    int ret = 0;
    char *country = NULL;

    char *ifname = s_tkl_wifi.sta;

    if (ccode == COUNTRY_CODE_CN){
        country = "CN";
    } else if (ccode == COUNTRY_CODE_US){
        country = "US";
    } else if (ccode == COUNTRY_CODE_JP){
        country = "JP";
    } else if (ccode == COUNTRY_CODE_EU){
        country = "EU";
    } else {
        TKL_LOGE("invalid param:%d\n",ccode); 
        return -1;
    }

    wifi_enable(ifname, 0);
    ret = wifi_set_country(country);
    wifi_enable(ifname, 1);

    return OPRT_OK;
}

#pragma pack(1)

typedef struct {
	uint8_t it_version;	/* Version 0. Only increases
				 * for drastic changes,
				 * introduction of compatible
				 * new fields does not count.
				 */
	uint8_t it_pad;
	uint16_t it_len;	/* length of the whole
				 * header in bytes, including
				 * it_version, it_pad,
				 * it_len, and data fields.
				 */
	uint32_t it_present;	/* A bitmap telling which
				 * fields are present. Set bit 31
				 * (0x80000000) to extend the
				 * bitmap by another 32 bits.
				 * Additional extensions are made
				 * by setting bit 31.
				 */
} ieee80211_radiotap_header;

#pragma pack(1)


static void *sniffer_process(void *arg)
{
    int sock    = 0;
    struct ifreq ifr;

    char *wlan  = s_tkl_wifi.sta;     /* default wlan0 */

    uint8_t rev_buffer[MAX_REV_BUFFER];
    TKL_LOGD("Sniffer Thread Create\n");

    pthread_detach (pthread_self());


    if ((sock = wifi_create_rawsocket(ETH_P_ALL)) == -1) {
        TKL_LOGE("WIFI Create socket failed");
        return NULL;
    }

    if (wifi_bind_rawsocket(wlan, sock, ETH_P_ALL) == -1) {
        TKL_LOGE("WIFI Bind socket failed");
        return NULL;
    }

    while((s_tkl_wifi.snifffer_cb != NULL) && (s_tkl_wifi.snifffer_flag == 1)) {
        int rev_num = recvfrom(sock,rev_buffer,MAX_REV_BUFFER,0,NULL,NULL);
        ieee80211_radiotap_header *pHeader = (ieee80211_radiotap_header *)rev_buffer;
        uint16_t  skipLen = pHeader->it_len;

        if(rev_num > skipLen) {
            s_tkl_wifi.snifffer_cb(rev_buffer+skipLen, rev_num-skipLen, 0);
            // tuya_debug_hex_dump("sniffer", 128, rev_buffer, rev_num);
        }
    }

    s_tkl_wifi.snifffer_cb = NULL;
    close(sock);

    return (void *)0;
}


OPERATE_RET tkl_wifi_set_sniffer(CONST BOOL_T en, CONST SNIFFER_CALLBACK cb)
{
    int iret = 0;

    char *ifname = s_tkl_wifi.sta;

    if (en == TRUE) {
        TKL_LOGD("WIFI enable Sniffer");
        tkl_wifi_set_work_mode(WWM_SNIFFER);
        s_tkl_wifi.snifffer_cb = cb;
        s_tkl_wifi.snifffer_flag = true;
        pthread_create(&s_tkl_wifi.snifffer, NULL, sniffer_process, NULL);
    } else {
        TKL_LOGD("Disable Sniffer");
        s_tkl_wifi.snifffer_flag = false;
        pthread_join(s_tkl_wifi.snifffer, NULL);
        tkl_wifi_set_work_mode(WWM_STATION);
        // wifi_set_mode(ifname,"managed");     /* 重新设置为station mode */
    }

    return OPRT_OK;
}


OPERATE_RET tkl_wifi_send_mgnt(CONST UCHAR_T *buf, CONST UINT_T len)
{
    char *ifname = WLAN_AP;
    struct ifreq t_ifr;
    struct packet_mreq t_mr;
    struct sockaddr_ll t_sll;
    int socket_raw;

    socket_raw = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (socket_raw < 0) {
        TKL_LOGE("socket failed\n");
        return -1;
    }

    memset(&t_ifr, 0, sizeof(t_ifr));
    strncpy(t_ifr.ifr_name, ifname, sizeof(t_ifr.ifr_name) - 1);
    if (ioctl(socket_raw, SIOCGIFINDEX, &t_ifr) < 0) {
        TKL_LOGE("ioctl failed\n");
        close(socket_raw);
        return -1;
    }

    memset(&t_sll, 0, sizeof(t_sll));
    t_sll.sll_family = AF_PACKET;
    t_sll.sll_ifindex = t_ifr.ifr_ifindex;
    t_sll.sll_protocol = htons(ETH_P_ALL);
    if (bind(socket_raw, (struct sockaddr*)&t_sll, sizeof(t_sll)) < 0) {
        perror("bind socket failed\n");
        close(socket_raw);
        return -1;
    }

    memset(&t_mr, 0, sizeof(t_mr));
    t_mr.mr_ifindex = t_sll.sll_ifindex;
    t_mr.mr_type = PACKET_MR_PROMISC;
    if (setsockopt(socket_raw, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &t_mr, sizeof(t_mr)) < 0) {
        perror("setsockopt failed\n");
        close(socket_raw);
        return -1;
    }

    //int t_size = write(socket_raw, tmp_buf, len + strlen(t_radiotap));
    int t_size = wifi_send_rawpacket(socket_raw,(unsigned char *)buf,len);
    if (t_size < 0) {
        TKL_LOGE("write() failed : seng packet failed\n");
        close(socket_raw);
        return -1;
    }

    close(socket_raw);

    return OPRT_OK;
}

static void *mgnt_process(void *arg)
{
    int sockraw;
    int ret = 0;
    unsigned char pkt[8192] = {0};
    char *if_name = WLAN_AP;

    if ((sockraw = wifi_create_rawsocket(ETH_P_ALL)) == -1) {
        TKL_LOGE("WIFI Create socket failed ret:%d",sockraw);
        return NULL;
    }

    if ((ret = wifi_bind_rawsocket(if_name, sockraw, ETH_P_ALL)) == -1) {
        TKL_LOGE("WIFI Bind socket failed ret:%d",ret);
        return NULL;
    }

    while (s_tkl_wifi.mgnt_flag) {
        if ((ret = wifi_recv_rawpacket(sockraw, pkt, sizeof(pkt))) > -1) {
            ieee80211_radiotap_header *pHeader = (ieee80211_radiotap_header *)pkt;
            uint16_t  skipLen = pHeader->it_len;
            if(ret > skipLen) {
                 s_tkl_wifi.mgnt_cb(pkt + skipLen, ret);
                // tuya_debug_hex_dump("sniffer", 128, rev_buffer, rev_num);
            }
        } else {
            TKL_LOGD("WIFI recv data failed");
        }
    }

    return NULL;
}

OPERATE_RET tkl_wifi_register_recv_mgnt_callback(CONST BOOL_T enable, CONST WIFI_REV_MGNT_CB recv_cb)
{
    //!: TODO: 一般接收管理帧方式，通常需要在wifi连着路由器的情况，此时需要额外的一张网卡来完成
    char *ifname  = s_tkl_wifi.sta;
    char *ifname2 = s_tkl_wifi.ap;

    if (enable == TRUE){
        TKL_LOGD("Enable WIFI recv mgnt frame");
        unsigned char channel;
        if (0 == wifi_get_channel(ifname, &channel)) {
            wifi_enable(ifname2, 1);
            wifi_set_channel(ifname2, channel);
        }
        wifi_set_mode(ifname2, "Monitor");
        s_tkl_wifi.mgnt_cb   = recv_cb;
        s_tkl_wifi.mgnt_flag = 1;
        pthread_create(&s_tkl_wifi.mgnt, NULL, mgnt_process, NULL);
    } else {
        TKL_LOGD("Disable recv frame");
        s_tkl_wifi.mgnt_flag = 0;
        pthread_join(s_tkl_wifi.mgnt, NULL);
        s_tkl_wifi.mgnt_cb = NULL;
    }

    return OPRT_OK;
}

OPERATE_RET tkl_wifi_set_rf_calibrated(VOID_T)
{
    TKL_LOGD("set rf calibrated is not support");
    return OPRT_COM_ERROR;
}

OPERATE_RET tkl_wifi_set_lp_mode(CONST BOOL_T enable, CONST UCHAR_T dtim)
{
    int iret = 0;

    TKL_LOGD("set lp mode is not support\n");

    return OPRT_COM_ERROR;
}

OPERATE_RET tkl_wifi_ioctl(WF_IOCTL_CMD_E cmd,  VOID *args)
{
    return OPRT_NOT_FOUND;
}

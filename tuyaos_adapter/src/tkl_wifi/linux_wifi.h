#ifndef __LINUX_WIFI_H__
#define __LINUX_WIFI_H__

#include "tkl_wifi.h"
#include "tkl_wifi_log.h"

#define MAX_REV_BUFFER      (512)

#ifndef WIFI_DB_PATH
#define WIFI_DB_PATH    "./tuyadb"
#endif

#ifndef WIFI_AP_DEFAULT_IP
#define WIFI_AP_DEFAULT_IP  "192.168.31.1"
#endif

#define MAX_AP_SEARCH       (64)
#define MAX_REV_BUFFER      (512)
#define MAX_SSID_LEN        (32)
#define MAX_IP_LEN          (16)
#define MAX_IFNAME_SZ       (16)


#define UDHCPC                  "udhcpc"
#define WPA_SUPPLICANT          "wpa_supplicant"
#define WPA_CTRL_DIR            "/var/run"
#define WPA_CTRL_INTERFACE      "/var/run/wpa_supplicant"

#define WIFI_STATION_CONF       ""WIFI_DB_PATH"/wpa_0_8.conf"
#define WIFI_HOSTAPD_CONF       ""WIFI_DB_PATH"/hostapd.conf"
#define WIFI_DHCPD_CONF         ""WIFI_DB_PATH"/udhcpd.conf"

#define CREATE_UDHCPD_DIR       "mkdir -p /var/lib/misc"
#define CREATE_UDHCPD_CFG_FILE  "touch /var/lib/misc/udhcpd.leases"


/* wifi work mode */
typedef enum {
    WIFI_MODE_AUTO = 0,
    WIFI_MODE_AD_HOC,
    WIFI_MODE_MANAGED,
    WIFI_MODE_MASTER,
    WIFI_MODE_REPEATER,
    WIFI_MODE_SECONDARY,
    WIFI_MODE_MONITOR,
    WIFI_MODE_NR,
} WIFI_MODE_E;

#pragma pack(1)
typedef struct {
    UCHAR_T ssid[WIFI_SSID_LEN+1];
    UCHAR_T passwd[WIFI_PASSWD_LEN+1];
    UCHAR_T bssid[6];
    UINT_T security;
    UCHAR_T chan; 
    UCHAR_T psk_set;
} FAST_CONNECTED_AP_INFO_S;

typedef struct wlan_80211_frame {
    unsigned char frmae_ctl1;
    unsigned char frmae_ctl2;
    unsigned short duration;
    unsigned char addr1[6];
    unsigned char addr2[6];
    unsigned char addr3[6];
    unsigned short seq_ctl;
    unsigned char addr4[6];
} WLAN_80211_HEAD;
#pragma pack()

typedef struct IP_UDHCPD {
    char ifname[MAX_IFNAME_SZ];
    char ip_start[MAX_IP_LEN];
    char ip_end[MAX_IP_LEN];
    char ip_route[MAX_IP_LEN];
    char ip_mask[MAX_IP_LEN];
    char ip_gw[MAX_IP_LEN];
} UDHCPD_IP_T;


int wifi_enable(char *ifname , char enable);

int wifi_get_ip(char *ifname, char *ip);
int wifi_get_ip_mask(char *ifname, char *mask);


int wifi_get_channel(char *ifname, unsigned char *channel);
int wifi_set_channel(char *ifname, const char channel);

int wifi_scan_ap(char *ifname , AP_IF_S *aps , int max_ap);

int wifi_get_mac(char *ifname, char *mac);
int wifi_set_mac(const char *ifname, const char *mac);


int wifi_set_mode(char *ifname , char *mode);
int wifi_get_mode(char *ifname , int *mode);

int wifi_get_curr_rssi(char *ifname, char *rssi);


int wifi_station_connect(const char *ssid, const char *passwd);
int wifi_disconnect(char *ifname);



int wifi_ap_start(char *ifname, WF_AP_CFG_IF_S *cfg);
int wifi_ap_stop(void);


int wifi_get_connected_ap_info(char *ssid, char *passwd);
int wifi_get_bssid(unsigned char *bssid);

int wifi_check_essid_on(char *ifname);


int wifi_create_rawsocket(int protocol_to_sniff);
int wifi_bind_rawsocket(char *device, int rawsock, int protocol);
int wifi_send_rawpacket(int rawsock, unsigned char *pkt, size_t pkt_len);
int wifi_recv_rawpacket(int rawsock, unsigned char *pkt, int pkt_len);




#endif
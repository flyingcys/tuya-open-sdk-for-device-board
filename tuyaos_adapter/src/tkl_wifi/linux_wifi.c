
#include "linux_wifi.h"
#include "iwlib.h"
#include "wpa_command.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <netpacket/packet.h>

#ifndef WIFI_SSID_MAX_LEN
#define WIFI_SSID_MAX_LEN 32
#endif

#ifndef UDHCPC_SCRIPT_PATH
#define UDHCPC_SCRIPT_PATH      "/usr/share/udhcpc/default.script"
#endif

int wifi_enable(char *ifname , char enable)
{
    int fd = 0;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        TKL_LOGE("create socket err : %s", strerror(errno));
        return -1;
    }
    strcpy(ifr.ifr_name, ifname);
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) <0 ) {
        TKL_LOGE("socket ioctl err:%s", strerror(errno));
        close(fd);
        return -1;
    }
    if (enable) {
        ifr.ifr_flags |= IFF_UP;
    } else {
        ifr.ifr_flags &= ~IFF_UP;
    }
    if (ioctl(fd, SIOCSIFFLAGS, &ifr) <0 ) {
        TKL_LOGE("socket ioctl err : %s", strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);

    return 0;
}

int wifi_get_ip(char *ifname, char *ip)
{
    int fd = 0;
    struct ifreq ifr;
    struct sockaddr_in *sin;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        TKL_LOGE("create socket err : %s",strerror(errno));
        return -1;
    }
    strcpy(ifr.ifr_name, ifname);
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) <0 ) {
        TKL_LOGE("socket ioctl err : %s",strerror(errno));
        close(fd);
        return -1;
    }
    if (!(ifr.ifr_flags & IFF_RUNNING)) {
        close(fd);
        return -1;
    }
    /* get ip */
    strcpy(ifr.ifr_name, ifname);
    if (ioctl(fd, SIOCGIFADDR, &ifr) <0 ) {
        TKL_LOGE("socket ioctl err : %s",strerror(errno));
        close(fd);
        return -1;
    }
    sin = (struct sockaddr_in *)&(ifr.ifr_addr);
    strcpy(ip, inet_ntoa(sin->sin_addr));
    close(fd);

    return 0;
}

int wifi_get_ip_mask(char *ifname, char *mask)
{
    int fd = 0;
    struct ifreq ifr;
    struct sockaddr_in *sin;

    if (mask == NULL) {
        TKL_LOGE("invalid param");
        return -1;
    }
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        TKL_LOGE("create socket err : %s",strerror(errno));
        return -1;
    }
    /* get mask*/
    memset(&ifr,0,sizeof(ifr));
    strcpy(ifr.ifr_name, ifname);
    if (ioctl(fd, SIOCGIFNETMASK, &ifr) < 0) {
        TKL_LOGE("ioctl error\n");
        close(fd);
    }
    sin = (struct sockaddr_in *)&(ifr.ifr_addr);
    strncpy(mask, inet_ntoa(sin->sin_addr),MAX_IP_LEN);
    close(fd);

    return 0;
}


int wifi_set_ip(const char *ifname, const char *ip)
{
    int fd = 0;
    struct ifreq ifr;
    struct sockaddr_in *addr;

    memset(&ifr, 0, sizeof(struct ifreq));
    strcpy(ifr.ifr_name, ifname);
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        TKL_LOGE("socket fail");
        return -1;
    }
    addr = (struct sockaddr_in *)&(ifr.ifr_addr);
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = inet_addr(ip);
    if (ioctl(fd,SIOCSIFADDR, &ifr) < 0) {
        TKL_LOGE("ioctl fail");
        return -1;
    }
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (ioctl(fd,SIOCSIFFLAGS, &ifr) < 0) {
        TKL_LOGE("ioctl fail:%s",strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);

    return 0;
}

int wifi_get_mac(char *ifname, char *mac)
{
    int fd = -1;
    struct ifreq ifr;

    bzero(&ifr, sizeof(struct ifreq));
    if (mac == NULL) {
        TKL_LOGE("invalid parm");
        return -1;
    }
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        TKL_LOGE("get %s mac address socket creat error", ifname);
        return -1;
    }
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        TKL_LOGE("get %s mac address error", ifname);
        close(fd);
        return -1;
    }
    memcpy(mac,(unsigned char*)ifr.ifr_hwaddr.sa_data,6);
    close(fd);

    return 0;
}

int wifi_set_mac(const char *ifname, const char *mac)
{
    int fd = -1;
    struct ifreq ifr;

    if (!ifname || !mac) {
        TKL_LOGE("invalid parm");
        return -1;
    }
    bzero(&ifr, sizeof(struct ifreq));
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        TKL_LOGE("get %s mac address socket creat error", ifname);
        return -1;
    }
    ifr.ifr_addr.sa_family = ARPHRD_ETHER;
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
    memcpy((unsigned char *)ifr.ifr_hwaddr.sa_data, mac, 6);
    if (ioctl(fd, SIOCSIFHWADDR, &ifr) < 0) {
        TKL_LOGE("set %s mac address error", ifname);
        close(fd);
        return -1;
    }
    close(fd);

    return 0;
}


int wifi_set_channel(char *ifname, const char channel)
{
    int ret;
    int fd;
    struct iw_range range;
    int has_range;
    struct iwreq    wrq;
    double  freq;

    if ((fd = iw_sockets_open()) < 0) {
        TKL_LOGE("socket fail");
        return -1;
    }

    has_range = (iw_get_range_info(fd, ifname, &range) >= 0);
    if ((!has_range) || (range.we_version_compiled < 14)) {
        TKL_LOGE( "%-8.16s  Interface doesn't support channel setting.",ifname);
        iw_sockets_close(fd);
        return(-1);
    }
    ret = iw_channel_to_freq(channel, &freq , &range);
    if (ret != channel) {
        TKL_LOGE("change_to_freq fail:%d",ret);
        return -1;
    }
    iw_float2freq(freq, &(wrq.u.freq));
    wrq.u.freq.flags = IW_FREQ_FIXED;
    ret = iw_set_ext(fd, ifname, SIOCSIWFREQ, &wrq);
    if (ret < 0) {
        TKL_LOGE("socket ioctl err : %s",strerror(errno));
        return -1;
    }
    iw_sockets_close(fd);

    return ret;
}

int wifi_get_channel(char *ifname, unsigned char *channel)
{
    int fd;
    struct iw_range range;
    int has_range;
    struct iwreq wrq;
    int ret = 0;
    double freq;

    if ((fd = iw_sockets_open()) < 0) {
        TKL_LOGE("socket fail");
        return -1;
    }
    has_range = (iw_get_range_info(fd, ifname, &range) >= 0);
    if ((!has_range) || (range.we_version_compiled < 14)) {
        TKL_LOGE( "%-8.16s  Interface doesn't support channel get.",ifname);
        iw_sockets_close(fd);
        return(-1);
    }
    ret = iw_get_ext(fd, ifname, SIOCGIWFREQ, &wrq);
    if (ret  < 0) {
        TKL_LOGE("socket ioctl err : %s",strerror(errno));
        iw_sockets_close(fd);
        return -1;
    } else {
        freq = iw_freq2float(&wrq.u.freq);
        *channel = iw_freq_to_channel(freq, &range);
    }
    iw_sockets_close(fd);

    return ret;
}

/*  mode:
 *    0 --- Auto
 *    1 --- Ad-Hoc
 *    2 --- Managed
 *    3 --- Master
 *    4 --- Repeater
 *    5 --- Secondary
 *    6 --- Monitor
*/
int wifi_get_mode(char *ifname , int *mode)
{
    int fd = 0;
    struct iwreq wrq;

    fd = iw_sockets_open();
    if (fd < 0) {
        TKL_LOGE("create socket err : %s",strerror(errno));
        return -1;
    }
    if (iw_get_ext(fd, ifname, SIOCGIWMODE, &wrq) < 0) {
        TKL_LOGE("socket ioctl err : %s",strerror(errno));
        iw_sockets_close(fd);
        return -1;
    }
    *mode = wrq.u.mode;
    iw_sockets_close(fd);

    return 0;
}

int wifi_set_mode(char *ifname , char *mode)
{
    int k;
    int fd = 0;
    struct iwreq wrq;
    int NUM_OPER_MODE = 7;

    for (k = 0; k < NUM_OPER_MODE; k++) {
        if (strncasecmp(mode, iw_operation_mode[k], 3) == 0) 
            break;
    }
    if (k >= NUM_OPER_MODE || k == 1 || k == 4 || k == 5) {
        TKL_LOGE("[%s:%d] set invalid mode : %s",__FUNCTION__,__LINE__,mode);
        return -1;
    }
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        TKL_LOGE("create socket err : %s",strerror(errno));
        return -1;
    }
    strcpy(wrq.ifr_name, ifname);
    wrq.u.mode = k;
    ioctl(fd, SIOCSIWMODE, &wrq);
    // if (ioctl(fd, SIOCSIWMODE, &wrq) < 0) {
    //     TKL_LOGE("socket ioctl err : %s",strerror(errno));
    //     close(fd);
    //     return -1;
    // }
    close(fd);

    return 0;
}


//! TODO: killall
static void process_kill(char *proc)
{
    char cmd_buf[128] = {0};

    snprintf(cmd_buf,sizeof(cmd_buf),"killall -9 '%s' ",proc);
    system(cmd_buf);

    // /* 获取到指定process的PID */
    // snprintf(cmd_buf, sizeof(cmd_buf),"ps | grep '%s' | grep -v grep | awk '{print $2}' | xargs kill",proc);
    // system(cmd_buf);
}

static int process_alive(char *process)
{
    int ret = 0;
    FILE* fp;
    int count;
    char buf[100];
    char command[150];

    snprintf(command, sizeof(command), "ps -ef | grep %s | grep -v grep | wc -l", process);
    if ((fp = popen(command,"r")) == NULL) {
        TKL_LOGE("popen failed");
        return -1;
    }
    if ((fgets(buf,100,fp)) != NULL) {
        count = atoi(buf);
        if(count == 0){
            ret = 0;
        } else if(count == 1) {
            ret = 1;
        } else{
            TKL_LOGE("there are too many wpa_supplicant process");
            ret = -1;
        }
    }

    pclose(fp);

    return ret;
}


int wifi_scan_ap(char *ifname , AP_IF_S *aps , int max_ap)
{
    int ret;
    int fd;
    struct iw_range   range;
    int has_range;
    wireless_scan_head ctx;
    char      buffer[128];
    int count = 0;
    wireless_scan *node;
    struct wireless_config *pconf;

    if ((fd = iw_sockets_open()) < 0) {
        perror("socket fail\n");
        return 0;
    }

    /* Get range stuff */
    has_range = (iw_get_range_info(fd, ifname, &range) >= 0);
    /* Check if the interface could support scanning. */
    if((!has_range) || (range.we_version_compiled < 14)) {
        TKL_LOGE( "%-8.16s  Interface doesn't support scanning.",ifname);
        iw_sockets_close(fd);
        return 0;
    }

    memset(&ctx,0 ,sizeof(wireless_scan_head));
    ret = iw_scan(fd,ifname,range.we_version_compiled , &ctx);
    if(ret < 0){
        TKL_LOGE("iw scan error:%d",ret);
        iw_sockets_close(fd);
        return 0;
    }

    node = ctx.result;

    while(node != NULL) {
        int length;
        AP_IF_S *tmp; 
        double freq; 
        int x0,x1,x2,x3,x4,x5;

        if(node->has_ap_addr == 0)
            continue;

        if(count >= max_ap)
            break;

        tmp = &aps[count++];
        pconf = &(node->b);
        //printf("essid len is %d ,ESSID : %s\n",pconf->essid_len,pconf->essid);
        length = strlen(pconf->essid);
        tmp->s_len = length > WIFI_SSID_MAX_LEN ? WIFI_SSID_MAX_LEN:length;
        memcpy(tmp->ssid , pconf->essid, tmp->s_len);
        //printf("len is %d ,SSID : %s\n",tmp->s_len,tmp->ssid);
        freq = pconf->freq;    /* Frequency/channel */
        tmp->channel = iw_freq_to_channel( freq, &range);

        memset(buffer,0,sizeof(buffer));
        iw_sawap_ntop(&node->ap_addr, buffer);
        sscanf(buffer,"%02x:%02x:%02x:%02x:%02x:%02x\n",&x0,&x1,&x2,&x3,&x4,&x5);
        tmp->bssid[0] = 0xff & x0;
        tmp->bssid[1] = 0xff & x1;
        tmp->bssid[2] = 0xff & x2;
        tmp->bssid[3] = 0xff & x3;
        tmp->bssid[4] = 0xff & x4;
        tmp->bssid[5] = 0xff & x5;
        if(node->has_stats)
            tmp->rssi = node->stats.qual.qual;
            //tmp->rssi = node->stats.qual.qual - 100;  /* rssi 转换 */
        //TKL_LOGD("SSID : %s key : %s key_size : %d key_flags : %d",pconf->essid,pconf->key,pconf->key_size,pconf->key_flags);

        if(pconf->key_flags & 0x8000) {     /* 判断是否加密 */
            tmp->security = 0;  // open
        } else {
            if(pconf->key_flags & 0x2000) {
                tmp->security = 0;  // open
            } else if(pconf->key_flags & 0x800){
                tmp->security = 4;
            }
        }
        node = node->next;
    }

    node = ctx.result;
    while(node != NULL) {
        wireless_scan *tmp = node;
        node = node->next;
        free(tmp);
    }

    iw_sockets_close(fd);

    return count;
}


static int wpa_config_prepare(const char *ssid, const char *psk)
{
    int fd = 0;
    int ret = 0;
    char buf[1024] = {0};

    unlink(WIFI_STATION_CONF);

    fd = open(WIFI_STATION_CONF, O_RDWR | O_CREAT, 0x666);
    if (fd < 0) {
        TKL_LOGE("create configure error %d\n", errno);
        return -1;
    }

    if (psk == NULL || 0 == strlen(psk)) {
        sprintf(buf,"ctrl_interface=%s\n"
            "update_config=1\n"
            "network={\n"
            "ssid=\"%s\"\n"
            "key_mgmt=NONE\n"
            "}\n", WPA_CTRL_INTERFACE, ssid);
    } else {
        sprintf(buf,"ctrl_interface=%s\n"
            "update_config=1\n"
            "network={\n"
            "key_mgmt=WPA-PSK\n"
            "ssid=\"%s\"\n"
            "psk=\"%s\"\n"
            "}\n", WPA_CTRL_INTERFACE, ssid, psk);
    }

    ret = write(fd, buf, strlen(buf));
    if (ret < 0) {
        TKL_LOGE("write hostapd buf errno %d\n", errno);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static void start_wpa(void)
{
    char *ifname = WLAN_DEV;
    char cmd_buf[128] = {0x0};
    DIR *dir = NULL;

    /* 确保控制接口/va/run是存在的,若不存在则创建该目录 */
    dir = opendir(WPA_CTRL_DIR);
    if (dir == NULL) {
        if (mkdir(WPA_CTRL_DIR, 0666) < 0) {        /* wpa_supplicant ctrl dir mode 0666 */
            TKL_LOGE("WiFi: Create '%s' fail", WPA_CTRL_DIR);
        }
    } else {
        closedir(dir);
    }

    snprintf(cmd_buf,sizeof(cmd_buf),WPA_SUPPLICANT" -i %s -Dnl80211 -c %s -B",ifname,WIFI_STATION_CONF);
    system(cmd_buf);
}

static int get_passwd_and_ssid_from_wpa_file(char *ssid, char *passwd)
{
    FILE *fp = NULL;
    char str[1024] = {0x0};
    char *opstr = NULL;
    char buffer[1024] = {0x0};
    char tmp_ssid[32 + 1] = {0x0};
    char tmp_pwd[64+1] = {0x0};

    fp = fopen(WIFI_STATION_CONF, "r");
    if(fp == NULL) {
        TKL_LOGE("WiFi : can'r open %s",WIFI_STATION_CONF);
        return -1;
    }

    while ((fgets(buffer,sizeof(buffer),fp)) != NULL) {
        strcat(str,buffer);
    }
    opstr = strstr(str, "ssid=");
    if (opstr) {
        sscanf(opstr + strlen("ssid="),"%s",tmp_ssid);
    } else {
        TKL_LOGE("get ssid failed");
        fclose(fp);
        return -1;
    }

    opstr = strstr(str, "psk=");
    if (opstr) {
        sscanf(opstr + strlen("psk="),"%s",tmp_pwd);
    } else {
        TKL_LOGE("get psk failed");
        fclose(fp);
        return -1;
    }

    strncpy(ssid,tmp_ssid,32);
    strncpy(passwd,tmp_pwd,64);

    fclose(fp);

    return 0;
}

static void start_udhcpc(void)
{
    char *ifname = WLAN_DEV;
    char cmd_buf[128] = {0x0};

    system("killall "UDHCPC"");
    snprintf(cmd_buf,sizeof(cmd_buf),""UDHCPC" -i %s -R &",ifname);
    system(cmd_buf);
}

int wifi_get_curr_rssi(char *ifname, char *rssi)
{
    int ret;
    char tmp;

    ret = wifi_get_signal(&tmp);
    if(ret) {
        TKL_LOGE("get rssi fail : %s",strerror(errno));
        return -1;
    }

    if (rssi) {
        *rssi = (char)tmp;
    } else {
        return -1;
    }

    return 0;
}


int wifi_check_essid_on(char *ifname)
{
    int fd;
    int ret;
    struct wireless_config conf;

    if ((fd = iw_sockets_open()) < 0) {
        perror("socket fail\n");
        return -1;
    }

    ret = iw_get_basic_config(fd, ifname, &conf);
    if(ret < 0 ) {
        TKL_LOGE("%-8.16s  Interface doesn't support iw_get_basic_config.",ifname);
        iw_sockets_close(fd);
        return(-1);
    }
    iw_sockets_close(fd);

    if (conf.essid_on && conf.has_essid == 1) {
        ret = 1;
    }

    return ret;
}


int wifi_get_connected_ap_info(char *ssid, char *passwd)
{
    int ret = 0;
    WF_CONNECTED_AP_INFO_S ap_info_s;
    memset(&ap_info_s,0,sizeof(WF_CONNECTED_AP_INFO_S));

    ret = wifi_get_ap_info(&ap_info_s);
    if (ret < 0) {
        TKL_LOGE("WiFi : Get conected ap info faied");
        return -1;
    } else {
        strcpy(ssid, (char*)ap_info_s.ssid);
        strcpy(passwd, (char*)ap_info_s.passwd);
    }

    if (strlen((char*)ap_info_s.ssid) == 0 || strlen((char*)ap_info_s.passwd) == 0) {
        ret = get_passwd_and_ssid_from_wpa_file(ssid,passwd);
        if (ret < 0) {
            TKL_LOGE("get ssid & passwd failed\n");
            return -1;
        }
    }

    return 0;
}

int wifi_get_bssid(unsigned char *bssid)
{
    int iret;
    iret = wifi_get_current_connect_ap_bssid((char*)bssid);
    if(iret < 0){
        // TKL_LOGE("wifi get bssid failed");
        return -1;
    }

    return 0;
}

int wifi_station_connect(const char *ssid, const char *passwd)
{
    int ret = 0;
    struct wifi_network net;

    memset(&net, 0, sizeof(struct wifi_network));
    strcpy(net.ssid, ssid);
    if (passwd == NULL || 0 == strlen(passwd)) {
        net.key_mgmt = WIFI_KEY_NONE;
    } else {
        strcpy(net.psk, passwd);
        net.key_mgmt = WIFI_KEY_WPA2PSK;    //default key_mgnt
    }

    ret = process_alive(WPA_SUPPLICANT);   /* 判断wpa_supplicant进程是否起来了 */
    if (ret == 0) {     /* wpa_supplicant 进程没有起来,手动启wpa_supplicant进程进行连接 */
        if (wpa_config_prepare(ssid, passwd)) {
            TKL_LOGE("WiFi : configure wpa_supplicant config file failed");
            return -1;
        }
        start_wpa();    /* 启动wpa_supplicant */
        usleep(500);
        start_udhcpc();
    } else if(ret == 1) {  /* wpa_supplicant 已启动,则调用相关的接口进行连接 */
        ret = wifi_join_network(&net); /* connect to network */
        if (ret < 0) {
            TKL_LOGE("connect network:%s failed",ssid);
            return -1;
        }
        if (process_alive(UDHCPC) == 0) { /* 重连时判断udhcpc是否开启，若未启动，则手动启动 */
            usleep(666);
            start_udhcpc();
        }
    } else {
        TKL_LOGE("wpa_suppplicant error");
        return -1;
    }

    return 0;
}


int wifi_disconnect(char *ifname)
{
    int ret = 0;

    ret = wifi_disconnect_network();
    if (ret < 0) {
        TKL_LOGE("station disconnect failed");
        return -1;
    }
    // ???? need  when change to ap or monitor wpa_supplicant still exist?
    process_kill(UDHCPC);
    usleep(200);

    return 0;
}

static int hostapd_conf_prepare(const WF_AP_CFG_IF_S *cfg)
{
    int     fd = 0;
    char    buf[1024] = {0};
    int     ret = 0;
    char   *ifname = WLAN_AP;
    int     maximum = 3;        /* max sta connect nums default:3 */
    int     interval = 100;     /* beacon intervall default:100 */
    int     hidden_ssid = 0;    /* default not hidden ssid */

    if (cfg->ssid_hidden) {
        hidden_ssid = cfg->ssid_hidden;
    }

    if (cfg->max_conn != 0) {
        maximum = cfg->max_conn;
    }

    if (cfg->ms_interval != 0) {
        interval = cfg->ms_interval;
    }

    if (cfg->md == WAAM_OPEN) {
        sprintf(buf,"interface=%s\n"
            "ctrl_interface_group=0\n"
            "#driver=nl80211\n"
            "ieee80211n=1\n"
            "ssid=%s\n"
            "hw_mode=g\n"
            "channel=%d\n"
            "max_num_sta=%d\n"
            "macaddr_acl=0\n"
            "#auth_algs=0\n"
            "beacon_int=%d\n"
            "ignore_broadcast_ssid=%d\n",
            ifname, cfg->ssid, cfg->chan, maximum, interval,hidden_ssid);
    } else if(cfg->md == WAAM_WEP){     /* 不建议使用，安全性太低 */
        sprintf(buf,"interface=%s\n"
            "ctrl_interface_group=0\n"
            "#driver=nl80211\n"
            "ieee80211n=1\n"
            "ssid=%s\nhw_mode=g\n"
            "channel=%d\n"
            "max_num_sta=%d\n"
            "macaddr_acl=0\n"
            "#auth_algs=1\n"
            "ignore_broadcast_ssid=%d\n"
            "wep_default_key=0\n"
            "wep_key0=1234567890\n"
            "wep_key1=\"vwxyz\"\n"
            "wep_key2=0102030405060708090a0b0c0d\n"
            "wep_key3=\".2.4.6.8.0.23\"\n"
            "wep_key_len_broadcast=13\n"
            "wep_key_len_unicast=13\n"
            "wep_rekey_period=300",
        ifname, cfg->ssid, cfg->chan, maximum, hidden_ssid);
    } else if(cfg->md == WAAM_WPA_PSK) {
        sprintf(buf,"interface=%s\n"
            "ctrl_interface_group=0\n"
            "#driver=nl80211\n"
            "ieee80211n=1\n"
            "ssid=%s\n"
            "hw_mode=g\n"
            "channel=%d\n"
            "max_num_sta=%d\n"
            "macaddr_acl=0\n"
            "auth_algs=1\n"
            "ignore_broadcast_ssid=%d\n"
            "wpa=1\n"
            "wpa_passphrase=%s\n"
            "wpa_key_mgmt=WPA-PSK\n"
            "#wpa_pairwise=TKIP CCMP\n"
            "rsn_pairwise=TKIP CCMP\n", 
            ifname, cfg->ssid, cfg->chan, maximum, hidden_ssid, cfg->passwd);
    } else if(cfg->md == WAAM_WPA2_PSK || cfg->md == WAAM_WPA_WPA2_PSK) {
        sprintf(buf,"interface=%s\n"
            "ctrl_interface_group=0\n"
            "#driver=nl80211\n"
            "ieee80211n=1\n"
            "ssid=%s\n"
            "hw_mode=g\n"
            "channel=%d\n"
            "max_num_sta=%d\n"
            "macaddr_acl=0\n"
            "auth_algs=1\n"
            "ignore_broadcast_ssid=%d\n"
            "wpa=3\n"
            "wpa_passphrase=%s\n"
            "wpa_key_mgmt=WPA-PSK\n"
            "wpa_pairwise=TKIP CCMP\n"
            "rsn_pairwise=TKIP CCMP\n",
            ifname, cfg->ssid, cfg->chan, maximum, hidden_ssid, cfg->passwd);
    } else if(cfg->md == WAAM_WPA_WPA3_SAE){     /* 暂不支持 */
        TKL_LOGD("WIFI doesn't support WPA3");
    } else {
        TKL_LOGE("Wrong key_mgnt");
        return -1;
    }

    unlink(WIFI_HOSTAPD_CONF);
    fd = open(WIFI_HOSTAPD_CONF, O_RDWR | O_CREAT, 0x666);
    if (fd < 0) {
        TKL_LOGE("create configure error %d\n", errno);
        return -1;
    }
    ret = write(fd, buf, strlen(buf));
    if(ret < 0) {
        TKL_LOGE("write hostapd.conf failed");
        close(fd);
        return -1;
    }
    close(fd);

    return 0;
}


static int udhcpd_conf_prepare(UDHCPD_IP_T udhcpd_ip_t)
{
    int fd = 0;
    char buf[256] = {0x0};
    int leases = 28800;       /* default value*/
    int ret = 0;

    unlink(WIFI_DHCPD_CONF);
    fd = open(WIFI_DHCPD_CONF,O_RDWR | O_CREAT, 0x666);
    if (fd < 0) {
        TKL_LOGE("create configure error %d\n", errno);
        return -1;
    }
    snprintf(buf,sizeof(buf), 
        "interface\t%s\nstart\t%s\nend\t%s\nopt subnet\t%s\nopt lease %d\nopt router\t%s\nopt dns %s\nopt domain Realtek\n",
        udhcpd_ip_t.ifname,udhcpd_ip_t.ip_start,udhcpd_ip_t.ip_end,udhcpd_ip_t.ip_mask,leases,udhcpd_ip_t.ip_route,udhcpd_ip_t.ip_route);

    ret = write(fd, buf, strlen(buf) + 1);
    if (ret < 0) {
        TKL_LOGE("write hostapd buf errno %d\n", errno);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int ip_prepare(char * src,char * dst)
{
    int i,dot = 0;
    char tmp_ip[MAX_IP_LEN] = {0x0};

    snprintf(tmp_ip, sizeof(tmp_ip), "%s",src);
    for(i = 0; i < strlen(tmp_ip); i++) {
        if(tmp_ip[i] == '.')
            dot++;
        if(dot == 3) {
            tmp_ip[i + 1] = 0;
            break;
        }
    }

    if(dot != 3) {
        TKL_LOGE("WIFI ipaddr(%s) is not legal",src);
        return -1;
    }

    strcpy(dst,tmp_ip);

    return 0;
}

int wifi_ap_start(char *ifname, WF_AP_CFG_IF_S *cfg)
{
    int  ret = 0;
    char cmd[128] = {0x0};
    char ipaddr_pre[13] = {0x0};
    UDHCPD_IP_T udhcpd_ip_t = {{0}};

    ret = hostapd_conf_prepare(cfg);
    if(ret < 0) {
        TKL_LOGE("WIFI prepare hostapd.conf failed");
        return -1;
    }
    snprintf(cmd,sizeof(cmd),"hostapd %s &", WIFI_HOSTAPD_CONF);
    system(cmd);
    ret = wifi_set_ip(ifname, cfg->ip.ip);
    if (ret < 0) {
        TKL_LOGE("WIFI set ip failed");
        return -1;
    }
    system(CREATE_UDHCPD_DIR);
    system(CREATE_UDHCPD_CFG_FILE);
    if (ip_prepare((char*)cfg->ip.ip, ipaddr_pre)) {
	    TKL_LOGE("ip_pepare fail\n");
        return -1;
    }
    snprintf(udhcpd_ip_t.ip_start, sizeof(udhcpd_ip_t.ip_start), "%s%d", ipaddr_pre, 2);
    snprintf(udhcpd_ip_t.ip_end,   sizeof(udhcpd_ip_t.ip_end),   "%s%d", ipaddr_pre, 254);
    strcpy(udhcpd_ip_t.ifname, ifname);
    strcpy(udhcpd_ip_t.ip_route, cfg->ip.ip);
    strcpy(udhcpd_ip_t.ip_mask,  cfg->ip.mask);

    ret = udhcpd_conf_prepare(udhcpd_ip_t);
    if(ret < 0) {
        TKL_LOGE("udhcpd_conf_prepare\n");
        system("killall -9 hostapd");
        return -1;
    }
    memset(cmd,0,sizeof(cmd));
    snprintf(cmd, sizeof(cmd), "udhcpd -f %s &", WIFI_DHCPD_CONF);
    system(cmd);

    return 0;
}

int wifi_ap_stop(void)
{
    char *process1 = "hostapd";
    char *process2 = "udhcpd";

    process_kill(process1);
    process_kill(process2);

    return 0;
}


int wifi_create_rawsocket(int protocol_to_sniff)
{
    int raw_sock;

    if ((raw_sock = socket(PF_PACKET, SOCK_RAW, htons(protocol_to_sniff))) == -1)
        perror("Error creating raw socket: ");

    return raw_sock;
}

int wifi_bind_rawsocket(char *device, int rawsock, int protocol)
{
    struct sockaddr_ll sll;
    struct ifreq ifr;
    int ret;

    bzero(&sll, sizeof(struct sockaddr_ll));
    bzero(&ifr, sizeof(struct ifreq));

    /* get interface index */
    strncpy((char *)ifr.ifr_name, device, IFNAMSIZ);
    if((ret = ioctl(rawsock, SIOCGIFINDEX, &ifr)) < 0)
    {
        perror("Error get interface index: ");
        goto out;
    }

    /* bind to interface */
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(protocol);
    if((ret = bind(rawsock, (struct sockaddr *)&sll, sizeof(sll))) == -1)
    {
        perror("Error binding raw socket to interface: ");
        goto out;
    }

out:
    return ret;
}

int wifi_send_rawpacket(int rawsock, unsigned char *pkt, size_t pkt_len)
{
    size_t pkt_sent = 0;

    // uint8_t *t_radiotap = (uint8_t*)"\x00\x00\x0d\x00\x04\x80\x02\x00\x02\x00\x00\x00\x00";
    uint8_t t_radiotap[] = {0x00, 0x00, 0x0d, 0x00, 0x04, 0x80, 0x02, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00};
    uint8_t *rawpacket   = malloc(pkt_len + sizeof(t_radiotap));

    if (NULL == rawpacket) {
        return -1;
    }
    memcpy(rawpacket, t_radiotap, sizeof(t_radiotap));
    memcpy(rawpacket + sizeof(t_radiotap), pkt, pkt_len);
    pkt_len += sizeof(t_radiotap);
    if((pkt_sent = write(rawsock, rawpacket, pkt_len)) != pkt_len) {
        perror("Eror sending raw socket data: ");
    }
    free(rawpacket);

    return pkt_sent;
}

int wifi_recv_rawpacket(int rawsock, unsigned char *pkt, int pkt_len)
{
    size_t recv = 0;

    recv = read(rawsock, pkt, pkt_len);

    return recv;
}


#include "include.h"
#include "wlan_ui_pub.h"
#include "rw_pub.h"
#include "vif_mgmt.h"
#include "sa_station.h"
#include "param_config.h"
#include "common/ieee802_11_defs.h"
#include "driver_beken.h"
#include "mac_ie.h"
#include "sa_ap.h"
#include "main_none.h"
#include "sm.h"
#include "mac.h"
#include "sm_task.h"
#include "scan_task.h"
#include "hal_machw.h"
#include "rtos_error.h"
#include "lwip_netif_address.h"
#include "lwip/inet.h"
#include <stdbool.h>
#include "rw_pub.h"
#include "power_save_pub.h"
#include "rwnx.h"
#include "net.h"
#include "mm_bcn.h"
#include "phy_trident.h"
#include "mcu_ps_pub.h"
#include "manual_ps_pub.h"
#include "gpio_pub.h"
#include "wdt_pub.h"
#include "start_type_pub.h"
#include "wpa_psk_cache.h"
#include "sys_ctrl_pub.h"
#include "lwip/net.h"
#include "txu_cntrl.h"
#include "rw_msdu.h"
#if CFG_WPA_CTRL_IFACE
#include "../wpa_supplicant/wlan_defs.h"
#include "wpa_ctrl.h"
#include "flash_pub.h"
#endif
#include "ate_app.h"
#include "bk7011_cal_pub.h"
#include "app.h"
#if CFG_ROLE_LAUNCH
#include "role_launch.h"
#endif
#include "soft_encrypt.h"

uint8_t ble_switch_mac_sleeped = 0;

monitor_data_cb_t g_monitor_cb = 0;
static monitor_data_cb_t g_bcn_cb = 0;
#if CFG_AP_MONITOR_COEXIST
static int g_ap_monitor_coexist = 0;
#endif

int g_set_channel_postpone_num = 0;
#ifdef CONFIG_AOS_MESH
monitor_data_cb_t g_mesh_monitor_cb = 0;
uint8_t g_mesh_bssid[6];
#endif
FUNC_1PARAM_PTR connection_status_cb = 0;

extern void mm_hw_ap_disable(void);
extern int hostapd_main_exit(void);
extern int supplicant_main_exit(void);
extern void net_wlan_add_netif(void *mac);
extern void wpa_hostapd_release_scan_rst(void);
extern int mm_bcn_get_tx_cfm(void);
extern void wlan_ui_bcn_callback(uint8_t *data, int len, hal_wifi_link_info_t *info);
extern void power_save_bcn_callback(uint8_t *data, int len, hal_wifi_link_info_t *info);
extern void bk_wlan_register_bcn_cb(monitor_data_cb_t fn);
extern void mcu_ps_bcn_callback(uint8_t *data, int len, hal_wifi_link_info_t *info);
extern uint32_t get_ate_mode_state(void);

static void rwnx_remove_added_interface(void)
{
    int ret;
    u8 test_mac[6];
    struct mm_add_if_cfm *cfm;
    struct apm_start_cfm *apm_cfm = 0;

	wifi_get_mac_address((char*)test_mac, CONFIG_ROLE_STA);
    cfm = (struct mm_add_if_cfm *)os_malloc(sizeof(struct mm_add_if_cfm));
    ret = rw_msg_send_add_if((const unsigned char *)&test_mac, 3, 0, cfm);

    if(ret || (cfm->status != CO_OK))
    {
        os_printf("[saap]MM_ADD_IF_REQ failed!\r\n");
        goto ERR_RETURN;
    }

    apm_cfm = (struct apm_start_cfm *)os_malloc(sizeof(struct apm_start_cfm));
    ret = rw_msg_send_apm_start_req(cfm->inst_nbr, 1, apm_cfm);

    if(ret || (apm_cfm->status != CO_OK))
    {
        os_printf("[saap]APM_START_REQ failed!\r\n");
        goto ERR_RETURN;
    }

    rw_msg_send_remove_if(cfm->inst_nbr);

ERR_RETURN:
    if(cfm)
    {
        os_free(cfm);
    }

    if(apm_cfm)
    {
        os_free(apm_cfm);
    }
}

void bk_wlan_connection_loss(void)
{
    struct vif_info_tag *p_vif_entry = vif_mgmt_first_used();

    while (p_vif_entry != NULL)
    {
        if(p_vif_entry->type == VIF_STA)
        {
            os_printf("bk_wlan_connection_loss vif:%d\r\n", p_vif_entry->index);
            sta_ip_down();
            rw_msg_send_connection_loss_ind(p_vif_entry->index);
        }
        p_vif_entry = vif_mgmt_next(p_vif_entry);
    }
}

uint32_t bk_sta_cipher_is_open(void)
{
    ASSERT(g_sta_param_ptr);
    return (SECURITY_TYPE_NONE == g_sta_param_ptr->cipher_suite);
}

uint32_t bk_sta_cipher_is_wep(void)
{
    ASSERT(g_sta_param_ptr);
    return (SECURITY_TYPE_WEP == g_sta_param_ptr->cipher_suite);
}

int bk_sta_cipher_type(void)
{
    if(!sta_ip_is_start())
    {
        return -1;
    }

    return g_sta_param_ptr->cipher_suite;
}

OSStatus bk_wlan_set_country(const wifi_country_t *country)
{
    return rw_ieee80211_set_country(country);
}

OSStatus bk_wlan_get_country(wifi_country_t *country)
{
    return rw_ieee80211_get_country(country);
}

void bk_wlan_set_max_txpwr(FP32 max_tx_pwr)
{
	void rwnx_cal_set_max_twper(FP32 max_tx_pwr);
	rwnx_cal_set_max_twper(max_tx_pwr);
}

uint32_t bk_wlan_ap_get_frequency(void)
{
    uint8_t channel = bk_wlan_ap_get_channel_config();

    return rw_ieee80211_get_centre_frequency(channel);
}

uint8_t bk_wlan_ap_get_channel_config(void)
{
    return g_ap_param_ptr->chann;
}

void bk_wlan_ap_set_channel_config(uint8_t channel)
{
    g_ap_param_ptr->chann = channel;
}

uint8_t bk_wlan_has_role(uint8_t role)
{
    VIF_INF_PTR vif_entry;
    uint32_t role_count = 0;

    vif_entry = (VIF_INF_PTR)rwm_mgmt_is_vif_first_used();
    while(vif_entry)
    {
        if(vif_entry->type == role)
        {
            role_count ++ ;
        }

        vif_entry = (VIF_INF_PTR)rwm_mgmt_next(vif_entry);
    }

    return role_count;
}

void bk_wlan_set_coexist_at_init_phase(uint8_t current_role)
{
    uint32_t coexit = 0;

    switch(current_role)
    {
        case CONFIG_ROLE_AP:
            if(bk_wlan_has_role(VIF_STA))
            {
                coexit = 1;
            }
            break;

        case CONFIG_ROLE_STA:
            if(bk_wlan_has_role(VIF_AP))
            {
                coexit = 1;
            }
            break;

        case CONFIG_ROLE_NULL:
            if(bk_wlan_has_role(VIF_STA)
                && bk_wlan_has_role(VIF_AP))
            {
                coexit = 1;
            }
            break;

        case CONFIG_ROLE_COEXIST:
            coexit = 1;
            ASSERT(CONFIG_ROLE_COEXIST == g_wlan_general_param->role);
            break;

        default:
            break;
    }

    if(coexit)
    {
        g_wlan_general_param->role = CONFIG_ROLE_COEXIST;
    }
}

uint16_t bk_wlan_sta_get_frequency(void)
{
    uint16_t frequency = 0;
    uint32_t sta_flag = 0;
    VIF_INF_PTR vif_entry;

    vif_entry = (VIF_INF_PTR)rwm_mgmt_is_vif_first_used();
    while(vif_entry)
    {
        if(vif_entry->type == VIF_STA)
        {
            sta_flag = 1;
            break;
        }

        vif_entry = (VIF_INF_PTR)rwm_mgmt_next(vif_entry);
    }

    if(0 == sta_flag)
    {
        goto get_exit;
    }

    frequency = chan_get_vif_frequency(vif_entry);

get_exit:
    return frequency;
}

uint8_t bk_wlan_sta_get_channel(void)
{
    uint8_t channel = 0;
    uint16_t frequency;

    frequency = bk_wlan_sta_get_frequency();
    if(frequency)
    {
        channel = rw_ieee80211_get_chan_id(frequency);
    }

    return channel;
}

uint8_t sys_channel = DEFAULT_CHANNEL_AP;
uint8_t bk_wlan_ap_get_default_channel(void)
{
    uint8_t channel;

    /* if ap and sta are coexist, ap channel shall match sta channel firstly*/
    channel = bk_wlan_sta_get_channel();
    if(0 == channel)
    {
        if(sys_channel == DEFAULT_CHANNEL_AP)
            channel = DEFAULT_CHANNEL_AP;
        else
            channel = sys_channel;
    }

    return channel;
}

void bk_wlan_ap_set_default_channel(uint8_t channel)
{
    sys_channel = channel;
}

void bk_wlan_ap_csa_coexist_mode(void *arg, uint8_t dummy)
{
    int ret = 0;
    uint16_t frequency;

    if(0 == bk_wlan_has_role(VIF_AP))
    {
        return;
    }

    frequency = bk_wlan_sta_get_frequency();
    if(frequency)
    {
        os_printf("\r\nhostapd_channel_switch\r\n");
#if CFG_ROLE_LAUNCH
        if(!fl_get_pre_sta_cancel_status())
#endif
        {
        	ret = hostapd_channel_switch(frequency);
        }
        if(ret)
        {
            os_printf("csa_failed:%x\r\n", ret);
        }
    }
}

void bk_wlan_reg_csa_cb_coexist_mode(void)
{
    /* the callback routine will be invoked at the timepoint of associating at station mode*/
    mhdr_connect_user_cb(bk_wlan_ap_csa_coexist_mode, 0);
}

#include "phy_trident.h"
void bk_wlan_phy_open_cca(void)
{
    CHECK_OPERATE_RF_REG_IF_IN_SLEEP();
	phy_open_cca();
    CHECK_OPERATE_RF_REG_IF_IN_SLEEP_END();
	os_printf("bk_wlan cca opened\r\n");
}

void bk_wlan_phy_close_cca(void)
{
    CHECK_OPERATE_RF_REG_IF_IN_SLEEP();
	phy_close_cca();
    CHECK_OPERATE_RF_REG_IF_IN_SLEEP_END();
	os_printf("bk_wlan cca closed\r\n");
}

void bk_wlan_phy_show_cca(void)
{
	phy_show_cca();
}
extern void delay_ms(UINT32 ms_count);
void bk_reboot(void)
{
    UINT32 wdt_val = 5;

    os_printf("bk_reboot\r\n");

    bk_misc_update_set_type(RESET_SOURCE_REBOOT);

    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();

    sddev_control(WDT_DEV_NAME, WCMD_POWER_DOWN, NULL);
    os_null_printf("wdt reboot\r\n");
    delay_ms(100);
    sddev_control(WDT_DEV_NAME, WCMD_SET_PERIOD, &wdt_val);
    sddev_control(WDT_DEV_NAME, WCMD_POWER_UP, NULL);
    while(1);
    GLOBAL_INT_RESTORE();
}
void bk_wlan_ap_init(network_InitTypeDef_st *inNetworkInitPara)
{
    os_printf("Soft_AP_start\r\n");

    if(!g_ap_param_ptr)
    {
        g_ap_param_ptr = (ap_param_t *)os_zalloc(sizeof(ap_param_t));
        ASSERT(g_ap_param_ptr);
    }

    os_memset(g_ap_param_ptr, 0x00, sizeof(*g_ap_param_ptr));

    if(MAC_ADDR_NULL((u8 *)&g_ap_param_ptr->bssid))
    {
        wifi_get_mac_address((char *)&g_ap_param_ptr->bssid, CONFIG_ROLE_AP);
    }

    bk_wlan_ap_set_channel_config(bk_wlan_ap_get_default_channel());

    if(!g_wlan_general_param)
    {
        g_wlan_general_param = (general_param_t *)os_zalloc(sizeof(general_param_t));
        ASSERT(g_wlan_general_param);
    }
    g_wlan_general_param->role = CONFIG_ROLE_AP;
    bk_wlan_set_coexist_at_init_phase(CONFIG_ROLE_AP);

    if(inNetworkInitPara)
    {
        g_ap_param_ptr->ssid.length = MIN(SSID_MAX_LEN, os_strlen(inNetworkInitPara->wifi_ssid));
        os_memcpy(g_ap_param_ptr->ssid.array, inNetworkInitPara->wifi_ssid, g_ap_param_ptr->ssid.length);
        g_ap_param_ptr->key_len = os_strlen(inNetworkInitPara->wifi_key);
        if(g_ap_param_ptr->key_len < 8)
        {
            g_ap_param_ptr->cipher_suite = SECURITY_TYPE_NONE;
        }
        else
        {
#if CFG_SOFTAP_WPA3
            g_ap_param_ptr->cipher_suite = SECURITY_TYPE_WPA3_WPA2_MIXED;
#else
            g_ap_param_ptr->cipher_suite = SECURITY_TYPE_WPA2_AES;
#endif
            os_memcpy(g_ap_param_ptr->key, inNetworkInitPara->wifi_key, g_ap_param_ptr->key_len);
        }

        if(inNetworkInitPara->dhcp_mode == DHCP_SERVER)
        {
            g_wlan_general_param->dhcp_enable = 1;
        }
        else
        {
            g_wlan_general_param->dhcp_enable = 0;
        }
        inet_aton(inNetworkInitPara->local_ip_addr, &(g_wlan_general_param->ip_addr));
        inet_aton(inNetworkInitPara->net_mask, &(g_wlan_general_param->ip_mask));
        inet_aton(inNetworkInitPara->gateway_ip_addr, &(g_wlan_general_param->ip_gw));

#if CFG_ROLE_LAUNCH
		if(rl_pre_ap_set_status(RL_STATUS_AP_INITING))
		{
			return;
		}
#endif
    }

    sa_ap_init();
}

OSStatus bk_wlan_start_ap(network_InitTypeDef_st *inNetworkInitParaAP)
{
#if !CFG_WPA_CTRL_IFACE
    int ret, flag ,empty;
    GLOBAL_INT_DECLARATION();

	while( 1 )
	{
		GLOBAL_INT_DISABLE();
		flag = mm_bcn_get_tx_cfm();
        empty = is_apm_bss_config_empty();
		if ( flag == 0 && empty == 1)
		{
			GLOBAL_INT_RESTORE();
			break;
		}
		else
		{
			GLOBAL_INT_RESTORE();
			rtos_delay_milliseconds(100);
		}
	}

	bk_wlan_stop(SOFT_AP);

    bk_wlan_ap_init(inNetworkInitParaAP);

    ret = hostapd_main_entry(2, 0);
    if(ret)
    {
        os_printf("bk_wlan_start softap failed!!\r\n");
        bk_wlan_stop(SOFT_AP);
        return -1;
    }

    net_wlan_add_netif(&g_ap_param_ptr->bssid);

    ip_address_set(SOFT_AP,
                   inNetworkInitParaAP->dhcp_mode,
                   inNetworkInitParaAP->local_ip_addr,
                   inNetworkInitParaAP->net_mask,
                   inNetworkInitParaAP->gateway_ip_addr,
                   inNetworkInitParaAP->dns_server_ip_addr);
    uap_ip_start();

    sm_build_broadcast_deauthenticate();
#else /* CFG_WPA_CTRL_IFACE */
	/* stop lwip netif */
    static beken_semaphore_t sema = NULL;
    if (!sema) {
       OSStatus ret = rtos_init_semaphore_adv(&sema, 1, 1);
       if (ret != kNoErr)
           return ret;
    }
    rtos_get_semaphore(&sema, BEKEN_NEVER_TIMEOUT);
	uap_ip_down();

	/* set AP parameter, ssid, akm, etc. */
    bk_wlan_ap_init(inNetworkInitParaAP);

	// enable hostapd
	wlan_ap_enable();

	// reload bss configuration
	wlan_ap_reload();

	/* now ap has started, set ip address to this interface */
    ip_address_set(SOFT_AP,
                   inNetworkInitParaAP->dhcp_mode,
                   inNetworkInitParaAP->local_ip_addr,
                   inNetworkInitParaAP->net_mask,
                   inNetworkInitParaAP->gateway_ip_addr,
                   inNetworkInitParaAP->dns_server_ip_addr);

	/* restart lwip network */
    uap_ip_start();
    rtos_set_semaphore(&sema);
#endif

    return kNoErr;
}

void bk_wlan_terminate_sta_rescan(void)
{
	os_printf("bk_wlan_terminate_sta_rescan\r\n");
	if(g_sta_param_ptr)
	{
		g_sta_param_ptr->retry_cnt = 0;
	}
}

void bk_wlan_sta_init(network_InitTypeDef_st *inNetworkInitPara)
{
	if(inNetworkInitPara)
	{
    	mhdr_set_station_status(RW_EVT_STA_IDLE);
	}

    if(!g_sta_param_ptr)
    {
        g_sta_param_ptr = (sta_param_t *)os_zalloc(sizeof(sta_param_t));
        ASSERT(g_sta_param_ptr);
    }

	g_sta_param_ptr->retry_cnt = MAX_STA_RETRY_COUNT;
    wifi_get_mac_address((char*)&g_sta_param_ptr->own_mac, CONFIG_ROLE_STA);
    if(!g_wlan_general_param)
    {
        g_wlan_general_param = (general_param_t *)os_zalloc(sizeof(general_param_t));
        ASSERT(g_wlan_general_param);
    }
    g_wlan_general_param->role = CONFIG_ROLE_STA;
    bk_wlan_set_coexist_at_init_phase(CONFIG_ROLE_STA);

    if(inNetworkInitPara)
    {
        g_sta_param_ptr->ssid.length = MIN(SSID_MAX_LEN, os_strlen(inNetworkInitPara->wifi_ssid));
        os_memcpy(g_sta_param_ptr->ssid.array,
                  inNetworkInitPara->wifi_ssid,
                  g_sta_param_ptr->ssid.length);

        g_sta_param_ptr->key_len = os_strlen(inNetworkInitPara->wifi_key);
        os_memcpy(g_sta_param_ptr->key, inNetworkInitPara->wifi_key, g_sta_param_ptr->key_len);
		g_sta_param_ptr->key[g_sta_param_ptr->key_len] = 0;		/* append \0 */
#if !RL_SUPPORT_FAST_CONNECT
        g_sta_param_ptr->orig_key_len = g_sta_param_ptr->key_len;
        os_memcpy(g_sta_param_ptr->orig_key, inNetworkInitPara->wifi_key, g_sta_param_ptr->orig_key_len);
        os_null_printf("%s:%d key_len=%d,orig_key_len=%d,%.*s,%.*s\r\n", __FUNCTION__, __LINE__, g_sta_param_ptr->key_len, g_sta_param_ptr->orig_key_len, g_sta_param_ptr->key_len, g_sta_param_ptr->key, g_sta_param_ptr->orig_key_len, g_sta_param_ptr->orig_key);
#endif

#if CFG_SUPPORT_ALIOS
        if(inNetworkInitPara->dhcp_mode == DHCP_CLIENT)
#else
        if(inNetworkInitPara->dhcp_mode == DHCP_CLIENT)
#endif
        {
            g_wlan_general_param->dhcp_enable = 1;
        }
        else
        {
            g_wlan_general_param->dhcp_enable = 0;
            inet_aton(inNetworkInitPara->local_ip_addr, &(g_wlan_general_param->ip_addr));
            inet_aton(inNetworkInitPara->net_mask, &(g_wlan_general_param->ip_mask));
            inet_aton(inNetworkInitPara->gateway_ip_addr, &(g_wlan_general_param->ip_gw));
        }

#if CFG_ROLE_LAUNCH
	    if(rl_pre_sta_set_status(RL_STATUS_STA_INITING))
	    {
	        return;
	    }
#endif
    }

    bk_wlan_reg_csa_cb_coexist_mode();
    sa_station_init();

    bk_wlan_register_bcn_cb(wlan_ui_bcn_callback);

}

#if CFG_WPA_CTRL_IFACE && CFG_WLAN_FAST_CONNECT
char wlan_fast_connect_buffer[sizeof(UINT32) + sizeof(struct wlan_fast_connect_info)] = { 0 };
void wlan_write_fast_connect_info(struct wlan_fast_connect_info *fci)
{
	UINT32 *length_ptr = (UINT32 *)wlan_fast_connect_buffer;

	*length_ptr = sizeof(struct wlan_fast_connect_info);

	os_memcpy(wlan_fast_connect_buffer + sizeof(UINT32), fci, sizeof(*fci));

	bk_printf("writed fci to flash ssid=%s\n", fci->ssid);
}

void wlan_clear_fast_connect_info(void)
{
	os_memset(wlan_fast_connect_buffer, 0x00, sizeof(wlan_fast_connect_buffer));
	os_printf("wlan_clear_fast_connect_info\n");
}

void *wlan_get_fast_connect_info(void)
{
	return (void *)wlan_fast_connect_buffer;
}
#endif


OSStatus bk_wlan_start_sta_fast(struct wlan_fast_connect_info *fci)
{
	size_t psk_len = 0;
	u8 *psk = 0;
#if CFG_WPA_CTRL_IFACE
    network_InitTypeDef_st wNetConfig;
    network_InitTypeDef_st *inNetworkInitPara;
	int chan = 0;

	/* diconnect previous connection if may */
	sta_ip_down();	// XXX: WLAN_DISCONNECT_EVENT may handle this
	wlan_sta_disconnect();
#else /* !CFG_WPA_CTRL_IFACE */
	rwnxl_reset_evt(0);

    bk_wlan_stop(STATION);
#endif

	/* clear it before connect, or init with current fci ? */
	wlan_clear_fast_connect_info();
    os_memset(&wNetConfig, 0x0, sizeof(network_InitTypeDef_st));
    inNetworkInitPara = &wNetConfig;

    os_strcpy((char *)inNetworkInitPara->wifi_ssid, (const char *)fci->ssid);
    os_strcpy((char *)inNetworkInitPara->wifi_key, (const char *)fci->pwd);

    inNetworkInitPara->wifi_mode = STATION;
    inNetworkInitPara->dhcp_mode = DHCP_CLIENT;

    bk_wlan_sta_init(inNetworkInitPara);

#if CFG_WPA_CTRL_IFACE
	chan = fci->channel;
	psk = fci->psk;
	psk_len = PMK_LEN * 2;

	bk_printf("fast_connect\n");
	if (os_strlen((char *)psk) == 0) {
		// no psk info, calcuate pmk
		psk = 0;
		psk_len = 0;
	}
#endif

	/*
	 * let wpa_psk_cal thread to caculate psk.
	 * XXX: If you know psk value, fill last two parameters of `wpa_psk_request()'.
	 */
	wpa_psk_request(g_sta_param_ptr->ssid.array, g_sta_param_ptr->ssid.length,
			(char*)g_sta_param_ptr->key, psk, psk_len);

#if CFG_WPA_CTRL_IFACE
	/* enable wpa_supplicant */
	wlan_sta_enable();

	/* set network parameters: ssid, passphase */
	wlan_sta_set((uint8_t *)inNetworkInitPara->wifi_ssid, os_strlen(inNetworkInitPara->wifi_ssid), (uint8_t *)inNetworkInitPara->wifi_key);

	/* connect to AP */
	wlan_sta_connect(chan);
#else /* !CFG_WPA_CTRL_IFACE */
    supplicant_main_entry(inNetworkInitPara->wifi_ssid);
    net_wlan_add_netif(&g_sta_param_ptr->own_mac);
#endif

	/* set IP mode */
    ip_address_set(inNetworkInitPara->wifi_mode,
                   inNetworkInitPara->dhcp_mode,
                   inNetworkInitPara->local_ip_addr,
                   inNetworkInitPara->net_mask,
                   inNetworkInitPara->gateway_ip_addr,
                   inNetworkInitPara->dns_server_ip_addr);

    return kNoErr;
}

/* only executed once */
void wlan_send_disconnect_after_reboot(uint16_t freq, uint8_t *bssid, uint8_t *sta, uint16_t reason)
{
	static bool disconnect_sent = false;
	uint8_t data[26] = {0};
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)data;
	wlan_sta_send_mlme_t mlme;

	if (disconnect_sent)
		return;
	disconnect_sent = true;

	hdr->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT, WLAN_FC_STYPE_DEAUTH);
	os_memcpy(hdr->addr1, bssid, ETH_ALEN);
	os_memcpy(hdr->addr2, sta, ETH_ALEN);
	os_memcpy(hdr->addr3, bssid, ETH_ALEN);
	*(uint16_t *)(hdr + 1) = host_to_le16(reason);  // reason code

	mlme.data = data;
	mlme.data_len = 26;
	mlme.freq = freq;

	// print_hex_dump("MLME: ", data, 26);

	wpa_ctrl_request(WPA_CTRL_CMD_STA_SEND_MLME, &mlme);

	// rtos_delay_milliseconds(100);
}

OSStatus bk_wlan_start_sta(network_InitTypeDef_st *inNetworkInitPara)
{
	size_t psk_len = 0;
	u8 *psk = 0;
#if CFG_WPA_CTRL_IFACE
	int chan = 0;

	/* diconnect previous connection if may */
	sta_ip_down();	// XXX: WLAN_DISCONNECT_EVENT may handle this
	wlan_sta_disconnect();
#else /* !CFG_WPA_CTRL_IFACE */
	rwnxl_reset_evt(0);

    bk_wlan_stop(STATION);
#endif

#if (CFG_SOC_NAME == SOC_BK7231N)
    if (get_ate_mode_state())
    {
        // cunliang20210407 set blk_standby_cfg with blk_txen_cfg like txevm, qunshan confirmed
        rwnx_cal_en_extra_txpa();
    }
#endif
    bk_wlan_sta_init(inNetworkInitPara);

	/*
	 * let wpa_psk_cal thread to caculate psk.
	 * XXX: If you know psk value, fill last two parameters of `wpa_psk_request()'.
	 */
	wpa_psk_request(g_sta_param_ptr->ssid.array, g_sta_param_ptr->ssid.length,
			(char*)g_sta_param_ptr->key, psk, psk_len);

#if CFG_WPA_CTRL_IFACE
	/* enable wpa_supplicant */
	wlan_sta_enable();

	/* set network parameters: ssid, passphase */
	wlan_sta_set((uint8_t *)inNetworkInitPara->wifi_ssid, os_strlen(inNetworkInitPara->wifi_ssid), (uint8_t *)inNetworkInitPara->wifi_key);

	/* connect to AP */
	wlan_sta_connect(chan);
#else /* !CFG_WPA_CTRL_IFACE */
    supplicant_main_entry(inNetworkInitPara->wifi_ssid);
    net_wlan_add_netif(&g_sta_param_ptr->own_mac);
#endif

	/* set IP mode */
    ip_address_set(inNetworkInitPara->wifi_mode,
                   inNetworkInitPara->dhcp_mode,
                   inNetworkInitPara->local_ip_addr,
                   inNetworkInitPara->net_mask,
                   inNetworkInitPara->gateway_ip_addr,
                   inNetworkInitPara->dns_server_ip_addr);

    return kNoErr;
}

OSStatus bk_wlan_start(network_InitTypeDef_st *inNetworkInitPara)
{
    int ret = 0;
#if CFG_ROLE_LAUNCH
    LAUNCH_REQ lreq;
#endif

    if(bk_wlan_is_monitor_mode())
    {
        os_printf("monitor not finish!\r\n");
        return ret;
    }

    if(inNetworkInitPara->wifi_mode == SOFT_AP)
    {
        #if CFG_ROLE_LAUNCH
        lreq.req_type = LAUNCH_REQ_AP;
        lreq.descr = *inNetworkInitPara;

        rl_ap_request_enter(&lreq, 0);
        #else
        bk_wlan_start_ap(inNetworkInitPara);
        #endif
    }
    else if(inNetworkInitPara->wifi_mode == STATION)
    {
        #if CFG_ROLE_LAUNCH
        lreq.req_type = LAUNCH_REQ_STA;
        lreq.descr = *inNetworkInitPara;

        rl_sta_request_enter(&lreq, 0);
        #else
        bk_wlan_start_sta(inNetworkInitPara);
        #endif
    }

    return 0;
}

int bk_wlan_start_scan(void)
{
    int ret = 0;

#if !CFG_WPA_CTRL_IFACE
    SCAN_PARAM_T scan_param = {0};
#endif

#if CFG_USE_STA_PS
    bk_wlan_dtim_rf_ps_disable_send_msg();
#endif

    if(bk_wlan_is_monitor_mode())
    {
        os_printf("monitor not finish!\r\n");
		#if CFG_ROLE_LAUNCH
		rl_pre_sta_set_status(RL_STATUS_STA_LAUNCH_FAILED);
		#endif
        return ret;
    }

    bk_wlan_sta_init(0);

#if CFG_WPA_CTRL_IFACE
	wlan_sta_enable();
	ret = wlan_sta_scan_once();
#else /* !CFG_WPA_CTRL_IFACE */
    os_memset(&scan_param.bssid, 0xff, ETH_ALEN);
	scan_param.vif_idx = INVALID_VIF_IDX;
	scan_param.num_ssids = 1;

    rw_msg_send_scanu_req(&scan_param);
#endif

    return ret;
}

void bk_wlan_scan_ap_reg_cb(FUNC_2PARAM_PTR ind_cb)
{
    mhdr_scanu_reg_cb(ind_cb,0);
}

unsigned char bk_wlan_get_scan_ap_result_numbers(void)
{
    struct scanu_rst_upload *scan_rst;
    unsigned char scanu_num = 0;

    scan_rst = sr_get_scan_results();
    if(scan_rst)
    {
        scanu_num = scan_rst->scanu_num;
    }

    return scanu_num;
}

void bk_wlan_get_scan_ap_result(SCAN_RST_ITEM_PTR scan_rst_table,unsigned char get_scanu_num)
{
    struct scanu_rst_upload *scan_rst;
    unsigned char scanu_num = 0,i;

    scan_rst = sr_get_scan_results();
    if(scan_rst)
    {
        scanu_num = (scan_rst->scanu_num) > get_scanu_num ? (get_scanu_num):(scan_rst->scanu_num);

        for(i = 0;i<scanu_num;i++)
        {
            scan_rst_table[i] = *(scan_rst->res[i]);
        }
    }

    sr_release_scan_results(scan_rst);
}


int bk_wlan_start_assign_scan(UINT8 **ssid_ary, UINT8 ssid_num)
{
    int ret = 0;

#if !CFG_WPA_CTRL_IFACE
    SCAN_PARAM_T scan_param = {0};

    bk_wlan_sta_init(0);

    os_memset(&scan_param.bssid, 0xff, ETH_ALEN);
	scan_param.vif_idx = INVALID_VIF_IDX;
    scan_param.num_ssids = ssid_num;
    for (int i = 0 ; i < ssid_num ; i++ )
    {
        scan_param.ssids[i].length = MIN(SSID_MAX_LEN, os_strlen((char*)ssid_ary[i]));
        os_memcpy(scan_param.ssids[i].array, ssid_ary[i], scan_param.ssids[i].length);
    }
    rw_msg_send_scanu_req(&scan_param);
#else
	wlan_sta_scan_param_t scan_param = {0};

	/* init hw */
	bk_wlan_sta_init(0);

	/* enable wpa_supplicant */
	wlan_sta_enable();

	/* set scan ssid list */
	scan_param.num_ssids = ssid_num;
    for (int i = 0 ; i < ssid_num ; i++) {
        scan_param.ssids[i].ssid_len = MIN(WLAN_SSID_MAX_LEN, os_strlen((char*)ssid_ary[i]));
        os_memcpy(scan_param.ssids[i].ssid, ssid_ary[i], scan_param.ssids[i].ssid_len);
    }

	/* start scan */
	ret = wlan_sta_scan(&scan_param);
#endif

    return ret;
}

void bk_wlan_sta_init_adv(network_InitTypeDef_adv_st *inNetworkInitParaAdv)
{
	if(inNetworkInitParaAdv)
	{
    	mhdr_set_station_status(RW_EVT_STA_IDLE);
	}

    if(!g_sta_param_ptr)
    {
        g_sta_param_ptr = (sta_param_t *)os_zalloc(sizeof(sta_param_t));
        ASSERT(g_sta_param_ptr);
    }

	g_sta_param_ptr->retry_cnt = MAX_STA_RETRY_COUNT;
    if(MAC_ADDR_NULL((u8 *)&g_sta_param_ptr->own_mac))
    {
        wifi_get_mac_address((char *)&g_sta_param_ptr->own_mac, CONFIG_ROLE_STA);
    }

    g_sta_param_ptr->ssid.length = MIN(SSID_MAX_LEN, os_strlen(inNetworkInitParaAdv->ap_info.ssid));
    os_memcpy(g_sta_param_ptr->ssid.array, inNetworkInitParaAdv->ap_info.ssid, g_sta_param_ptr->ssid.length);

	g_sta_param_ptr->cipher_suite = inNetworkInitParaAdv->ap_info.security;
    g_sta_param_ptr->fast_connect_set = 1;
    g_sta_param_ptr->fast_connect.chann = inNetworkInitParaAdv->ap_info.channel;
    os_memcpy(g_sta_param_ptr->fast_connect.bssid, inNetworkInitParaAdv->ap_info.bssid, ETH_ALEN);
    g_sta_param_ptr->key_len = inNetworkInitParaAdv->key_len;
    os_memcpy((uint8_t *)g_sta_param_ptr->key, inNetworkInitParaAdv->key, inNetworkInitParaAdv->key_len);
	g_sta_param_ptr->key[inNetworkInitParaAdv->key_len] = 0;	/* append \0 */
    os_null_printf("%s:%d key_len=%d,orig_key_len=%d,%.*s,%.*s\r\n", __FUNCTION__, __LINE__, g_sta_param_ptr->key_len, g_sta_param_ptr->orig_key_len, g_sta_param_ptr->key_len, g_sta_param_ptr->key, g_sta_param_ptr->orig_key_len, g_sta_param_ptr->orig_key);

    if(!g_wlan_general_param)
    {
        g_wlan_general_param = (general_param_t *)os_malloc(sizeof(general_param_t));
    }
    g_wlan_general_param->role = CONFIG_ROLE_STA;
    bk_wlan_set_coexist_at_init_phase(CONFIG_ROLE_STA);

    if(inNetworkInitParaAdv->dhcp_mode == DHCP_CLIENT)
    {
        g_wlan_general_param->dhcp_enable = 1;
    }
    else
    {
        g_wlan_general_param->dhcp_enable = 0;
        inet_aton(inNetworkInitParaAdv->local_ip_addr, &(g_wlan_general_param->ip_addr));
        inet_aton(inNetworkInitParaAdv->net_mask, &(g_wlan_general_param->ip_mask));
        inet_aton(inNetworkInitParaAdv->gateway_ip_addr, &(g_wlan_general_param->ip_gw));
    }

#if CFG_ROLE_LAUNCH
	    if(rl_pre_sta_set_status(RL_STATUS_STA_CONNECTING))
	    {
	        return;
	    }
#endif

    bk_wlan_reg_csa_cb_coexist_mode();
    sa_station_init();
}

#if (!CFG_SUPPORT_ALIOS)
void bk_wlan_ap_init_adv(network_InitTypeDef_ap_st *inNetworkInitParaAP)
{
    if(!g_ap_param_ptr)
    {
        g_ap_param_ptr = (ap_param_t *)os_zalloc(sizeof(ap_param_t));
        ASSERT(g_ap_param_ptr);
    }

    if(MAC_ADDR_NULL((u8 *)&g_ap_param_ptr->bssid))
    {
        wifi_get_mac_address((char *)&g_ap_param_ptr->bssid, CONFIG_ROLE_AP);
    }

    if(!g_wlan_general_param)
    {
        g_wlan_general_param = (general_param_t *)os_zalloc(sizeof(general_param_t));
        ASSERT(g_wlan_general_param);
    }
    g_wlan_general_param->role = CONFIG_ROLE_AP;
    bk_wlan_set_coexist_at_init_phase(CONFIG_ROLE_AP);

    if(inNetworkInitParaAP)
    {
        if(0 == inNetworkInitParaAP->channel)
        {
            g_ap_param_ptr->chann = bk_wlan_ap_get_default_channel();
        }
        else
        {
            g_ap_param_ptr->chann = inNetworkInitParaAP->channel;
        }

        g_ap_param_ptr->ssid.length = MIN(SSID_MAX_LEN, os_strlen(inNetworkInitParaAP->wifi_ssid));
        os_memcpy(g_ap_param_ptr->ssid.array, inNetworkInitParaAP->wifi_ssid, g_ap_param_ptr->ssid.length);
        g_ap_param_ptr->key_len = os_strlen(inNetworkInitParaAP->wifi_key);
        os_memcpy(g_ap_param_ptr->key, inNetworkInitParaAP->wifi_key, g_ap_param_ptr->key_len);

        g_ap_param_ptr->cipher_suite = inNetworkInitParaAP->security;

        if(inNetworkInitParaAP->dhcp_mode == DHCP_SERVER)
        {
            g_wlan_general_param->dhcp_enable = 1;
        }
        else
        {
            g_wlan_general_param->dhcp_enable = 0;
        }
        inet_aton(inNetworkInitParaAP->local_ip_addr, &(g_wlan_general_param->ip_addr));
        inet_aton(inNetworkInitParaAP->net_mask, &(g_wlan_general_param->ip_mask));
        inet_aton(inNetworkInitParaAP->gateway_ip_addr, &(g_wlan_general_param->ip_gw));
    }

    sa_ap_init();
}
#endif

OSStatus bk_wlan_start_sta_adv(network_InitTypeDef_adv_st *inNetworkInitParaAdv)
{
    if(bk_wlan_is_monitor_mode())
    {
        os_printf("airkiss not finish!\r\n");
        return 0;
    }

#if !CFG_WPA_CTRL_IFACE
    bk_wlan_stop(STATION);

#if CFG_ROLE_LAUNCH
    if(rl_pre_sta_set_status(RL_STATUS_STA_INITING))
    {
        return 0;
    }
#endif

#if (CFG_SOC_NAME == SOC_BK7231N)
    if (get_ate_mode_state())
    {
        // cunliang20210407 set blk_standby_cfg with blk_txen_cfg like txevm, qunshan confirmed
        rwnx_cal_en_extra_txpa();
    }
#endif
    bk_wlan_sta_init_adv(inNetworkInitParaAdv);

	/*
	 * let wpa_psk_cal thread to caculate psk.
	 * XXX: If you know psk value, fill last two parameters of `wpa_psk_request()'.
	 */
	wpa_psk_request(g_sta_param_ptr->ssid.array, g_sta_param_ptr->ssid.length,
			(char*)(g_sta_param_ptr->key), NULL, 0);

    supplicant_main_entry(inNetworkInitParaAdv->ap_info.ssid);

    net_wlan_add_netif(&g_sta_param_ptr->own_mac);


#else /* CFG_WPA_CTRL_IFACE */
	wlan_sta_config_t config;

#if (CFG_SOC_NAME == SOC_BK7231N)
    if (get_ate_mode_state())
    {
        // cunliang20210407 set blk_standby_cfg with blk_txen_cfg like txevm, qunshan confirmed
        rwnx_cal_en_extra_txpa();
    }
#endif
    bk_wlan_sta_init_adv(inNetworkInitParaAdv);

	/*
	 * let wpa_psk_cal thread to caculate psk.
	 * XXX: If you know psk value, fill last two parameters of `wpa_psk_request()'.
	 */
	wpa_psk_request(g_sta_param_ptr->ssid.array, g_sta_param_ptr->ssid.length,
			(char *)g_sta_param_ptr->key, NULL, 0);

	/* disconnect previous connected network */
	wlan_sta_disconnect();

	/* start wpa_supplicant */
	wlan_sta_enable();

	/* set network parameters: ssid, passphase */
	wlan_sta_set((uint8_t *)inNetworkInitParaAdv->ap_info.ssid, os_strlen(inNetworkInitParaAdv->ap_info.ssid),
			(uint8_t *)inNetworkInitParaAdv->key);

	/* set fast connect bssid */
	os_memset(&config, 0, sizeof(config));
	os_memcpy(config.u.bssid, inNetworkInitParaAdv->ap_info.bssid, ETH_ALEN);
	config.field = WLAN_STA_FIELD_BSSID;
	wpa_ctrl_request(WPA_CTRL_CMD_STA_SET, &config);

	/* set fast connect freq */
	os_memset(&config, 0, sizeof(config));
	config.u.channel = inNetworkInitParaAdv->ap_info.channel;
	config.field = WLAN_STA_FIELD_FREQ;
	wpa_ctrl_request(WPA_CTRL_CMD_STA_SET, &config);

	/* connect to AP */
	wlan_sta_connect(g_sta_param_ptr->fast_connect_set ? g_sta_param_ptr->fast_connect.chann : 0);

#endif
    ip_address_set(STATION, inNetworkInitParaAdv->dhcp_mode,
                   inNetworkInitParaAdv->local_ip_addr,
                   inNetworkInitParaAdv->net_mask,
                   inNetworkInitParaAdv->gateway_ip_addr,
                   inNetworkInitParaAdv->dns_server_ip_addr);

    return 0;
}

#if (!CFG_SUPPORT_ALIOS)
OSStatus bk_wlan_start_ap_adv(network_InitTypeDef_ap_st *inNetworkInitParaAP)
{
    int ret = 0;

    if(bk_wlan_is_monitor_mode())
    {
        os_printf("monitor not finish!\r\n");
        return ret;
    }

#if !CFG_WPA_CTRL_IFACE
    bk_wlan_stop(SOFT_AP);

#if CFG_ROLE_LAUNCH
    if(rl_pre_ap_set_status(RL_STATUS_AP_INITING))
    {
        return 0;
    }
#endif

    bk_wlan_ap_init_adv(inNetworkInitParaAP);

    ret = hostapd_main_entry(2, 0);
    if(ret)
    {
        os_printf("bk_wlan_start_ap_adv failed!!\r\n");
        bk_wlan_stop(SOFT_AP);
        return -1;
    }

    net_wlan_add_netif(&g_ap_param_ptr->bssid);

    ip_address_set(SOFT_AP, inNetworkInitParaAP->dhcp_mode,
                   inNetworkInitParaAP->local_ip_addr,
                   inNetworkInitParaAP->net_mask,
                   inNetworkInitParaAP->gateway_ip_addr,
                   inNetworkInitParaAP->dns_server_ip_addr);
    uap_ip_start();

    sm_build_broadcast_deauthenticate();
#else	/* CFG_WPA_CTRL_IFACE */
	/* stop lwip netif */
	uap_ip_down();

	/* set AP parameter, ssid, akm, etc. */
    bk_wlan_ap_init_adv(inNetworkInitParaAP);

	// enable hostapd
	wlan_ap_enable();

	// reload bss configuration
	wlan_ap_reload();

	/* now ap has started, set ip address to this interface */
    ip_address_set(SOFT_AP, inNetworkInitParaAP->dhcp_mode,
                   inNetworkInitParaAP->local_ip_addr,
                   inNetworkInitParaAP->net_mask,
                   inNetworkInitParaAP->gateway_ip_addr,
                   inNetworkInitParaAP->dns_server_ip_addr);

	/* restart lwip network */
    uap_ip_start();
#endif

    return kNoErr;
}
#endif

int bk_wlan_stop(char mode)
{
    int ret = kNoErr;

    mhdr_set_station_status(RW_EVT_STA_IDLE);
#if CFG_USE_STA_PS
    bk_wlan_dtim_rf_ps_disable_send_msg();
#endif

    switch(mode)
    {
    case SOFT_AP:
        mm_hw_ap_disable();

#if !CFG_WPA_CTRL_IFACE
        uap_ip_down();
        bk_printf("%x:%x:%x:%x:%x:%x\r\n", g_ap_param_ptr->bssid.array[0], g_ap_param_ptr->bssid.array[1], g_ap_param_ptr->bssid.array[2],
            g_ap_param_ptr->bssid.array[3], g_ap_param_ptr->bssid.array[4], g_ap_param_ptr->bssid.array[5]);
        net_wlan_remove_netif(&g_ap_param_ptr->bssid);
        hostapd_main_exit();
#else
		wlan_ap_disable();
#endif
        if(bk_wlan_has_role(VIF_STA))
        {
            g_wlan_general_param->role = CONFIG_ROLE_STA;
        }

#if CFG_ROLE_LAUNCH
        rl_pre_ap_set_status(RL_STATUS_AP_LAUNCHED);
#endif
        break;

    case STATION:
#if (CFG_SOC_NAME == SOC_BK7231N)
        if (get_ate_mode_state())
        {
            // cunliang20210407 recovery blk_standby_cfg like txevm -e 0, qunshan confirmed
            rwnx_cal_dis_extra_txpa();
        }
#endif
        sta_ip_down();
#if !CFG_WPA_CTRL_IFACE
        net_wlan_remove_netif(&g_sta_param_ptr->own_mac);
        supplicant_main_exit();
        wpa_hostapd_release_scan_rst();
#else
		wlan_sta_disable();	/* same but call in wpas task */
#endif
        if(bk_wlan_has_role(VIF_AP))
        {
            g_wlan_general_param->role = CONFIG_ROLE_AP;
        }

#if CFG_ROLE_LAUNCH
        rl_pre_sta_set_status(RL_STATUS_STA_LAUNCHED);
#endif
        break;

    default:
        ret = kGeneralErr;
        break;
    }

    return ret;
}

OSStatus bk_wlan_set_ip_status(IPStatusTypedef *inNetpara, WiFi_Interface inInterface)
{
    OSStatus ret = kNoErr;

    switch ( inInterface )
    {
    case SOFT_AP :
        if ( uap_ip_is_start() )
        {
            uap_ip_down();
        }
        else
        {
            ret = kGeneralErr;
        }
        break;

    case STATION :
        if ( sta_ip_is_start() )
        {
            sta_ip_down();
        }
        else
        {
            ret = kGeneralErr;
        }
        break;

    default:
        ret = kGeneralErr;
        break;
    }

    if ( ret == kNoErr )
    {
        ip_address_set(inInterface, inNetpara->dhcp, inNetpara->ip,
                       inNetpara->mask, inNetpara->gate, inNetpara->dns);
        if ( inInterface == SOFT_AP )
        {
            uap_ip_is_start();
        }
        else if ( inInterface == STATION )
        {
            sta_ip_start();
        }
    }

    return ret;
}

OSStatus bk_wlan_get_ip_status(IPStatusTypedef *outNetpara, WiFi_Interface inInterface)
{
    OSStatus ret = kNoErr;
    struct wlan_ip_config addr;

    os_memset(&addr, 0, sizeof(struct wlan_ip_config));

    switch ( inInterface )
    {
    case SOFT_AP :
        net_get_if_addr(&addr, net_get_uap_handle());
        net_get_if_macaddr(outNetpara->mac, net_get_uap_handle());
        break;

    case STATION :
        net_get_if_addr(&addr, net_get_sta_handle());
        net_get_if_macaddr(outNetpara->mac, net_get_sta_handle());
        break;

    default:
        ret = kGeneralErr;
        break;
    }

    if ( ret == kNoErr )
    {
        outNetpara->dhcp = addr.ipv4.addr_type;
        os_strcpy(outNetpara->ip, inet_ntoa(addr.ipv4.address));
        os_strcpy(outNetpara->mask, inet_ntoa(addr.ipv4.netmask));
        os_strcpy(outNetpara->gate, inet_ntoa(addr.ipv4.gw));
        os_strcpy(outNetpara->dns, inet_ntoa(addr.ipv4.dns1));
    }

    return ret;
}

OSStatus bk_wlan_get_link_status(LinkStatusTypeDef *outStatus)
{
    struct sm_get_bss_info_cfm *cfm;
#if CFG_SUPPORT_ALIOS
    msg_sta_states sta_stat;
#endif
    int ret;
    u8 vif_idx = 0, ssid_len;

    if( !sta_ip_is_start() )
    {
        return kGeneralErr;
    }

#if CFG_SUPPORT_ALIOS
	sta_stat = mhdr_get_station_status();
	if(sta_stat == MSG_GOT_IP)
	{
    	outStatus->is_connected = true;
	}
	else
	{
	    outStatus->is_connected = false;
	}
#else
    outStatus->conn_state = mhdr_get_station_status();
#endif

    cfm = os_malloc(sizeof(struct sm_get_bss_info_cfm));
    if(!cfm)
    {
        return kNoMemoryErr;
    }

    vif_idx = rwm_mgmt_vif_mac2idx((void *)&g_sta_param_ptr->own_mac);
    ret = rw_msg_get_bss_info(vif_idx, (void *)cfm);
    if(ret)
    {
        os_free(cfm);
        return kGeneralErr;
    }

    outStatus->wifi_strength = cfm->rssi;
    if(outStatus->wifi_strength >=0)
    {
        // app accept rssi less than 0
        outStatus->wifi_strength = -1;
    }
    outStatus->channel = rw_ieee80211_get_chan_id(cfm->freq);
	#if (!CFG_SUPPORT_ALIOS)
    outStatus->security = g_sta_param_ptr->cipher_suite;
	#endif
    os_memcpy(outStatus->bssid, cfm->bssid, 6);
    ssid_len = MIN(SSID_MAX_LEN, os_strlen((char*)cfm->ssid));
    os_memcpy(outStatus->ssid, cfm->ssid, ssid_len);

    os_free(cfm);

    return kNoErr;
}

#if (!CFG_SUPPORT_ALIOS)
void bk_wlan_ap_para_info_get(network_InitTypeDef_ap_st *ap_info)
{
    IPStatusTypedef ap_ips;

    if((!ap_info)||(!uap_ip_is_start()))
    {
        return;
    }

    memcpy(ap_info->wifi_ssid,g_ap_param_ptr->ssid.array,g_ap_param_ptr->ssid.length);
    memcpy(ap_info->wifi_key,g_ap_param_ptr->key,g_ap_param_ptr->key_len);
    ap_info->channel = g_ap_param_ptr->chann;
    ap_info->security = g_ap_param_ptr->cipher_suite;

    bk_wlan_get_ip_status(&ap_ips,SOFT_AP);
    memcpy(ap_info->local_ip_addr,ap_ips.ip,16);
    memcpy(ap_info->gateway_ip_addr,ap_ips.gate,16);
    memcpy(ap_info->net_mask,ap_ips.mask,16);
    memcpy(ap_info->dns_server_ip_addr,ap_ips.dns,16);

     ap_info->dhcp_mode = g_wlan_general_param->dhcp_enable;
}
#endif

uint32_t bk_wlan_get_INT_status(void)
{
    return platform_is_in_interrupt_context();
}

/** @brief  Add the packet type which monitor should receive
 *
 *  @detail This function can be called many times to receive different wifi packets.
 */
int bk_wlan_monitor_rx_type(int type)
{
    switch(type)
    {
    case WLAN_RX_BEACON:
        nxmac_accept_beacon_setf(1);
        break;
    case WLAN_RX_PROBE_REQ:
        nxmac_accept_probe_req_setf(1);
        break;
    case WLAN_RX_PROBE_RES:
        nxmac_accept_probe_resp_setf(1);
        break;
    case WLAN_RX_ACTION:
        break;
    case WLAN_RX_MANAGEMENT:
        nxmac_accept_other_mgmt_frames_setf(1);
        break;
    case WLAN_RX_DATA:
        nxmac_accept_other_data_frames_setf(1);
        nxmac_accept_qo_s_null_setf(1);
        nxmac_accept_qcfwo_data_setf(1);
        nxmac_accept_q_data_setf(1);
        nxmac_accept_cfwo_data_setf(1);
        nxmac_accept_data_setf(1);
        break;
    case WLAN_RX_MCAST_DATA:
        nxmac_accept_multicast_setf(1);
        nxmac_accept_broadcast_setf(1);
        break;
    case WLAN_RX_ALL:
        nxmac_rx_cntrl_set((uint32_t)0x7FFFFFFC);
        break;
    }

    return 0;
}

void bk_wlan_enable_lsig(void)
{
    hal_machw_allow_rx_rts_cts();
    phy_enable_lsig_intr();
}

void bk_wlan_disable_lsig(void)
{
    hal_machw_disallow_rx_rts_cts();
    phy_disable_lsig_intr();
}

#if CFG_AP_MONITOR_COEXIST
/**
 * set AP+EZ mode
 * @val: 1 if enable AP+EZ, 0: pure EZ
 */
void bk_wlan_set_ap_monitor_coexist(int val)
{
	g_ap_monitor_coexist = !!val;
}

int bk_wlan_get_ap_monitor_coexist()
{
	return g_ap_monitor_coexist;
}
#endif

/** @brief  Start wifi monitor mode
 *
 *  @detail This function disconnect wifi station and softAP.
 *
 */
int bk_wlan_start_monitor(void)
{
#if !CFG_AP_MONITOR_COEXIST
    monitor_data_cb_t cb_bakup = g_monitor_cb;
    g_monitor_cb = NULL;

    bk_wlan_stop(SOFT_AP);
    bk_wlan_stop(STATION);

    g_monitor_cb = cb_bakup;

#if CFG_SUPPORT_ALIOS
    lsig_init();
#endif
    bk_wlan_ap_init(0);
    rwnx_remove_added_interface();
    g_wlan_general_param->role = CONFIG_ROLE_NULL;
#else
	if (bk_wlan_get_ap_monitor_coexist()) {
		/* AP+Monitor Mode, don't need to remove all interfaces */
		hal_machw_enter_monitor_mode();
	} else {
		/* Just pure monitor mode */
		monitor_data_cb_t cb_bakup = g_monitor_cb;
		g_monitor_cb = NULL;

		bk_wlan_stop(SOFT_AP);
		bk_wlan_stop(STATION);

		g_monitor_cb = cb_bakup;

#if CFG_SUPPORT_ALIOS
		lsig_init();
#endif
		bk_wlan_ap_init(0);
		rwnx_remove_added_interface();
		g_wlan_general_param->role = CONFIG_ROLE_NULL;
	}
#endif
    return 0;
}

/** @brief  Stop wifi monitor mode
 *
 */
int bk_wlan_stop_monitor(void)
{
    if(g_monitor_cb)
    {
        g_monitor_cb = 0;
        hal_machw_exit_monitor_mode();
    }

    return 0;
}

int bk_wlan_get_channel(void)
{
    int channel, freq;

    freq = rw_msg_get_channel(NULL);
    channel = rw_ieee80211_get_chan_id(freq);

    return channel;
}

/** @brief  Set the monitor channel
 *
 *  @detail This function change the monitor channel (from 1~13).
 *       it can change the channel dynamically, don't need restart monitor mode.
 */
int bk_wlan_set_channel_sync(int channel)
{
    rwnxl_reset_evt(0);
    rw_msg_set_channel(channel, PHY_CHNL_BW_20, NULL);

    return 0;
}

int bk_wlan_get_channel_with_band_width(int *channel, int *band_width)
{
    struct phy_channel_info info;

    phy_get_channel(&info, 0);

    *channel = rw_ieee80211_get_chan_id(info.info2);
    *band_width = (info.info1 >> 8) & 0xFF;

    return 0;
}

int bk_wlan_set_channel_with_band_width(int channel, int band_width)
{
    rwnxl_reset_evt(0);
    rw_msg_set_channel((uint32_t)channel, (uint32_t)band_width, NULL);

    return 0;
}

/** @brief  Set channel at the asynchronous method
 *
 *  @detail This function change the monitor channel (from 1~13).
 *       it can change the channel dynamically, don't need restart monitor mode.
 */
int bk_wlan_set_channel(int channel)
{
	wifi_country_t country;
	GLOBAL_INT_DECLARATION();

    if(0 == channel)
    {
        channel = 1;
    }

	rw_ieee80211_get_country(&country);
	if(channel < country.schan || channel > country.nchan)
	{
		return channel;
	}

	GLOBAL_INT_DISABLE();
	g_set_channel_postpone_num = channel;
	GLOBAL_INT_RESTORE();

    CHECK_OPERATE_RF_REG_IF_IN_SLEEP();
	ke_evt_set(KE_EVT_RESET_BIT);
    CHECK_OPERATE_RF_REG_IF_IN_SLEEP_END();

    return 0;
}

/** @brief  Register the monitor callback function
 *        Once received a 802.11 packet call the registered function to return the packet.
 */
void bk_wlan_register_monitor_cb(monitor_data_cb_t fn)
{
	if(fn)
	{
	    g_monitor_cb = fn;
	}
}

monitor_data_cb_t bk_wlan_get_monitor_cb(void)
{
    if (g_monitor_cb)
    {
#if CFG_SUPPORT_ALIOS
        return bk_monitor_callback;
#else
    	return g_monitor_cb;
#endif
    }
    else
	{
        return NULL;
    }
}

int bk_wlan_is_monitor_mode(void)
{
    return (0 == g_monitor_cb) ? false : true;
}

void wlan_ui_bcn_callback(uint8_t *data, int len, hal_wifi_link_info_t *info)
{
#if CFG_USE_STA_PS
    if(power_save_if_ps_rf_dtim_enabled())
    {
        power_save_bcn_callback(data,len,info);
    }
#endif
#if CFG_USE_MCU_PS
    mcu_ps_bcn_callback(data,len,info);
#endif
}

void bk_wlan_register_bcn_cb(monitor_data_cb_t fn)
{
    	g_bcn_cb = fn;
}

monitor_data_cb_t bk_wlan_get_bcn_cb(void)
{
    return g_bcn_cb;
}

extern void bmsg_ps_sender(uint8_t ioctl);

#if CFG_USE_DEEP_PS
extern void bk_enter_deep_sleep_mode ( PS_DEEP_CTRL_PARAM *deep_param );
#endif

#if CFG_USE_STA_PS
static uint32_t rf_ps_enabled = 0;
/** @brief  Enable dtim power save,close rf,and wakeup by ieee dtim dynamical
 *
 */
int bk_wlan_dtim_rf_ps_mode_enable(void )
{
    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();
    rf_ps_enabled = 1;
    GLOBAL_INT_RESTORE();
    bmsg_ps_sender(PS_BMSG_IOCTL_RF_ENABLE);

    return 0;
}


/** @brief  When in dtim rf off mode,user can manual wakeup before dtim wakeup time.
 *          this function can not be called in "core_thread" context
 */
extern uint8_t ble_switch_mac_sleeped;
int bk_wlan_dtim_rf_ps_mode_do_wakeup()
{
    void *sem_list = NULL;
	UINT32 ret = 0;

	sem_list = power_save_rf_ps_wkup_semlist_create();

	if (!sem_list)
    {
        os_null_printf("err ---- NULL\r\n");
        ASSERT(0);
    }

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    if((power_save_if_ps_rf_dtim_enabled()
            && power_save_if_rf_sleep()) || ble_switch_mac_sleeped)
    {
        power_save_rf_ps_wkup_semlist_wait(sem_list);
    }
    else
    {
        power_save_rf_ps_wkup_semlist_destroy(sem_list);
        os_free(sem_list);
	sem_list = NULL;
    }

    GLOBAL_INT_RESTORE();

    power_save_rf_ps_wkup_semlist_get(sem_list);

    return ret;
}

int bk_wlan_dtim_rf_ps_disable_send_msg(void)
{
    power_save_dtim_rf_ps_disable_send_msg();

    return 0;
}

/** @brief  Request exit dtim dynamical ps mode
 */
int bk_wlan_dtim_rf_ps_mode_disable(void)
{
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    rf_ps_enabled = 0;
    GLOBAL_INT_RESTORE();
    bk_wlan_dtim_rf_ps_disable_send_msg();
    return 0;
}

int bk_wlan_dtim_rf_ps_timer_start(void)
{
    bmsg_ps_sender(PS_BMSG_IOCTL_RF_PS_TIMER_INIT);
    return 0;
}

int bk_wlan_dtim_rf_ps_timer_pause(void)
{
    bmsg_ps_sender(PS_BMSG_IOCTL_RF_PS_TIMER_DEINIT);
    return 0;
}


int bk_wlan_dtim_rf_ps_set_linger_time(UINT32 time_ms)
{
    power_save_set_linger_time(time_ms);

    return 0;
}

int auto_check_dtim_rf_ps_mode(void )
{
    if(1 == rf_ps_enabled)
    {
        bmsg_ps_sender(PS_BMSG_IOCTL_RF_ENABLE);
    }

    return 0;
}
#endif

int bk_wlan_mcu_suppress_and_sleep(UINT32 sleep_ticks )
{
#if CFG_USE_MCU_PS
	#if (CFG_OS_FREERTOS)
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    TickType_t missed_ticks = 0;

    missed_ticks = mcu_power_save( sleep_ticks );
    fclk_update_tick( missed_ticks );
    GLOBAL_INT_RESTORE();
	#endif
#endif
    return 0;
}

#if CFG_USE_MCU_PS
/** @brief  Enable mcu power save,close mcu ,and wakeup by irq
 */
int bk_wlan_mcu_ps_mode_enable(void)
{
    bmsg_ps_sender(PS_BMSG_IOCTL_MCU_ENABLE);

    return 0;
}

/** @brief  Disable mcu power save mode
 */
int bk_wlan_mcu_ps_mode_disable(void)
{
    bmsg_ps_sender(PS_BMSG_IOCTL_MCU_DISANABLE);

    return 0;
}
#endif

#if PS_DTIM_WITH_NORMAL
/** @brief  Open dtim with normal flag
 */
int bk_wlan_dtim_with_normal_open()
{
    ps_dtim_normal_enable = 1;
}

/** @brief  Close dtim with normal flag
 */
int bk_wlan_dtim_with_normal_close()
{
    ps_dtim_normal_enable = 0;
}
#endif

BK_PS_LEVEL global_ps_level = 0;
int bk_wlan_power_save_set_level(BK_PS_LEVEL level)
{
    if(level & PS_DEEP_SLEEP_BIT)
    {
#if CFG_USE_STA_PS
        if(global_ps_level & PS_RF_SLEEP_BIT)
        {
            bk_wlan_dtim_rf_ps_mode_disable();
        }
#endif
#if CFG_USE_MCU_PS
        if(global_ps_level & PS_MCU_SLEEP_BIT)
        {
            bk_wlan_mcu_ps_mode_disable();
        }
#endif
        rtos_delay_milliseconds(100);
#if CFG_USE_DEEP_PS
      ///  bk_enter_deep_sleep(0xc000,0x0);
#endif
    }

    if((global_ps_level & PS_RF_SLEEP_BIT) ^ (level & PS_RF_SLEEP_BIT))
    {
#if CFG_USE_STA_PS
        if(global_ps_level & PS_RF_SLEEP_BIT)
        {
            bk_wlan_dtim_rf_ps_mode_disable();
        }
        else
        {
            bk_wlan_dtim_rf_ps_mode_enable();
        }
#endif
    }

    if((global_ps_level & PS_MCU_SLEEP_BIT) ^ (level & PS_MCU_SLEEP_BIT))
    {
#if CFG_USE_MCU_PS
        if(global_ps_level & PS_MCU_SLEEP_BIT)
        {
            bk_wlan_mcu_ps_mode_disable();
        }
        else
        {
            bk_wlan_mcu_ps_mode_enable();
        }
#endif
    }

    global_ps_level = level;

    return 0;
}

void bk_wlan_status_register_cb(FUNC_1PARAM_PTR cb)
{
	connection_status_cb = cb;
}

FUNC_1PARAM_PTR bk_wlan_get_status_cb(void)
{
	return connection_status_cb;
}

#if CFG_SUPPORT_RTT
OSStatus bk_wlan_ap_is_up(void)
{
	#if CFG_ROLE_LAUNCH
		if(RL_STATUS_AP_INITING < rl_pre_ap_get_status())
		{
			return 1;
		}
	#endif

	return 0;
}

OSStatus bk_wlan_sta_is_connected(void)
{
	#if CFG_ROLE_LAUNCH
		if(RL_STATUS_STA_LAUNCHED <= rl_pre_sta_get_status())
		{
			return 1;
		}
	#endif

	return 0;
}
#endif

UINT32 if_other_mode_rf_sleep(void)
{
    if(!bk_wlan_has_role(VIF_AP)
        &&!bk_wlan_has_role(VIF_MESH_POINT)
        &&!bk_wlan_has_role(VIF_IBSS)
        &&!bk_wlan_is_monitor_mode())
    {
        return 1;
    }
    else
        {
        return 0;
    }
}

uint32_t bk_wlan_start_ez_of_sta(void)
{
	return hal_machw_enter_ez_of_sta();
}

uint32_t bk_wlan_stop_ez_of_sta(void)
{
	return hal_machw_exit_ez_of_sta();
}

uint32_t bk_wlan_reg_rx_mgmt_cb(mgmt_rx_cb_t cb, uint32_t rx_mgmt_flag)
{
	return rxu_reg_mgmt_cb(cb, rx_mgmt_flag);
}

int bk_wlan_sync_send_raw_frame(uint8_t *buffer, int len)
{
	// TODO

	return 0;
}

extern int bmsg_tx_raw_cb_sender(uint8_t *buffer, int length, void *cb, void *param);
int bk_wlan_send_raw_frame_with_cb(uint8_t *buffer, int len, void *cb, void *param)
{
	return bmsg_tx_raw_cb_sender(buffer, len, cb, param);
}

extern int bmsg_tx_raw_sender(uint8_t *payload, uint16_t length);
int bk_wlan_send_80211_raw_frame(uint8_t *buffer, int len)
{
	uint8_t *pkt;
	int ret;

	pkt = os_malloc(len);
	if (pkt == NULL)
	{
		return -1;
	}

	os_memcpy(pkt, buffer, len);
	ret = bmsg_tx_raw_sender(pkt, len);
	return ret;
}

int bk_wlan_send_80211_beacon_frame(uint8_t channel, uint8_t *ssid, uint8_t ssid_len)
{
	int ret;
	BUS_MSG_PARAM_T *bcn_param;

	bcn_param = (BUS_MSG_PARAM_T*)os_zalloc(sizeof(BUS_MSG_PARAM_T));
	if (bcn_param == NULL)
		return -1;
	if (ssid_len >= sizeof(bcn_param->ssid))
		return -1;
        bcn_param->channel = channel;
	os_memcpy(bcn_param->ssid, ssid, ssid_len);
	ret = bmsg_tx_beacon_sender(bcn_param);

	return ret;
}

// eof


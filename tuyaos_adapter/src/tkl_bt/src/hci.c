#include "tkl_bluetooth.h"

#include "bluetooth_api.h"
#include "bluetooth.h"
#include "hci.h"
#include "hci_lib.h"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HDEV       0
#define ADV_HANDLE 0x01

#define BLE_ADV_ITVL_MS(t)  ((t)*1000 / 625)
#define BLE_SCAN_ITVL_MS(t) ((t)*1000 / 625)
#define BLE_SCAN_WIN_MS(t)  ((t)*1000 / 625)

STATIC BOOL_T hci_task_enable               = FALSE;
STATIC UINT8_T hci_version                  = 0xFF; // 0x08 == 4.2; 0x09 == 5.0
STATIC INT_T g_dd                           = -1;
STATIC TKL_BLE_GAP_EVT_FUNC_CB g_gap_evt_cb = NULL;
STATIC pthread_t hci_task_thId;

STATIC VOID_T __hci_evt_callback(TKL_BLE_GAP_EVT_TYPE_E type, INT_T result);

/* Open HCI device.
 * Returns device descriptor (dd). */
STATIC INT_T __hci_open_dev(INT_T dev_id)
{
    struct sockaddr_hci a;
    INT_T dd, err;
    struct hci_filter flt;

    /* Check for valid device id */
    if (dev_id < 0) {
        errno = ENODEV;
        return -1;
    }

    /* Create HCI socket */
    dd = socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (dd < 0)
        return dd;

    /* Setup filter */
    hci_filter_clear(&flt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
    hci_filter_all_events(&flt);

    if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
        goto failed;
    }

    /* Bind socket to the HCI device */
    memset(&a, 0, sizeof(a));
    a.hci_family  = AF_BLUETOOTH;
    a.hci_dev     = dev_id;
    a.hci_channel = HCI_CHANNEL_RAW;
    if (bind(dd, (struct sockaddr *)&a, sizeof(a)) < 0) {
        goto failed;
    }

    return dd;

failed:
    err = errno;
    close(dd);
    errno = err;

    return -1;
}

STATIC INT_T __hci_close_dev(INT_T dd)
{
    return close(dd);
}

STATIC INT_T __hci_send_cmd(INT_T dd, UINT16_T ogf, UINT16_T ocf, UINT8_T plen, VOID_T *param)
{
    UINT8_T type = HCI_COMMAND_PKT;
    hci_command_hdr hc;
    struct iovec iv[3];
    INT_T ivn;

    hc.opcode = htobs(cmd_opcode_pack(ogf, ocf));
    hc.plen   = plen;

    iv[0].iov_base = &type;
    iv[0].iov_len  = 1;
    iv[1].iov_base = &hc;
    iv[1].iov_len  = HCI_COMMAND_HDR_SIZE;
    ivn            = 2;

    if (plen) {
        iv[2].iov_base = param;
        iv[2].iov_len  = plen;
        ivn            = 3;
    }

    while (writev(dd, iv, ivn) < 0) {
        if (errno == EAGAIN || errno == EINTR)
            continue;
        return -1;
    }
    return 0;
}

STATIC INT_T __hci_send_req(INT_T dd, struct hci_request *r, INT_T to)
{
    UCHAR_T buf[HCI_MAX_EVENT_SIZE], *ptr;
    UINT16_T opcode = htobs(cmd_opcode_pack(r->ogf, r->ocf));
    struct hci_filter nf, of;
    socklen_t olen;
    hci_event_hdr *hdr;
    INT_T err, try;

    olen = sizeof(of);
    if (getsockopt(dd, SOL_HCI, HCI_FILTER, &of, &olen) < 0)
        return -1;

    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_CMD_STATUS, &nf);
    hci_filter_set_event(EVT_CMD_COMPLETE, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);
    hci_filter_set_event(r->event, &nf);
    hci_filter_set_opcode(opcode, &nf);
    if (setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0)
        return -1;

    if (__hci_send_cmd(dd, r->ogf, r->ocf, r->clen, r->cparam) < 0)
        goto failed;

    try = 10;
    while (try--) {
        evt_cmd_complete *cc;
        evt_cmd_status *cs;
        evt_remote_name_req_complete *rn;
        evt_le_meta_event *me;
        remote_name_req_cp *cp;
        INT_T len;

        if (to) {
            struct pollfd p;
            INT_T n;

            p.fd     = dd;
            p.events = POLLIN;
            while ((n = poll(&p, 1, to)) < 0) {
                if (errno == EAGAIN || errno == EINTR)
                    continue;
                goto failed;
            }

            if (!n) {
                errno = ETIMEDOUT;
                goto failed;
            }

            to -= 10;
            if (to < 0)
                to = 0;
        }

        while ((len = read(dd, buf, sizeof(buf))) < 0) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            goto failed;
        }

        hdr = (VOID_T *)(buf + 1);
        ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
        len -= (1 + HCI_EVENT_HDR_SIZE);

        switch (hdr->evt) {
        case EVT_CMD_STATUS:
            cs = (VOID_T *)ptr;

            if (cs->opcode != opcode)
                continue;

            if (r->event != EVT_CMD_STATUS) {
                if (cs->status) {
                    errno = EIO;
                    goto failed;
                }
                break;
            }

            r->rlen = MIN(len, r->rlen);
            memcpy(r->rparam, ptr, r->rlen);
            goto done;

        case EVT_CMD_COMPLETE:
            cc = (VOID_T *)ptr;

            if (cc->opcode != opcode)
                continue;

            ptr += EVT_CMD_COMPLETE_SIZE;
            len -= EVT_CMD_COMPLETE_SIZE;

            r->rlen = MIN(len, r->rlen);
            memcpy(r->rparam, ptr, r->rlen);
            goto done;

        case EVT_REMOTE_NAME_REQ_COMPLETE:
            if (hdr->evt != r->event)
                break;

            rn = (VOID_T *)ptr;
            cp = r->cparam;

            if (bacmp(&rn->bdaddr, &cp->bdaddr))
                continue;

            r->rlen = MIN(len, r->rlen);
            memcpy(r->rparam, ptr, r->rlen);
            goto done;

        case EVT_LE_META_EVENT:
            me = (VOID_T *)ptr;

            if (me->subevent != r->event)
                continue;

            len -= 1;
            r->rlen = MIN(len, r->rlen);
            memcpy(r->rparam, me->data, r->rlen);
            goto done;

        default:
            if (hdr->evt != r->event)
                break;

            r->rlen = MIN(len, r->rlen);
            memcpy(r->rparam, ptr, r->rlen);
            goto done;
        }
    }
    errno = ETIMEDOUT;

failed:
    err = errno;
    if (setsockopt(dd, SOL_HCI, HCI_FILTER, &of, sizeof(of)) < 0)
        err = errno;
    errno = err;
    return -1;

done:
    if (setsockopt(dd, SOL_HCI, HCI_FILTER, &of, sizeof(of)) < 0)
        return -1;
    return 0;
}

OPERATE_RET hci_dev_up(VOID_T)
{
    __hci_evt_callback(TKL_BLE_EVT_STACK_INIT, 0);

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    if (ioctl(g_dd, HCIDEVUP, HDEV) < 0) {
        if (errno == EALREADY)
            return OPRT_OK;
        printf("Can't init device\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET hci_dev_down(VOID_T)
{
    __hci_evt_callback(TKL_BLE_EVT_STACK_DEINIT, 0);

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    if (ioctl(g_dd, HCIDEVDOWN, HDEV) < 0) {
        printf("Can't down HCI socket.\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __hci_dev_info(struct hci_dev_info *di)
{
    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    if (ioctl(g_dd, HCIGETDEVINFO, (VOID_T *)&di)) {
        printf("Can't get device info.\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET hci_dev_get_addr(UINT8_T *addr)
{
    OPERATE_RET op_ret = OPRT_OK;
    struct hci_dev_info di;

    di.dev_id = HDEV;
    IF_FAIL_RETURN(__hci_dev_info(&di));
    memcpy(addr, di.bdaddr.b, 6);

    return OPRT_OK;
}

STATIC OPERATE_RET __hci_dev_read_local_version(UINT8_T *ver)
{
    read_local_version_rp rp;
    struct hci_request rq;
    INT_T ret;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&rq, 0, sizeof(rq));
    rq.ogf    = OGF_INFO_PARAM;
    rq.ocf    = OCF_READ_LOCAL_VERSION;
    rq.rparam = &rp;
    rq.rlen   = READ_LOCAL_VERSION_RP_SIZE;

    ret = __hci_send_req(g_dd, &rq, 1000);

    if (ret < 0) {
        printf("Get version failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    if (rp.status) {
        printf("Get version failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }
    //! 10
    *ver            = rp.hci_ver;
    hci_task_enable = TRUE;

    return OPRT_OK;
}

STATIC OPERATE_RET __hci_dev_set_scan_enable(UINT8_T enable)
{
    le_set_scan_enable_cp scan_cp;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&scan_cp, 0, sizeof(scan_cp));
    scan_cp.enable     = enable;
    scan_cp.filter_dup = 1;

    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_SCAN_ENABLE, LE_SET_SCAN_ENABLE_CP_SIZE, &scan_cp) < 0) {
        printf("Set scan:%d failed\n", enable);
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __hci_dev_set_extended_scan_enable(UINT8_T enable)
{
    le_set_extended_scan_enable_cp scan_cp;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&scan_cp, 0, sizeof(scan_cp));
    scan_cp.enable     = enable;
    scan_cp.filter_dup = 1;
    scan_cp.duration   = 0;
    scan_cp.period     = 0;
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_EXTENDED_SCAN_ENABLE, LE_SET_EXTENDED_SCAN_ENABLE_CP_SIZE, &scan_cp) < 0) {
        printf("Set scan:%d failed\n", enable);
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET hci_dev_set_scan_enable(UINT8_T enable)
{
    if (hci_version == 0xFF) {
        __hci_dev_read_local_version(&hci_version);
    }

    if (hci_version <= 0x08) {
        return __hci_dev_set_scan_enable(enable);
    } else {
        return __hci_dev_set_extended_scan_enable(enable);
    }
}

STATIC OPERATE_RET __hci_dev_set_advertise_enable(UINT8_T enable)
{
    le_set_advertise_enable_cp adv_cp;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&adv_cp, 0, sizeof(adv_cp));
    adv_cp.enable = enable;
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_ADVERTISE_ENABLE, LE_SET_ADVERTISE_ENABLE_CP_SIZE, &adv_cp) < 0) {
        printf("Set advertise:%d failed\n", enable);
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __hci_dev_set_extended_advertise_enable(UINT8_T enable)
{
    CHAR_T buf[10]                                 = { 0 };
    le_set_extended_advertise_enable_cp *ad_enable = (le_set_extended_advertise_enable_cp *)buf;
    le_extended_advertising_set *set               = (le_extended_advertising_set *)(buf + LE_SET_EXTENDED_ADVERTISE_ENABLE_CP_SIZE);

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    ad_enable->enable      = enable;
    ad_enable->num_of_sets = 1;
    set->handle            = ADV_HANDLE;
    set->duration          = 0;
    set->max_events        = 0;
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_EXTENDED_ADVERTISE_ENABLE, 
                        LE_SET_EXTENDED_ADVERTISE_ENABLE_CP_SIZE + LE_SET_EXTENDED_ADVERTISING_SET_SIZE, buf) < 0) {
        printf("Set advertise:%d failed\n", enable);
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET hci_dev_set_advertise_enable(UINT8_T enable)
{
    if (hci_version == 0xFF) {
        __hci_dev_read_local_version(&hci_version);
    }

    if (hci_version <= 0x08) {
        return __hci_dev_set_advertise_enable(enable);
    } else {
        return __hci_dev_set_extended_advertise_enable(enable);
    }
}

STATIC OPERATE_RET __hci_dev_set_adv_parameters(TKL_BLE_GAP_ADV_PARAMS_T CONST *p_adv_params)
{
    le_set_advertising_parameters_cp adv_params_cp;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&adv_params_cp, 0, sizeof(adv_params_cp));
    adv_params_cp.min_interval = BLE_ADV_ITVL_MS(p_adv_params->adv_interval_min);
    adv_params_cp.max_interval = BLE_ADV_ITVL_MS(p_adv_params->adv_interval_max);

    if (p_adv_params->adv_type == TKL_BLE_GAP_ADV_TYPE_CONN_SCANNABLE_UNDIRECTED) {
        adv_params_cp.advtype = 0;
    } else if (p_adv_params->adv_type == TKL_BLE_GAP_ADV_TYPE_NONCONN_SCANNABLE_UNDIRECTED) {
        adv_params_cp.advtype = 3;
    } else {
        return OPRT_INVALID_PARM; // Invalid Parameters
    }
    adv_params_cp.chan_map           = p_adv_params->adv_channel_map;
    adv_params_cp.direct_bdaddr_type = p_adv_params->direct_addr.type;
    memcpy(adv_params_cp.direct_bdaddr.b, p_adv_params->direct_addr.addr, 6);

    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_ADVERTISING_PARAMETERS, LE_SET_ADVERTISING_PARAMETERS_CP_SIZE, &adv_params_cp) < 0) {
        printf("Set advertise parameters failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __hci_dev_set_extended_adv_parameters(TKL_BLE_GAP_ADV_PARAMS_T CONST *p_adv_params)
{
    le_set_extended_advertising_parameters_cp params;
    UINT16_T itvl = 0;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&params, 0, LE_SET_EXTENDED_ADVERTISING_PARAMETERS_CP_SIZE);
    itvl = BLE_ADV_ITVL_MS(p_adv_params->adv_interval_min);
    memcpy(params.min_interval, (UINT8_T *)&itvl, 2);
    itvl = BLE_ADV_ITVL_MS(p_adv_params->adv_interval_max);
    memcpy(params.max_interval, (UINT8_T *)&itvl, 2);
    params.peer_addr_type = p_adv_params->direct_addr.type;
    memcpy(params.peer_addr, p_adv_params->direct_addr.addr, 6);
    params.channel_map = p_adv_params->adv_channel_map;

    if (p_adv_params->adv_type == TKL_BLE_GAP_ADV_TYPE_CONN_SCANNABLE_UNDIRECTED) {
        params.evt_properties = 0x0013;
    } else if (p_adv_params->adv_type == TKL_BLE_GAP_ADV_TYPE_NONCONN_SCANNABLE_UNDIRECTED) {
        params.evt_properties = 0x0012;
    } else {
        return OPRT_INVALID_PARM; // Invalid Parameters
    }

    params.handle        = ADV_HANDLE;
    params.tx_power      = 0x7F;
    params.primary_phy   = 1;
    params.secondary_phy = 1;
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_EXTENDED_ADVERTISING_PARAMETERS, 
                        LE_SET_EXTENDED_ADVERTISING_PARAMETERS_CP_SIZE, &params) < 0) {
        printf("Set advertise parameters failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET hci_dev_set_adv_parameters(TKL_BLE_GAP_ADV_PARAMS_T CONST *p_adv_params)
{
    if (hci_version == 0xFF) {
        __hci_dev_read_local_version(&hci_version);
    }

    if (hci_version <= 0x08) {
        return __hci_dev_set_adv_parameters(p_adv_params);
    } else {
        return __hci_dev_set_extended_adv_parameters(p_adv_params);
    }
}

STATIC OPERATE_RET __hci_dev_set_adv_data(TKL_BLE_DATA_T CONST *p_adv)
{
    le_set_advertising_data_cp adv_data_cp;

    if (p_adv->length > 31) {
        return OPRT_OS_ADAPTER_INVALID_PARM;
    }

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&adv_data_cp, 0, sizeof(adv_data_cp));
    adv_data_cp.length = p_adv->length;
    memcpy(adv_data_cp.data, p_adv->p_data, p_adv->length);
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_ADVERTISING_DATA, LE_SET_ADVERTISING_DATA_CP_SIZE, &adv_data_cp) < 0) {
        printf("Set advertise data failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __hci_dev_set_extended_adv_data(TKL_BLE_DATA_T CONST *p_adv)
{
    CHAR_T buf[255 + 4]                              = { 0 };
    le_set_extended_advertising_data_cp *adv_data_cp = (le_set_extended_advertising_data_cp *)buf;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    adv_data_cp->handle              = ADV_HANDLE;
    adv_data_cp->operation           = 3;
    adv_data_cp->fragment_preference = 1;
    adv_data_cp->data_len            = p_adv->length;
    memcpy(adv_data_cp->data, p_adv->p_data, p_adv->length);
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_EXTENDED_ADVERTISING_DATA, 
                        LE_SET_EXTENDED_ADVERTISING_DATA_CP_SIZE + p_adv->length, buf) < 0) {
        printf("Set advertise data failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET hci_dev_set_adv_data(TKL_BLE_DATA_T CONST *p_adv)
{
    if (hci_version == 0xFF) {
        __hci_dev_read_local_version(&hci_version);
    }

    if (hci_version <= 0x08) {
        return __hci_dev_set_adv_data(p_adv);
    } else {
        return __hci_dev_set_extended_adv_data(p_adv);
    }
}

STATIC OPERATE_RET __hci_dev_set_scan_parameters(TKL_BLE_GAP_SCAN_PARAMS_T CONST *p_scan_params)
{
    le_set_scan_parameters_cp param_cp;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&param_cp, 0, sizeof(param_cp));
    param_cp.type            = p_scan_params->active;
    param_cp.interval        = p_scan_params->interval;
    param_cp.window          = p_scan_params->window;
    param_cp.own_bdaddr_type = LE_PUBLIC_ADDRESS;
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_SCAN_PARAMETERS, LE_SET_SCAN_PARAMETERS_CP_SIZE, &param_cp) < 0) {
        printf("Set scan parameters failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __hci_dev_set_extended_scan_parameters(TKL_BLE_GAP_SCAN_PARAMS_T CONST *p_scan_params)
{
    CHAR_T buf[10]                                  = { 0 };
    le_set_extended_scan_parameters_cp *scan_params = (le_set_extended_scan_parameters_cp *)buf;
    le_set_extended_scan_phy_info *phy_info         = (le_set_extended_scan_phy_info *)(buf + LE_SET_EXTENDED_SCAN_PARAMETERS_CP_SIZE);

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    scan_params->own_bdaddr_type = LE_PUBLIC_ADDRESS;
    scan_params->filter          = 0;
    scan_params->scanning_PHYs   = TKL_BLE_GAP_PHY_1MBPS;
    phy_info->type               = 0x01;
    phy_info->interval           = BLE_SCAN_ITVL_MS(p_scan_params->interval);
    phy_info->window             = BLE_SCAN_WIN_MS(p_scan_params->window);
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_EXTENDEDD_SCAN_PARAMETERS, 
                        LE_SET_EXTENDED_SCAN_PARAMETERS_CP_SIZE + LE_SET_EXTENDED_SCAN_PHY_INFO_SIZE, buf) < 0) {
        printf("Set scan parameters failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET hci_dev_set_scan_parameters(TKL_BLE_GAP_SCAN_PARAMS_T CONST *p_scan_params)
{
    if (hci_version == 0xFF) {
        __hci_dev_read_local_version(&hci_version);
    }

    if (hci_version <= 0x08) {
        return __hci_dev_set_scan_parameters(p_scan_params);
    } else {
        return __hci_dev_set_extended_scan_parameters(p_scan_params);
    }
}

STATIC OPERATE_RET __hci_dev_set_scan_rsp_data(TKL_BLE_DATA_T CONST *p_scan_rsp)
{
    le_set_scan_response_data_cp scan_rsp_cp;

    if (p_scan_rsp->length > 31) {
        return OPRT_OS_ADAPTER_INVALID_PARM;
    }

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&scan_rsp_cp, 0, sizeof(scan_rsp_cp));
    scan_rsp_cp.length = p_scan_rsp->length;
    memcpy(scan_rsp_cp.data, p_scan_rsp->p_data, p_scan_rsp->length);
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_SCAN_RESPONSE_DATA, LE_SET_SCAN_RESPONSE_DATA_CP_SIZE, &scan_rsp_cp) < 0) {
        printf("Set scan response data failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __hci_dev_set_extended_scan_rsp_data(TKL_BLE_DATA_T CONST *p_scan_rsp)
{
    CHAR_T buf[255 + 4]                                = { 0 };
    le_set_extended_scan_response_data_cp *scan_rsp_cp = (le_set_extended_scan_response_data_cp *)buf;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    scan_rsp_cp->handle              = ADV_HANDLE;
    scan_rsp_cp->operation           = 3;
    scan_rsp_cp->fragment_preference = 1;
    scan_rsp_cp->data_len            = p_scan_rsp->length;
    memcpy(scan_rsp_cp->data, p_scan_rsp->p_data, p_scan_rsp->length);
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_SET_EXTENDED_SCAN_RESPONSE_DATA, 
                        LE_SET_EXTENDED_SCAN_RESPONSE_DATA_CP_SIZE + p_scan_rsp->length, buf) < 0) {
        printf("Set scan response data failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET hci_dev_set_scan_rsp_data(TKL_BLE_DATA_T CONST *p_scan_rsp)
{
    if (hci_version == 0xFF) {
        __hci_dev_read_local_version(&hci_version);
    }

    if (hci_version <= 0x08) {
        return __hci_dev_set_scan_rsp_data(p_scan_rsp);
    } else {
        return __hci_dev_set_extended_scan_rsp_data(p_scan_rsp);
    }
}

STATIC OPERATE_RET __hci_dev_create_conn(TKL_BLE_GAP_ADDR_T CONST *p_peer_addr, TKL_BLE_GAP_SCAN_PARAMS_T CONST *p_scan_params, TKL_BLE_GAP_CONN_PARAMS_T CONST *p_conn_params)
{
    le_create_connection_cp create_conn_cp;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&create_conn_cp, 0, sizeof(create_conn_cp));
    create_conn_cp.interval         = p_scan_params->interval;
    create_conn_cp.window           = p_scan_params->window;
    create_conn_cp.initiator_filter = 0;
    create_conn_cp.peer_bdaddr_type = p_peer_addr->type;
    memcpy(create_conn_cp.peer_bdaddr.b, p_peer_addr->addr, 6);
    create_conn_cp.own_bdaddr_type     = LE_PUBLIC_ADDRESS;
    create_conn_cp.min_interval        = p_conn_params->conn_interval_min;
    create_conn_cp.max_interval        = p_conn_params->conn_interval_max;
    create_conn_cp.latency             = p_conn_params->conn_latency;
    create_conn_cp.supervision_timeout = p_conn_params->conn_sup_timeout;
    create_conn_cp.min_ce_length       = 0;
    create_conn_cp.max_ce_length       = 0;
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_CREATE_CONN, LE_CREATE_CONN_CP_SIZE, &create_conn_cp) < 0) {
        printf("Create connection failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __hci_dev_extended_create_conn(TKL_BLE_GAP_ADDR_T CONST *p_peer_addr, TKL_BLE_GAP_SCAN_PARAMS_T CONST *p_scan_params, TKL_BLE_GAP_CONN_PARAMS_T CONST *p_conn_params)
{
    UINT8_T buf[30]                                  = { 0 };
    le_extended_create_connection_cp *create_conn_cp = (le_extended_create_connection_cp *)buf;
    le_extended_create_conn_phy *conn_phy            = (le_extended_create_conn_phy *)(buf + LE_EXTENDED_CREATE_CONN_CP_SIZE);

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    create_conn_cp->filter_policy  = 0;
    create_conn_cp->own_addr_type  = LE_PUBLIC_ADDRESS;
    create_conn_cp->peer_addr_type = p_peer_addr->type;
    memcpy(create_conn_cp->peer_addr, p_peer_addr->addr, 6);
    create_conn_cp->phys    = 1;
    conn_phy->scan_interval = p_scan_params->interval;
    conn_phy->scan_window   = p_scan_params->window;
    conn_phy->min_interval  = p_conn_params->conn_interval_min;
    conn_phy->max_interval  = p_conn_params->conn_interval_max;
    conn_phy->latency       = p_conn_params->conn_latency;
    conn_phy->supv_timeout  = p_conn_params->conn_sup_timeout;
    conn_phy->min_length    = 0;
    conn_phy->max_length    = 0;
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_EXTENDED_CREATE_CONN, 
                        LE_EXTENDED_CREATE_CONN_CP_SIZE + LE_EXTENDED_CREATE_CONN_PHY_SIZE, buf) < 0) {
        printf("Create connection failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET hci_dev_create_conn(TKL_BLE_GAP_ADDR_T CONST *p_peer_addr, TKL_BLE_GAP_SCAN_PARAMS_T CONST *p_scan_params, TKL_BLE_GAP_CONN_PARAMS_T CONST *p_conn_params)
{
    if (hci_version == 0xFF) {
        __hci_dev_read_local_version(&hci_version);
    }

    if (hci_version <= 0x08) {
        return __hci_dev_create_conn(p_peer_addr, p_scan_params, p_conn_params);
    } else {
        return __hci_dev_extended_create_conn(p_peer_addr, p_scan_params, p_conn_params);
    }
}

OPERATE_RET hci_dev_disconnect(UINT16_T conn_handle, UINT8_T hci_reason)
{
    disconnect_cp cp;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&cp, 0, sizeof(cp));
    cp.handle = conn_handle;
    cp.reason = hci_reason;
    if (__hci_send_cmd(g_dd, OGF_LINK_CTL, OCF_DISCONNECT, DISCONNECT_CP_SIZE, &cp) < 0) {
        printf("Disconnect failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET hci_dev_conn_update(UINT16_T conn_handle, TKL_BLE_GAP_CONN_PARAMS_T CONST *p_conn_params)
{
    le_connection_update_cp cp;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    memset(&cp, 0, sizeof(cp));
    cp.handle              = conn_handle;
    cp.min_interval        = p_conn_params->conn_interval_min;
    cp.max_interval        = p_conn_params->conn_interval_max;
    cp.latency             = p_conn_params->conn_latency;
    cp.supervision_timeout = p_conn_params->conn_sup_timeout;
    cp.min_ce_length       = 1;
    cp.max_ce_length       = 1;
    if (__hci_send_cmd(g_dd, OGF_LE_CTL, OCF_LE_CONN_UPDATE, LE_CONN_UPDATE_CP_SIZE, &cp) < 0) {
        printf("Connection update failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET hci_dev_read_rssi(UINT16_T handle)
{
    read_rssi_rp rp;
    struct hci_request rq;
    INT_T ret;

    if (g_dd < 0) {
        g_dd = __hci_open_dev(HDEV);
        if (g_dd < 0) {
            printf("Can't open HCI device.\n");
            return OPRT_OS_ADAPTER_COM_ERROR;
        }
    }

    if (__hci_send_cmd(g_dd, OGF_STATUS_PARAM, OCF_READ_RSSI, 2, &handle) < 0) {
        printf("Read rssi failed\n");
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}

STATIC VOID_T __hci_evt_callback(TKL_BLE_GAP_EVT_TYPE_E type, INT_T result)
{
    TKL_BLE_GAP_PARAMS_EVT_T init_event;

    init_event.type   = type;
    init_event.result = result;
    if (g_gap_evt_cb) {
        g_gap_evt_cb(&init_event);
    }
}

STATIC VOID_T __le_conn_complete_evt(VOID_T *data, UINT8_T size)
{
    evt_le_connection_complete *evt = data;
    TKL_BLE_GAP_PARAMS_EVT_T conn_evt;

    conn_evt.type                   = TKL_BLE_GAP_EVT_CONNECT;
    conn_evt.result                 = evt->status;
    conn_evt.conn_handle            = evt->handle;
    conn_evt.gap_event.connect.role = TKL_BLE_ROLE_SERVER;

    if (g_gap_evt_cb) {
        g_gap_evt_cb(&conn_evt);
    }
}

STATIC VOID_T __le_adv_report_evt(VOID_T *data, UINT8_T size)
{
    le_advertising_info *evt = data;
    TKL_BLE_GAP_PARAMS_EVT_T adv_evt;
    UINT8_T evt_len;

    adv_evt.type        = TKL_BLE_GAP_EVT_ADV_REPORT;
    adv_evt.result      = 0;
    adv_evt.conn_handle = 0;

    if ((evt->evt_type == ADV_REPORT_EVT_TYPE_ADV_IND) || (evt->evt_type == ADV_REPORT_EVT_TYPE_ADV_NO_CONN_IND)) {
        adv_evt.gap_event.adv_report.adv_type = TKL_BLE_ADV_DATA;
    } else if (evt->evt_type == ADV_REPORT_EVT_TYPE_SCAN_RSP) {
        adv_evt.gap_event.adv_report.adv_type = TKL_BLE_RSP_DATA;
    }
    adv_evt.gap_event.adv_report.rssi        = evt->data[evt->length];
    adv_evt.gap_event.adv_report.data.length = evt->length;
    adv_evt.gap_event.adv_report.data.p_data = evt->data;
    memcpy(adv_evt.gap_event.adv_report.peer_addr.addr, evt->bdaddr.b, 6);
    adv_evt.gap_event.adv_report.peer_addr.type = evt->bdaddr_type;

    if (g_gap_evt_cb) {
        g_gap_evt_cb(&adv_evt);
    }
}

STATIC VOID_T __le_conn_update_complete_evt(VOID_T *data, UINT8_T size)
{
    evt_le_connection_update_complete *evt = data;
    TKL_BLE_GAP_PARAMS_EVT_T conn_update_evt;

    conn_update_evt.type                                   = TKL_BLE_GAP_EVT_CONN_PARAM_UPDATE;
    conn_update_evt.result                                 = 0;
    conn_update_evt.conn_handle                            = evt->handle;
    conn_update_evt.gap_event.conn_param.conn_interval_min = evt->interval;
    conn_update_evt.gap_event.conn_param.conn_interval_max = evt->interval;
    conn_update_evt.gap_event.conn_param.conn_latency      = evt->latency;
    conn_update_evt.gap_event.conn_param.conn_sup_timeout  = evt->supervision_timeout;

    if (g_gap_evt_cb) {
        g_gap_evt_cb(&conn_update_evt);
    }
}

STATIC VOID_T __le_enhanced_conn_complete_evt(VOID_T *data, UINT8_T size)
{
    evt_le_enhanced_conn_complete *evt = data;
    TKL_BLE_GAP_PARAMS_EVT_T conn_evt;

    conn_evt.type                   = TKL_BLE_GAP_EVT_CONNECT;
    conn_evt.result                 = evt->status;
    conn_evt.conn_handle            = evt->handle;
    conn_evt.gap_event.connect.role = TKL_BLE_ROLE_SERVER;

    if (g_gap_evt_cb) {
        g_gap_evt_cb(&conn_evt);
    }
}

STATIC VOID_T __le_ext_adv_report_evt(VOID_T *data, UINT8_T size)
{
    evt_le_ext_advertising_info *evt = data;
    le_ext_advertising_info *report;
    TKL_BLE_GAP_PARAMS_EVT_T adv_evt;

    data += sizeof(evt->num_reports);

    for (INT_T i = 0; i < evt->num_reports; ++i) {
        report              = data;
        adv_evt.type        = TKL_BLE_GAP_EVT_ADV_REPORT;
        adv_evt.result      = 0;
        adv_evt.conn_handle = 0;

        if ((report->event_type == 0x13) || (report->event_type == 0x10)) {
            adv_evt.gap_event.adv_report.adv_type = TKL_BLE_ADV_DATA;
        } else if ((report->event_type == 0x1A) || (report->event_type == 0x1B)) {
            adv_evt.gap_event.adv_report.adv_type = TKL_BLE_RSP_DATA;
        }

        adv_evt.gap_event.adv_report.rssi        = report->rssi;
        adv_evt.gap_event.adv_report.data.length = report->data_len;
        adv_evt.gap_event.adv_report.data.p_data = report->data;
        memcpy(adv_evt.gap_event.adv_report.peer_addr.addr, report->direct_addr, 6);
        adv_evt.gap_event.adv_report.peer_addr.type = report->direct_addr_type;

        if (g_gap_evt_cb) {
            g_gap_evt_cb(&adv_evt);
        }

        data += sizeof(le_ext_advertising_info);
        data += report->data_len;
    }
}

struct subevent_data {
    UINT8_T subevent;
    CONST CHAR_T *str;
    VOID_T (*func) (VOID_T *data, UINT8_T size);
};

STATIC CONST struct subevent_data le_meta_event_table[] = {
    { EVT_LE_CONN_COMPLETE, "LE Connection Complete", __le_conn_complete_evt },
    { EVT_LE_ADVERTISING_REPORT, "LE Advertising Report", __le_adv_report_evt },
    { EVT_LE_CONN_UPDATE_COMPLETE, "LE Connection Update Complete", __le_conn_update_complete_evt },
    { EVT_LE_ENHANCED_CONN_COMPLETE, "LE Enhanced Connection Complete", __le_enhanced_conn_complete_evt },
    { EVT_LE_EXT_ADVERTISING_REPORT, "LE Extended Advertising Report", __le_ext_adv_report_evt },
    {}
};

STATIC VOID_T __disconnect_complete_evt(VOID_T *data, UINT8_T size)
{
    evt_disconn_complete *evt = data;
    TKL_BLE_GAP_PARAMS_EVT_T disc_evt;

    disc_evt.type                        = TKL_BLE_GAP_EVT_DISCONNECT;
    disc_evt.conn_handle                 = evt->handle;
    disc_evt.result                      = 0;
    disc_evt.gap_event.disconnect.reason = evt->reason;
    // disc_evt.gap_event.disconnect.role = TKL_BLE_ROLE_CLIENT;

    if (g_gap_evt_cb) {
        g_gap_evt_cb(&disc_evt);
    }
}

STATIC VOID_T __cmd_complete_evt(VOID_T *data, UINT8_T size)
{
    evt_cmd_complete *evt = data;

    if (evt->opcode == cmd_opcode_pack(OGF_STATUS_PARAM, OCF_READ_RSSI)) {
        read_rssi_rp *rp = data + EVT_CMD_COMPLETE_SIZE;
        TKL_BLE_GAP_PARAMS_EVT_T rssi_evt;
        rssi_evt.type                = TKL_BLE_GAP_EVT_CONN_RSSI;
        rssi_evt.result              = 0;
        rssi_evt.conn_handle         = rp->handle;
        rssi_evt.gap_event.link_rssi = rp->rssi;

        if (g_gap_evt_cb) {
            g_gap_evt_cb(&rssi_evt);
        }
    }
}

STATIC VOID_T __le_meta_event_evt(VOID_T *data, UINT8_T size)
{
    UINT8_T subevent = *((UINT8_T *)data);

    for (INT_T i = 0; le_meta_event_table[i].str; i++) {
        if (le_meta_event_table[i].subevent == subevent) {
            le_meta_event_table[i].func(data + 1, size - 1);
            break;
        }
    }
}

struct event_data {
    UINT8_T event;
    CONST CHAR_T *str;
    VOID_T (*func) (VOID_T *data, UINT8_T size);
};

STATIC CONST struct event_data event_table[] = {
    { EVT_DISCONN_COMPLETE, "Disconnect Complete", __disconnect_complete_evt },
    { EVT_CMD_COMPLETE, "Command Complete", __cmd_complete_evt },
    { EVT_LE_META_EVENT, "LE Meta Event", __le_meta_event_evt },
    {}
};

STATIC VOID_T __hci_evt_handler(UCHAR_T *data, UINT_T len)
{
    hci_event_hdr *hdr = (hci_event_hdr *)data;

    if (len < HCI_EVENT_HDR_SIZE) {
        return;
    }

    data += HCI_EVENT_HDR_SIZE;
    len -= HCI_EVENT_HDR_SIZE;

    for (INT_T i = 0; event_table[i].str; i++) {
        if (event_table[i].event == hdr->evt) {
            event_table[i].func(data, hdr->plen);
            break;
        }
    }
}

STATIC VOID_T *__hci_task(VOID_T *arg)
{
    UCHAR_T buf[HCI_MAX_FRAME_SIZE];
    SIZE_T len;

    while (1) {
        if (hci_task_enable) {
            len = read(g_dd, buf, HCI_MAX_FRAME_SIZE);
            if (len < 0) {
                break;
            }

            switch (buf[0]) {
            case HCI_COMMAND_PKT:
                break;
            case HCI_EVENT_PKT:
                __hci_evt_handler(buf + 1, len - 1);
                break;
            case HCI_ACLDATA_PKT:
                break;
            case HCI_SCODATA_PKT:
                break;
            default:
                break;
            }
        }
    }

    return NULL;
}

OPERATE_RET hci_dev_gap_callback_register(CONST TKL_BLE_GAP_EVT_FUNC_CB gap_evt)
{
    g_gap_evt_cb = gap_evt;
    pthread_create(&hci_task_thId, NULL, __hci_task, NULL);

    return OPRT_OK;
}


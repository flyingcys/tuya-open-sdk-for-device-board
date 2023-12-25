#ifndef BLUETOOTH_API_H__
#define BLUETOOTH_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tkl_bluetooth.h"

typedef struct {
    CHAR_T name[16];
    CHAR_T bd_addr[6];
    INT8_T rssi;
} ble_device_t;

// SD_BUS
OPERATE_RET sd_bus_init(VOID_T);
OPERATE_RET sd_bus_loop_start(VOID_T);
OPERATE_RET sd_bus_deinit(VOID_T);
OPERATE_RET sd_bus_gatts_service_add(TKL_BLE_GATTS_PARAMS_T *p_service);
OPERATE_RET sd_bus_gatts_value_set(USHORT_T conn_handle, USHORT_T char_handle, UCHAR_T *p_data, USHORT_T length);
OPERATE_RET sd_bus_gatts_value_notify(USHORT_T conn_handle, USHORT_T char_handle, UCHAR_T *p_data, USHORT_T length);
OPERATE_RET sd_bus_gatt_callback_register(CONST TKL_BLE_GATT_EVT_FUNC_CB gatt_evt);

// HCI
OPERATE_RET hci_dev_up(VOID_T);
OPERATE_RET hci_dev_down(VOID_T);
OPERATE_RET hci_dev_get_addr(UINT8_T *addr);
OPERATE_RET hci_dev_set_scan_enable(UINT8_T enable);
OPERATE_RET hci_dev_set_advertise_enable(UINT8_T enable);
OPERATE_RET hci_dev_set_adv_parameters(TKL_BLE_GAP_ADV_PARAMS_T CONST *p_adv_params);
OPERATE_RET hci_dev_set_adv_data(TKL_BLE_DATA_T CONST *p_adv);
OPERATE_RET hci_dev_set_scan_parameters(TKL_BLE_GAP_SCAN_PARAMS_T CONST *p_scan_params);
OPERATE_RET hci_dev_set_scan_rsp_data(TKL_BLE_DATA_T CONST *p_scan_rsp);
OPERATE_RET hci_dev_create_conn(TKL_BLE_GAP_ADDR_T CONST *p_peer_addr, TKL_BLE_GAP_SCAN_PARAMS_T CONST *p_scan_params, TKL_BLE_GAP_CONN_PARAMS_T CONST *p_conn_params);
OPERATE_RET hci_dev_disconnect(uint16_t conn_handle, uint8_t hci_reason);
OPERATE_RET hci_dev_conn_update(uint16_t conn_handle, TKL_BLE_GAP_CONN_PARAMS_T CONST *p_conn_params);
OPERATE_RET hci_dev_read_rssi(uint16_t handle);
OPERATE_RET hci_dev_gap_callback_register(CONST TKL_BLE_GAP_EVT_FUNC_CB gap_evt);

#define IF_FAIL_RETURN(x)       \
    do {                        \
        if ((op_ret = x) < 0) { \
            return op_ret;      \
        }                       \
    } while (0)

#define IF_FAIL_RETURN_FREE(x, y) \
    do {                          \
        if ((op_ret = x) < 0) {   \
            free(y);              \
            y = NULL;             \
            return op_ret;        \
        }                         \
    } while (0)

#define IF_FAIL_RETURN_INFO(x, info) \
    do {                             \
        if ((op_ret = x) < 0) {      \
            printf(info);            \
            return op_ret;           \
        }                            \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif

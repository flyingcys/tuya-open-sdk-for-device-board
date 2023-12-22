#if (CFG_USE_BK_HOST)
/**
 ****************************************************************************************
 *
 * @file hci.h
 *
 * @brief This file contains definitions related to the HCI module.
 *
 * Copyright (C) RivieraWaves 2009-2015
 *
 *
 ****************************************************************************************
 */

#ifndef HCI_H_
#define HCI_H_

/**
 ****************************************************************************************
 * @addtogroup HCI Host Controller Interface
 * @ingroup ROOT
 * @brief HCI module handling communication between lower and higher layers in split
 * architecture.
 * @{
 ****************************************************************************************
 */


/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "rwip_config.h"       // SW configuration

#if (HCI_PRESENT)

#include <stddef.h>          // standard definition
#include <stdint.h>          // standard integer
#include "common_bt.h"           // BT standard definitions

#include "rwip_task.h"       // Task definitions

/*
 * DEFINES
 ****************************************************************************************
 */

#if (BLE_EMB_PRESENT || BLE_HOST_PRESENT)
#if (BLE_CENTRAL || BLE_PERIPHERAL)
#define HCI_BLE_CON_SUPPORT   1
#else // (BLE_CENTRAL || BLE_PERIPHERAL)
#define HCI_BLE_CON_SUPPORT   0
#endif // (BLE_CENTRAL || BLE_PERIPHERAL)
#else //(BLE_EMB_PRESENT || BLE_HOST_PRESENT)
#define HCI_BLE_CON_SUPPORT   0
#endif //(BLE_EMB_PRESENT || BLE_HOST_PRESENT)

/// Length of HCI Reset Message
#define HCI_RESET_MSG_LEN     4

/// HCI Reset Message use to resync.
#define HCI_RESET_MSG_BUF    {HCI_CMD_MSG_TYPE, (HCI_RESET_CMD_OPCODE & 0xFF), ((HCI_RESET_CMD_OPCODE >> 8) & 0xFF), 0}

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/// Message API of the HCI task
/*@TRACE*/
enum hci_msg_id
{
    HCI_MSG_ID_FIRST = TASK_BLE_FIRST_MSG(TASK_BLE_ID_HCI),

    HCI_CMD_CMP_EVENT,
    HCI_CMD_STAT_EVENT,
    HCI_EVENT,
    HCI_LE_EVENT,

    HCI_COMMAND,

    #if (HCI_BLE_CON_SUPPORT || BT_EMB_PRESENT)
    HCI_ACL_DATA,
    #endif // (HCI_BLE_CON_SUPPORT)

    #if BT_EMB_PRESENT
    #if VOICE_OVER_HCI
    HCI_SYNC_DATA,
    #endif //VOICE_OVER_HCI
    #endif //BT_EMB_PRESENT

    HCI_DBG_EVT,

    HCI_MSG_ID_LAST
};

/// Status of HCI command header processing
enum HCI_CMD_HDR
{
    /// Header is correct
    HCI_CMD_HDR_STATUS_OK,
    /// Opcode is unknown
    HCI_CMD_HDR_STATUS_UNKNOWN,
    /// Header is not correct
    HCI_CMD_HDR_STATUS_FAIL
};



///HCI Command header components structure
struct hci_cmd_hdr
{
    /// Opcode field
    uint16_t opcode;
    ///Parameter length - the number of bytes of the command parameters
    uint8_t parlen;
};

///HCI ACL data packets header structure
struct hci_acl_hdr
{
    ///Connection handle & Data Flags
    uint16_t hdl_flags;
    ///Data length in number of bytes
    uint16_t datalen;
};

#if (BT_EMB_PRESENT)
#if (VOICE_OVER_HCI)
///HCI synchronous data packets header structure
struct hci_sync_hdr
{
    /// Connection handle & Data Flags
    uint16_t conhdl_flags;
    /// Data total length in number of bytes
    uint8_t data_total_len;
};
#endif // (VOICE_OVER_HCI)
#endif // (BT_EMB_PRESENT)

///HCI Event header components structure - contains all details possible in an event
struct hci_evt_hdr
{
    ///Event code
    uint8_t  code;
    ///Event parameters length
    uint8_t  parlen;
};

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */


/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
****************************************************************************************
* @brief Initialize HCI (including transport)
*
* @param[in] init_type  Type of initialization (@see enum rwip_init_type)
*****************************************************************************************
*/
void hci_init(uint8_t init_type);

#if (BLE_EMB_PRESENT || BT_EMB_PRESENT)
/**
 ****************************************************************************************
 * @brief Function called when an internal task needs to send a HCI message to Host
 *
 * This function decides whether the message is sent externally onto HCI Transport Layer
 * or redirected into an internal task of the other side of the HCI.
 *
 * The input message ID, length and parameters must be filled.
 * In case the message is an HCI command or event, the source ID must be filled with the
 * command opcode or event code.
 * In case the message concerns a particular BT or BLE link, the destination ID must be
 * filled with the associated link ID.
 *
 * @param[in]  param   Pointer to the parameters of the Kernel message carrying the HCI message
 *****************************************************************************************
 */
void hci_send_2_host(void *param);
#endif // (BLE_EMB_PRESENT || BT_EMB_PRESENT)

#if BLE_HOST_PRESENT
/**
 ****************************************************************************************
 * @brief Function called when an internal task needs to send a HCI message to Controller
 *
 * This function decides whether the message is sent externally onto HCI Transport Layer
 * or redirected into an internal task of the other side of the HCI.
 *
 * The input message ID, length and parameters must be filled.
 * In case the message is an HCI command or event, the source ID must be filled with the
 * command opcode or event code.
 * In case the message concerns a particular BT or BLE link, the destination ID must be
 * filled with the associated link ID.
 *
 * @param[in]  param   Pointer to the parameters of the Kernel message carrying the HCI message
 *****************************************************************************************
 */
void hci_send_2_controller(void *param);

/**
 ****************************************************************************************
 * @brief function used to send a basic command, without parameters to the controller.
 *
 * @param[in] opcode Operation code of the command
 *****************************************************************************************
 */
void hci_basic_cmd_send_2_controller(uint16_t opcode);
#endif //BLE_HOST_PRESENT

#if (BLE_EMB_PRESENT && HCI_BLE_CON_SUPPORT)
/**
 ****************************************************************************************
 * @brief Register connection handle for a BLE connection
 *
 * @param[in]  link_id  BLE connection link ID
 *****************************************************************************************
 */
void hci_ble_conhdl_register(uint8_t link_id);

/**
 ****************************************************************************************
 * @brief Unregister a BLE connection
 *
 * @param[in]  link_id  BLE connection link ID
 *****************************************************************************************
 */
void hci_ble_conhdl_unregister(uint8_t link_id);
#endif //(BLE_EMB_PRESENT && !BLE_HOST_PRESENT && HCI_BLE_CON_SUPPORT)

#if (BT_EMB_PRESENT)
/**
 ****************************************************************************************
 * @brief Register BD address for a BT ACL connection
 *
 * @param[in]  link_id  BT ACL connection link ID
 * @param[in]  bd_addr  Pointer to the device BD address associated to the connection
 *****************************************************************************************
 */
void hci_bt_acl_bdaddr_register(uint8_t link_id, struct bd_addr* bd_addr);
/**
 ****************************************************************************************
 * @brief Register connection handle for a BT ACL connection
 *
 * @param[in]  link_id  BT ACL connection link ID
 *****************************************************************************************
 */
void hci_bt_acl_conhdl_register(uint8_t link_id);

/**
 ****************************************************************************************
 * @brief Unregister a BT ACL connection
 *
 * @param[in]  link_id  BT ACL connection link ID
 *****************************************************************************************
 */
void hci_bt_acl_bdaddr_unregister(uint8_t link_id);

#endif //(BT_EMB_PRESENT)

/**
 ****************************************************************************************
 * @brief Set the event mask
 *
 * @param[in] evt_msk Pointer to the new event mask
 * @param[in] page indicate which event page should be changed
 *
 * @return The status of the event mask saving
 *****************************************************************************************
 */
uint8_t hci_evt_mask_set(struct evt_mask const *evt_msk, uint8_t page);

#if (BT_EMB_PRESENT)
/**
 ****************************************************************************************
 * @brief Add an event filter according to the parameters of the HCI command
 *
 * Note: the consistency of the parameters according to the input has already been checked by HCI during the special
 * unpacking.
 *
 * @param[in] param      Pointer to the HCI parameter
 *
 * @return The status of the filter addition
 *****************************************************************************************
 */
uint8_t hci_evt_filter_add(struct hci_set_evt_filter_cmd const *param);

#if (MAX_NB_SYNC > 0)
/**
 ****************************************************************************************
 * @brief Get voice setting (for SCO auto-accept via event filter)
 *
 * @return   Voice settings
 *****************************************************************************************
 */
uint16_t hci_voice_settings_get(void);

/**
 ****************************************************************************************
 * @brief Set voice setting (for SCO auto-accept via event filter)
 *
 * @param[in]   voice_settings    Voice settings
 *
 * @return   Status (0: Success | Others: failure)
 *****************************************************************************************
 */
uint8_t hci_voice_settings_set(uint16_t voice_settings);
#endif // (MAX_NB_SYNC > 0)
#endif //(BT_EMB_PRESENT)

#if (HCI_TL_SUPPORT)
#if  (BLE_EMB_PRESENT || BT_EMB_PRESENT)
/**
 ****************************************************************************************
 * @brief Get the maximum parameter size for a specific command
 *
 * This function is used by TL to know the theoretical maximum parameters size for a
 * specific HCI command.
 * Note: if the command is not supported by HCI (unknown), the maximum possible value of
 * 255 bytes is returned.
 *
 * @param[in]  opcode  Opcode received
 *
 * @return     The command maximum parameters size / 255 if command is unknown
 *****************************************************************************************
 */
uint8_t hci_cmd_get_max_param_size(uint16_t opcode);

/**
 ****************************************************************************************
 * @brief Indicates that a HCI command has been received
 *
 * This function is used by TL to indicate the reception of a HCI command.
 *
 * @param[in]  opcode   Command Opcode
 * @param[in]  length   Parameters length
 * @param[in]  payload  Pointer to payload
 *****************************************************************************************
 */
void hci_cmd_received(uint16_t opcode, uint8_t length, uint8_t *payload);

/**
 ****************************************************************************************
 * @brief Allocates the reception buffer for ACL TX data
 *
 * @param[in]   hdl_flags Connection handle and data flags from HCI ACL packet header
 * @param[in]   len       Length to receive (from HCI ACL packet header)
 *
 * @return   Buffer for data reception (NULL if not possible to allocate one)
 *****************************************************************************************
 */
uint8_t* hci_acl_tx_data_alloc(uint16_t hdl_flags, uint16_t len);

/**
 ****************************************************************************************
 * @brief Indicates that a HCI ACL TX data packet has been received
 *
 * This function is used by TL to indicate the reception of a HCI ACL TX data.
 *
 * @param[in]   hdl_flags Connection handle and data flags from HCI ACL packet header
 * @param[out]  datalen   Data length
 * @param[in]   payload   Pointer to payload
 *****************************************************************************************
 */
void hci_acl_tx_data_received(uint16_t hdl_flags, uint16_t datalen, uint8_t * payload);

#if (BT_EMB_PRESENT)
#if (VOICE_OVER_HCI)
/**
 ****************************************************************************************
 * @brief Allocates the reception buffer for Sync TX data
 *
 * @param[in]   conhdl_flags    Connection handle and data flags from HCI Sync packet header
 * @param[in]   len             Length to receive (from HCI Sync packet header)
 *
 * @return   Buffer for data reception (NULL if not possible to allocate one)
 *****************************************************************************************
 */
uint8_t* hci_sync_tx_data_alloc(uint16_t conhdl_flags, uint8_t len);

/**
 ****************************************************************************************
 * @brief Indicates that a HCI Sync TX data packet has been received
 *
 * This function is used by TL to indicate the reception of a HCI Sync TX data.
 *
 * @param[in]   conhdl_flags    Connection handle and data flags from HCI Sync packet header
 * @param[in]   len             Length to receive (from HCI Sync packet header)
 * @param[in]   payload         Pointer to payload
 *****************************************************************************************
 */
void hci_sync_tx_data_received(uint16_t conhdl_flags, uint8_t len, uint8_t * payload);
#endif // (VOICE_OVER_HCI)
#endif // (BT_EMB_PRESENT)
#endif // (BLE_EMB_PRESENT || BT_EMB_PRESENT)

#if ((BLE_HOST_PRESENT) && (!BLE_EMB_PRESENT))
/**
 ****************************************************************************************
 * @brief Allocates the reception buffer for ACL RX data
 *
 * @param[in]   hdl_flags Connection handle and data flags from HCI ACL RX packet header
 * @param[in]   len       Length to receive (from HCI ACL packet header)
 *
 * @return   Buffer for data reception (NULL if not possible to allocate one)
 *****************************************************************************************
 */
uint8_t* hci_acl_rx_data_alloc(uint16_t hdl_flags, uint16_t len);

/**
 ****************************************************************************************
 * @brief Indicates that a HCI ACL RX data packet has been received
 *
 * This function is used by TL to indicate the reception of a HCI ACL RX data.
 *
 * @param[in]   hdl_flags Connection handle and data flags from HCI ACL packet header
 * @param[out]  datalen   Data length
 * @param[in]   payload   Pointer to payload
 *****************************************************************************************
 */
void hci_acl_rx_data_received(uint16_t hdl_flags, uint16_t datalen, uint8_t * payload);

/**
 ****************************************************************************************
 * @brief Indicates that a HCI event has been received
 *
 * This function is used by TL to indicate the reception of a HCI event.
 *
 * @param[in]  code     Event code
 * @param[in]  length   Parameters length
 * @param[in]  payload  Pointer to payload
 *
 * @return status of receive operation
 *****************************************************************************************
 */
uint8_t hci_evt_received(uint8_t code, uint8_t length, uint8_t *payload);
#endif // ((BLE_HOST_PRESENT) && (!BLE_EMB_PRESENT))

#endif // HCI_TL_SUPPORT

//common for both BLE & BT
/**
 ****************************************************************************************
 * @brief  process HostBufferSize
 *
 * @param[in] acl_pkt_len     ACL packet length
 * @param[in] nb_acl_pkts     Number of ACL packets
 *
 * @return status
 *****************************************************************************************
 */
uint8_t hci_fc_acl_buf_size_set(uint16_t acl_pkt_len, uint16_t nb_acl_pkts);
/**
 ****************************************************************************************
 * @brief  process HostBufferSize
 *
 * @param[in] sync_pkt_len    SYNC packet length
 * @param[in] nb_sync_pkts    Number of SYNC packets
 *
 * @return status
 *****************************************************************************************
 */
uint8_t hci_fc_sync_buf_size_set(uint8_t sync_pkt_len, uint16_t nb_sync_pkts);

/**
 ****************************************************************************************
 * @brief set the state of the ACL flow control
 *
 * @param[in] flow_enable   boolean state of control
 *
 * @return status
 *****************************************************************************************
 */
uint8_t hci_fc_acl_en(bool flow_enable);

/**
 ****************************************************************************************
 * @brief set the state of the SYNC flow control
 *
 * @param[in] flow_enable   boolean state of control
 *****************************************************************************************
 */
void hci_fc_sync_en(bool flow_enable);

/**
 ****************************************************************************************
 * @brief update data packet counters according to HostNumberOfCompletePackets
 *
 * @param[in] acl_pkt_nb     accumulated number for ACL handles
 ***************************************************************************************a**
 */
void hci_fc_host_nb_acl_pkts_complete(uint16_t acl_pkt_nb);

/**
 ****************************************************************************************
 * @brief update data packet counters according to HostNumberOfCompletePackets
 *
 * @param[in] sync_pkt_nb    accumulated number for SCO handles
 ***************************************************************************************a**
 */
void hci_fc_host_nb_sync_pkts_complete(uint16_t sync_pkt_nb);

#endif //HCI_PRESENT

/// @} HCI

#endif // HCI_H_

#else
#ifndef _BK72XX_HCI_EX_H_
#define _BK72XX_HCI_EX_H_

#include "rtos_pub.h"

#include "string.h"
#include "common_error.h"        // error definition
#include "common_endian.h"       // common endianess definition
#include "common_list.h"         // list definition

#include "uart_pub.h"
#include "common_bt_defines.h"
#include "rwip_config.h"       // SW configuration

#include <stddef.h>          // standard definition
#include <stdint.h>          // standard integer
#include "common_bt.h"           // BT standard definitions

#include "rwip_task.h"       // Task definitions

/*
 * DEFINES
 ****************************************************************************************
 */

/**
 * CONFIG_BT_RX_BUF_LEN: the max length for rx buffer
 * range 73 to 2000
 */
#ifndef CONFIG_BT_RX_BUF_LEN
#define CONFIG_BT_RX_BUF_LEN     250
#endif

#define _DEVICE_NAME_MAX_LEN      (18)

#define HCI_RECV_BUF_LEN	CONFIG_BT_RX_BUF_LEN

#ifndef HCI_LOG
#define HCI_LOG		os_printf
#endif
#ifndef HCI_INFO
#define HCI_INFO	HCI_LOG
#endif
#ifndef HCI_DBG
#define HCI_DBG		HCI_LOG
#endif

#define HCI_MALLOC(size)   os_malloc(size)
#define HCI_FREE(ptr)      os_free(ptr)

#ifndef CONFIG_BK_HCI_TASK_PRI
#define CONFIG_BK_HCI_TASK_PRI				(BEKEN_DEFAULT_WORKER_PRIORITY - 0)
#endif

#ifndef CONFIG_BK_HCI_RECV_PRIO_TASK_PRI
#define CONFIG_BK_HCI_RECV_PRIO_TASK_PRI	(BEKEN_DEFAULT_WORKER_PRIORITY - 0)
#endif

#ifndef CONFIG_BK_HCI_RECV_TASK_PRI
#define CONFIG_BK_HCI_RECV_TASK_PRI			(BEKEN_DEFAULT_WORKER_PRIORITY - 0)
#endif

#ifndef CONFIG_BK_HCI_TASK_SEM_TIMEROUT
#define CONFIG_BK_HCI_TASK_SEM_TIMEROUT		(SECONDS/10)
#endif

#define CONFIG_BK_HCI_RECV_PRIO_TASK_STACK_SIZE   3 * 1024
#define CONFIG_BK_HCI_RECV_TASK_STACK_SIZE        3 * 1024
//#define CONFIG_BK_HCI_TASK_STACK_SIZE             3 * 1024

/**
 * Destination field decoding
 *
 * bit |     7 6      | 5 4 | 3..0 |
 * def | Rsvd (pkupk) |  HL |  LL  |
 */
#define HCI_CMD_DEST_LL_POS           0
#define HCI_CMD_DEST_LL_MASK          0x0F
#define HCI_CMD_DEST_HL_POS           4
#define HCI_CMD_DEST_HL_MASK          0x30

/// Macro for building a command descriptor in full mode (without parameters packing/unpacking)
#define CMD(opcode, dest_ll, dest_hl)  {HCI_##opcode##_CMD_OPCODE, (dest_ll << HCI_CMD_DEST_LL_POS) | (dest_hl << HCI_CMD_DEST_HL_POS)}

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
/// Message API of the HCI task
/*@TRACE*/
enum hci_msg_id {
	HCI_MSG_ID_FIRST = TASK_BLE_FIRST_MSG(TASK_BLE_ID_HCI),
	HCI_CMD_CMP_EVENT,
	HCI_CMD_STAT_EVENT,
	HCI_EVENT,
	HCI_LE_EVENT,

	HCI_COMMAND,

	HCI_ACL_DATA,

#if BT_EMB_PRESENT
#if VOICE_OVER_HCI
	HCI_SYNC_DATA,
#endif //VOICE_OVER_HCI
#endif //BT_EMB_PRESENT

	HCI_DBG_EVT,

	HCI_MSG_ID_LAST
};

/// Possible destination field values within lower layers
enum HCI_MSG_DEST_LL {
	MNG,
	CTRL,
	BLE_MNG,
	BLE_CTRL,
	BT_MNG,
	BT_CTRL_CONHDL,
	BT_CTRL_BD_ADDR,
	BT_BCST,
	DBG,
	BLE_ISO,
	LL_UNDEF,
};

/// Possible destination field values within higher layers
enum HCI_MSG_DEST_HL {
	HL_MNG,
	HL_CTRL,
	HL_DATA,
	HL_ISO0,
	HL_UNDEF,
};

#pragma pack(push, 1) /* push current alignment to stack & set alignment to 1 byte boundary */
///HCI Command header components structure
struct hci_cmd_hdr {
	/// Opcode field
	uint16_t opcode;
	///Parameter length - the number of bytes of the command parameters
	uint8_t parlen;
};

///HCI ACL data packets header structure
struct hci_acl_hdr {
	///Connection handle & Data Flags
	uint16_t hdl_flags;
	///Data length in number of bytes
	uint16_t datalen;
};

#if (BT_EMB_PRESENT)
#if (VOICE_OVER_HCI)
///HCI synchronous data packets header structure
struct hci_sync_hdr {
	/// Connection handle & Data Flags
	uint16_t conhdl_flags;
	/// Data total length in number of bytes
	uint8_t data_total_len;
};
#endif // (VOICE_OVER_HCI)
#endif // (BT_EMB_PRESENT)

///HCI Event header components structure - contains all details possible in an event
struct hci_evt_hdr {
	///Event code
	uint8_t  code;
	///Event parameters length
	uint8_t  parlen;
};
#pragma pack(pop) /* restore original alignment from stack */

typedef struct bk_ble_hci_buf_s {
	/// list header
	struct common_list_hdr hdr;

	uint8_t type;
	uint16_t len;
	uint8_t buf[];
} bk_ble_hci_buf_t;

/// HCI command descriptor structure
struct hci_cmd_desc_tab_ref {
	/// OpCode Group Field (OGF)
	uint8_t ogf;

	/// Number of commands supported in this group
	uint16_t nb_cmds;

	/// Command descriptor table
	const struct hci_cmd_desc_tag* cmd_desc_tab;
};

/// HCI command descriptor structure
struct hci_cmd_desc_tag {
	/// Command opcode with flags indicating if a special packing or unpacking is needed
	uint16_t opcode;

	/// Destination field (used to find the internal destination task)
	uint8_t dest_field;
};

//hci event cb
typedef int (*hci_func_evt_cb)(uint8_t *p_buf, uint8_t buf_len);

/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Function called when an internal task needs to send a HCI message to Host
 *
 * This function decides whether the message is sent externally onto HCI Transport Layer
 * or redirected into an internal task of the other side of the HCI.
 *
 * The input message ID, length and parameters must be filled.
 * In case the message is an HCI command or event, the source ID must be filled with the
 * command opcode or event code.
 * In case the message concerns a particular BT or BLE link, the destination ID must be
 * filled with the associated link ID.
 *
 * @param[in]  param   Pointer to the parameters of the Kernel message carrying the HCI message
 *****************************************************************************************
 */

void hci_send_2_host(void *param);

/**
 ****************************************************************************************
 * @brief Function called when an internal task needs to send a HCI message to Controller
 *
 * This function decides whether the message is sent externally onto HCI Transport Layer
 * or redirected into an internal task of the other side of the HCI.
 *
 * The input message ID, length and parameters must be filled.
 * In case the message is an HCI command or event, the source ID must be filled with the
 * command opcode or event code.
 * In case the message concerns a particular BT or BLE link, the destination ID must be
 * filled with the associated link ID.
 *
 * @param[in]  param   Pointer to the parameters of the Kernel message carrying the HCI message
 *****************************************************************************************
 */
void hci_send_2_controller(void *param);

/**
 ****************************************************************************************
 * @brief Register connection handle for a BLE connection
 *
 * @param[in]  link_id  BLE connection link ID
 *****************************************************************************************
 */
void hci_ble_conhdl_register(uint8_t link_id);

/**
 ****************************************************************************************
 * @brief Unregister a BLE connection
 *
 * @param[in]  link_id  BLE connection link ID
 *****************************************************************************************
 */
void hci_ble_conhdl_unregister(uint8_t link_id);

/**
 ****************************************************************************************
 * @brief Set the event mask
 *
 * @param[in] evt_msk Pointer to the new event mask
 * @param[in] page indicate which event page should be changed
 *
 * @return The status of the event mask saving
 *****************************************************************************************
 */
uint8_t hci_evt_mask_set(struct evt_mask const *evt_msk, uint8_t page);

/**
 ****************************************************************************************
 * @brief set the state of the ACL flow control
 *
 * @param[in] flow_enable   boolean state of control
 *
 * @return status
 *****************************************************************************************
 */
uint8_t hci_fc_acl_en(bool flow_enable);

//common for both BLE & BT
/**
 ****************************************************************************************
 * @brief  process HostBufferSize
 *
 * @param[in] acl_pkt_len     ACL packet length
 * @param[in] nb_acl_pkts     Number of ACL packets
 *
 * @return status
 *****************************************************************************************
 */
uint8_t hci_fc_acl_buf_size_set(uint16_t acl_pkt_len, uint16_t nb_acl_pkts);

/**
 ****************************************************************************************
 * @brief update data packet counters according to HostNumberOfCompletePackets
 *
 * @param[in] acl_pkt_nb     accumulated number for ACL handles
 ***************************************************************************************a**
 */
void hci_fc_host_nb_acl_pkts_complete(uint16_t acl_pkt_nb);

int hci_driver_open(void);

int hci_driver_send(uint8_t type, uint16_t len, uint8_t *buf);

/**
 ****************************************************************************************
 * @brief set event cmd and acl data call back
 *
 * @param[in] sync_pkt_nb    accumulated number for SCO handles
 ***************************************************************************************a**
 */
void hci_set_event_callback(hci_func_evt_cb hci_cmd_cb, hci_func_evt_cb hci_acl_cb);

#endif
#endif

///Eof


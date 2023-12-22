/**
 ****************************************************************************************
 *
 * @file hci_tl.c
 *
 * @brief HCI module source file.
 *
 * Copyright (C) RivieraWaves 2009-2015
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup HCI
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "rwip_config.h"       // SW configuration

#if (HCI_PRESENT)

#if (HCI_TL_SUPPORT)

#include <string.h>          // string manipulation
#include "common_error.h"        // error definition
#include "common_utils.h"        // common utility definition
#include "common_endian.h"       // common endianess definition
#include "common_list.h"         // list definition

#include "hci.h"             // hci definition
#include "hci_int.h"         // hci internal definition

#include "kernel_msg.h"          // kernel message declaration
#include "kernel_task.h"         // kernel task definition
#include "kernel_mem.h"          // kernel memory definition
#include "kernel_timer.h"        // kernel timer definition
#include "rwip.h"            // rw bt core interrupt
#include "dbg.h"             // debug

#if (H4TL_SUPPORT)
#include "h4tl.h"            // H4TL definitions
#endif // H4TL_SUPPORT

#if BLE_EMB_PRESENT
#include "ble_util_buf.h"
#include "em_map.h"
#include "ble_reg_access.h"
#endif //BLE_EMB_PRESENT

#if BT_EMB_PRESENT
#include "bt_util_buf.h"
#include "em_map.h"
#include "ble_reg_access.h"
#endif //BT_EMB_PRESENT

#if ((BLE_HOST_PRESENT) && (!BLE_EMB_PRESENT))
#include "gapc.h"
#include "gap.h"
#endif // ((BLE_HOST_PRESENT) && (!BLE_EMB_PRESENT))

/*
 * DEFINES
 ****************************************************************************************
 */

/// Offset of the Kernel message parameters compared to the event packet position
#define HCI_CMD_PARAM_OFFSET               3
#define HCI_EVT_CC_PARAM_OFFSET            5
#define HCI_EVT_CS_PARAM_OFFSET            5
#define HCI_EVT_PARAM_OFFSET               2
#define HCI_EVT_LE_PARAM_OFFSET            2
#define HCI_EVT_DBG_PARAM_OFFSET           2


/*
 * ENUMERATIONS DEFINITIONS
 ****************************************************************************************
 */

#if (BLE_EMB_PRESENT)
#if (BLE_OBSERVER)
/// Status of an advertising report chain
enum adv_rep_chain_stat
{
    /// No chain
    HCI_ADV_REP_CHAIN_NO         = 0,
    /// A valid advertising report chain is ongoing
    HCI_ADV_REP_CHAIN_VALID      = 1,
    /// A filtered advertising report chain is ongoing
    HCI_ADV_REP_CHAIN_FILTERED   = 2,
};
#endif // (BLE_OBSERVER)
#endif // (BLE_EMB_PRESENT)

///HCI TX states
enum HCI_TX_STATE
{
    ///HCI TX Start State - when packet is ready to be sent
    HCI_STATE_TX_ONGOING,
    ///HCI TX Done State - TX ended with no error
    HCI_STATE_TX_IDLE
};


/*
 * STRUCTURES DEFINITIONS
 ****************************************************************************************
 */

///HCI Environment context structure
struct hci_tl_env_tag
{
    /// Queue of kernel messages corresponding to HCI TX packets
    struct common_list tx_queue;

    #if (BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)
    /// acl data messages
    struct common_list acl_queue;
    #endif //(BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)

    #if (BT_EMB_PRESENT)
    #if VOICE_OVER_HCI
    /// sco data messages
    struct common_list sync_queue;
    #endif //VOICE_OVER_HCI
    #endif //BT_EMB_PRESENT

    /// keep the link to message in transmission to a Host
    struct kernel_msg *curr_tx_msg;

    ///Tx state - either transmitting, done or error.
    uint8_t  tx_state;

    /// Allowed number of commands from Host to controller (sent from controller to host)
    int8_t  nb_h2c_cmd_pkts;

    /// This flag indicates that the current ACL payload is received in a trash buffer
    bool acl_trash_payload;

    #if (BLE_EMB_PRESENT)
    #if (BLE_OBSERVER)
    /// Counter of IQ report
    uint8_t iq_rep_cnt;

    /// Counter of advertising report fragments
    uint8_t adv_rep_frag_cnt;

    /// Status of an advertising report chain from 1M scan
    uint8_t adv_rep_chain_stat_1M;

    /// Status of an advertising report chain from coded scan
    uint8_t adv_rep_chain_stat_coded;

    /// Status of an advertising report chain from periodic scans
    uint8_t per_adv_rep_chain_stat[BLE_ACTIVITY_MAX];
    #endif // (BLE_OBSERVER)
    #endif // (BLE_EMB_PRESENT)
};

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

///HCI environment structure external global variable declaration
static struct hci_tl_env_tag hci_tl_env;

/*
 * LOCAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

static void hci_tx_done(void);

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

#if (BLE_EMB_PRESENT)
#if (BLE_OBSERVER)
/**
 ****************************************************************************************
 * @brief Filter advertising reports if exceeding the maximum capacity
 *
 * This function controls the flow of HCI advertising reports to the Host. It monitors
 * the number of accumulated reports. If the maximum number of reports has been exceeded,
 * new reports are trashed.
 *
 * @param[in]   msg    Kernel message containing the event
 * @return      True: message is filtered | False: message can be transmitted
 *****************************************************************************************
 */
static bool hci_ble_adv_report_filter_check(struct kernel_msg * msg)
{
    bool filtered = false;

    do
    {
        uint8_t chain_stat = HCI_ADV_REP_CHAIN_NO;
        uint8_t subcode = 0xFF;
        bool adv_rep = false;

        // Check if the message is an LE event
        if(msg->id != HCI_LE_EVENT)
            break;

        // Retrieve event subcode
        subcode = *((uint8_t *) msg->param);

        // Check event subcode HCI_LE_ADV_Report or HCI_LE_Ext_ADV_Report
        switch (subcode)
        {
            case HCI_LE_EXT_ADV_REPORT_EVT_SUBCODE:
            {
                // Point to event parameters
                struct hci_le_ext_adv_report_evt* evt = (struct hci_le_ext_adv_report_evt*) &msg->param;
                bool more_data = (GETF(evt->adv_rep[0].evt_type, ADV_EVT_DATA_STATUS) == ADV_EVT_DATA_STATUS_INCOMPLETE);

                chain_stat = (evt->adv_rep[0].phy == PHY_1MBPS_VALUE) ? hci_tl_env.adv_rep_chain_stat_1M : hci_tl_env.adv_rep_chain_stat_coded;

                // Check if report can be queued for transmission, or must be flushed
                if(   (chain_stat == HCI_ADV_REP_CHAIN_FILTERED)
                   || ((chain_stat == HCI_ADV_REP_CHAIN_NO) && (hci_tl_env.adv_rep_frag_cnt >= BLE_MAX_NB_ADV_REP_FRAG)) )
                {
                    filtered = true;
                }

                // Evaluate new chain status
                if (!more_data)
                {
                    chain_stat = HCI_ADV_REP_CHAIN_NO;
                }
                else
                {
                    chain_stat = filtered ? HCI_ADV_REP_CHAIN_FILTERED : HCI_ADV_REP_CHAIN_VALID;
                }

                // Update chain info for the scan PHY
                if(evt->adv_rep[0].phy == PHY_1MBPS_VALUE)
                {
                    hci_tl_env.adv_rep_chain_stat_1M = chain_stat;
                    DBG_SWDIAG(LEADVFILT, CHAIN_STAT_1M, chain_stat);
                }
                else
                {
                    hci_tl_env.adv_rep_chain_stat_coded = chain_stat;
                }

                adv_rep = true;
            }
            break;

            case HCI_LE_ADV_REPORT_EVT_SUBCODE:
            {
                // Filter legacy report if the max number is reached
                filtered = (hci_tl_env.adv_rep_frag_cnt >= BLE_MAX_NB_ADV_REP_FRAG);

                adv_rep = true;
            }
            break;

            case HCI_LE_PER_ADV_REPORT_EVT_SUBCODE:
            {
                // Point to event parameters
                struct hci_le_per_adv_report_evt* evt = (struct hci_le_per_adv_report_evt*) &msg->param;
                bool more_data = (evt->status == PER_ADV_EVT_DATA_STATUS_INCOMPLETE);

                chain_stat = hci_tl_env.per_adv_rep_chain_stat[BLE_SYNCHDL_TO_ACTID(evt->sync_handle)];

                // Check if report can be queued for transmission, or must be flushed
                if(   (chain_stat == HCI_ADV_REP_CHAIN_FILTERED)
                   || ((chain_stat == HCI_ADV_REP_CHAIN_NO) && (hci_tl_env.adv_rep_frag_cnt >= BLE_MAX_NB_ADV_REP_FRAG)) )
                {
                    filtered = true;
                }

                // Evaluate new chain status
                if (!more_data)
                {
                    chain_stat = HCI_ADV_REP_CHAIN_NO;
                }
                else
                {
                    chain_stat = filtered ? HCI_ADV_REP_CHAIN_FILTERED : HCI_ADV_REP_CHAIN_VALID;
                }

                // Update chain info for the sync handle
                hci_tl_env.per_adv_rep_chain_stat[BLE_SYNCHDL_TO_ACTID(evt->sync_handle)] = chain_stat;

                adv_rep = true;
            }
            break;

            case HCI_LE_CONLESS_IQ_REPORT_EVT_SUBCODE:
            {
                // Check if report can be queued for transmission, or must be flushed
                filtered = (hci_tl_env.iq_rep_cnt >= BLE_MAX_NB_IQ_REP);
                
                // If transmitted, increment report counter
                if(!filtered)
                {
                    DBG_SWDIAG(LEADVFILT, OK, 1);

                    // Increment report counter
                    hci_tl_env.iq_rep_cnt++;

                    DBG_SWDIAG(LEADVFILT, OK, 0);
                }
            }
            break;

            default:
            {
            }
            break;
        }

        if(filtered)
        {
            DBG_SWDIAG(LEADVFILT, KO, 1);

            // Trash the report
            kernel_msg_free(msg);

            DBG_SWDIAG(LEADVFILT, KO, 0);
        }
        else if (adv_rep)
        {
            DBG_SWDIAG(LEADVFILT, OK, 1);

            // Increment report fragment counter
            hci_tl_env.adv_rep_frag_cnt++;
            DBG_SWDIAG(LEADVFILT, CNT, hci_tl_env.adv_rep_frag_cnt);

            DBG_SWDIAG(LEADVFILT, OK, 0);
        }

    } while(0);

    return filtered;
}

/**
 ****************************************************************************************
 * @brief Update advertising reports flow control when message is transmitted
 *
 * @param[in]   msg    Kernel message containing the event
 *****************************************************************************************
 */
static void hci_ble_adv_report_tx_check(struct kernel_msg * msg)
{
    do
    {
        uint8_t subcode;

        // Check if the message is an LE event
        if(msg->id != HCI_LE_EVENT)
            break;

        // Retrieve event subcode
        subcode = *((uint8_t *) msg->param);

        // Check if the event is HCI_LE_ADV_Report or HCI_LE_Ext_ADV_Report or HCI_LE_Per_ADV_Report
        if(  (subcode == HCI_LE_ADV_REPORT_EVT_SUBCODE) || (subcode == HCI_LE_EXT_ADV_REPORT_EVT_SUBCODE)
          || (subcode == HCI_LE_PER_ADV_REPORT_EVT_SUBCODE) )
        {
            // Check that at least one advertising report is queued for transmission
            if(hci_tl_env.adv_rep_frag_cnt == 0)
            {
                BLE_ASSERT_ERR(0);
                break;
            }

            // Decrement report fragment counter
            hci_tl_env.adv_rep_frag_cnt--;
            DBG_SWDIAG(LEADVFILT, CNT, hci_tl_env.adv_rep_frag_cnt);

            break;
        }

        // Check if the event is HCI_LE_Conless_IQ_Report
        if(subcode == HCI_LE_CONLESS_IQ_REPORT_EVT_SUBCODE)
        {
            // Check that at least one IQ report is queued for transmission
            if(hci_tl_env.iq_rep_cnt == 0)
            {
                BLE_ASSERT_ERR(0);
                break;
            }

            // Decrement IQ report counter
            hci_tl_env.iq_rep_cnt--;

            break;
        }

    } while(0);
}
#endif // (BLE_OBSERVER)
#endif // (BLE_EMB_PRESENT)

#if (BLE_EMB_PRESENT || BT_EMB_PRESENT)

/**
 ****************************************************************************************
 * @brief Reject a HCI command
 *
 * This function creates a CS or CC event Kernel message to reject a command.
 *
 * @param[in]   cmd_desc    Command descriptor (NULL if command unknown)
 * @return      Pointer to the beginning of the event
 *****************************************************************************************
 */
static void hci_cmd_reject(const struct hci_cmd_desc_tag* cmd_desc, uint16_t opcode, uint8_t error, uint8_t * payload)
{
    if(cmd_desc != NULL)
    {
        // Check if this command shall be replied with a CC or CS event
        if(cmd_desc->ret_par_fmt != NULL)
        {
            // Get size of the Command Complete Event message associated with the opcode
            uint16_t ret_par_len;
            uint8_t status = COMMON_UTIL_PACK_OK;

            // Check if the generic packer can be used (basic fixed-length format)
            if(!(cmd_desc->dest_field & HCI_CMD_DEST_SPEC_RET_PAR_PK_MSK))
            {
                // Use the generic unpacker to get unpacked parameters length
                status = common_util_unpack(NULL, NULL, &ret_par_len, 0xFFFF, cmd_desc->ret_par_fmt);
            }
            else
            {
                // Use the special unpacker to get unpacked parameters length
                status = ((hci_pkupk_func_t)cmd_desc->ret_par_fmt)(NULL, NULL, &ret_par_len, 0);
            }

            if(status == COMMON_UTIL_PACK_OK)
            {
                // Send a CC event with returned parameters (eventually copy received connection handle or BD address if needed)
                if(!memcmp(cmd_desc->ret_par_fmt, "BH", 2))
                {
                    struct hci_basic_conhdl_cmd_cmp_evt* evt = (struct hci_basic_conhdl_cmd_cmp_evt*) kernel_msg_alloc(HCI_CMD_CMP_EVENT, 0, opcode, ret_par_len);
                    evt->status = error;
                    memcpy(&evt->conhdl, payload, 2);
                    hci_send_2_host(evt);
                }
                else if(!memcmp(cmd_desc->ret_par_fmt, "B6B", 3))
                {
                    struct hci_basic_bd_addr_cmd_cmp_evt* evt = (struct hci_basic_bd_addr_cmd_cmp_evt*) kernel_msg_alloc(HCI_CMD_CMP_EVENT, 0, opcode, ret_par_len);
                    evt->status = error;
                    memcpy(&evt->bd_addr, payload, 6);
                    hci_send_2_host(evt);
                }
                else
                {
                    struct hci_basic_cmd_cmp_evt* evt = (struct hci_basic_cmd_cmp_evt*) kernel_msg_alloc(HCI_CMD_CMP_EVENT, 0, opcode, ret_par_len);
                    evt->status = error;
                    hci_send_2_host(evt);
                }
            }
            else
            {
                BLE_ASSERT_INFO(0, status, opcode);
            }
        }
        else
        {
            // Send a CS event with error code "Unknown connection identifier"
            struct hci_cmd_stat_event* evt = KERNEL_MSG_ALLOC(HCI_CMD_STAT_EVENT, 0, opcode, hci_cmd_stat_event);
            evt->status = error;
            hci_send_2_host(evt);
        }
    }
    else
    {
        // Send a CC event with error code "Unknown HCI command"
        struct hci_basic_cmd_cmp_evt * evt = KERNEL_MSG_ALLOC(HCI_CMD_CMP_EVENT, 0, opcode, hci_basic_cmd_cmp_evt);
        evt->status = error;
        hci_send_2_host(evt);
    }
}

/**
 ****************************************************************************************
 * @brief Build a HCI Command Status event
 *
 * This function build a HCI CS event from the CS event Kernel message.
 *
 * @param[in]   msg    Kernel message containing the CS event
 * @return      Pointer to the beginning of the event
 *****************************************************************************************
 */
static uint8_t* hci_build_cs_evt(struct kernel_msg * msg)
{
    uint8_t* param = kernel_msg2param(msg);
    uint8_t* buf = param - HCI_EVT_CS_PARAM_OFFSET;
    uint8_t* pk = buf;
    uint16_t opcode = msg->src_id;

    //pack event code
    *pk++ = HCI_CMD_STATUS_EVT_CODE;

    //pack event parameter length
    *pk++ = HCI_CSEVT_PARLEN;

    //pack the status
    *pk++ = *param;

    //pack the number of h2c packets
    *pk++ = (hci_tl_env.nb_h2c_cmd_pkts > 0)? (uint8_t) hci_tl_env.nb_h2c_cmd_pkts : 0;

    //pack opcode
    common_write16p(pk, common_htobs(opcode));

    return buf;
}

/**
 ****************************************************************************************
 * @brief Build a HCI Command Complete event
 *
 * This function build a HCI CC event from the CC event Kernel message.
 *
 * @param[in]   msg    Kernel message containing the CC event
 * @return      Pointer to the beginning of the event
 *****************************************************************************************
 */
static uint8_t* hci_build_cc_evt(struct kernel_msg * msg)
{
    uint8_t* param = kernel_msg2param(msg);
    uint8_t* buf = param - HCI_EVT_CC_PARAM_OFFSET;
    uint8_t* pk = buf;
    uint16_t opcode = msg->src_id;
    uint16_t ret_par_len = msg->param_len;

    // Look for the command descriptor
    const struct hci_cmd_desc_tag* cmd_desc = hci_look_for_cmd_desc(opcode);

    if((cmd_desc != NULL) && (ret_par_len > 0))
    {
        uint8_t status = COMMON_UTIL_PACK_ERROR;

        if(cmd_desc->ret_par_fmt != NULL)
        {
            // Check if the generic packer can be used (basic fixed-length format)
            if(!(cmd_desc->dest_field & HCI_CMD_DEST_SPEC_RET_PAR_PK_MSK))
            {
                // Pack the returned parameters using the generic packer
                status = common_util_pack(param, param, &ret_par_len, ret_par_len, cmd_desc->ret_par_fmt);
            }
            else
            {
                // Pack the return parameters using the special packer
                status = ((hci_pkupk_func_t)cmd_desc->ret_par_fmt)(param, param, &ret_par_len, ret_par_len);
            }
        }

        if(status != COMMON_UTIL_PACK_OK)
        {
            BLE_ASSERT_INFO((status == COMMON_UTIL_PACK_OK), status, opcode);
        }
    }
    else if (opcode != HCI_NO_OPERATION_CMD_OPCODE)
    {
        // Set the status "Unknown HCI command" as unique returned parameter
        *param = COMMON_ERROR_UNKNOWN_HCI_COMMAND;
    }
    else
    {
        BLE_ASSERT_INFO(0, opcode, ret_par_len);
    }

    //pack event code
    *pk++ = HCI_CMD_CMP_EVT_CODE;

    //pack event parameter length
    *pk++ = HCI_CCEVT_HDR_PARLEN + ret_par_len;

    //pack the number of h2c packets
    *pk++ = (hci_tl_env.nb_h2c_cmd_pkts > 0)? (uint8_t) hci_tl_env.nb_h2c_cmd_pkts : 0;

    //pack opcode
    common_write16p(pk, common_htobs(opcode));

    return buf;
}

/**
 ****************************************************************************************
 * @brief Build a HCI event
 *
 * This function build a HCI event from the event Kernel message.
 *
 * @param[in]   msg    Kernel message containing the event
 * @return      Pointer to the beginning of the event
 *****************************************************************************************
 */
static uint8_t* hci_build_evt(struct kernel_msg * msg)
{
    uint8_t* param = kernel_msg2param(msg);
    uint8_t* buf = param - HCI_EVT_LE_PARAM_OFFSET;
    uint8_t* pk = buf;
    uint8_t evt_code = (uint8_t) msg->src_id;
    uint16_t par_len = msg->param_len;
    uint8_t status = COMMON_UTIL_PACK_OK;

    BLE_ASSERT_INFO(msg->src_id <= UINT8_MAX, msg->src_id, 0);

    // Look for the event descriptor
    const struct hci_evt_desc_tag* evt_desc = hci_look_for_evt_desc(evt_code);

    if(evt_desc != NULL)
    {
        if(evt_desc->par_fmt != NULL)
        {
            // Check if the generic packer can be used (basic fixed-length format)
            if(evt_desc->special_pack == PK_GEN)
            {
                // Pack the returned parameters using the generic packer
                status = common_util_pack(param, param, &par_len, par_len, evt_desc->par_fmt);
            }
            else
            {
                // Pack the parameters using the special packer
                status = ((hci_pkupk_func_t)evt_desc->par_fmt)(param, param, &par_len, par_len);
            }
        }
        else if(msg->param_len > 0)
        {
            status = COMMON_UTIL_PACK_ERROR;
        }

        if(status != COMMON_UTIL_PACK_OK)
        {
            BLE_ASSERT_INFO((status == COMMON_UTIL_PACK_OK), status, evt_code);
        }

        BLE_ASSERT_INFO(par_len <= msg->param_len, status, evt_code);

        //pack event code
        *pk++ = evt_code;

        //pack event parameter length
        *pk++ = par_len;
    }
    else
    {
        BLE_ASSERT_INFO(0, evt_code, 0);
    }

    return buf;
}

#if (RW_DEBUG)
/**
 ****************************************************************************************
 * @brief Build a HCI DBG event
 *
 * This function build a HCI LE event from the DBG event Kernel message.
 *
 * @param[in]   msg    Kernel message containing the DBG event
 * @return      Pointer to the beginning of the event
 *****************************************************************************************
 */
static uint8_t* hci_build_dbg_evt(struct kernel_msg * msg)
{
    uint8_t* param = kernel_msg2param(msg);
    uint8_t* buf = param - HCI_EVT_DBG_PARAM_OFFSET;
    uint8_t* pk = buf;
    uint8_t subcode = *param;
    uint16_t par_len = msg->param_len;

    // Look for the event descriptor
    const struct hci_evt_desc_tag* evt_desc = hci_look_for_dbg_evt_desc(subcode);

    if(evt_desc != NULL)
    {
        uint8_t status = COMMON_UTIL_PACK_ERROR;

        if(evt_desc->par_fmt != NULL)
        {
            // Check if the generic packer can be used (basic fixed-length format)
            if(evt_desc->special_pack == PK_GEN)
            {
                // Pack the returned parameters
                status = common_util_pack(param, param, &par_len, par_len, evt_desc->par_fmt);
            }
            else
            {
                // Pack the parameters using the special packer
                status = ((hci_pkupk_func_t)evt_desc->par_fmt)(param, param, &par_len, par_len);
            }
        }

        BLE_ASSERT_INFO((status == COMMON_UTIL_PACK_OK), status, subcode);
        BLE_ASSERT_INFO(par_len <= msg->param_len, par_len, subcode);
        if(status == COMMON_UTIL_PACK_OK)
        {
            //pack event code
            *pk++ = HCI_DBG_META_EVT_CODE;

            //pack event parameter length
            *pk++ = par_len;
        }
    }
    else
    {
        BLE_ASSERT_INFO(0, subcode, 0);
    }

    return buf;
}
#endif // (RW_DEBUG)


#if BLE_EMB_PRESENT
/**
 ****************************************************************************************
 * @brief Build a HCI LE event
 *
 * This function build a HCI LE event from the LE event Kernel message.
 *
 * @param[in]   msg    Kernel message containing the LE event
 * @return      Pointer to the beginning of the event
 *****************************************************************************************
 */
static uint8_t* hci_build_le_evt(struct kernel_msg * msg)
{
    uint8_t* param = kernel_msg2param(msg);
    uint8_t* buf = param - HCI_EVT_LE_PARAM_OFFSET;
    uint8_t* pk = buf;
    uint8_t subcode = *param;
    uint16_t par_len = msg->param_len;

    // Look for the event descriptor
    const struct hci_evt_desc_tag* evt_desc = hci_look_for_le_evt_desc(subcode);

    if(evt_desc != NULL)
    {
        uint8_t status = COMMON_UTIL_PACK_ERROR;

        if(evt_desc->par_fmt != NULL)
        {
            // Check if the generic packer can be used (basic fixed-length format)
            if(evt_desc->special_pack == PK_GEN)
            {
                // Pack the returned parameters
                status = common_util_pack(param, param, &par_len, par_len, evt_desc->par_fmt);
            }
            else
            {
                // Pack the parameters using the special packer
                status = ((hci_pkupk_func_t)evt_desc->par_fmt)(param, param, &par_len, par_len);
            }
        }

        BLE_ASSERT_INFO((status == COMMON_UTIL_PACK_OK), status, subcode);
        BLE_ASSERT_INFO(par_len <= msg->param_len, par_len, subcode);
        if(status == COMMON_UTIL_PACK_OK)
        {
            //pack event code
            *pk++ = HCI_LE_META_EVT_CODE;

            //pack event parameter length
            *pk++ = par_len;
        }
    }
    else
    {
        BLE_ASSERT_INFO(0, subcode, 0);
    }

    return buf;
}
#endif //BLE_EMB_PRESENT
#endif //(BLE_EMB_PRESENT || BT_EMB_PRESENT)

#if BT_EMB_PRESENT
#if VOICE_OVER_HCI
/**
 ****************************************************************************************
 * @brief Build a HCI SYNC data packet
 *
 * This function build a HCI SYNC data packet from the Kernel message.
 *
 * @param[in]   msg    Kernel message associated to the HCI SYNC data
 *****************************************************************************************
 */
static uint8_t* hci_build_sync_data(struct kernel_msg * msg)
{
    struct hci_sync_data *param = (struct hci_sync_data *) kernel_msg2param(msg);
    uint16_t pk_start_addr = param->buf_ptr - HCI_SYNC_HDR_LEN;

    // Pack connection handle and packet status flags
    em_wr16p(pk_start_addr + HCI_SYNC_HDR_HDL_FLAGS_POS, param->conhdl_psf);

    // Pack the data length
    em_wr8p(pk_start_addr + HCI_SYNC_HDR_DATA_LEN_POS, param->length);

    return (uint8_t *)(EM_BASE_ADDR + pk_start_addr);
}
#endif //VOICE_OVER_HCI
#endif //BT_EMB_PRESENT

#if (BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)
/**
 ****************************************************************************************
 * @brief Build a HCI ACL data packet
 *
 * This function build a HCI ACL data packet from the Kernel message.
 *
 * @param[in]   msg    Kernel message associated to the HCI ACL data
 *****************************************************************************************
 */
static uint8_t* hci_build_acl_data(struct kernel_msg * msg)
{
    // Point to message parameters structure
    struct hci_acl_data *param = (struct hci_acl_data *) kernel_msg2param(msg);
    uint16_t handle_flags      = param->conhdl_pb_bc_flag;
    uint8_t* buf = ((uint8_t*)param->buf_ptr) - HCI_ACL_HDR_LEN;;
    uint8_t* pk;

    #if (!BLE_HOST_PRESENT || BLE_EMB_PRESENT)
    buf += EM_BASE_ADDR;
    #endif // (!BLE_HOST_PRESENT || BLE_EMB_PRESENT)

    pk = buf;

    // Pack connection handle and data flags
    common_write16p(pk, common_htobs(handle_flags));
    pk +=2;

    // Pack the data length
    common_write16p(pk, common_htobs(param->length));
    pk +=2;

    return (buf);
}
#endif // (BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)


#if ((BLE_HOST_PRESENT) && (!BLE_EMB_PRESENT))

#if (HCI_BLE_CON_SUPPORT)
/**
 ****************************************************************************************
 * @brief Extract connection handle from a given format message
 *
 * This function returns the handle contained in a descriptor
 *
 * @param[in]   code    Kernel message containing the HCI command
 * @param[in]   payload Data
 * @param[in]   format  String containning the format
 * @return      Connection Handle
 *****************************************************************************************
 */
static uint16_t hci_look_for_conhdl(uint8_t code, uint8_t *payload, uint8_t *format)
{
    uint8_t *params = payload;
    uint8_t index = 0;
    uint16_t conhdl = GAP_INVALID_CONHDL;

    // Different frame format for CC events
    if ((code == HCI_CMD_CMP_EVT_CODE) ||
            (code == HCI_CMD_STATUS_EVT_CODE))
    {
        params += HCI_CMD_OPCODE_LEN + HCI_CMDEVT_PARLEN_LEN;
    }

    while(*(format + index) != '\0')
    {
        // Assume all parameters before connection handle are single bytes
        if(*(format + index) == 'H')
        {
            // Read connection handle
            conhdl = common_read16p(params + index);
            break;
        }
        // Increment index
        index++;
    }

    return (conhdl);
}

/**
****************************************************************************************
* @brief Search if a CS is expected
*
* @param[in]  code      message code
*
* @return     Connection index
*****************************************************************************************
*/
static uint8_t hci_search_cs_index(uint8_t code)
{
    uint8_t idx = GAP_INVALID_CONIDX;

    for (uint8_t i=0; i<BLE_ACTIVITY_MAX; i++)
    {
        if (code == hci_env.ble_acl_con_tab[i].code)
        {
            // Get index
            idx = i;
            // Clear data
            hci_env.ble_acl_con_tab[i].code = 0;
            break;
        }
    }

    return(idx);
}

#endif //(HCI_BLE_CON_SUPPORT)
/**
 ****************************************************************************************
 * @brief Build a HCI command
 *
 * This function builds a HCI command from the Kernel message.
 *
 * @param[in]   msg    Kernel message containing the HCI command
 * @return      Pointer to the beginning of the event
 *****************************************************************************************
 */
static uint8_t* hci_build_cmd(struct kernel_msg * msg)
{
    uint8_t* param = kernel_msg2param(msg);
    uint8_t* buf = param - HCI_CMD_PARAM_OFFSET;
    uint8_t* pk = buf;
    uint16_t opcode = msg->src_id;
    uint16_t par_len = msg->param_len;
    uint8_t status = COMMON_UTIL_PACK_ERROR;

    // Look for the command descriptor
    const struct hci_cmd_desc_tag* cmd_desc = hci_look_for_cmd_desc(opcode);

    if((cmd_desc != NULL) && (par_len > 0))
    {
        if(cmd_desc->par_fmt != NULL)
        {
            // Check if the generic packer can be used (basic fixed-length format)
            if(!(cmd_desc->dest_field & HCI_CMD_DEST_SPEC_PAR_PK_MSK))
            {
                // Pack the returned parameters using the generic packer
                status = common_util_pack(param, param, &par_len, par_len, cmd_desc->par_fmt);

                #if ((HCI_BLE_CON_SUPPORT) && (BLE_HOST_PRESENT) && (!BLE_EMB_PRESENT))
                // Check if CS is expected

                uint8_t hl_type = (cmd_desc->dest_field & HCI_CMD_DEST_HL_MASK) >> HCI_CMD_DEST_HL_POS;
                if ((hl_type == HL_CTRL) && (cmd_desc->ret_par_fmt == NULL))
                {
                    // Parse connection handle
                    uint16_t conhdl = hci_look_for_conhdl(opcode, param, (uint8_t *)cmd_desc->par_fmt);
                    // Save code in order to route CS correctly
                    if (conhdl != GAP_INVALID_CONHDL)
                    {
                        // dest_id actually contains the connection index
                        hci_env.ble_acl_con_tab[msg->dest_id].code = opcode;
                    }
                    else
                    {
                        BLE_ASSERT_INFO(0, opcode, conhdl);
                    }
                }
                #endif //((HCI_BLE_CON_SUPPORT) && (BLE_HOST_PRESENT) && (!BLE_EMB_PRESENT))
            }
            else
            {
                // Pack the return parameters using the special packer
                status = ((hci_pkupk_func_t)cmd_desc->par_fmt)(param, param, &par_len, par_len);
            }
        }

        BLE_ASSERT_INFO((status == COMMON_UTIL_PACK_OK), status, opcode);
    }
    // command with empty parameters
    else if((cmd_desc != NULL) && (par_len == 0))
    {
        status = COMMON_UTIL_PACK_OK;
    }
    else
    {
        // Command not found
        BLE_ASSERT_INFO(0, par_len, opcode);
    }

    //pack command opcode
    common_write16p(pk, common_htobs(opcode));
    pk += 2;

    //pack command parameter length
    *pk++ = par_len;

    return (status == COMMON_UTIL_PACK_OK) ? buf : NULL;
}
#endif // ((BLE_HOST_PRESENT) && (!BLE_EMB_PRESENT))

/**
 ****************************************************************************************
 * @brief Check if a transmission has to be done and start it.
 *****************************************************************************************
 */
static void hci_tx_start(void)
{
    struct kernel_msg *msg;

    do
    {
        // Check default TX queue
        msg = (struct kernel_msg *)common_list_pick(&hci_tl_env.tx_queue);
        if (msg != NULL)
            break;

        #if (BT_EMB_PRESENT)
        #if VOICE_OVER_HCI
        // Check SYNC data flow control
        if (hci_fc_check_host_available_nb_sync_packets())
        {
            msg = (struct kernel_msg *)common_list_pick(&hci_tl_env.sync_queue);
            if (msg != NULL)
                break;
        }
        #endif //VOICE_OVER_HCI
        #endif //(BT_EMB_PRESENT)

        #if (BT_EMB_PRESENT || (HCI_BLE_CON_SUPPORT && BLE_EMB_PRESENT))
        // Check ACL data flow control
        if (hci_fc_check_host_available_nb_acl_packets())
        {
            // Check ACL data queue
           msg = (struct kernel_msg *) common_list_pick(&hci_tl_env.acl_queue);
           if (msg != NULL)
               break;
        }
        #elif (HCI_BLE_CON_SUPPORT && BLE_HOST_PRESENT)
        // Check ACL TX queue
        msg = (struct kernel_msg *)common_list_pick(&hci_tl_env.acl_queue);
        if (msg != NULL)
            break;
        #endif //(BT_EMB_PRESENT ||  (HCI_BLE_CON_SUPPORT && BLE_EMB_PRESENT))

    } while(0);


    if (msg != NULL)
    {
        uint8_t *buf = NULL;
        uint16_t len = 0;
        uint8_t type = 0;

        // Store message of the packet under transmission
        hci_tl_env.curr_tx_msg = msg;

        // Check what kind of TX packet
        switch(msg->id)
        {
            #if (BLE_EMB_PRESENT || BT_EMB_PRESENT)
            case HCI_CMD_CMP_EVENT:
            {
                // Increase the number of commands available for reception
                if(hci_tl_env.nb_h2c_cmd_pkts < HCI_NB_CMD_PKTS)
                {
                    hci_tl_env.nb_h2c_cmd_pkts++;
                }
                // Build the HCI event
                buf = hci_build_cc_evt(msg);

                // Extract information (buffer, length, type)
                len = HCI_EVT_HDR_LEN + *(buf + HCI_EVT_CODE_LEN);
                type = HCI_EVT_MSG_TYPE;
            }
            break;
            case HCI_CMD_STAT_EVENT:
            {
                // Increase the number of commands available for reception
                if(hci_tl_env.nb_h2c_cmd_pkts < HCI_NB_CMD_PKTS)
                {
                    hci_tl_env.nb_h2c_cmd_pkts++;
                }

                // Build the HCI event
                buf = hci_build_cs_evt(msg);

                // Extract information (buffer, length, type)
                len = HCI_EVT_HDR_LEN + *(buf + HCI_EVT_CODE_LEN);
                type = HCI_EVT_MSG_TYPE;
            }
            break;
            case HCI_EVENT:
            {
                // Build the HCI event
                buf = hci_build_evt(msg);

                // Extract information (buffer, length, type)
                len = HCI_EVT_HDR_LEN + *(buf + HCI_EVT_CODE_LEN);
                type = HCI_EVT_MSG_TYPE;
            }
            break;
            #if BLE_EMB_PRESENT
            case HCI_LE_EVENT:
            {
                // Build the HCI event
                buf = hci_build_le_evt(msg);

                // Extract information (buffer, length, type)
                len = HCI_EVT_HDR_LEN + *(buf + HCI_EVT_CODE_LEN);
                type = HCI_EVT_MSG_TYPE;
            }
            break;
            #endif //BLE_EMB_PRESENT
            #if (BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)
            case HCI_ACL_DATA:
            {
                // Build the HCI data packet from Kernel message
                buf = hci_build_acl_data(msg);

                // Extract information (buffer, length, type)
                len = HCI_ACL_HDR_LEN + common_read16p(buf + HCI_ACL_HDR_HDL_FLAGS_LEN);
                type = HCI_ACL_MSG_TYPE;
            }
            break;
            #endif // (BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)
            #if BT_EMB_PRESENT
            #if VOICE_OVER_HCI
            case HCI_SYNC_DATA:
            {
                // Build the HCI data packet from Kernel message
                buf = hci_build_sync_data(msg);

                // Extract information (buffer, length, type)
                len = HCI_SYNC_HDR_LEN + *(buf + HCI_SYNC_HDR_DATA_LEN_POS);
                type = HCI_SYNC_MSG_TYPE;
            }
            break;
            #endif //VOICE_OVER_HCI
            #endif //BT_EMB_PRESENT
            #endif //(BLE_EMB_PRESENT || BT_EMB_PRESENT)

            #if ((BLE_HOST_PRESENT) && (!BLE_EMB_PRESENT))
            case HCI_COMMAND:
            {
                // Build the HCI command
                buf = hci_build_cmd(msg);

                // Extract information (buffer, length, type)
                len = HCI_CMD_HDR_LEN + *(buf + HCI_CMD_OPCODE_LEN);
                type = HCI_CMD_MSG_TYPE;
            }
            break;

            #if (HCI_BLE_CON_SUPPORT)
            case HCI_ACL_DATA:
            {
                // Build the HCI data packet from Kernel message
                buf = hci_build_acl_data(msg);

                // Extract information (buffer, length, type)
                len = HCI_ACL_HDR_LEN + common_read16p(buf + HCI_ACL_HDR_HDL_FLAGS_LEN);
                type = HCI_ACL_MSG_TYPE;
            }
            break;
            #endif // (HCI_BLE_CON_SUPPORT)
            #endif // ((BLE_HOST_PRESENT) && (!BLE_EMB_PRESENT))

            #if (BLE_EMB_PRESENT || BT_EMB_PRESENT)
            // only one debug event supported for the moment, so flag to be
            // removed as soon as more debug event are supported
            #if (RW_DEBUG)
            case HCI_DBG_EVT:
            {
                // Build the DBG event
                buf = hci_build_dbg_evt(msg);

                // Extract information (buffer, length, type)
                len = HCI_EVT_HDR_LEN + *(buf + HCI_EVT_CODE_LEN);
                type = HCI_EVT_MSG_TYPE;
            }
            break;
            #endif // (RW_DEBUG)
            #endif // (BLE_EMB_PRESENT || BT_EMB_PRESENT)

            default:
            {
                BLE_ASSERT_INFO(0, msg->id, 0);
            }
            break;
        }

        // Set TX state
        hci_tl_env.tx_state = HCI_STATE_TX_ONGOING;

        #if (H4TL_SUPPORT)
        // Forward the message to the H4TL for immediate transmission
        h4tl_write(type, buf, len, &hci_tx_done);
        #endif //(H4TL_SUPPORT)
    }
}

/**
 ****************************************************************************************
 * @brief Function called after sending message through UART, to free kernel_msg space and
 * push the next message for transmission if any.
 *
 * The message is popped from the tx queue kept in hci_tl_env and freed using kernel_msg_free.
 *****************************************************************************************
 */
static void hci_tx_done(void)
{
    struct kernel_msg *msg = hci_tl_env.curr_tx_msg;
    GLOBAL_INT_DIS();

    #if (BLE_EMB_PRESENT)
    #if (BLE_OBSERVER)
    // Check advertising report flow control mechanism
    hci_ble_adv_report_tx_check(msg);
    #endif // (BLE_OBSERVER)
    #endif // (BLE_EMB_PRESENT)

    // Go back to IDLE state
    hci_tl_env.tx_state = HCI_STATE_TX_IDLE;

    // Free the message resources
    switch(msg->id)
    {
        case HCI_EVENT:
        #if (BLE_EMB_PRESENT)
        case HCI_LE_EVENT:
        #endif // (BLE_EMB_PRESENT)
        #if (RW_DEBUG)
        case HCI_DBG_EVT:
        #endif // (RW_DEBUG)
        case HCI_CMD_CMP_EVENT:
        case HCI_CMD_STAT_EVENT:
        case HCI_COMMAND:
        {
            // Nothing to do
        }
        break;
        #if (BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)
        case HCI_ACL_DATA:
        {
            // Pop message from HCI parameters
            struct hci_acl_data *data_msg = (struct hci_acl_data *) kernel_msg2param(msg);
            // Pop message from data queue
            common_list_pop_front(&hci_tl_env.acl_queue);

            #if (BT_EMB_PRESENT || BLE_EMB_PRESENT)
            // Get connection handle
            uint16_t conhdl = GETF(data_msg->conhdl_pb_bc_flag, HCI_ACL_HDR_HDL);
            #endif // (BT_EMB_PRESENT || BLE_EMB_PRESENT)

            #if (HCI_BLE_CON_SUPPORT)
            #if (BLE_EMB_PRESENT)
            // If connection is BLE
            if(conhdl <= BLE_CONHDL_MAX)
            {
                // Free the RX buffer associated with the message
                ble_util_buf_rx_free((uint16_t) data_msg->buf_ptr);
            }
            #elif (BLE_HOST_PRESENT)
            // free ACL TX buffer
            kernel_free((uint8_t *) data_msg->buf_ptr - (HCI_ACL_HDR_LEN + HCI_TRANSPORT_HDR_LEN));
            #endif // (BLE_EMB_PRESENT)
            #endif // (HCI_BLE_CON_SUPPORT)

            #if (BT_EMB_PRESENT)
            // If connection is BT
            if((conhdl >= BT_ACL_CONHDL_MIN) && (conhdl <= BT_ACL_CONHDL_MAX))
            {
                // Free the RX buffer associated with the message
                bt_util_buf_acl_rx_free((uint16_t) data_msg->buf_ptr);
            }
            #endif //BT_EMB_PRESENT

            hci_tl_env.curr_tx_msg = NULL;

            #if (BLE_EMB_PRESENT || BT_EMB_PRESENT)
            //count packets for flow control
            hci_fc_acl_packet_sent();
            #endif // (BT_EMB_PRESENT || BLE_EMB_PRESENT)
        }
        break;
        #endif // (BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)
        #if (BT_EMB_PRESENT)
        #if VOICE_OVER_HCI
        case HCI_SYNC_DATA:
        {
            struct hci_sync_data *data_rx;
            // Pop event from the event queue
            msg = (struct kernel_msg *)common_list_pop_front(&hci_tl_env.sync_queue);
            data_rx = (struct hci_sync_data *)(msg->param);
            // Free the RX buffer associated with the message
            bt_util_buf_sync_rx_free(BT_SYNC_CONHDL_LID(GETF(data_rx->conhdl_psf, HCI_SYNC_HDR_HDL)), data_rx->buf_ptr);
            hci_tl_env.curr_tx_msg = NULL;
            //count packets for flow control
            hci_fc_sync_packet_sent();
        }
        break;
        #endif //VOICE_OVER_HCI
        #endif // (BT_EMB_PRESENT)
        default:
        {
            BLE_ASSERT_INFO(0, msg->id, 0);
        }
        break;
    }

    if (hci_tl_env.curr_tx_msg != NULL)
    {
        // Pop event from the event queue
        msg = (struct kernel_msg *)common_list_pop_front(&hci_tl_env.tx_queue);
    }

    // Free the kernel message space
    kernel_msg_free(msg);

    GLOBAL_INT_RES();

    // Check if there is a new message pending for transmission
    hci_tx_start();
}

/**
 ****************************************************************************************
 * @brief Trigger the transmission over HCI TL if possible
 *****************************************************************************************
 */
static void hci_tx_trigger(void)
{
    // Check if there is no transmission ongoing
    if (hci_tl_env.tx_state == HCI_STATE_TX_IDLE)
    {
        // Start the transmission
        hci_tx_start();
    }
}


/*
 * MODULES INTERNAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

void hci_tl_send(struct kernel_msg *msg)
{
    GLOBAL_INT_DIS();

    /// put the message into corresponding queue
    switch(msg->id)
    {
        #if (BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)
        case HCI_ACL_DATA:
        {
            common_list_push_back(&hci_tl_env.acl_queue, &msg->hdr);
        }
        break;
        #endif //(BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)

        #if (BT_EMB_PRESENT)
        #if VOICE_OVER_HCI
        case HCI_SYNC_DATA:
        {
            common_list_push_back(&hci_tl_env.sync_queue, &msg->hdr);
        }
        break;
        #endif //VOICE_OVER_HCI
        #endif //(BT_EMB_PRESENT)
        default:
        {
            #if (BLE_EMB_PRESENT)
            #if (BLE_OBSERVER)
            // Flow control ADV reports
            if(hci_ble_adv_report_filter_check(msg))
                break;
            #endif // (BLE_OBSERVER)
            #endif // (BLE_EMB_PRESENT)

            // Push the message into the HCI queue
            common_list_push_back(&hci_tl_env.tx_queue, &msg->hdr);
        }
        break;
    }

    GLOBAL_INT_RES();

    // Trigger the HCI transmission
    hci_tx_trigger();
}


/*
 * EXPORTED FUNCTION DEFINITIONS
 ****************************************************************************************
 */

void hci_tl_init(uint8_t init_type)
{
    if(!init_type)
    {
        // Reset the HCI environment
        memset(&hci_tl_env, 0, sizeof(hci_tl_env));

        // Initialize the HCI event transmit queues
        common_list_init(&hci_tl_env.tx_queue);

    #if (BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)
        // Initialize the BT ACL transmit queues
        common_list_init(&hci_tl_env.acl_queue);
    #endif //(BT_EMB_PRESENT || HCI_BLE_CON_SUPPORT)

    #if (BT_EMB_PRESENT)
    #if VOICE_OVER_HCI
        // Initialize the BT SYNC transmit queues
        common_list_init(&hci_tl_env.sync_queue);
    #endif //VOICE_OVER_HCI
    #endif //(BT_EMB_PRESENT)


        // Initialize TX state machine
        hci_tl_env.tx_state = HCI_STATE_TX_IDLE;
    }
    // Initialize the number of HCI commands the stack can handle simultaneously
    hci_tl_env.nb_h2c_cmd_pkts = HCI_NB_CMD_PKTS;
}

#if (BLE_EMB_PRESENT || BT_EMB_PRESENT)
uint8_t hci_cmd_get_max_param_size(uint16_t opcode)
{
    uint16_t max_param_size = HCI_MAX_CMD_PARAM_SIZE;

    // Find a command descriptor associated to the command opcode
    const struct hci_cmd_desc_tag* cmd_desc = hci_look_for_cmd_desc(opcode);

    // Check if the command is supported
    if(cmd_desc != NULL)
    {
        max_param_size = cmd_desc->par_size_max;
    }

    return (uint8_t) max_param_size;
}

void hci_cmd_received(uint16_t opcode, uint8_t length, uint8_t *payload)
{
    // Find a command descriptor associated to the command opcode
    const struct hci_cmd_desc_tag* cmd_desc = hci_look_for_cmd_desc(opcode);

    #if (BLE_EMB_PRESENT && BLE_HOST_PRESENT && HCI_TL_SUPPORT)
    // HCI used by external Host
    hci_ext_host = true;
    #endif // (BLE_EMB_PRESENT && BLE_HOST_PRESENT && HCI_TL_SUPPORT)

    // Decrease the number of commands available for reception
    if(hci_tl_env.nb_h2c_cmd_pkts > INT8_MIN)
        hci_tl_env.nb_h2c_cmd_pkts--;

    // Check if the command is supported
    if(cmd_desc != NULL)
    {
        // Check if a new command can be received
        if (hci_tl_env.nb_h2c_cmd_pkts >= 0)
        {
            uint16_t dest = TASK_BLE_NONE;
            uint8_t ll_type = (cmd_desc->dest_field & HCI_CMD_DEST_LL_MASK) >> HCI_CMD_DEST_LL_POS;

            // Find the lower layers destination task
            switch(ll_type)
            {
                #if BT_EMB_PRESENT
                case BT_MNG:
                case MNG:
                {
                    dest = TASK_BLE_LM;
                }
                break;
                case BT_BCST:
                {
                    dest = TASK_BLE_LB;
                }
                break;
                #endif //BT_EMB_PRESENT
                #if BLE_EMB_PRESENT
                case BLE_MNG:
                #if !BT_EMB_PRESENT
                case MNG:
                #endif //BT_EMB_PRESENT
                {
                    dest = TASK_BLE_LLM;
                }
                break;
                #endif //BLE_EMB_PRESENT
                case DBG:
                {
                    dest = TASK_BLE_DBG;
                }
                break;
                #if (BT_EMB_PRESENT || (BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT)))
                #if BT_EMB_PRESENT
                case BT_CTRL_CONHDL:
                case BT_CTRL_BD_ADDR:
                #endif //BT_EMB_PRESENT
                #if (BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT))
                case BLE_CTRL:
                #endif //(BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT))
                case CTRL:
                {
                    #if BT_EMB_PRESENT
                    if(ll_type == BT_CTRL_BD_ADDR)
                    {
                        if(length >= sizeof(struct bd_addr))
                        {
                            // Look for a BD address matching in the table
                            for(int i = 0 ; i < MAX_NB_ACTIVE_ACL ; i++)
                            {
                                // Check BT connection state and BD address (assuming BD address is located at payload 1st 6 bytes)
                                if(   (hci_env.bt_acl_con_tab[i].state != HCI_BT_ACL_STATUS_NOT_ACTIVE)
                                   && (!memcmp(payload, &hci_env.bt_acl_con_tab[i].bd_addr.addr[0], sizeof(struct bd_addr))) )
                                {
                                    // Build the destination task ID
                                    dest = KERNEL_BUILD_ID(TASK_BLE_LC, i);
                                    break;
                                }
                            }
                        }
                    }
                    else
                    #endif //BT_EMB_PRESENT
                    {
                        // Check if the parameters can contain a connection handle
                        if(length >= 2)
                        {
                            // Retrieve connection handle from command parameters (expecting at payload 1st 2 bytes)
                            uint16_t conhdl = common_btohs(common_read16p(payload));

                            #if (BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT))
                            // Check if the connection handle corresponds to an active BLE link
                            if((conhdl <= BLE_CONHDL_MAX) && hci_env.ble_con_state[conhdl])
                            {
                                // Build the destination task ID
                                dest = KERNEL_BUILD_ID(TASK_BLE_LLC, conhdl);
                            }
                            #endif //(BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT))

                            #if BT_EMB_PRESENT
                            // Check if the connection handle corresponds to an active BT link (ACL or SCO)
                            conhdl &= ~(BT_SYNC_CONHDL_MSK);
                            if((conhdl >= BT_ACL_CONHDL_MIN) && (conhdl <= BT_ACL_CONHDL_MAX))
                            {
                                if(hci_env.bt_acl_con_tab[(conhdl - BT_ACL_CONHDL_MIN)].state == HCI_BT_ACL_STATUS_BD_ADDR_CONHDL)
                                {
                                    // Build the destination task ID
                                    dest = KERNEL_BUILD_ID(TASK_BLE_LC, conhdl - BT_ACL_CONHDL_MIN);
                                }
                            }
                            #endif //BT_EMB_PRESENT
                        }
                    }

                    // Reject command if not possible to match a valid connection
                    if(dest == TASK_BLE_NONE)
                    {
                        // Reject the command with error code "Unknown connection identifier"
                        hci_cmd_reject(cmd_desc, opcode, COMMON_ERROR_UNKNOWN_CONNECTION_ID, payload);
                    }
                }
                break;
                #if (BLE_EMB_PRESENT && BLE_ISO_PRESENT)
                case BLE_ISO:
                {
                    dest = TASK_BLE_LLI;
                }
                break;
                #endif // (BLE_EMB_PRESENT && BLE_ISO_PRESENT)
                #endif // (BT_EMB_PRESENT || (BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT)))
                default:
                {
                    BLE_ASSERT_INFO(0, ll_type, opcode);
                }
                break;
            }

            // Check if the command can be handled by the controller
            if(dest != TASK_BLE_NONE)
            {
                uint16_t unpk_length = 0;
                uint8_t status = COMMON_UTIL_PACK_OK;

                // Check if there are parameters (to compute the unpacked parameters size for Kernel message allocation)
                if (length > 0)
                {
                    if(cmd_desc->par_fmt != NULL)
                    {
                        // Check if the generic packer can be used (basic fixed-length format)
                        if(!(cmd_desc->dest_field & HCI_CMD_DEST_SPEC_PAR_PK_MSK))
                        {
                            // Compute the space needed for unpacked parameters
                            status = common_util_unpack(NULL, payload, &unpk_length, length, cmd_desc->par_fmt);
                        }
                        else
                        {
                            status = ((hci_pkupk_func_t)cmd_desc->par_fmt)(NULL, payload, &unpk_length, length);
                        }
                    }
                    else
                    {
                        status = COMMON_UTIL_PACK_ERROR;
                    }
                }

                // Check if there is input buffer overflow (received parameter size is less than expected for this command)
                if(status == COMMON_UTIL_PACK_IN_BUF_OVFLW)
                {
                    // Reject the command with error code "Invalid HCI parameters"
                    hci_cmd_reject(cmd_desc, opcode, COMMON_ERROR_INVALID_HCI_PARAM, NULL);
                }
                else
                {
                    // Allocate a Kernel message (with space for unpacked parameters)
                    void* cmd = kernel_msg_alloc(HCI_COMMAND, dest, opcode, unpk_length);

                    BLE_ASSERT_INFO(status == COMMON_UTIL_PACK_OK, status, opcode);

                    if(cmd == NULL)
                    {
                        // Reject the command with error code "Memory Capacity Exceeded"
                        hci_cmd_reject(cmd_desc, opcode, COMMON_ERROR_MEMORY_CAPA_EXCEED, NULL);
                    }
                    else
                    {
                        // Check if there are parameters to unpack
                        if ((unpk_length > 0) && (cmd_desc->par_fmt != NULL))
                        {
                            // Check if the generic packer can be used (basic fixed-length format)
                            if(!(cmd_desc->dest_field & HCI_CMD_DEST_SPEC_PAR_PK_MSK))
                            {
                                // Unpack parameters
                                status = common_util_unpack((uint8_t*) cmd, payload, &unpk_length, length, cmd_desc->par_fmt);
                            }
                            else
                            {
                                status = ((hci_pkupk_func_t)cmd_desc->par_fmt)((uint8_t*) cmd, payload, &unpk_length, length);
                            }
                        }

                        BLE_ASSERT_INFO(status == COMMON_UTIL_PACK_OK, status, opcode);

                        // Send the command to the internal destination task associated to this command
                        kernel_msg_send(cmd);
                    }
                }
            }
        }
        else
        {
            // Reject the command with error code "Memory Capacity Exceeded"
            hci_cmd_reject(cmd_desc, opcode, COMMON_ERROR_MEMORY_CAPA_EXCEED, NULL);
        }
    }
    else
    {
        // Reject the command with error code "Unknown HCI Command"
        hci_cmd_reject(NULL, opcode, COMMON_ERROR_UNKNOWN_HCI_COMMAND, NULL);
    }
}

uint8_t* hci_acl_tx_data_alloc(uint16_t hdl_flags, uint16_t len)
{
    uint8_t* buf = NULL;
    uint16_t dest = TASK_BLE_NONE;
    uint16_t max_len = 0;

    #if ((BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT)) || BT_EMB_PRESENT)
    // Retrieve connection handle from command parameters (expecting at payload 1st 2 bytes)
    uint16_t conhdl  = GETF(hdl_flags, HCI_ACL_HDR_HDL);
    uint8_t  bc_flag = GETF(hdl_flags, HCI_ACL_HDR_BC_FLAG);
    #endif // ((BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT)) || BT_EMB_PRESENT)

    #if (BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT))
    // Check if the connection handle corresponds to an active BLE link
    if((conhdl <= BLE_CONHDL_MAX) && (bc_flag == BCF_P2P))
    {
        // Build the destination task ID
        dest = KERNEL_BUILD_ID(TASK_BLE_LLC, conhdl);
        max_len = BLE_MAX_OCTETS;
    }
    #endif //(BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT))

    #if BT_EMB_PRESENT
    // Check if broadcast flags are set
    if (bc_flag == BCF_ACTIVE_SLV_BCST)
    {
        dest = TASK_BLE_LB;
        max_len = ACL_DATA_BUF_SIZE;
    }
    // Check if the connection handle corresponds to an active BT link
    else if(((bc_flag & BCF_MASK) == BCF_P2P) && (conhdl >= BT_ACL_CONHDL_MIN) && (conhdl <= BT_ACL_CONHDL_MAX))
    {
        if(hci_env.bt_acl_con_tab[(conhdl - BT_ACL_CONHDL_MIN)].state == HCI_BT_ACL_STATUS_BD_ADDR_CONHDL)
        {
            // Build the destination task ID
            dest = KERNEL_BUILD_ID(TASK_BLE_LC, conhdl - BT_ACL_CONHDL_MIN);
            max_len = ACL_DATA_BUF_SIZE;
        }
    }
    #endif //BT_EMB_PRESENT

    // Check if the requested size fits within BT/BLE data max length
    if((dest != TASK_BLE_NONE) && (len <= max_len))
    {
        switch(KERNEL_TYPE_GET(dest))
        {
            #if (BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT))
            case TASK_BLE_LLC:
            {
                // Try allocating a buffer from BLE pool
                uint16_t buf_ptr = ble_util_buf_acl_tx_alloc();

                if(buf_ptr != 0)
                {
                    // Give the pointer to the data space
                    buf = (uint8_t*) (EM_BASE_ADDR + buf_ptr);
                }
            }
            break;
            #endif //(BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT))

            #if BT_EMB_PRESENT
            case TASK_BLE_LC:
            case TASK_BLE_LB:
            {
                // Try allocating a buffer from BT pool
                uint16_t buf_ptr = bt_util_buf_acl_tx_alloc();

                if(buf_ptr != 0)
                {
                    // Give the pointer to the data space
                    buf = (uint8_t*) (EM_BASE_ADDR + buf_ptr);
                }
            }
            break;
            #endif //BT_EMB_PRESENT

            default:
            {
                BLE_ASSERT_ERR(0);
            }
            break;
        }

        if(buf == NULL)
        {
            // Report buffer oveflow
            struct hci_data_buf_ovflw_evt * evt = KERNEL_MSG_ALLOC(HCI_EVENT, 0, HCI_DATA_BUF_OVFLW_EVT_CODE, hci_data_buf_ovflw_evt);
            evt->link_type = ACL_TYPE;
            hci_send_2_host(evt);

            // Allocate a temporary buffer from heap
            buf = kernel_malloc(len, KERNEL_MEM_KERNEL_MSG);

            // Indicate the reception of ACL payload in a trash buffer
            hci_tl_env.acl_trash_payload = true;
        }
    }

    return buf;
}

void hci_acl_tx_data_received(uint16_t hdl_flags, uint16_t datalen, uint8_t * payload)
{
    do
    {
        #if ((BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT)) || BT_EMB_PRESENT)
        uint16_t conhdl = GETF(hdl_flags, HCI_ACL_HDR_HDL);
        #if BT_EMB_PRESENT
        uint8_t  bc_flag = GETF(hdl_flags, HCI_ACL_HDR_BC_FLAG);
        #endif //BT_EMB_PRESENT
        #endif // ((BLE_EMB_PRESENT && (HCI_BLE_CON_SUPPORT)) || BT_EMB_PRESENT)

        // If payload is received in trash buffer
        if(hci_tl_env.acl_trash_payload)
        {
            // Free the temporary buffer
            kernel_free(payload);

            // Clear the indication of ACL payload reception in a trash buffer
            hci_tl_env.acl_trash_payload = false;

            break;
        }

        #if (HCI_BLE_CON_SUPPORT)
        // Check if the received packet was considered BLE one at header reception
        if(conhdl <= BLE_CONHDL_MAX)
        {
            // Allocate a Kernel message
            struct hci_acl_data* data_tx = KERNEL_MSG_ALLOC(HCI_ACL_DATA, KERNEL_BUILD_ID(TASK_BLE_LLC, conhdl), 0, hci_acl_data);
            data_tx->conhdl_pb_bc_flag          = hdl_flags;
            data_tx->length                     = datalen;
            data_tx->buf_ptr                    = (datalen > 0) ? (((uint32_t) payload) - EM_BASE_ADDR) : 0;
            kernel_msg_send(data_tx);

            break;
        }
        #endif //(HCI_BLE_CON_SUPPORT)

        #if BT_EMB_PRESENT
        // Check if the received packet was considered BT one at header reception
        if( ((bc_flag == BCF_ACTIVE_SLV_BCST) || ((conhdl >= BT_ACL_CONHDL_MIN) && (conhdl <= BT_ACL_CONHDL_MAX))) )
        {
            // Select the destination task, according to the broadcast flag
            uint16_t dest_id = (bc_flag == BCF_ACTIVE_SLV_BCST) ? TASK_BLE_LB : KERNEL_BUILD_ID(TASK_BLE_LC, conhdl - BT_ACL_CONHDL_MIN);

            // Allocate a Kernel message
            struct hci_acl_data* data_tx = KERNEL_MSG_ALLOC(HCI_ACL_DATA, dest_id, 0, hci_acl_data);
            data_tx->conhdl_pb_bc_flag          = hdl_flags;
            data_tx->length                     = datalen;
            data_tx->buf_ptr                    = (datalen > 0) ? (((uint32_t) payload) - EM_BASE_ADDR) : 0;
            kernel_msg_send(data_tx);

            break;
        }
        #endif //BT_EMB_PRESENT

        BLE_ASSERT_INFO(0, datalen, hdl_flags);
    } while(0);
}

#if (BT_EMB_PRESENT)
#if (VOICE_OVER_HCI)
uint8_t* hci_sync_tx_data_alloc(uint16_t conhdl_flags, uint8_t len)
{
    uint8_t* buf = NULL;

    // Retrieve connection handle from command parameters (expecting at payload 1st 2 bytes)
    uint16_t conhdl = GETF(conhdl_flags, HCI_SYNC_HDR_HDL);
    uint8_t sync_link_id = BT_SYNC_CONHDL_LID(conhdl);

    // There are many conditions where the packet is not valid (invalid connection handle, no SCO link, not in Voice over HCI)
    do
    {
        // Check if the connection handle corresponds to a possible synchronous link
        if(sync_link_id >= MAX_NB_SYNC)
            break;

        // Check if the connection handle corresponds to a possible an BT link
        conhdl &= ~(BT_SYNC_CONHDL_MSK);
        if((conhdl < BT_ACL_CONHDL_MIN) && (conhdl > BT_ACL_CONHDL_MAX))
            break;

        // Check if the ACL link is active
        if(hci_env.bt_acl_con_tab[(conhdl - BT_ACL_CONHDL_MIN)].state != HCI_BT_ACL_STATUS_BD_ADDR_CONHDL)
            break;

        // Try allocating a buffer from BT pool
        buf = (uint8_t *) (uint32_t) bt_util_buf_sync_tx_alloc(sync_link_id, len);

    } while (0);

    // If a buffer element has been allocated
    if(buf != 0)
    {
        // Give the pointer to the data space
        buf += EM_BASE_ADDR;
    }
    else
    {
        // Allocate a temporary buffer from heap
        buf = kernel_malloc(len, KERNEL_MEM_KERNEL_MSG);

        // Indicate the reception of ACL payload in a trash buffer
        hci_tl_env.acl_trash_payload = true;
    }

    return buf;
}

void hci_sync_tx_data_received(uint16_t conhdl_flags, uint8_t len, uint8_t * payload)
{
    uint16_t conhdl = GETF(conhdl_flags, HCI_SYNC_HDR_HDL);
    conhdl &= ~(BT_SYNC_CONHDL_MSK);

    // Check if the received packet is in a valid buffer
    if(!hci_tl_env.acl_trash_payload)
    {
        // Sends the Kernel message (with space for unpacked parameters)
        struct hci_sync_data* data_tx = KERNEL_MSG_ALLOC(HCI_SYNC_DATA, KERNEL_BUILD_ID(TASK_BLE_LC, conhdl - BT_ACL_CONHDL_MIN), 0, hci_sync_data);
        data_tx->buf_ptr = (uint16_t) ((uint32_t)payload - EM_BASE_ADDR);
        data_tx->conhdl_psf = conhdl_flags;
        data_tx->length = len;
        kernel_msg_send(data_tx);
    }
    else
    {
        // Free the temporary buffer
        kernel_free(payload);

        // Clear the indication of ACL payload reception in a trash buffer
        hci_tl_env.acl_trash_payload = false;
    }
}

#endif //(VOICE_OVER_HCI)
#endif //(BT_EMB_PRESENT)
#endif //(BLE_EMB_PRESENT || BT_EMB_PRESENT)

#if (BLE_HOST_PRESENT && !BLE_EMB_PRESENT)
#if (HCI_BLE_CON_SUPPORT)
uint8_t* hci_acl_rx_data_alloc(uint16_t hdl_flags, uint16_t len)
{
    return kernel_malloc(len, KERNEL_MEM_NON_RETENTION);
}

void hci_acl_rx_data_received(uint16_t hdl_flags, uint16_t datalen, uint8_t * payload)
{
    uint16_t conhdl = GETF(hdl_flags, HCI_ACL_HDR_HDL);

    // Get connection index
    uint8_t idx = gapc_get_conidx(conhdl);

    if ((conhdl != GAP_INVALID_CONHDL) &&
            (idx != GAP_INVALID_CONIDX))
    {
        // Allocate a Kernel message (with space for unpacked parameters)
        struct hci_acl_data* data_rx = KERNEL_MSG_ALLOC(HCI_ACL_DATA, KERNEL_BUILD_ID(TASK_BLE_L2CC, idx), 0, hci_acl_data);
        data_rx->conhdl_pb_bc_flag = hdl_flags;

        data_rx->length = datalen;

        // retrieve data buffer
        data_rx->buf_ptr = (uint32_t) payload;

        // Send the kernel message
        kernel_msg_send(data_rx);
    }
    else
    {
        BLE_ASSERT_INFO(0, conhdl, idx);
    }
}
#endif // (HCI_BLE_CON_SUPPORT)

uint8_t hci_evt_received(uint8_t code, uint8_t length, uint8_t *payload)
{
    uint8_t status = COMMON_UTIL_PACK_OK;

    switch(code)
    {
        case HCI_CMD_CMP_EVT_CODE:
        {
            if(length >= HCI_CCEVT_HDR_PARLEN)
            {
                // Retrieve opcode from parameters, expected at position 1 in payload (after nb cmp pkts)
                uint16_t opcode = common_btohs(common_read16p(payload + 1));
                // Look for the command descriptor
                const struct hci_cmd_desc_tag* cmd_desc = hci_look_for_cmd_desc(opcode);

                // Check if the command is supported
                if(cmd_desc != NULL)
                {
                    uint16_t dest = TASK_BLE_NONE;
                    uint8_t hl_type = (cmd_desc->dest_field & HCI_CMD_DEST_HL_MASK) >> HCI_CMD_DEST_HL_POS;

                    // Find the higher layers destination task
                    switch(hl_type)
                    {
                        case HL_MNG:
                        {
                            // Build the destination task ID
                            dest = TASK_BLE_GAPM;
                        }
                        break;
                        case HL_CTRL:
                        case HL_DATA:
                        {
#if (HCI_BLE_CON_SUPPORT)
                            // Parse connection handle
                            uint16_t conhdl = hci_look_for_conhdl(code, payload, (uint8_t *)cmd_desc->ret_par_fmt);
                            // Get connection index
                            uint8_t idx = gapc_get_conidx(conhdl);

                            if ((conhdl != GAP_INVALID_CONHDL) &&
                                    (idx != GAP_INVALID_CONIDX))
                            {
                                // Build the destination task ID if found
                                dest = (hl_type == HL_CTRL) ?
                                        KERNEL_BUILD_ID(TASK_BLE_GAPC, idx) : KERNEL_BUILD_ID(TASK_BLE_L2CC, idx);
                            }
#else
                            // Forward message to the first instance
                            dest = (hl_type == HL_CTRL) ?
                                    TASK_BLE_GAPC : TASK_BLE_L2CC;
#endif //(HCI_BLE_CON_SUPPORT)
                        }
                        break;
                        #if (BLE_ISO_MODE_0)
                        case HL_ISO0:
                        {
                            // Parse connection handle
                            uint16_t conhdl = hci_look_for_conhdl(code, payload, (uint8_t *)cmd_desc->ret_par_fmt);
                            // Get connection index
                            uint8_t idx = gapc_get_conidx(conhdl);

                            if ((conhdl != GAP_INVALID_CONHDL) && (idx != GAP_INVALID_CONIDX))
                            {
                                // Build the destination task ID if found
                                dest = KERNEL_BUILD_ID(TASK_BLE_AM0, idx);
                            }
                        }break;
                        #endif // (BLE_ISO_MODE_0)

                        default:
                        {
                            BLE_ASSERT_INFO(0, hl_type, opcode);
                        }
                        break;
                    }

                    if(dest != TASK_BLE_NONE)
                    {
                        uint16_t unpk_length = length - HCI_CCEVT_HDR_PARLEN;
                        void* evt;

                        // Check if there are parameters
                        if (unpk_length > 0)
                        {
                            if(cmd_desc->ret_par_fmt != NULL)
                            {
                                // Check if the generic packer can be used (basic fixed-length format)
                                if(!(cmd_desc->dest_field & HCI_CMD_DEST_SPEC_RET_PAR_PK_MSK))
                                {
                                    // Compute the space needed for unpacked parameters
                                    status = common_util_unpack(NULL, NULL, &unpk_length, length - HCI_CCEVT_HDR_PARLEN, cmd_desc->ret_par_fmt);
                                }
                                else
                                {
                                    status = ((hci_pkupk_func_t)cmd_desc->ret_par_fmt)(NULL, NULL, &unpk_length, length - HCI_CCEVT_HDR_PARLEN);
                                }
                            }
                            else
                            {
                                status = COMMON_UTIL_PACK_ERROR;
                            }

                            BLE_ASSERT_INFO((status == COMMON_UTIL_PACK_OK), status, opcode);
                        }

                        // Allocate a Kernel message (with space for unpacked parameters)
                        evt = kernel_msg_alloc(HCI_CMD_CMP_EVENT, dest, opcode, unpk_length);

                        // Check if there are parameters to unpack
                        if (unpk_length > 0)
                        {
                            if(cmd_desc->ret_par_fmt != NULL)
                            {
                                // Check if the generic unpacker can be used (basic fixed-length format)
                                if(!(cmd_desc->dest_field & HCI_CMD_DEST_SPEC_RET_PAR_PK_MSK))
                                {
                                    // Unpack parameters
                                    status = common_util_unpack((uint8_t*) evt, payload + HCI_CCEVT_HDR_PARLEN, &unpk_length, length - HCI_CCEVT_HDR_PARLEN, cmd_desc->ret_par_fmt);
                                }
                                else
                                {
                                    status = ((hci_pkupk_func_t)cmd_desc->ret_par_fmt)((uint8_t*) evt, payload + HCI_CCEVT_HDR_PARLEN, &unpk_length, length - HCI_CCEVT_HDR_PARLEN);
                                }
                            }
                        }

                        // Send the command to the internal destination task associated to this event
                        kernel_msg_send(evt);
                    }
                    else
                    {
                        BLE_ASSERT_INFO(0, hl_type, opcode);
                    }
                }
                else
                {
                    BLE_ASSERT_INFO(0, opcode, 0);
                }
            }
            else
            {
                BLE_ASSERT_INFO(0, length, 0);
            }
        }
        break;
        case HCI_CMD_STATUS_EVT_CODE:
        {
            if(length == HCI_CSEVT_PARLEN)
            {
                // Retrieve opcode from parameters, expected at position 2 in payload (after status and nb cmp pkts)
                uint16_t opcode = common_btohs(common_read16p(payload + 2));
                // Look for the command descriptor
                const struct hci_cmd_desc_tag* cmd_desc = hci_look_for_cmd_desc(opcode);

                // Check if the command is supported
                if(cmd_desc != NULL)
                {
                    uint16_t dest = TASK_BLE_NONE;
                    uint8_t hl_type = (cmd_desc->dest_field & HCI_CMD_DEST_HL_MASK) >> HCI_CMD_DEST_HL_POS;

                    // Find the higher layers destination task
                    switch(hl_type)
                    {
                        case HL_MNG:
                        {
                            // Build the destination task ID
                            dest = TASK_BLE_GAPM;
                        }
                        break;

                        case HL_CTRL:
                        case HL_DATA:
                        {
#if (HCI_BLE_CON_SUPPORT)
                            // Get connection index
                            uint8_t idx = hci_search_cs_index(opcode);

                            if (idx != GAP_INVALID_CONIDX)
                            {
                                // Build the destination task ID if found
                                dest = (hl_type == HL_CTRL) ?
                                        KERNEL_BUILD_ID(TASK_BLE_GAPC, idx) : KERNEL_BUILD_ID(TASK_BLE_L2CC, idx);
                            }
#else
                            // Forward message to the first instance
                            dest = (hl_type == HL_CTRL) ?
                                    TASK_BLE_GAPC : TASK_BLE_L2CC;
#endif //(HCI_BLE_CON_SUPPORT)
                        }
                        break;

                        default:
                        {
                            BLE_ASSERT_INFO(0, hl_type, opcode);
                        }
                        break;
                    }

                    if(dest != TASK_BLE_NONE)
                    {
                        // Allocate a Kernel message (with space for unpacked parameters)
                        void* evt = kernel_msg_alloc(HCI_CMD_STAT_EVENT, dest, opcode, length);

                        // Send the command to the internal destination task associated to this event
                        kernel_msg_send(evt);
                    }
                    else
                    {
                        BLE_ASSERT_INFO(0, hl_type, opcode);
                    }
                }
                else
                {
                    BLE_ASSERT_INFO(0, opcode, 0);
                }
            }
            else
            {
                BLE_ASSERT_INFO(0, length, 0);
            }
        }
        break;
        case HCI_LE_META_EVT_CODE:
        default:
        {
            uint8_t code_or_subcode = code;
            // Find an event descriptor associated to the event code
            const struct hci_evt_desc_tag* evt_desc;
            if(code_or_subcode == HCI_LE_META_EVT_CODE)
            {
                code_or_subcode = *payload;
                evt_desc = hci_look_for_le_evt_desc(code_or_subcode);
            }
            else
            {
                evt_desc = hci_look_for_evt_desc(code_or_subcode);
            }

            // Check if the evt is supported
            if(evt_desc != NULL)
            {
                uint16_t dest = TASK_BLE_NONE;
                uint8_t hl_type = (evt_desc->dest_field & HCI_EVT_DEST_HL_MASK) >> HCI_EVT_DEST_HL_POS;

                // Find the higher layers destination task
                switch(hl_type)
                {
                    case HL_MNG:
                    {
                        // Build the destination task ID
                        dest = TASK_BLE_GAPM;
                    }
                    break;

                    case HL_CTRL:
                    case HL_DATA:
                    {
                        #if (HCI_BLE_CON_SUPPORT)
                        // Parse connection handle
                        uint16_t conhdl = hci_look_for_conhdl(code, payload, (uint8_t *)evt_desc->par_fmt);
                        // Get connection index
                        uint8_t idx = gapc_get_conidx(conhdl);

                        if ((conhdl != GAP_INVALID_CONHDL) && (idx != GAP_INVALID_CONIDX))
                        {
                            // Build the destination task ID if found
                            dest = (hl_type == HL_CTRL) ? KERNEL_BUILD_ID(TASK_BLE_GAPC, idx) : KERNEL_BUILD_ID(TASK_BLE_L2CC, idx);
                        }
                        else
                        {
                            dest = HL_MNG; // let manager to forward message later
                        }

                        #else
                        // Forward message to the first instance
                        dest = (hl_type == HL_CTRL) ? TASK_BLE_GAPC : TASK_BLE_L2CC;
                        #endif //(HCI_BLE_CON_SUPPORT)
                    }
                    break;

                    default:
                    {
                        BLE_ASSERT_INFO(0, hl_type, code);
                    }
                    break;
                }

                if(dest != TASK_BLE_NONE)
                {
                    uint16_t unpk_length = 0;
                    void* evt;

                    // Check if there are parameters
                    if (length > 0)
                    {
                        if(evt_desc->par_fmt != NULL)
                        {
                            // Check if the generic packer can be used (basic fixed-length format)
                            if(evt_desc->special_pack == PK_GEN)
                            {
                                // Compute the space needed for unpacked parameters
                                status = common_util_unpack(NULL, NULL, &unpk_length, length, evt_desc->par_fmt);
                            }
                            else
                            {
                                status = ((hci_pkupk_func_t)evt_desc->par_fmt)(NULL, NULL, &unpk_length, length);
                            }

                            BLE_ASSERT_INFO((status == COMMON_UTIL_PACK_OK), status, code);
                        }
                        else
                        {
                            status = COMMON_UTIL_PACK_ERROR;
                        }
                    }

                    // Allocate a Kernel message (with space for unpacked parameters)
                    if(code == HCI_LE_META_EVT_CODE)
                    {
                        evt = kernel_msg_alloc(HCI_LE_EVENT, dest, 0, unpk_length);
                    }
                    else
                    {
                        evt = kernel_msg_alloc(HCI_EVENT, dest, code_or_subcode, unpk_length);
                    }

                    // Check if there are parameters to unpack
                    if (unpk_length > 0)
                    {
                        if(evt_desc->par_fmt != NULL)
                        {
                            // Check if the generic unpacker can be used (basic fixed-length format)
                            if(evt_desc->special_pack == PK_GEN)
                            {
                                // Unpack parameters
                                status = common_util_unpack((uint8_t*) evt, payload, &unpk_length, length, evt_desc->par_fmt);
                            }
                            else
                            {
                                status = ((hci_pkupk_func_t)evt_desc->par_fmt)((uint8_t*) evt, payload, &unpk_length, length);
                            }
                        }

                        BLE_ASSERT_INFO((status == COMMON_UTIL_PACK_OK), status, code);
                    }

                    // Send the command to the internal destination task associated to this event
                    kernel_msg_send(evt);
                }
                else
                {
                    BLE_ASSERT_INFO(0, hl_type, code_or_subcode);
                }
            }
            else
            {
                BLE_ASSERT_INFO(0, code_or_subcode, 0);
            }
        }
        break;
    }

    return (status);
}
#endif //(BLE_HOST_PRESENT && !BLE_EMB_PRESENT)


#endif // (HCI_TL_SUPPORT)
#endif //(HCI_PRESENT)

/// @} HCI

/**
 ****************************************************************************************
 *
 * @file hci.c
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

#if (CFG_USE_BK_HOST)
#if (HCI_PRESENT)
#include "rwip.h"            // IP definitions
#include <string.h>          // string manipulation
#include "common_error.h"        // error definition
#include "common_utils.h"        // common utility definition
#include "common_list.h"         // list definition

#include "hci.h"             // hci definition
#include "hci_int.h"         // hci internal definition

#include "kernel_msg.h"          // kernel message declaration
#include "kernel_task.h"         // kernel task definition
#include "kernel_event.h"        // kernel event definition
#include "kernel_mem.h"          // kernel memory definition
#include "kernel_timer.h"        // kernel timer definition

#if (BLE_HOST_PRESENT)
#include "gapc.h"            // use to retrieve the connection index from the connection handle
#endif //(BLE_HOST_PRESENT)

#include "dbg.h"

/*
 * DEFINES
 ****************************************************************************************
 */


/*
 * ENUMERATIONS DEFINITIONS
 ****************************************************************************************
 */

/*
 * STRUCTURES DEFINITIONS
 ****************************************************************************************
 */



/*
 * CONSTANTS DEFINITIONS
 ****************************************************************************************
 */

/// Default event mask
static const struct evt_mask hci_def_evt_msk = {EVT_MASK_DEFAULT};

#if (BLE_EMB_PRESENT)
/// Default LE event mask
static const struct evt_mask hci_le_def_evt_msk = {BLE_EVT_MASK};
#endif // (BLE_EMB_PRESENT)

/// Reserved event mask
static const struct evt_mask hci_rsvd_evt_msk = {{0x00, 0x60, 0x04, 0x00, 0xF8, 0x07, 0x40, 0x02}};


/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

#if (BLE_EMB_PRESENT && BLE_HOST_PRESENT && HCI_TL_SUPPORT)
/**
 * Indicate if HCI is used by external Host, or internal Host.
 * Used in Full mode only. By default HCI is used by internal Host.
 * HCI switches to external host as soon as an HCI command is received.
 * This variable should not be reset.
 */
bool hci_ext_host;
#endif // (BLE_EMB_PRESENT && BLE_HOST_PRESENT && HCI_TL_SUPPORT)

///HCI environment context
struct hci_env_tag hci_env;


/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

#if (BLE_EMB_PRESENT || BT_EMB_PRESENT)
/**
 ****************************************************************************************
 * @brief Check if the event to be sent to the host is masked or not
 *
 * @param[in] msg  Pointer to the message containing the event
 *
 * @return true id the message has to be filtered out, false otherwise
 *****************************************************************************************
 */
static bool hci_evt_mask_check(struct kernel_msg *msg)
{
    bool masked = false;
    uint8_t evt_code;

    switch(msg->id)
    {
        case HCI_LE_EVENT:
        {
            // LE meta event
            evt_code = HCI_LE_META_EVT_CODE;
        }
        break;
        case HCI_EVENT:
        {
            // Get event code
            evt_code = msg->src_id;
        }
        break;

        default:
        {
            // Cannot be masked
            return false;
        }
    }

    // Check if this event is maskable
    if(evt_code < HCI_MAX_EVT_MSK_PAGE_1_CODE)
    {
        uint8_t index = evt_code - 1;

        //Checking if the event is masked or not
        masked = ((hci_env.evt_msk.mask[index>>3] & (1<<(index & 0x07))) == 0x00);

        #if (BLE_EMB_PRESENT)
        if (!masked && (evt_code == HCI_LE_META_EVT_CODE))
        {
            // Get Sub-code of the mevent
            uint8_t *subcode = (uint8_t*)kernel_msg2param(msg);
            //Translate the subcode in index to parse the mask
            uint8_t index = *subcode - 1;
            //Checking if the event is masked or not
            masked =((hci_env.le_evt_msk.mask[index>>3] & (1<<(index & 0x07))) == 0x00);
        }
        #endif // (BLE_EMB_PRESENT)
    }
    else if(evt_code < HCI_MAX_EVT_MSK_PAGE_2_CODE)
    {
        // In this area the evt code is in the range [EVT_MASK_CODE_MAX<evt_code<HCI_MAX_EVT_MSK_CODE]
        // The index should be < EVT_MASK_CODE_MAX to avoid evt_msk_page_2.mask array overflow
        uint8_t index = evt_code - EVT_MASK_CODE_MAX;

        //Checking if the event is masked or not
        masked = ((hci_env.evt_msk_page_2.mask[index>>3] & (1<<(index & 0x07))) == 0x00);
    }
    return masked;
}
#endif //(BLE_EMB_PRESENT || BT_EMB_PRESENT)

#if BT_EMB_PRESENT
/**
 ****************************************************************************************
 * @brief Check if a connection request event has to be forwarded to the host or not
 * This function also takes the required decisions according to the auto_accept parameter
 *
 * @param[in] bdaddr    BDADDR contained in the inquiry result event
 * @param[in] class     Class of device contained in the inquiry result event
 * @param[in] link_type Type of link that is requested (asynchronous or synchronous)
 *
 * @return true if the event has to be filtered out, false otherwise
 *****************************************************************************************
 */
static uint8_t hci_evt_filter_con_check(struct bd_addr *bdaddr, struct devclass   *classofdev, uint8_t link_type, uint8_t link_id)
{
    uint8_t filtered = false;
    uint8_t auto_accept = DO_NOT_AUTO_ACCEPT_CONNECTION;

    /* Check if a Connection type is present (start from last to first)           */
    for (int i = HCI_FILTER_NB - 1 ; i >= 0 ; i--)
    {
        struct hci_evt_filter_tag *filter = &hci_env.evt_filter[i];

        // If this filter is a ConnectionSetupFilter
        if ((filter->in_use == true) && (filter->type == CONNECTION_FILTER_TYPE))
        {
            // There is at least one connection filter set, so now we should reject by default unless
            // the connection request matches one of the filters
            auto_accept = 0xFF;

            // If the condition is a All Device type
            if (filter->condition == ALL_FILTER_CONDITION_TYPE)
            {
                // This connection will not be rejected
                auto_accept = filter->auto_accept;
                break;
            }
            // If the condition is a ClassOfDevice type
            else if (filter->condition == CLASS_FILTER_CONDITION_TYPE)
            {
                struct devclass class_masked;
                struct classofdevcondition *cond = &filter->param.device_class;

                // Remove don't care bit of Class Of Device
                class_masked.A[0] = classofdev->A[0] & cond->class_mask.A[0];
                class_masked.A[1] = classofdev->A[1] & cond->class_mask.A[1];
                class_masked.A[2] = classofdev->A[2] & cond->class_mask.A[2];

                // Check if any device class is allowed or the Class of the filter matches the Class of the Inquiry result
                if (    (cond->classofdev.A[0] == 0 && cond->classofdev.A[1] == 0 && cond->classofdev.A[2] == 0)
                     || !memcmp(&class_masked, &cond->classofdev, sizeof(struct devclass)))
                {
                    // This connection will not be rejected
                    auto_accept = filter->auto_accept;
                    break;
                }
            }
            /* If this filter is a ConnectionFilter                                     */
            else if (filter->condition == BD_ADDR_FILTER_CONDITION_TYPE)
            {
                // Check if the BdAddr of the filter matches the BdAddr of the Connect req
                if (!memcmp(bdaddr, &filter->param.bdaddr, sizeof(struct bd_addr)))
                {
                    // This connection will not be rejected
                    auto_accept = filter->auto_accept;
                    break;
                }
            }
        }
    }

    if ((auto_accept == ACCEPT_CONNECTION_SLAVE) || (auto_accept == ACCEPT_CONNECTION_MASTER))
    {
        // Auto accept the connection request
        filtered = true;

        // If this is an ACL link
        if (link_type == ACL_TYPE)
        {
            struct hci_accept_con_req_cmd *cmd = KERNEL_MSG_ALLOC(HCI_COMMAND, KERNEL_BUILD_ID(TASK_BLE_LC, link_id), HCI_ACCEPT_CON_REQ_CMD_OPCODE, hci_accept_con_req_cmd);

            // Fill-in the parameter structure
            if (auto_accept == ACCEPT_CONNECTION_SLAVE)
            {
                cmd->role = ACCEPT_REMAIN_SLAVE;
            }
            else
            {
                cmd->role = ACCEPT_SWITCH_TO_MASTER;
            }
            cmd->bd_addr = *bdaddr;

            // Send the message
            kernel_msg_send(cmd);

            // Save opcode, used to filter the returning CS event
            hci_env.auto_accept_opcode = HCI_ACCEPT_CON_REQ_CMD_OPCODE;
        }
        #if (MAX_NB_SYNC > 0)
        // If this is a Synchronous Link (SCO or eSCO)
        else
        {
            struct hci_accept_sync_con_req_cmd *cmd = KERNEL_MSG_ALLOC(HCI_COMMAND, KERNEL_BUILD_ID(TASK_BLE_LC, link_id), HCI_ACCEPT_SYNC_CON_REQ_CMD_OPCODE, hci_accept_sync_con_req_cmd);

            // Fill in parameter structure
            cmd->bd_addr = *bdaddr;
            cmd->tx_bw = SYNC_BANDWIDTH_DONT_CARE;
            cmd->rx_bw = SYNC_BANDWIDTH_DONT_CARE;
            cmd->max_lat = SYNC_DONT_CARE_LATENCY;
            cmd->vx_set = hci_env.voice_settings;
            cmd->retx_eff = SYNC_RE_TX_DONT_CARE;
            cmd->pkt_type = 0xFFFF;   /// All packet type

            // Send the message
            kernel_msg_send(cmd);

            // Save opcode, used to filter the returning CS event
            hci_env.auto_accept_opcode = HCI_ACCEPT_SYNC_CON_REQ_CMD_OPCODE;
        }
        #endif // (MAX_NB_SYNC > 0)
    }
    else if (auto_accept != DO_NOT_AUTO_ACCEPT_CONNECTION)
    {
        // This Device does not match a filter => filtered and rejected
        filtered = true;

        // If this is an ACL link
        if (link_type == ACL_TYPE)
        {
            struct hci_reject_con_req_cmd *cmd = KERNEL_MSG_ALLOC(HCI_COMMAND, KERNEL_BUILD_ID(TASK_BLE_LC, link_id), HCI_REJECT_CON_REQ_CMD_OPCODE, hci_reject_con_req_cmd);
            cmd->bd_addr = *bdaddr;
            cmd->reason = COMMON_ERROR_CONN_REJ_UNACCEPTABLE_BDADDR;
            kernel_msg_send(cmd);

            // Save opcode, used to filter the returning CS event
            hci_env.auto_accept_opcode = HCI_REJECT_CON_REQ_CMD_OPCODE;
        }
        #if (MAX_NB_SYNC > 0)
        // If this is a Synchronous Link (SCO or eSCO)
        else
        {
            struct hci_reject_sync_con_req_cmd *cmd = KERNEL_MSG_ALLOC(HCI_COMMAND, KERNEL_BUILD_ID(TASK_BLE_LC, link_id), HCI_REJECT_SYNC_CON_REQ_CMD_OPCODE, hci_reject_sync_con_req_cmd);
            cmd->bd_addr = *bdaddr;
            cmd->reason = COMMON_ERROR_CONN_REJ_UNACCEPTABLE_BDADDR;
            kernel_msg_send(cmd);

            // Save opcode, used to filter the returning CS event
            hci_env.auto_accept_opcode = HCI_REJECT_SYNC_CON_REQ_CMD_OPCODE;
        }
        #endif // (MAX_NB_SYNC > 0)
    }

    return(filtered);
}

/**
 ****************************************************************************************
 * @brief Check if an inquiry result event has to be forwarded to the host or not
 *
 * @param[in] bdaddr  BDADDR contained in the inquiry result event
 * @param[in] class   Class of device contained in the inquiry result event
 *
 * @return true if the event has to be filtered out, false otherwise
 *****************************************************************************************
 */
static uint8_t hci_evt_filter_inq_check(struct bd_addr *bdaddr, struct devclass *classofdev)
{
    int     i;
    uint8_t filtered = false;  // By default the event is not filtered
    struct devclass empty_classofdev = {{0,0,0}};

    // Check if an inquiry filter type is present
    for (i = 0; i < HCI_FILTER_NB; i++)
    {
        struct hci_evt_filter_tag *filter = &hci_env.evt_filter[i];

        // If this filter is an InquiryFilter
        if ((filter->in_use == true) && (filter->type == INQUIRY_FILTER_TYPE))
        {
            // There is at least one inquiry filter set, so now we should filter unless
            // the inquiry result matches one of the filters
            filtered = true;

            // If the condition is a ClassOfDevice type
            if (filter->condition == CLASS_FILTER_CONDITION_TYPE)
            {
                struct devclass class_masked;
                struct classofdevcondition *cond = &filter->param.device_class;

                // Remove don't care bit of Class Of Device
                class_masked.A[0] = classofdev->A[0] & cond->class_mask.A[0];
                class_masked.A[1] = classofdev->A[1] & cond->class_mask.A[1];
                class_masked.A[2] = classofdev->A[2] & cond->class_mask.A[2];

                // Check if the Class of the filter match the Class of the Inquiry result
                if (   !memcmp(&empty_classofdev.A[0], &cond->classofdev.A[0], sizeof(struct devclass))
                    || !memcmp(&class_masked.A[0]    , &cond->classofdev.A[0], sizeof(struct devclass)) )
                {
                    // This InquiryResult must NOT be filtered
                    filtered = false;
                    break;
                }
            }
            // If this filter is a BDADDR type
            else if (filter->condition == BD_ADDR_FILTER_CONDITION_TYPE)
            {
                // Check if the BdAddr of the filter match the BdAddr of the Inquiry res
                if (!memcmp(bdaddr, &filter->param.bdaddr, sizeof(struct bd_addr)))
                {
                    // This InquiryResult must NOT be filtered
                    filtered = false;
                    break;
                }
            }
        }
    }

    return(filtered);
}

/**
 ****************************************************************************************
 * @brief Check if an event has to be forwarded to the host or not
 *
 * @param[in] msg  Pointer to the event message to be transmitted to the host
 *
 * @return true if the event has to be filtered out, false otherwise
 *****************************************************************************************
 */
static uint8_t hci_evt_filter_check(struct kernel_msg *msg)
{
    uint8_t filtered = false;

    switch(msg->id)
    {
        case HCI_EVENT:
        {
            // Get event code
            uint8_t evt_code = msg->src_id;
            uint8_t *param = (uint8_t *)msg->param;

            switch(evt_code)
            {
                // InquiryResult Event
                case HCI_INQ_RES_EVT_CODE:
                case HCI_INQ_RES_WITH_RSSI_EVT_CODE:
                case HCI_EXT_INQ_RES_EVT_CODE:
                {
                    struct devclass *classofdev;
                    // Retrieve the information required for the filtering from the PDU
                    struct bd_addr * bdaddr = (struct bd_addr *)&param[1];
                    if(evt_code == HCI_INQ_RES_EVT_CODE)
                    {
                        struct hci_inq_res_evt *evt = (struct hci_inq_res_evt *) param;
                        classofdev = (struct devclass *)&evt->class_of_dev;
                    }
                    else
                    {
                        struct hci_ext_inq_res_evt *evt = (struct hci_ext_inq_res_evt *) param;
                        classofdev = (struct devclass *)&evt->class_of_dev;
                    }

                    // Check if the event has to be filtered or not
                    filtered = hci_evt_filter_inq_check(bdaddr, classofdev);
                }
                break;
                // Connection Request Event
                case HCI_CON_REQ_EVT_CODE:
                {
                    // Retrieve the information required for the filtering from the PDU
                    struct bd_addr * bdaddr = (struct bd_addr *)&param[0];
                    struct devclass *classofdev = (struct devclass *)&param[6];
                    uint8_t link_type = param[9];

                    // Check if the event has to be filtered or not
                    filtered = hci_evt_filter_con_check(bdaddr, classofdev, link_type, msg->dest_id);
                }
                break;
                // Connection Complete Event
                case HCI_CON_CMP_EVT_CODE:
                {
                    // Check if a connection was auto-rejected
                    if(hci_env.auto_reject)
                    {
                        filtered = true;
                        hci_env.auto_reject = false;
                    }
                }
                break;
                default:
                {
                    // Nothing to do
                }
            }
        }
        break;

        case HCI_CMD_STAT_EVENT:
        {
            // Filter CS event associated to the current auto-accept command
            if(msg->src_id == hci_env.auto_accept_opcode)
            {
                filtered = true;
                hci_env.auto_accept_opcode = 0x0000;

                if(msg->src_id == HCI_REJECT_CON_REQ_CMD_OPCODE)
                {
                    hci_env.auto_reject = true;
                }
            }
        }
        break;

        default:
        {
            // Not an event
        }
        break;
    }

    return filtered;
}

/**
 ****************************************************************************************
 * @brief Reset the list of event filters
 *
 * @return Status
 *****************************************************************************************
 */
static uint8_t hci_evt_filter_reset(void)
{
    for (int i = 0 ; i < HCI_FILTER_NB; i++)
    {
        hci_env.evt_filter[i].in_use = false;
    }
    return (COMMON_ERROR_NO_ERROR);
}

/**
 ****************************************************************************************
 * @brief Allocate an event filter structure
 *
 * @return A pointer to the allocated filter, if any, NULL otherwise
 *****************************************************************************************
 */
static struct hci_evt_filter_tag *hci_evt_filter_alloc(void)
{
    int i;
    struct hci_evt_filter_tag *evt_filter;

    for (i = 0; i < HCI_FILTER_NB; i++)
    {
        if (hci_env.evt_filter[i].in_use == false)
        {
            break;
        }
    }
    if (i < HCI_FILTER_NB)
    {
        evt_filter = &hci_env.evt_filter[i];
        evt_filter->in_use = true;
    }
    else
    {
        evt_filter = NULL;
    }

    return(evt_filter);
}

/**
 ****************************************************************************************
 * @brief Add an inquiry event filter
 *
 * @param[in] condition  Filter condition type
 * @param[in] param      Pointer to the condition parameters
 *
 * @return The status of the filter addition
 *****************************************************************************************
 */
static uint8_t hci_evt_filter_inq_add(struct inq_res_filter const * filter)
{
    uint8_t status = COMMON_ERROR_INVALID_HCI_PARAM;
    struct hci_evt_filter_tag *evt_filter;

    switch (filter->cond_type)
    {
        case ALL_FILTER_CONDITION_TYPE:
        {
            // Remove all Inquiry type
            for (int i = 0 ; i < HCI_FILTER_NB ; i++)
            {
                if (hci_env.evt_filter[i].type == INQUIRY_FILTER_TYPE)
                {
                    hci_env.evt_filter[i].in_use = false;
                }
            }

            status = COMMON_ERROR_NO_ERROR;
        }
        break;

        case CLASS_FILTER_CONDITION_TYPE:
        case BD_ADDR_FILTER_CONDITION_TYPE:
        {
            // Add the new filter
            evt_filter = hci_evt_filter_alloc();
            if (evt_filter != NULL)
            {
                evt_filter->type = INQUIRY_FILTER_TYPE;
                evt_filter->condition = filter->cond_type;
                if (filter->cond_type == CLASS_FILTER_CONDITION_TYPE)
                {
                    struct classofdevcondition *dev_class = &evt_filter->param.device_class;
                    // Store the mask
                    dev_class->class_mask = filter->cond.cond_1.class_of_dev_msk;

                    // Store the class, masked with the class mask
                    dev_class->classofdev.A[0] = filter->cond.cond_1.class_of_dev.A[0] & filter->cond.cond_1.class_of_dev_msk.A[0];
                    dev_class->classofdev.A[1] = filter->cond.cond_1.class_of_dev.A[1] & filter->cond.cond_1.class_of_dev_msk.A[1];
                    dev_class->classofdev.A[2] = filter->cond.cond_1.class_of_dev.A[2] & filter->cond.cond_1.class_of_dev_msk.A[2];
                }
                else
                {
                    evt_filter->param.bdaddr = filter->cond.cond_2.bd_addr;
                }
                status = COMMON_ERROR_NO_ERROR;
            }
            else
            {
                status = COMMON_ERROR_MEMORY_CAPA_EXCEED;
            }
        }
        break;

        default:
        {
            // Nothing to do
        }
        break;
    }
    return(status);
}


/**
 ****************************************************************************************
 * @brief Add a connection event filter
 *
 * @param[in] condition  Filter condition type
 * @param[in] param      Pointer to the condition parameters
 *
 * @return The status of the filter addition
 *****************************************************************************************
 */
static uint8_t hci_evt_filter_con_add(struct con_set_filter const * filter)
{
    uint8_t status = COMMON_ERROR_NO_ERROR;
    struct hci_evt_filter_tag *evt_filter;

    switch (filter->cond_type)
    {
        case ALL_FILTER_CONDITION_TYPE:
        {
            uint8_t auto_accept = filter->cond.cond_0.auto_accept;
            // Check auto_accept parameter
            if ((auto_accept >= DO_NOT_AUTO_ACCEPT_CONNECTION) && (auto_accept <= ACCEPT_CONNECTION_MASTER))
            {
                // Remove all Connection type
                for (int i = 0 ; i < HCI_FILTER_NB ; i++)
                {
                    if (hci_env.evt_filter[i].type == CONNECTION_FILTER_TYPE)
                    {
                        hci_env.evt_filter[i].in_use = false;
                    }
                }
                // Add the new filter
                evt_filter = hci_evt_filter_alloc();
                if (evt_filter != NULL)
                {
                    evt_filter->type = CONNECTION_FILTER_TYPE;
                    evt_filter->condition = ALL_FILTER_CONDITION_TYPE;
                    evt_filter->auto_accept = auto_accept;
                }
                else
                {
                    status = COMMON_ERROR_MEMORY_CAPA_EXCEED;
                }
            }
        }
        break;

        case CLASS_FILTER_CONDITION_TYPE:
        case BD_ADDR_FILTER_CONDITION_TYPE:
        {
            uint8_t auto_accept = filter->cond.cond_1.auto_accept;
            // Check auto_accept parameter
            if ((auto_accept >= DO_NOT_AUTO_ACCEPT_CONNECTION) && (auto_accept <= ACCEPT_CONNECTION_MASTER))
            {
                // Remove all Connection type with ALL_FILTER_CONDITION_TYPE set
                for (int i = 0; i < HCI_FILTER_NB ; i++)
                {
                    if ((hci_env.evt_filter[i].in_use == true) &&
                        (hci_env.evt_filter[i].type == CONNECTION_FILTER_TYPE) &&
                        (hci_env.evt_filter[i].condition == ALL_FILTER_CONDITION_TYPE))
                    {
                        hci_env.evt_filter[i].in_use = false;
                    }
                }
                // Add the new filter
                evt_filter = hci_evt_filter_alloc();
                if (evt_filter != NULL)
                {
                    evt_filter->type = CONNECTION_FILTER_TYPE;
                    evt_filter->condition = filter->cond_type;
                    evt_filter->auto_accept = auto_accept;
                    if (filter->cond_type == CLASS_FILTER_CONDITION_TYPE)
                    {
                        struct classofdevcondition *dev_class = &evt_filter->param.device_class;
                        // Store the mask
                        dev_class->class_mask = filter->cond.cond_1.class_of_dev_msk;

                        // Store the class, masked with the class mask
                        dev_class->classofdev.A[0] = filter->cond.cond_1.class_of_dev.A[0] & filter->cond.cond_1.class_of_dev_msk.A[0];
                        dev_class->classofdev.A[1] = filter->cond.cond_1.class_of_dev.A[1] & filter->cond.cond_1.class_of_dev_msk.A[1];
                        dev_class->classofdev.A[2] = filter->cond.cond_1.class_of_dev.A[2] & filter->cond.cond_1.class_of_dev_msk.A[2];
                    }
                    else
                    {
                        evt_filter->param.bdaddr = filter->cond.cond_2.bd_addr;
                    }
                }
                else
                {
                    status = COMMON_ERROR_MEMORY_CAPA_EXCEED;
                }
            }
            else
            {
                status = COMMON_ERROR_INVALID_HCI_PARAM;
            }
        }
        break;

        default:
        {
            // Nothing to do
        }
        break;
    }

    return(status);
}
#endif //BT_EMB_PRESENT


/*
 * MODULES INTERNAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */


/*
 * EXPORTED FUNCTION DEFINITIONS
 ****************************************************************************************
 */

void hci_init(uint8_t init_type)
{
    switch (init_type)
    {
        case RWIP_INIT:
        {
            #if (BLE_EMB_PRESENT && BLE_HOST_PRESENT && HCI_TL_SUPPORT)
            if(!init_type)
            {
                // HCI used by internal Host by default
                hci_ext_host = false;
            }
            #endif // (BLE_EMB_PRESENT && BLE_HOST_PRESENT && HCI_TL_SUPPORT)
        }
        // No break

        case RWIP_RST:
        {
            memset(&hci_env, 0, sizeof(hci_env));

            // Initialize event mask
            hci_evt_mask_set(&hci_def_evt_msk, HCI_PAGE_DFT);

            #if (BLE_EMB_PRESENT)
            // Initialize LE event mask
            hci_evt_mask_set(&hci_le_def_evt_msk, HCI_PAGE_LE);
            #endif // (BLE_EMB_PRESENT)

            #if (BT_EMB_PRESENT || BLE_EMB_PRESENT)
            hci_fc_init();
            #endif //(BT_EMB_PRESENT || BLE_EMB_PRESENT)
        }
        break;

        case RWIP_1ST_RST:
        {
           // Do nothing
        }
        break;

        default:
        {
            // Do nothing
        }
        break;
    }

    #if (HCI_TL_SUPPORT)
    // Reset the HCI
    hci_tl_init(init_type);
    #endif //(HCI_TL_SUPPORT)
}

#if (BLE_EMB_PRESENT || BT_EMB_PRESENT)
void hci_send_2_host(void *param)
{
    struct kernel_msg *msg = kernel_param2msg(param);

    if(   hci_evt_mask_check(msg)
    #if BT_EMB_PRESENT
       || hci_evt_filter_check(msg)
    #endif //BT_EMB_PRESENT
      )
    {
        // Free the kernel message space
        kernel_msg_free(msg);
        return;
    }

    #if BLE_HOST_PRESENT
    #if(BLE_EMB_PRESENT && HCI_TL_SUPPORT)
    // check if communication is performed over embedded host
    if(!hci_ext_host)
    #endif // (BLE_EMB_PRESENT && HCI_TL_SUPPORT)
    {
        kernel_task_id_t dest = TASK_BLE_NONE;
        uint8_t hl_type = HL_UNDEF;

        // The internal destination first depends on the message type (command, event, data)
        switch(msg->id)
        {
            case HCI_CMD_STAT_EVENT:
            case HCI_CMD_CMP_EVENT:
            {
                if(msg->src_id != HCI_NO_OPERATION_CMD_OPCODE)
                {
                    // Find a command descriptor associated to the command opcode
                    const struct hci_cmd_desc_tag* cmd_desc = hci_look_for_cmd_desc(msg->src_id);

                    // Check if the command is supported
                    if(cmd_desc != NULL)
                    {
                        hl_type = (cmd_desc->dest_field & HCI_CMD_DEST_HL_MASK) >> HCI_CMD_DEST_HL_POS;
                    }
                }
                else
                {
                    hl_type = HL_MNG;
                }
            }
            break;
            case HCI_EVENT:
            {
                // Find an event descriptor associated to the event code
                const struct hci_evt_desc_tag* evt_desc = hci_look_for_evt_desc(msg->src_id);

                // Check if the event is supported
                if(evt_desc != NULL)
                {
                    hl_type = (evt_desc->dest_field & HCI_EVT_DEST_HL_MASK) >> HCI_EVT_DEST_HL_POS;
                }
            }
            break;
            case HCI_LE_EVENT:
            {
                uint8_t subcode = *((uint8_t *)kernel_msg2param(msg));

                // Find an LE event descriptor associated to the LE event subcode
                const struct hci_evt_desc_tag* evt_desc = hci_look_for_le_evt_desc(subcode);

                // Check if the event is supported
                if(evt_desc != NULL)
                {
                    hl_type = (evt_desc->dest_field & HCI_EVT_DEST_HL_MASK) >> HCI_EVT_DEST_HL_POS;
                }
            }
            break;
            #if (RW_DEBUG || BLE_ISOGEN)
            case HCI_DBG_EVT:
            {
                uint8_t subcode = *((uint8_t *)kernel_msg2param(msg));

                // Find an VS event descriptor associated to the VS event subcode
                const struct hci_evt_desc_tag* evt_desc = hci_look_for_dbg_evt_desc(subcode);

                // Check if the event is supported
                if(evt_desc != NULL)
                {
                    hl_type = (evt_desc->dest_field & HCI_EVT_DEST_HL_MASK) >> HCI_EVT_DEST_HL_POS;
                }
            } break;
            #endif //  (RW_DEBUG || BLE_ISOGEN)
            #if (HCI_BLE_CON_SUPPORT)
            case HCI_ACL_DATA:
            {
                hl_type = HL_DATA;
            }
            break;
            #endif // (HCI_BLE_CON_SUPPORT)

            default:
            {
                // Nothing to do
            }
            break;
        }

        // Find the higher layers destination task
        switch(hl_type)
        {
            case HL_MNG:
            {
                // Build the destination task ID
                dest = TASK_BLE_GAPM;
            }
            break;

            #if (HCI_BLE_CON_SUPPORT)
            case HL_CTRL:
            {
                // Check if the link identifier in the dest_id field corresponds to an active BLE link
                if(msg->dest_id < BLE_ACTIVITY_MAX)
                {
                    #if (BLE_HOST_PRESENT)
                    uint8_t conidx = gapc_get_conidx(msg->dest_id);

                    if (conidx != GAP_INVALID_CONIDX)
                    {
                        // Build the destination task ID
                        dest = KERNEL_BUILD_ID(TASK_BLE_GAPC, conidx);
                    }
                    else
                    {
                        // Send it to the first instance
                        dest = TASK_BLE_GAPC;
                    }
                    #else
                    #endif //()
                }
                else
                {
                    BLE_ASSERT_INFO(0, msg->id, msg->dest_id);
                }
            }
            break;

            case HL_DATA:
            {
                // Forward to L2CAP handler
                dest = TASK_BLE_L2CC;
            }
            break;
            #endif //(HCI_BLE_CON_SUPPORT)
            #if (BLE_ISO_MODE_0)
            case HL_ISO0:
            {
                // Check if the link identifier in the dest_id field corresponds to an active BLE link
                if(msg->dest_id < BLE_ACTIVITY_MAX)
                {
                    uint8_t conidx = gapc_get_conidx(msg->dest_id);

                    if (conidx != GAP_INVALID_CONIDX)
                    {
                        // Build the destination task ID
                        dest = KERNEL_BUILD_ID(TASK_BLE_AM0, conidx);
                    }
                    else
                    {
                        // Send the message to the first instance
                        dest = TASK_BLE_AM0;
                    }
                }
                else
                {
                    // Send the message to the first instance
                    dest = TASK_BLE_AM0;
                }
            }
            break;
            #endif // (BLE_ISO_MODE_0)

            default:
            {
                BLE_ASSERT_INFO(0, hl_type, msg->id);
            }
            break;
        }

        // Check it the destination has been found
        if(dest != TASK_BLE_NONE)
        {
            // Send the command to the internal destination task associated to this command
            msg->dest_id = dest;

            switch(msg->id)
            {
                case HCI_CMD_STAT_EVENT:
                {
                    TRC_REQ_HCI_CMD_STAT_EVT(msg->src_id, msg->param_len, msg->param);
                }
                break;

                case HCI_CMD_CMP_EVENT:
                {
                    TRC_REQ_HCI_CMD_CMP_EVT(msg->src_id, msg->param_len, msg->param);
                }
                break;

                case HCI_EVENT:
                {
                    TRC_REQ_HCI_EVT(msg->src_id, msg->param_len, msg->param);
                }
                break;

                case HCI_LE_EVENT:
                {
                    #if (TRACER_PRESENT && TRC_HCI)
                    uint8_t subcode = *((uint8_t *)kernel_msg2param(msg));
                    #endif
                    TRC_REQ_HCI_LE_EVT(subcode, msg->param_len, msg->param);
                }
                break;

                default:
                {
                    // Nothing to do
                }
                break;
            }

            kernel_msg_send(param);
        }
        else
        {
            BLE_ASSERT_INFO(0, msg->id, msg->src_id);

            // Free message to avoid memory leak
            kernel_msg_free(msg);
        }
    }
    #if(BLE_EMB_PRESENT && HCI_TL_SUPPORT)
    else
    #endif // (BLE_EMB_PRESENT && HCI_TL_SUPPORT)
    #endif //BLE_HOST_PRESENT
    #if (HCI_TL_SUPPORT)
    {
        // Send the HCI message over TL
        hci_tl_send(msg);
    }
    #endif //(HCI_TL_SUPPORT)
}
#endif //(BLE_EMB_PRESENT || BT_EMB_PRESENT)

#if BLE_HOST_PRESENT
void hci_send_2_controller(void *param)
{
    struct kernel_msg *msg = kernel_param2msg(param);

    #if (HCI_TL_SUPPORT && !BLE_EMB_PRESENT)
    // Send the HCI message over TL
    hci_tl_send(msg);
    #else //(HCI_TL_SUPPORT)
    #if BLE_EMB_PRESENT

    #if(BLE_EMB_PRESENT && HCI_TL_SUPPORT)
    // check if communication is performed over embedded host
    if(!hci_ext_host)
    #endif // (BLE_EMB_PRESENT && HCI_TL_SUPPORT)
    {
        kernel_task_id_t dest = TASK_BLE_NONE;
        uint8_t ll_type = LL_UNDEF;

        // The internal destination first depends on the message type (command, event, data)
        switch(msg->id)
        {
            case HCI_COMMAND:
            {
                TRC_REQ_HCI_CMD(msg->src_id, msg->param_len, msg->param);

                // Find a command descriptor associated to the command opcode
                const struct hci_cmd_desc_tag* cmd_desc = hci_look_for_cmd_desc(msg->src_id);

                // Check if the command is supported
                if(cmd_desc != NULL)
                {
                    ll_type = (cmd_desc->dest_field & HCI_CMD_DEST_LL_MASK) >> HCI_CMD_DEST_LL_POS;
                }
            }
            break;
            #if (HCI_BLE_CON_SUPPORT)
            case HCI_ACL_DATA:
            {
                ll_type = BLE_CTRL;
            }
            break;
            #endif // (HCI_BLE_CON_SUPPORT)

            default:
            {
                // Nothing to do
            }
            break;
        }

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

            case BLE_MNG:
            #if !BT_EMB_PRESENT
            case MNG:
            #endif //BT_EMB_PRESENT
            {
                dest = TASK_BLE_LLM;
            }
            break;

            #if (HCI_BLE_CON_SUPPORT)
            case CTRL:
            case BLE_CTRL:
            {
                // Check if the link identifier in the dest_id field corresponds to an active BLE link
                if(msg->dest_id < BLE_ACTIVITY_MAX)
                {
                    // Build the destination task ID
                    dest = KERNEL_BUILD_ID(TASK_BLE_LLC, msg->dest_id);
                }
                else
                {
                    BLE_ASSERT_INFO(0, msg->id, msg->dest_id);
                }
            }
            break;
            #endif //(HCI_BLE_CON_SUPPORT)

            #if (BLE_EMB_PRESENT && BLE_ISO_PRESENT)
            case BLE_ISO:
            {
                dest = TASK_BLE_LLI;
            }
            break;
            #endif //(BLE_EMB_PRESENT && BLE_ISO_PRESENT)

            #if (BLE_EMB_PRESENT)
            case DBG:
            {
                dest = TASK_BLE_DBG;
            } break;
            #endif // (BLE_EMB_PRESENT)

            default:
            {
                // Nothing to do
            }
            break;
        }

        // Check it the destination has been found
        if(dest != TASK_BLE_NONE)
        {
            // Send the command to the internal destination task associated to this command
            msg->dest_id = dest;
            kernel_msg_send(param);
        }
        else
        {
            BLE_ASSERT_INFO(0, msg->id, msg->src_id);

            // Free message to avoid memory leak
            kernel_msg_free(msg);
        }
    }
    #if(BLE_EMB_PRESENT && HCI_TL_SUPPORT)
    else
    {
        // receiving a message from internal host is not expected at all
        BLE_ASSERT_ERR(0);
        // Free message to avoid memory leak
        kernel_msg_free(msg);
    }
    #endif // (BLE_EMB_PRESENT && HCI_TL_SUPPORT)
    #endif //BLE_EMB_PRESENT
    #endif //(HCI_TL_SUPPORT)
}

void hci_basic_cmd_send_2_controller(uint16_t opcode)
{
    void *no_param = kernel_msg_alloc(HCI_COMMAND, 0, opcode, 0);
    hci_send_2_controller(no_param);
}

#endif //BLE_HOST_PRESENT

#if (BLE_EMB_PRESENT && HCI_BLE_CON_SUPPORT)
void hci_ble_conhdl_register(uint8_t link_id)
{
    BLE_ASSERT_INFO(link_id < BLE_ACTIVITY_MAX, link_id, BLE_ACTIVITY_MAX);
    BLE_ASSERT_ERR(!hci_env.ble_con_state[link_id]);

    // Link ID associated with BD address AND connection handle
    hci_env.ble_con_state[link_id] = true;
}

void hci_ble_conhdl_unregister(uint8_t link_id)
{
    BLE_ASSERT_INFO(link_id < BLE_ACTIVITY_MAX, link_id, BLE_ACTIVITY_MAX);
    BLE_ASSERT_ERR(hci_env.ble_con_state[link_id]);

    // Link ID associated with BD address
    hci_env.ble_con_state[link_id] = false;
}
#endif //(BLE_EMB_PRESENT && !BLE_HOST_PRESENT && HCI_BLE_CON_SUPPORT)

#if (BT_EMB_PRESENT)
void hci_bt_acl_bdaddr_register(uint8_t link_id, struct bd_addr* bd_addr)
{
    BLE_ASSERT_INFO(link_id < MAX_NB_ACTIVE_ACL, link_id, 0);
    BLE_ASSERT_INFO(hci_env.bt_acl_con_tab[link_id].state == HCI_BT_ACL_STATUS_NOT_ACTIVE, hci_env.bt_acl_con_tab[link_id].state, 0);

    // Store BD address
    memcpy(&hci_env.bt_acl_con_tab[link_id].bd_addr.addr[0], &bd_addr->addr[0], sizeof(struct bd_addr));

    // Link ID associated with BD address
    hci_env.bt_acl_con_tab[link_id].state = HCI_BT_ACL_STATUS_BD_ADDR;
}

void hci_bt_acl_conhdl_register(uint8_t link_id)
{
    BLE_ASSERT_INFO(link_id < MAX_NB_ACTIVE_ACL, link_id, 0);
    BLE_ASSERT_INFO(hci_env.bt_acl_con_tab[link_id].state == HCI_BT_ACL_STATUS_BD_ADDR, hci_env.bt_acl_con_tab[link_id].state, 0);

    // Link ID associated with BD address AND connection handle
    hci_env.bt_acl_con_tab[link_id].state = HCI_BT_ACL_STATUS_BD_ADDR_CONHDL;
}

void hci_bt_acl_bdaddr_unregister(uint8_t link_id)
{
    BLE_ASSERT_INFO(link_id < MAX_NB_ACTIVE_ACL, link_id, 0);

    // Link ID associated with BD address
    hci_env.bt_acl_con_tab[link_id].state = HCI_BT_ACL_STATUS_NOT_ACTIVE;
}
#endif //(BT_EMB_PRESENT)

uint8_t hci_evt_mask_set(struct evt_mask const *evt_msk, uint8_t page)
{
    switch(page)
    {
        case HCI_PAGE_0:
        case HCI_PAGE_1:
        {
            BLE_ASSERT_INFO(page == HCI_PAGE_DFT,page,page);
        }break;
        case HCI_PAGE_2:
        {
            // Store event mask
            memcpy(&hci_env.evt_msk_page_2.mask[0], &evt_msk->mask[0], EVT_MASK_LEN);
        }break;
        case HCI_PAGE_DFT:
        {
            // Store event mask
            memcpy(&hci_env.evt_msk.mask[0], &evt_msk->mask[0], EVT_MASK_LEN);

            // ensure that reserved bit are set
            for (uint8_t i = 0; i < EVT_MASK_LEN; i++)
            {
                hci_env.evt_msk.mask[i] |= hci_rsvd_evt_msk.mask[i];
            }
        }break;
        #if (BLE_EMB_PRESENT)
        case HCI_PAGE_LE:
        {
            // Store event mask
            memcpy(&hci_env.le_evt_msk.mask[0], &evt_msk->mask[0], EVT_MASK_LEN);
        }break;
        #endif // (BLE_EMB_PRESENT)
        default:
        {
            BLE_ASSERT_ERR(0);
        }break;
    }
    return COMMON_ERROR_NO_ERROR;
}

#if (BT_EMB_PRESENT)
uint8_t hci_evt_filter_add(struct hci_set_evt_filter_cmd const *param)
{
    uint8_t status = COMMON_ERROR_INVALID_HCI_PARAM;

    // Perform the requested action according to the filter type
    switch (param->filter_type)
    {
        case CLEAR_ALL_FILTER_TYPE:
        {
            // Reset all Filters
            status = hci_evt_filter_reset();
        }
        break;

        case INQUIRY_FILTER_TYPE:
        {
            // Add inquiry event filter
            status = hci_evt_filter_inq_add(&param->filter.inq_res);
        }
        break;

        case CONNECTION_FILTER_TYPE:
        {
            // Add connection event filter
            status = hci_evt_filter_con_add(&param->filter.con_set);
        }
        break;

        default:
        {
            // Nothing to do
        }
        break;
    }

    return (status);
}

#if (MAX_NB_SYNC > 0)
uint16_t hci_voice_settings_get(void)
{
    return(hci_env.voice_settings);
}

uint8_t hci_voice_settings_set(uint16_t voice_settings)
{
    uint8_t status = COMMON_ERROR_UNSUPPORTED;

    // Check if the requested voice settings are supported
    switch (voice_settings & AIR_COD_MSK)
    {
        case AIR_COD_CVSD:
        case AIR_COD_MULAW:
        case AIR_COD_ALAW:
        case AIR_COD_TRANS:
        {
            hci_env.voice_settings = voice_settings;
            status = COMMON_ERROR_NO_ERROR;
        }
        break;
        default:
            break;
    }

    return(status);
}
#endif // (MAX_NB_SYNC > 0)
#endif //(BT_EMB_PRESENT)

#endif //(HCI_PRESENT)
#else
/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */


/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "architect.h"
#include "lowlevel.h"
#include "rtos_pub.h"
#include "mem_pub.h"

#include "uart_pub.h"
#include "string.h"

#include "kernel_msg.h"          // kernel message declaration
#include "kernel_task.h"         // kernel task definition
#include "kernel_event.h"        // kernel event definition
#include "kernel_mem.h"          // kernel memory definition
#include "ble_util_buf.h"
#include "em_map.h"

#include "common_utils.h"
#include "hci.h"
#include "start_type_pub.h"

/// Offset of the Kernel message parameters compared to the event packet position
#define HCI_CMD_PARAM_OFFSET               3
#define HCI_EVT_CC_PARAM_OFFSET            5
#define HCI_EVT_CS_PARAM_OFFSET            5
#define HCI_EVT_PARAM_OFFSET               2
#define HCI_EVT_LE_PARAM_OFFSET            2
#define HCI_EVT_DBG_PARAM_OFFSET           2

#define HCI_ACL_HANDLE_MASK                0x3fff
#ifndef HCI_DBG
#define HCI_DBG(...)
#endif

#define HCI2H_MALLOC(size)   os_malloc(size)
#define HCI2H_FREE(ptr)      os_free(ptr)

static struct common_list recv_wait_list;

/// Event mask
static struct evt_mask evt_msk;

/// Event mask page 2
static struct evt_mask evt_msk_page_2;

#if (BLE_EMB_PRESENT)
/// LE Event mask
static struct evt_mask le_evt_msk;
#endif // (BLE_EMB_PRESENT)


struct hci_context {
	beken_mutex_t lock;
	int initialized;
};

static struct hci_context g_hci_send_context;
static hci_func_evt_cb g_hci_cmd_cb = NULL;
static hci_func_evt_cb g_hci_acl_cb = NULL;

/*
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */
static void hci_lock(struct hci_context *context)
{
	if (context->initialized)
		rtos_lock_mutex(&(context->lock));
}

static void hci_unlock(struct hci_context *context)
{
	if (context->initialized)
		rtos_unlock_mutex(&(context->lock));
}

void ble_recv_handler(void *arg)
{
    bk_ble_hci_buf_t *recv_buf_info;

    while (!common_list_is_empty(&recv_wait_list)) {
        ///get receive buffer
        GLOBAL_INT_DIS();
        recv_buf_info = (bk_ble_hci_buf_t*)common_list_pop_front(&recv_wait_list);
        GLOBAL_INT_RES();
        if (recv_buf_info->type == HCI_EVT_MSG_TYPE) {
            if (g_hci_cmd_cb) {
                g_hci_cmd_cb((uint8_t *)(recv_buf_info->buf), recv_buf_info->len);
            }
        } else if (recv_buf_info->type == HCI_ACL_MSG_TYPE) {
           if (g_hci_acl_cb) {
               g_hci_acl_cb((uint8_t *)(recv_buf_info->buf), recv_buf_info->len);
           }
        } else {
            os_printf("%s:unknow msg type:%d\r\n", __FUNCTION__, recv_buf_info->type);
        }

        HCI2H_FREE(recv_buf_info);
        recv_buf_info = NULL;
	}
}

/// TODO:Add flowcontrl feature
uint8_t hci_fc_acl_en(bool flow_enable)
{
	return COMMON_ERROR_UNSUPPORTED;
}

uint8_t hci_fc_acl_buf_size_set(uint16_t acl_pkt_len, uint16_t nb_acl_pkts)
{
	return COMMON_ERROR_UNSUPPORTED;
}

void hci_ble_conhdl_unregister(uint8_t link_id)
{
	return;
}

void hci_ble_conhdl_register(uint8_t link_id)
{
	return;
}

void hci_fc_host_nb_acl_pkts_complete(uint16_t acl_pkt_nb)
{
	return;
}

static uint8_t* hci_build_cc_evt(struct kernel_msg * msg)
{
	uint8_t* param = kernel_msg2param(msg);
	uint8_t* buf = param - HCI_EVT_CC_PARAM_OFFSET;
	uint8_t* pk = buf;
	uint16_t opcode = msg->src_id;
	uint16_t ret_par_len = msg->param_len;

	//pack event code
	*pk++ = HCI_CMD_CMP_EVT_CODE;

	//pack event parameter length
	*pk++ = HCI_CCEVT_HDR_PARLEN + ret_par_len;

	//pack the number of h2c packets
	*pk++ = 1U;

	//pack opcode
	common_write16p(pk, common_htobs(opcode));

	return buf;
}

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
	*pk++ = 1U;

	//pack opcode
	common_write16p(pk, common_htobs(opcode));

	return buf;
}

static uint8_t* hci_build_evt(struct kernel_msg * msg)
{
	uint8_t* param = kernel_msg2param(msg);
	uint8_t* buf = param - HCI_EVT_LE_PARAM_OFFSET;
	uint8_t* pk = buf;
	uint8_t evt_code = (uint8_t) msg->src_id;
	uint16_t par_len = msg->param_len;

	//pack event code
	*pk++ = evt_code;

	//pack event parameter length
	*pk++ = par_len;

	return buf;
}

#if ( HCI_TL_SUPPORT == 0 )
/// Status returned by generic packer-unpacker
enum HCI_PACK_STATUS
{
    HCI_PACK_OK,
    HCI_PACK_IN_BUF_OVFLW,
    HCI_PACK_OUT_BUF_OVFLW,
    HCI_PACK_WRONG_FORMAT,
    HCI_PACK_ERROR,
};

static uint8_t hci_pack_bytes(uint8_t** pp_in, uint8_t** pp_out, uint8_t* p_in_end, uint8_t* p_out_end, uint8_t len)
{
    uint8_t status = HCI_PACK_OK;

    // Check if enough space in input buffer to read
    if((*pp_in + len) > p_in_end)
    {
        status = HCI_PACK_IN_BUF_OVFLW;
    }
    else
    {
        if(p_out_end != NULL)
        {
            // Check if enough space in out buffer to write
            if((*pp_out + len) > p_out_end)
            {
                status = HCI_PACK_OUT_BUF_OVFLW;
            }

            // Copy BD Address
            memcpy(*pp_out, *pp_in, len);
        }
        *pp_in = *pp_in + len;
        *pp_out = *pp_out + len;
    }

    return (status);
}

/// Special packing/unpacking function for HCI LE Advertising Report Event
static uint8_t hci_le_adv_report_evt_pkupk(uint8_t *out, uint8_t *in, uint16_t* out_len, uint16_t in_len)
{
    #if BLE_EMB_PRESENT

    /*
     * PACKING FUNCTION
     */
    struct hci_le_adv_report_evt temp_out;
    struct hci_le_adv_report_evt* s;
    uint8_t* p_in = in;
    uint8_t* p_out;
    uint8_t* p_in_end = in + in_len;
    uint8_t* p_out_start;
    uint8_t status = HCI_PACK_OK;

    // Check if there is input data to parse
    if(in != NULL)
    {
        uint8_t* p_out_end;

        // Check if there is output buffer to write to, else use temp_out buffer
        if (out != NULL)
        {
            s = (struct hci_le_adv_report_evt*)(out);
            p_out_start = out;
            p_out_end = out + *out_len;
        }
        else
        {
            s = (struct hci_le_adv_report_evt*)(&temp_out);
            p_out_start = (uint8_t*)&temp_out;
            p_out_end = p_out_start + sizeof(temp_out);
        }

        p_out = p_out_start;

        do
        {
            // Sub-code
            p_in = &s->subcode;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // Number of reports
            p_in = &s->nb_reports;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            for (int i=0; i< s->nb_reports; i++)
            {
                uint8_t data_len;

                // Event type
                p_in = &s->adv_rep[i].evt_type;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // Address type
                p_in = &s->adv_rep[i].adv_addr_type;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // BD Address
                p_in = &s->adv_rep[i].adv_addr.addr[0];
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, BD_ADDR_LEN);
                if(status != HCI_PACK_OK)
                    break;

                // Data Length
                p_in = &s->adv_rep[i].data_len;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                data_len = s->adv_rep[i].data_len;
				//bk_printf("[jerry][advlen %d]\r\n",data_len);
                // ADV data
                p_in = &s->adv_rep[i].data[0];
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, data_len);
                if(status != HCI_PACK_OK)
                    break;

                // RSSI
                p_in = (uint8_t*) &s->adv_rep[i].rssi;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;
            }

        } while(0);

        *out_len =  (uint16_t)(p_out - p_out_start);
    }
    else
    {
        *out_len = 0;
    }

    return (status);

    #elif BLE_HOST_PRESENT

    /*
     * UNPACKING FUNCTION
     */

    // TODO unpack message as per compiler
    *out_len = in_len;
    if((out != NULL) && (in != NULL))
    {
        // copy adv report
        memcpy(out, in, in_len);
    }

    return (HCI_PACK_OK);

    #endif //BLE_EMB_PRESENT
}

static uint8_t hci_le_dir_adv_report_evt_pkupk(uint8_t *out, uint8_t *in, uint16_t* out_len, uint16_t in_len)
{
    #if BLE_EMB_PRESENT

    /*
     * PACKING FUNCTION
     */
    struct hci_le_dir_adv_rep_evt temp_out;
    struct hci_le_dir_adv_rep_evt* s;
    uint8_t* p_in = in;
    uint8_t* p_out;
    uint8_t* p_in_end = in + in_len;
    uint8_t* p_out_start;
    uint8_t status = HCI_PACK_OK;

    // Check if there is input data to parse
    if(in != NULL)
    {
        uint8_t* p_out_end;

        // Check if there is output buffer to write to, else use temp_out buffer
        if (out != NULL)
        {
            s = (struct hci_le_dir_adv_rep_evt*)(out);
            p_out_start = out;
            p_out_end = out + *out_len;
        }
        else
        {
            s = (struct hci_le_dir_adv_rep_evt*)(&temp_out);
            p_out_start = (uint8_t*)&temp_out;
            p_out_end = p_out_start + sizeof(temp_out);
        }

        p_out = p_out_start;

        do
        {
            // Sub-code
            p_in = &s->subcode;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // Number of reports
            p_in = &s->nb_reports;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            for (int i=0; i< s->nb_reports; i++)
            {
                // Event type
                p_in = &s->adv_rep[i].evt_type;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;


                // Address type
                p_in = &s->adv_rep[i].addr_type;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // BD Address
                p_in = &s->adv_rep[i].addr.addr[0];
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, BD_ADDR_LEN);
                if(status != HCI_PACK_OK)
                    break;

                // Direct Address type
                p_in = &s->adv_rep[i].dir_addr_type;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // Direct BD Address
                p_in = &s->adv_rep[i].dir_addr.addr[0];
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, BD_ADDR_LEN);
                if(status != HCI_PACK_OK)
                    break;

                // RSSI
                p_in = (uint8_t*) &s->adv_rep[i].rssi;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;
            }

        } while(0);

        *out_len =  (uint16_t)(p_out - p_out_start);
    }
    else
    {
        *out_len = 0;
    }

    return (status);

    #elif BLE_HOST_PRESENT

    /*
     * UNPACKING FUNCTION
     */

    // TODO unpack message as per compiler
    *out_len = in_len;
    if((out != NULL) && (in != NULL))
    {
        // copy adv report
        memcpy(out, in, in_len);
    }

    return (HCI_PACK_OK);

    #endif //BLE_EMB_PRESENT
}

static uint8_t hci_le_ext_adv_report_evt_pkupk(uint8_t *out, uint8_t *in, uint16_t* out_len, uint16_t in_len)
{
    #if BLE_EMB_PRESENT

    /*
     * PACKING FUNCTION
     */
    struct hci_le_ext_adv_report_evt temp_out;
    struct hci_le_ext_adv_report_evt* s;
    uint8_t* p_in = in;
    uint8_t* p_out;
    uint8_t* p_in_end = in + in_len;
    uint8_t* p_out_start;
    uint8_t status = HCI_PACK_OK;

    // Check if there is input data to parse
    if(in != NULL)
    {
        uint8_t* p_out_end;

        // Check if there is output buffer to write to, else use temp_out buffer
        if (out != NULL)
        {
            s = (struct hci_le_ext_adv_report_evt*)(out);
            p_out_start = out;
            p_out_end = out + *out_len;
        }
        else
        {
            s = (struct hci_le_ext_adv_report_evt*)(&temp_out);
            p_out_start = (uint8_t*)&temp_out;
            p_out_end = p_out_start + sizeof(temp_out);
        }

        p_out = p_out_start;

        do
        {
            // Sub-code
            p_in = &s->subcode;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // Number of reports
            p_in = &s->nb_reports;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            for (int i=0; i< s->nb_reports; i++)
            {
                uint8_t data_len;

                // Event type
                p_in = (uint8_t*) &s->adv_rep[i].evt_type;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 2);
                if(status != HCI_PACK_OK)
                    break;

                // Adv Address type
                p_in = &s->adv_rep[i].adv_addr_type;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // Adv Address
                p_in = &s->adv_rep[i].adv_addr.addr[0];
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, BD_ADDR_LEN);
                if(status != HCI_PACK_OK)
                    break;

                // Primary PHY
                p_in = &s->adv_rep[i].phy;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // Secondary PHY
                p_in = &s->adv_rep[i].phy2;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // Advertising SID
                p_in = &s->adv_rep[i].adv_sid;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // Tx Power
                p_in = &s->adv_rep[i].tx_power;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // RSSI
                p_in = (uint8_t*) &s->adv_rep[i].rssi;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // Periodic Advertising Interval
                p_in = (uint8_t*) &s->adv_rep[i].interval;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 2);
                if(status != HCI_PACK_OK)
                    break;

                // Direct address type
                p_in = (uint8_t*) &s->adv_rep[i].dir_addr_type;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // Direct BD Address
                p_in = &s->adv_rep[i].dir_addr.addr[0];
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, BD_ADDR_LEN);
                if(status != HCI_PACK_OK)
                    break;

                // Data Length
                p_in = &s->adv_rep[i].data_len;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                data_len = s->adv_rep[i].data_len;

                // ADV data
                p_in = &s->adv_rep[i].data[0];
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, data_len);
                if(status != HCI_PACK_OK)
                    break;
            }

        } while(0);

        *out_len =  (uint16_t)(p_out - p_out_start);
    }
    else
    {
        *out_len = 0;
    }

    return (status);

    #elif BLE_HOST_PRESENT

    /*
     * UNPACKING FUNCTION
     */
    //TODO unpack message as per compiler
    *out_len = in_len;
    if((out != NULL) && (in != NULL))
    {
        // copy adv report
        memcpy(out, in, in_len);
    }

    return (HCI_PACK_OK);

    #endif //BLE_EMB_PRESENT
}

static uint8_t hci_le_conless_iq_report_evt_pkupk(uint8_t *out, uint8_t *in, uint16_t* out_len, uint16_t in_len)
{
    #if BLE_EMB_PRESENT

    /*
     * PACKING FUNCTION
     */
    struct hci_le_conless_iq_report_evt temp_out;
    struct hci_le_conless_iq_report_evt* s;
    uint8_t* p_in = in;
    uint8_t* p_out;
    uint8_t* p_in_end = in + in_len;
    uint8_t* p_out_start;
    uint8_t status = HCI_PACK_OK;

    // Check if there is input data to parse
    if(in != NULL)
    {
        uint8_t* p_out_end;

        // Check if there is output buffer to write to, else use temp_out buffer
        if (out != NULL)
        {
            s = (struct hci_le_conless_iq_report_evt*)(out);
            p_out_start = out;
            p_out_end = out + *out_len;
        }
        else
        {
            s = (struct hci_le_conless_iq_report_evt*)(&temp_out);
            p_out_start = (uint8_t*)&temp_out;
            p_out_end = p_out_start + sizeof(temp_out);
        }

        p_out = p_out_start;

        do
        {
            uint8_t sample_cnt = 0;

            // Sub-code
            p_in = &s->subcode;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // Sync handle
            p_in = (uint8_t*) &s->sync_hdl;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 2);
            if(status != HCI_PACK_OK)
                break;

            // Channel index
            p_in = &s->channel_idx;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // RSSI
            p_in = (uint8_t*) &s->rssi;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 2);
            if(status != HCI_PACK_OK)
                break;

            // RSSI antenna ID
            p_in = &s->rssi_antenna_id;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // CTE type
            p_in = &s->cte_type;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // Slot durations
            p_in = &s->slot_dur;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // Packet status
            p_in = &s->pkt_status;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // paEventCounter
            p_in = (uint8_t*) &s->pa_evt_cnt;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 2);
            if(status != HCI_PACK_OK)
                break;

            // Sample count
            sample_cnt = s->sample_cnt;
            p_in = &s->sample_cnt;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            for (int i=0; i < sample_cnt; i++)
            {
                // I sample
                p_in = (uint8_t *) &s->iq_sample[i].i;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // Q sample
                p_in = (uint8_t *) &s->iq_sample[i].q;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;
            }

        } while(0);

        *out_len =  (uint16_t)(p_out - p_out_start);
    }
    else
    {
        *out_len = 0;
    }

    return (status);

    #elif BLE_HOST_PRESENT

    /*
     * UNPACKING FUNCTION
     */
    //TODO unpack message as per compiler
    *out_len = in_len;
    if((out != NULL) && (in != NULL))
    {
        // copy adv report
        memcpy(out, in, in_len);
    }

    return (HCI_PACK_OK);

    #endif //BLE_EMB_PRESENT
}

static uint8_t hci_le_con_iq_report_evt_pkupk(uint8_t *out, uint8_t *in, uint16_t* out_len, uint16_t in_len)
{
    #if BLE_EMB_PRESENT

    /*
     * PACKING FUNCTION
     */
    struct hci_le_con_iq_report_evt temp_out;
    struct hci_le_con_iq_report_evt* s;
    uint8_t* p_in = in;
    uint8_t* p_out;
    uint8_t* p_in_end = in + in_len;
    uint8_t* p_out_start;
    uint8_t status = HCI_PACK_OK;

    // Check if there is input data to parse
    if(in != NULL)
    {
        uint8_t* p_out_end;

        // Check if there is output buffer to write to, else use temp_out buffer
        if (out != NULL)
        {
            s = (struct hci_le_con_iq_report_evt*)(out);
            p_out_start = out;
            p_out_end = out + *out_len;
        }
        else
        {
            s = (struct hci_le_con_iq_report_evt*)(&temp_out);
            p_out_start = (uint8_t*)&temp_out;
            p_out_end = p_out_start + sizeof(temp_out);
        }

        p_out = p_out_start;

        do
        {
            uint8_t sample_cnt = 0;

            // Sub-code
            p_in = &s->subcode;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // Connection handle
            p_in = (uint8_t*) &s->conhdl;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 2);
            if(status != HCI_PACK_OK)
                break;

            // Rx PHY
            p_in = &s->rx_phy;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // Data channel index
            p_in = &s->data_channel_idx;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // RSSI
            p_in = (uint8_t*) &s->rssi;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 2);
            if(status != HCI_PACK_OK)
                break;

            // RSSI antenna ID
            p_in = &s->rssi_antenna_id;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // CTE type
            p_in = &s->cte_type;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // Slot durations
            p_in = &s->slot_dur;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // Packet status
            p_in = &s->pkt_status;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            // connEventCounter
            p_in = (uint8_t*) &s->con_evt_cnt;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 2);
            if(status != HCI_PACK_OK)
                break;

            // Sample count
            sample_cnt = s->sample_cnt;
            p_in = &s->sample_cnt;
            status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
            if(status != HCI_PACK_OK)
                break;

            for (int i=0; i < sample_cnt; i++)
            {
                // I sample
                p_in = (uint8_t *) &s->iq_sample[i].i;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;

                // Q sample
                p_in = (uint8_t *) &s->iq_sample[i].q;
                status = hci_pack_bytes(&p_in, &p_out, p_in_end, p_out_end, 1);
                if(status != HCI_PACK_OK)
                    break;
            }

        } while(0);

        *out_len =  (uint16_t)(p_out - p_out_start);
    }
    else
    {
        *out_len = 0;
    }

    return (status);

    #elif BLE_HOST_PRESENT

    /*
     * UNPACKING FUNCTION
     */
    //TODO unpack message as per compiler
    *out_len = in_len;
    if((out != NULL) && (in != NULL))
    {
        // copy adv report
        memcpy(out, in, in_len);
    }

    return (HCI_PACK_OK);

    #endif //BLE_EMB_PRESENT
}
#endif //( HCI_TL_SUPPORT == 0 )

static uint8_t* hci_build_le_evt(struct kernel_msg * msg)
{
	uint8_t* param = kernel_msg2param(msg);
	uint8_t* buf = param - HCI_EVT_LE_PARAM_OFFSET;
	uint8_t* pk = buf;
	uint16_t par_len = msg->param_len;
	uint8_t subcode = *param;
	uint8_t status = COMMON_UTIL_PACK_OK;

#if ( HCI_TL_SUPPORT == 0 )
	switch(subcode) {
		case HCI_LE_ADV_REPORT_EVT_SUBCODE:
			status = hci_le_adv_report_evt_pkupk(param, param, &par_len, par_len);
			break;
		case HCI_LE_DIR_ADV_REP_EVT_SUBCODE:
			status = hci_le_dir_adv_report_evt_pkupk(param, param, &par_len, par_len);
			break;
		case HCI_LE_EXT_ADV_REPORT_EVT_SUBCODE:
			status = hci_le_ext_adv_report_evt_pkupk(param, param, &par_len, par_len);
			break;
		case HCI_LE_CONLESS_IQ_REPORT_EVT_SUBCODE:
			status = hci_le_conless_iq_report_evt_pkupk(param, param, &par_len, par_len);
			break;
		case HCI_LE_CON_IQ_REPORT_EVT_SUBCODE:
			status = hci_le_con_iq_report_evt_pkupk(param, param, &par_len, par_len);
			break;
		default:
            {
				bk_printf("[warning][%s][%d]subcode:0x%x\r\n",__func__,__LINE__,subcode);
			}
			break;
	}
#endif //( HCI_TL_SUPPORT == 0 )
	if(status == COMMON_UTIL_PACK_OK) {
		//pack event code
		*pk++ = HCI_LE_META_EVT_CODE;

		//pack event parameter length
		*pk++ = par_len;
	} else {
		BLE_ASSERT_INFO(0, subcode, 0);
        buf = NULL;
	}

	return buf;
}

static uint8_t* hci_build_acl_data(struct kernel_msg * msg)
{
    // Point to message parameters structure
    struct hci_acl_data *param = (struct hci_acl_data *) kernel_msg2param(msg);
    uint16_t handle_flags      = param->conhdl_pb_bc_flag;
    uint8_t* buf = ((uint8_t*)param->buf_ptr) - HCI_ACL_HDR_LEN;
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

uint8_t hci_evt_mask_set(struct evt_mask const *param, uint8_t page)
{
    switch (page) {
        case HCI_PAGE_0:
        case HCI_PAGE_1:
            BLE_ASSERT_INFO(page == HCI_PAGE_DFT,page,page);
            break;
        case HCI_PAGE_2:
            // Store event mask
            memcpy(&evt_msk_page_2.mask[0], &param->mask[0], EVT_MASK_LEN);
            break;
        case HCI_PAGE_DFT:
            // Store event mask
            memcpy(&evt_msk.mask[0], &param->mask[0], EVT_MASK_LEN);
            break;
#if (BLE_EMB_PRESENT)
        case HCI_PAGE_LE:
            // Store event mask
            memcpy(&le_evt_msk.mask[0], &param->mask[0], EVT_MASK_LEN);
            break;
#endif // (BLE_EMB_PRESENT)
        default:
            BLE_ASSERT_ERR(0);
            break;
    }
    return COMMON_ERROR_NO_ERROR;
}

static bool hci_evt_mask_check(struct kernel_msg *msg)
{
    bool masked = false;
    uint8_t evt_code;

    switch (msg->id)
    {
        case HCI_LE_EVENT:
            // LE meta event
            evt_code = HCI_LE_META_EVT_CODE;
            break;
        case HCI_EVENT:
            // Get event code
            evt_code = msg->src_id;

            if (evt_code == HCI_NB_CMP_PKTS_EVT_CODE){
                //TRACE_PRINT("err evt_code 0x13\r\n");
                return false;
            }
            break;

        default:
            // Cannot be masked
            return false;
    }

    // Check if this event is maskable
    if (evt_code < HCI_MAX_EVT_MSK_PAGE_1_CODE)
    {
        uint8_t index = evt_code - 1;

        //Checking if the event is masked or not
        masked = ((evt_msk.mask[index>>3] & (1<<(index & 0x07))) == 0x00);

#if (BLE_EMB_PRESENT)
        if (!masked && (evt_code == HCI_LE_META_EVT_CODE)) {
            // Get Sub-code of the mevent
            uint8_t *subcode = (uint8_t*)kernel_msg2param(msg);
            //Translate the subcode in index to parse the mask
            uint8_t index = *subcode - 1;
            //Checking if the event is masked or not
            masked =((le_evt_msk.mask[index>>3] & (1<<(index & 0x07))) == 0x00);
        }
#endif // (BLE_EMB_PRESENT)
    } else if(evt_code < HCI_MAX_EVT_MSK_PAGE_2_CODE)
    {
        // In this area the evt code is in the range [EVT_MASK_CODE_MAX<evt_code<HCI_MAX_EVT_MSK_CODE]
        // The index should be < EVT_MASK_CODE_MAX to avoid evt_msk_page_2.mask array overflow
        uint8_t index = evt_code - EVT_MASK_CODE_MAX;

        //Checking if the event is masked or not
        masked = ((evt_msk_page_2.mask[index>>3] & (1<<(index & 0x07))) == 0x00);
    }
    return masked;
}

void hci_send_2_host(void *param)
{
	if (!param) {
		os_printf("%s:param is null\r\n", __FUNCTION__);
		return;
	}
	struct kernel_msg *msg = kernel_param2msg(param);
	uint8_t *buf = NULL;
	uint16_t len = 0;
	uint8_t type = 0;

	if (hci_evt_mask_check(msg)) {
		os_printf("do not support this command:%04x\r\n", msg->id);
		// Free the kernel message space
		kernel_msg_free(msg);
		return;
	}

	switch (msg->id) {
	case HCI_CMD_CMP_EVENT:
		buf = hci_build_cc_evt(msg);
		len = HCI_EVT_HDR_LEN + *(buf + HCI_EVT_CODE_LEN);
		type = HCI_EVT_MSG_TYPE;
		break;
	case HCI_CMD_STAT_EVENT:
		buf = hci_build_cs_evt(msg);
		len = HCI_EVT_HDR_LEN + *(buf + HCI_EVT_CODE_LEN);
		type = HCI_EVT_MSG_TYPE;
		break;
	case HCI_EVENT:
		buf = hci_build_evt(msg);
		len = HCI_EVT_HDR_LEN + *(buf + HCI_EVT_CODE_LEN);
		type = HCI_EVT_MSG_TYPE;
		break;
	case HCI_LE_EVENT:
		buf = hci_build_le_evt(msg);
		len = HCI_EVT_HDR_LEN + *(buf + HCI_EVT_CODE_LEN);
		type = HCI_EVT_MSG_TYPE;
		break;
	case HCI_ACL_DATA:
		buf = hci_build_acl_data(msg);
		len = HCI_ACL_HDR_LEN + common_read16p(buf + HCI_ACL_HDR_HDL_FLAGS_LEN);
		type = HCI_ACL_MSG_TYPE;
		break;
	default:
		break;
	}

    int i = 0;
    if (type == HCI_EVT_MSG_TYPE && buf) {
        if (g_hci_cmd_cb) {
            g_hci_cmd_cb((uint8_t *)(buf), len);
        }
    } else if (type == HCI_ACL_MSG_TYPE ) {
        if (g_hci_acl_cb) {
            g_hci_acl_cb((uint8_t *)(buf), len);
        }
    } else {
        os_printf("%s:unknow msg type:%d\r\n", __FUNCTION__, type);
    }

	if (type == HCI_ACL_MSG_TYPE) {
        //os_printf("acl free mem :%p \r\n",((struct hci_acl_data *)param)->buf_ptr);
		ble_util_buf_rx_free(((struct hci_acl_data *)param)->buf_ptr);
	}

	kernel_msg_free(msg);
}

const struct hci_cmd_desc_tag hci_cmd_desc_tab_lk_ctrl[] = {
	CMD(DISCONNECT,                  CTRL,         HL_CTRL),
	CMD(RD_REM_VER_INFO,             CTRL,         HL_CTRL),
};

/// HCI command descriptors (OGF controller and baseband)
const struct hci_cmd_desc_tag hci_cmd_desc_tab_ctrl_bb[] = {
	CMD(SET_EVT_MASK,                  MNG,       HL_MNG ),
	CMD(RESET,                         MNG,       HL_MNG ),
	CMD(RD_TX_PWR_LVL,                 CTRL,      HL_CTRL),
	CMD(SET_CTRL_TO_HOST_FLOW_CTRL,    MNG,       HL_MNG ),
	CMD(HOST_BUF_SIZE,                 MNG,       HL_MNG ),
	CMD(HOST_NB_CMP_PKTS,              MNG,       HL_MNG ),
	CMD(RD_AUTH_PAYL_TO,               CTRL,      HL_CTRL),
	CMD(WR_AUTH_PAYL_TO,               CTRL,      HL_CTRL),
	CMD(SET_EVT_MASK_PAGE_2,           MNG,       HL_MNG ),
};

/// HCI command descriptors (OGF informational parameters)
const struct hci_cmd_desc_tag hci_cmd_desc_tab_info_par[] = {
	CMD(RD_LOCAL_VER_INFO,    MNG,    HL_MNG),
	CMD(RD_LOCAL_SUPP_CMDS,   MNG,    HL_MNG),
	CMD(RD_LOCAL_SUPP_FEATS,  MNG,    HL_MNG),
	CMD(RD_BD_ADDR,           MNG,    HL_MNG),
};

/// HCI command descriptors (OGF status parameters)
const struct hci_cmd_desc_tag hci_cmd_desc_tab_stat_par[] = {
	CMD(RD_RSSI,              CTRL,           HL_CTRL),
};

/// HCI command descriptors (OGF LE controller)
const struct hci_cmd_desc_tag hci_cmd_desc_tab_le[] = {
	CMD(LE_SET_EXT_ADV_EN,                    BLE_MNG,  HL_MNG ),
	CMD(LE_SET_EXT_ADV_PARAM,                 BLE_MNG,  HL_MNG ),
	CMD(LE_SET_EXT_ADV_DATA,                  BLE_MNG,  HL_MNG ),
	CMD(LE_SET_EVT_MASK,                      BLE_MNG,  HL_MNG ),
	CMD(LE_RD_BUFF_SIZE,                      BLE_MNG,  HL_MNG ),
	CMD(LE_RD_LOCAL_SUPP_FEATS,               BLE_MNG,  HL_MNG ),
	CMD(LE_SET_RAND_ADDR,                     BLE_MNG,  HL_MNG ),
	CMD(LE_RD_ADV_CHNL_TX_PW,                 BLE_MNG,  HL_MNG ),
	CMD(LE_SET_ADV_PARAM,                     BLE_MNG,  HL_MNG ),
	CMD(LE_SET_ADV_DATA,                      BLE_MNG,  HL_MNG ),
	CMD(LE_SET_SCAN_RSP_DATA,                 BLE_MNG,  HL_MNG ),
	CMD(LE_SET_ADV_EN,                        BLE_MNG,  HL_MNG ),
	CMD(LE_SET_SCAN_PARAM,                    BLE_MNG,  HL_MNG ),
	CMD(LE_SET_SCAN_EN,                       BLE_MNG,  HL_MNG ),
	CMD(LE_CREATE_CON,                        BLE_MNG,  HL_MNG ),
	CMD(LE_CREATE_CON_CANCEL,                 BLE_MNG,  HL_MNG ),
	CMD(LE_RD_WLST_SIZE,                      BLE_MNG,  HL_MNG ),
	CMD(LE_CLEAR_WLST,                        BLE_MNG,  HL_MNG ),
	CMD(LE_ADD_DEV_TO_WLST,                   BLE_MNG,  HL_MNG ),
	CMD(LE_RMV_DEV_FROM_WLST,                 BLE_MNG,  HL_MNG ),
	CMD(LE_CON_UPDATE,                        BLE_CTRL, HL_CTRL),
	CMD(LE_SET_HOST_CH_CLASS,                 BLE_MNG,  HL_MNG ),
	CMD(LE_RD_CHNL_MAP,                       BLE_CTRL, HL_CTRL),
	CMD(LE_RD_REM_FEATS,                      BLE_CTRL, HL_CTRL),
	CMD(LE_ENC,                               BLE_MNG,  HL_MNG ),
	CMD(LE_RAND,                              BLE_MNG,  HL_MNG ),
	CMD(LE_START_ENC,                         BLE_CTRL, HL_CTRL),
	CMD(LE_LTK_REQ_REPLY,                     BLE_CTRL, HL_CTRL),
	CMD(LE_LTK_REQ_NEG_REPLY,                 BLE_CTRL, HL_CTRL),
	CMD(LE_RD_SUPP_STATES,                    BLE_MNG,  HL_MNG ),
	CMD(LE_RX_TEST_V1,                        BLE_MNG,  HL_MNG ),
	CMD(LE_TX_TEST_V1,                        BLE_MNG,  HL_MNG ),
	CMD(LE_TEST_END,                          BLE_MNG,  HL_MNG ),
	CMD(LE_REM_CON_PARAM_REQ_REPLY,           BLE_CTRL, HL_CTRL),
	CMD(LE_REM_CON_PARAM_REQ_NEG_REPLY,       BLE_CTRL, HL_CTRL),
	CMD(LE_SET_DATA_LEN,                      BLE_CTRL, HL_CTRL),
	CMD(LE_RD_SUGGTED_DFT_DATA_LEN,           BLE_MNG,  HL_MNG ),
	CMD(LE_WR_SUGGTED_DFT_DATA_LEN,           BLE_MNG,  HL_MNG ),
	CMD(LE_RD_LOC_P256_PUB_KEY,               BLE_MNG,  HL_MNG ),
	CMD(LE_GEN_DHKEY_V1,                      BLE_MNG,  HL_MNG ),
	CMD(LE_ADD_DEV_TO_RSLV_LIST,              BLE_MNG,  HL_MNG ),
	CMD(LE_RMV_DEV_FROM_RSLV_LIST,            BLE_MNG,  HL_MNG ),
	CMD(LE_CLEAR_RSLV_LIST,                   BLE_MNG,  HL_MNG ),
	CMD(LE_RD_RSLV_LIST_SIZE,                 BLE_MNG,  HL_MNG ),
	CMD(LE_RD_PEER_RSLV_ADDR,                 BLE_MNG,  HL_MNG ),
	CMD(LE_RD_LOC_RSLV_ADDR,                  BLE_MNG,  HL_MNG ),
	CMD(LE_SET_ADDR_RESOL_EN,                 BLE_MNG,  HL_MNG ),
	CMD(LE_SET_RSLV_PRIV_ADDR_TO,             BLE_MNG,  HL_MNG ),
	CMD(LE_RD_MAX_DATA_LEN,                   BLE_MNG,  HL_MNG ),
	CMD(LE_RD_PHY,                            BLE_CTRL, HL_CTRL),
	CMD(LE_SET_DFT_PHY,                       BLE_MNG,  HL_MNG ),
	CMD(LE_SET_PHY,                           BLE_CTRL, HL_CTRL),
	CMD(LE_RX_TEST_V2,                        BLE_MNG,  HL_MNG ),
	CMD(LE_TX_TEST_V2,                        BLE_MNG,  HL_MNG ),
	CMD(LE_SET_ADV_SET_RAND_ADDR,             BLE_MNG,  HL_MNG ),
	CMD(LE_SET_EXT_SCAN_RSP_DATA,             BLE_MNG,  HL_MNG ),
	CMD(LE_RD_MAX_ADV_DATA_LEN,               BLE_MNG,  HL_MNG ),
	CMD(LE_RD_NB_SUPP_ADV_SETS,               BLE_MNG,  HL_MNG ),
	CMD(LE_RMV_ADV_SET,                       BLE_MNG,  HL_MNG ),
	CMD(LE_CLEAR_ADV_SETS,                    BLE_MNG,  HL_MNG ),
	CMD(LE_SET_PER_ADV_PARAM,                 BLE_MNG,  HL_MNG ),
	CMD(LE_SET_PER_ADV_DATA,                  BLE_MNG,  HL_MNG ),
	CMD(LE_SET_PER_ADV_EN,                    BLE_MNG,  HL_MNG ),
	CMD(LE_SET_EXT_SCAN_PARAM,                BLE_MNG,  HL_MNG ),
	CMD(LE_SET_EXT_SCAN_EN,                   BLE_MNG,  HL_MNG ),
	CMD(LE_EXT_CREATE_CON,                    BLE_MNG,  HL_MNG ),
	CMD(LE_PER_ADV_CREATE_SYNC,               BLE_MNG,  HL_MNG ),
	CMD(LE_PER_ADV_CREATE_SYNC_CANCEL,        BLE_MNG,  HL_MNG ),
	CMD(LE_PER_ADV_TERM_SYNC,                 BLE_MNG,  HL_MNG ),
	CMD(LE_ADD_DEV_TO_PER_ADV_LIST,           BLE_MNG,  HL_MNG ),
	CMD(LE_RMV_DEV_FROM_PER_ADV_LIST,         BLE_MNG,  HL_MNG ),
	CMD(LE_CLEAR_PER_ADV_LIST,                BLE_MNG,  HL_MNG ),
	CMD(LE_RD_PER_ADV_LIST_SIZE,              BLE_MNG,  HL_MNG ),
	CMD(LE_RD_TX_PWR,                         BLE_MNG,  HL_MNG ),
	CMD(LE_RD_RF_PATH_COMP,                   BLE_MNG,  HL_MNG ),
	CMD(LE_WR_RF_PATH_COMP,                   BLE_MNG,  HL_MNG ),
	CMD(LE_SET_PRIV_MODE,                     BLE_MNG,  HL_MNG ),
	CMD(LE_RX_TEST_V3,                        BLE_MNG,  HL_MNG ),
	CMD(LE_TX_TEST_V3,                        BLE_MNG,  HL_MNG ),
	CMD(LE_SET_CONLESS_CTE_TX_PARAM,          BLE_MNG,  HL_MNG ),
	CMD(LE_SET_CONLESS_CTE_TX_EN,             BLE_MNG,  HL_MNG ),
	CMD(LE_SET_CONLESS_IQ_SAMPL_EN,           BLE_MNG,  HL_MNG ),
	CMD(LE_SET_CON_CTE_RX_PARAM,              BLE_CTRL, HL_CTRL),
	CMD(LE_CON_CTE_REQ_EN,                    BLE_CTRL, HL_CTRL),
	CMD(LE_SET_CON_CTE_TX_PARAM,              BLE_CTRL, HL_CTRL),
	CMD(LE_CON_CTE_RSP_EN,                    BLE_CTRL, HL_CTRL),
	CMD(LE_RD_ANTENNA_INF,                    BLE_MNG,  HL_MNG ),
	CMD(LE_SET_PER_ADV_REC_EN,                BLE_MNG,  HL_MNG ),
	CMD(LE_PER_ADV_SYNC_TRANSF,               BLE_CTRL, HL_CTRL),
	CMD(LE_PER_ADV_SET_INFO_TRANSF,           BLE_CTRL, HL_CTRL),
	CMD(LE_SET_PER_ADV_SYNC_TRANSF_PARAM,     BLE_CTRL, HL_MNG ),
	CMD(LE_SET_DFT_PER_ADV_SYNC_TRANSF_PARAM, BLE_MNG,  HL_MNG ),
	CMD(LE_GEN_DHKEY_V2,                      BLE_MNG,  HL_MNG ),
	CMD(LE_MOD_SLEEP_CLK_ACC,                 BLE_MNG,  HL_MNG ),
};

/// HCI command descriptors root table (classified by OGF)
const struct hci_cmd_desc_tab_ref hci_cmd_desc_root_tab[] = {
	{LK_CNTL_OGF,  ARRAY_LEN(hci_cmd_desc_tab_lk_ctrl),  hci_cmd_desc_tab_lk_ctrl },
	{CNTLR_BB_OGF, ARRAY_LEN(hci_cmd_desc_tab_ctrl_bb),  hci_cmd_desc_tab_ctrl_bb },
	{INFO_PAR_OGF, ARRAY_LEN(hci_cmd_desc_tab_info_par), hci_cmd_desc_tab_info_par},
	{STAT_PAR_OGF, ARRAY_LEN(hci_cmd_desc_tab_stat_par), hci_cmd_desc_tab_stat_par},
	{LE_CNTLR_OGF, ARRAY_LEN(hci_cmd_desc_tab_le),       hci_cmd_desc_tab_le      },
};

const struct hci_cmd_desc_tag* hci_look_for_cmd_desc(uint16_t opcode)
{
	const struct hci_cmd_desc_tag* tab = NULL;
	const struct hci_cmd_desc_tag* desc = NULL;
	uint16_t nb_cmds = 0;
	uint16_t index = 0;
	uint16_t ocf = HCI_OP2OCF(opcode);
	uint16_t ogf = HCI_OP2OGF(opcode);

	// Find table corresponding to this OGF
	for (index = 0; index < ARRAY_LEN(hci_cmd_desc_root_tab); index++) {
		// Compare the command opcodes
		if (hci_cmd_desc_root_tab[index].ogf == ogf) {
			// Get the command descriptors table information (size and pointer)
			tab = hci_cmd_desc_root_tab[index].cmd_desc_tab;
			nb_cmds = hci_cmd_desc_root_tab[index].nb_cmds;
			break;
		}
	}

	// Check if a table has been found for this OGF
	if (tab != NULL) {
		// Find the command descriptor associated to this OCF
		for (index = 0; index < nb_cmds; index++) {
			// Compare the command opcodes
			if (HCI_OP2OCF(tab->opcode) == ocf) {
				// Get the command descriptor pointer
				desc = tab;
				break;
			}

			// Jump to next command descriptor
			tab++;
		}
	}

	return (desc);
}

void hci_send_2_controller(void *param)
{
	struct kernel_msg *msg = kernel_param2msg(param);

#if BLE_EMB_PRESENT
	kernel_task_id_t dest = TASK_BLE_NONE;
	uint8_t ll_type = LL_UNDEF;
	const struct hci_cmd_desc_tag* cmd_desc;

	// The internal destination first depends on the message type (command, event, data)
	switch (msg->id) {
	case HCI_COMMAND:
		// Find a command descriptor associated to the command opcode
		cmd_desc = hci_look_for_cmd_desc(msg->src_id);

		// Check if the command is supported
		if(cmd_desc != NULL)
			ll_type = (cmd_desc->dest_field & HCI_CMD_DEST_LL_MASK) >> HCI_CMD_DEST_LL_POS;
		break;
	case HCI_ACL_DATA:
		ll_type = BLE_CTRL;
		break;

	default:
		break;
	}

	switch (ll_type) {

	case BLE_MNG:
	case MNG:
		dest = TASK_BLE_LLM;
		break;

	case CTRL:
	case BLE_CTRL:
		// Check if the link identifier in the dest_id field corresponds to an active BLE link
		if(msg->dest_id < BLE_ACTIVITY_MAX)
			// Build the destination task ID
			dest = KERNEL_BUILD_ID(TASK_BLE_LLC, msg->dest_id);
		else
			BLE_ASSERT_INFO(0, msg->id, msg->dest_id);
		break;

#if (BLE_EMB_PRESENT && BLE_ISO_PRESENT)
	case BLE_ISO:
		dest = TASK_BLE_LLI;
		break;
#endif //(BLE_EMB_PRESENT && BLE_ISO_PRESENT)

	default:
		break;
	}

	// Check it the destination has been found
	if (dest != TASK_BLE_NONE) {
		// Send the command to the internal destination task associated to this command
		msg->dest_id = dest;
		kernel_msg_send(param);
	} else {
		BLE_ASSERT_INFO(0, msg->id, msg->src_id);

		// Free message to avoid memory leak
		kernel_msg_free(msg);
	}
#endif //BLE_EMB_PRESENT
}

int hci_driver_send(uint8_t type, uint16_t len, uint8_t *buf)
{
	uint16_t err = 0;
	uint16_t  conidx  = 0;  //No need use in controller, so use zero

	hci_lock(&g_hci_send_context);

	if (NULL == buf) {
		os_printf("%s, buffer is NULL\r\n", __func__);
		hci_unlock(&g_hci_send_context);
		return -1;
	}

	switch (type) {
	case HCI_CMD_MSG_TYPE://HCI_EVT_MSG_TYPE:
		if (len != (((struct hci_cmd_hdr *)buf)->parlen + 3)) {
			os_printf("%s, type:%d, data length is error\r\n", __func__, type);
			err = -1;
			break;
		}

		///create send command message
		void *cmd_data = kernel_msg_alloc(HCI_COMMAND, conidx, ((struct hci_cmd_hdr *)buf)->opcode, ((struct hci_cmd_hdr *)buf)->parlen);
		if(cmd_data) {
			memcpy(cmd_data, (buf + sizeof(struct hci_cmd_hdr)), ((struct hci_cmd_hdr *)buf)->parlen);

			///send command to controller
			hci_send_2_controller(cmd_data);
		}else {
			os_printf("create send command message error!\r\n");
			err = -1;
		}
		break;

	case HCI_ACL_MSG_TYPE:
		//HCI_INFO("send ACL handle:0x%x\r\n", ((struct hci_acl_hdr *)buf)->hdl_flags);
		if (len != (((struct hci_acl_hdr *)buf)->datalen + 4)) {
			os_printf("%s, type:%d, data length is error\r\n", __func__, type);
			err = -1;
			break;
		}

		///get hci acl data buffer point
		uint16_t acl_buf = ble_util_buf_acl_tx_alloc();
		if (acl_buf) {
			struct hci_acl_data *data_tx = KERNEL_MSG_ALLOC(HCI_ACL_DATA, ((struct hci_acl_hdr *)buf)->hdl_flags & HCI_ACL_HANDLE_MASK,
									TASK_BLE_NONE, hci_acl_data);
			// Fill packet length
			data_tx->conhdl_pb_bc_flag = ((struct hci_acl_hdr *)buf)->hdl_flags;
			data_tx->length = ((struct hci_acl_hdr *)buf)->datalen;
			data_tx->buf_ptr = (uint32_t)acl_buf;
			uint8_t *buffer = (uint8_t *)(EM_BASE_ADDR + ((uint32_t)acl_buf));
			memcpy((uint8_t *)buffer, (buf + sizeof(struct hci_acl_hdr)), ((struct hci_acl_hdr *)buf)->datalen);

			// Send message
			hci_send_2_controller(data_tx);
		} else {
			os_printf("create acl message error!\r\n");
			err = -1;
		}
		break;

	default:
		os_printf("Unknown buffer type");
		err = -1;
		break;
	}

	hci_unlock(&g_hci_send_context);
	return err;
}
//#include "tuya_hal_system.h"
int hci_driver_open(void)
{
	//bk_printf("initial BLE...%d\r\n", tuya_hal_system_getheapsize());

	if (kNoErr == rtos_init_mutex(&g_hci_send_context.lock))
		g_hci_send_context.initialized = 1;
	else {
        os_printf("ble 0 failed\r\n");
        goto err_return;
    }

    common_list_init(&recv_wait_list);

    //bk_printf("initial BLE END...%d\r\n", tuya_hal_system_getheapsize());
    return 0;

err_return:
	if (g_hci_send_context.lock) {
		g_hci_send_context.initialized = 0;
		rtos_deinit_mutex(&g_hci_send_context.lock);
		g_hci_send_context.lock = NULL;
	}
	os_printf("ble open failed\r\n");
	return -1;
}

void hci_set_event_callback(hci_func_evt_cb hci_cmd_cb, hci_func_evt_cb hci_acl_cb)
{
    g_hci_cmd_cb = hci_cmd_cb;
    g_hci_acl_cb = hci_acl_cb;
}

///Eof

#endif

/// @} HCI

/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2000-2001  Qualcomm Incorporated
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2002-2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 */

#ifndef __HCI_LIB_H
#define __HCI_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

struct hci_request {
    UINT16_T ogf;
    UINT16_T ocf;
    INT_T event;
    VOID_T *cparam;
    INT_T clen;
    VOID_T *rparam;
    INT_T rlen;
};

STATIC INLINE VOID_T hci_set_bit(INT_T nr, VOID_T *addr)
{
    *((UINT32_T *)addr + (nr >> 5)) |= (1 << (nr & 31));
}

STATIC INLINE VOID_T hci_clear_bit(INT_T nr, VOID_T *addr)
{
    *((UINT32_T *)addr + (nr >> 5)) &= ~(1 << (nr & 31));
}

STATIC INLINE INT_T hci_test_bit(INT_T nr, VOID_T *addr)
{
    return *((UINT32_T *)addr + (nr >> 5)) & (1 << (nr & 31));
}

/* HCI filter tools */
STATIC INLINE VOID_T hci_filter_clear(struct hci_filter *f)
{
    memset(f, 0, sizeof(*f));
}

STATIC INLINE VOID_T hci_filter_set_ptype(INT_T t, struct hci_filter *f)
{
    hci_set_bit((t == HCI_VENDOR_PKT) ? 0 : (t & HCI_FLT_TYPE_BITS), &f->type_mask);
}

STATIC INLINE VOID_T hci_filter_clear_ptype(INT_T t, struct hci_filter *f)
{
    hci_clear_bit((t == HCI_VENDOR_PKT) ? 0 : (t & HCI_FLT_TYPE_BITS), &f->type_mask);
}

STATIC INLINE INT_T hci_filter_test_ptype(INT_T t, struct hci_filter *f)
{
    return hci_test_bit((t == HCI_VENDOR_PKT) ? 0 : (t & HCI_FLT_TYPE_BITS), &f->type_mask);
}

STATIC INLINE VOID_T hci_filter_all_ptypes(struct hci_filter *f)
{
    memset((VOID_T *)&f->type_mask, 0xff, sizeof(f->type_mask));
}

STATIC INLINE VOID_T hci_filter_set_event(INT_T e, struct hci_filter *f)
{
    hci_set_bit((e & HCI_FLT_EVENT_BITS), &f->event_mask);
}

STATIC INLINE VOID_T hci_filter_clear_event(INT_T e, struct hci_filter *f)
{
    hci_clear_bit((e & HCI_FLT_EVENT_BITS), &f->event_mask);
}

STATIC INLINE INT_T hci_filter_test_event(INT_T e, struct hci_filter *f)
{
    return hci_test_bit((e & HCI_FLT_EVENT_BITS), &f->event_mask);
}

STATIC INLINE VOID_T hci_filter_all_events(struct hci_filter *f)
{
    memset((VOID_T *)f->event_mask, 0xff, sizeof(f->event_mask));
}

STATIC INLINE VOID_T hci_filter_set_opcode(INT_T opcode, struct hci_filter *f)
{
    f->opcode = opcode;
}

STATIC INLINE VOID_T hci_filter_clear_opcode(struct hci_filter *f)
{
    f->opcode = 0;
}

STATIC INLINE INT_T hci_filter_test_opcode(INT_T opcode, struct hci_filter *f)
{
    return (f->opcode == opcode);
}

#ifdef __cplusplus
}
#endif

#endif /* __HCI_LIB_H */

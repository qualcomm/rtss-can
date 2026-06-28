// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef RTSS_MB_WRAPPER_H
#define RTSS_MB_WRAPPER_H

#include <stddef.h>
#include "rtss_can_structures.h"
#include <rtss_mailbox_api.h>


/*
 * Open the TX mailbox channel (/dev/rtss/can0).
 * On success, *handle is set to an allocated handle; caller owns it.
 */
int rtss_mb_init_shared_tx(struct rtss_mb_handle **handle);

/*
 * Open the RX mailbox channel (/dev/rtss/can1).
 * On success, *handle is set to an allocated handle; caller owns it.
 */
int rtss_mb_init_shared_rx(struct rtss_mb_handle **handle);

/*
 * Write sz bytes from buf to the TX mailbox.
 * Returns number of bytes written on success, -1 on error.
 */
int rtss_mb_write_shared(struct rtss_mb_handle *handle, void *buf, size_t sz);

/*
 * Blocking read of up to sz bytes into buf from the RX mailbox.
 * Returns number of bytes read on success, -1 on error.
 */
int rtss_mb_read_shared(struct rtss_mb_handle *handle, void *buf, size_t sz);

/*
 * Close the mailbox handle and set *handle to NULL.
 * Returns 0 on success, -1 on error.
 */
int rtss_mb_close_shared(struct rtss_mb_handle **handle);

#endif /* RTSS_MB_WRAPPER_H */

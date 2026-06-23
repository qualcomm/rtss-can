// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

/*
 * @file rtss_mb_wrapper.c
 * @brief RTSS Mailbox API Implementation
 *
 * This file implements the RTSS mailbox API wrapper functions for the CAN daemon.
 * It provides helper functions and utilities for RTSS mailbox communication.
 */

#include "rtss_mb_wrapper.h"
#include "rtss_can_logging.h"
#include <string.h>
#include <errno.h>

#define RTSS_CAN_TX_CHANNEL "/dev/sail/can0"
#define RTSS_CAN_RX_CHANNEL "/dev/sail/can1"

int rtss_mb_init_shared_tx(struct rtss_mb_handle **handle)
{
	int ret;

	if (!handle) {
		LOG_ERROR_MSG("RTSS_WRPR", "NULL handle pointer for TX init");
		return -EINVAL;
	}

	ret = rtss_mb_open(handle, RTSS_CAN_TX_CHANNEL);
	if (ret < 0) {
		switch (ret) {
		case -ENOENT:
			LOG_ERROR_MSG("RTSS_WRPR", "TX mailbox device not found: %s",
				      RTSS_CAN_TX_CHANNEL);
			break;
		case -EPERM:
			LOG_ERROR_MSG("RTSS_WRPR", "Permission denied opening TX mailbox: %s",
				      RTSS_CAN_TX_CHANNEL);
			break;
		case -EINVAL:
			LOG_ERROR_MSG("RTSS_WRPR", "Invalid device path for TX mailbox: %s",
				      RTSS_CAN_TX_CHANNEL);
			break;
		default:
			LOG_ERROR_MSG("RTSS_WRPR", "Failed to open TX mailbox %s: %s",
				      RTSS_CAN_TX_CHANNEL, strerror(-ret));
			break;
		}
		return ret;
	}

	LOG_INFO_MSG("RTSS_WRPR", "TX mailbox opened: %s", RTSS_CAN_TX_CHANNEL);
	return 0;
}

int rtss_mb_init_shared_rx(struct rtss_mb_handle **handle)
{
	int ret;

	if (!handle) {
		LOG_ERROR_MSG("RTSS_WRPR", "NULL handle pointer for RX init");
		return -EINVAL;
	}

	ret = rtss_mb_open(handle, RTSS_CAN_RX_CHANNEL);
	if (ret < 0) {
		switch (ret) {
		case -ENOENT:
			LOG_ERROR_MSG("RTSS_WRPR", "RX mailbox device not found: %s",
				      RTSS_CAN_RX_CHANNEL);
			break;
		case -EPERM:
			LOG_ERROR_MSG("RTSS_WRPR", "Permission denied opening RX mailbox: %s",
				      RTSS_CAN_RX_CHANNEL);
			break;
		case -EINVAL:
			LOG_ERROR_MSG("RTSS_WRPR", "Invalid device path for RX mailbox: %s",
				      RTSS_CAN_RX_CHANNEL);
			break;
		default:
			LOG_ERROR_MSG("RTSS_WRPR", "Failed to open RX mailbox %s: %s",
				      RTSS_CAN_RX_CHANNEL, strerror(-ret));
			break;
		}
		return ret;
	}

	LOG_INFO_MSG("RTSS_WRPR", "RX mailbox opened: %s", RTSS_CAN_RX_CHANNEL);
	return 0;
}

int rtss_mb_write_shared(struct rtss_mb_handle *handle, void *buf, size_t sz)
{
	int ret;

	if (!handle || !buf || sz == 0) {
		LOG_ERROR_MSG("RTSS_WRPR", "Invalid parameters for mailbox write");
		return -EINVAL;
	}

	do {
		ret = rtss_mb_write(handle, buf, sz);
	} while (ret == -EINTR);

	if (ret < 0) {
		switch (ret) {
		case -ENOBUFS:
			LOG_WARN_MSG("RTSS_WRPR", "TX mailbox buffer full (ENOBUFS), %zu bytes dropped",
				     sz);
			break;
		case -EMSGSIZE:
			LOG_ERROR_MSG("RTSS_WRPR", "Message too large for TX mailbox: %zu bytes",
				      sz);
			break;
		case -EBADF:
			LOG_ERROR_MSG("RTSS_WRPR", "TX mailbox handle invalid (EBADF)");
			break;
		case -ECONNRESET:
			LOG_ERROR_MSG("RTSS_WRPR", "TX mailbox connection reset by RTSS (ECONNRESET)");
			break;
		case -EIO:
			LOG_ERROR_MSG("RTSS_WRPR", "TX mailbox I/O error (EIO)");
			break;
		default:
			LOG_ERROR_MSG("RTSS_WRPR", "TX mailbox write failed: %s", strerror(-ret));
			break;
		}
		return ret;
	}

	LOG_DEBUG_MSG("RTSS_WRPR", "Wrote %zu bytes to TX mailbox", sz);
	return 0;
}

int rtss_mb_read_shared(struct rtss_mb_handle *handle, void *buf, size_t sz)
{
	int ret;

	if (!handle || !buf || sz == 0) {
		LOG_ERROR_MSG("RTSS_WRPR", "Invalid parameters for mailbox read");
		return -EINVAL;
	}

	/*
	 * Retry on EINTR (signal during blocking read, e.g. SIGHUP config reload).
	 * pthread_cancel() cancels the thread at this call as a cancellation point
	 * and never returns here, so the loop does not prevent graceful shutdown.
	 */
	do {
		ret = rtss_mb_read(handle, buf, sz);
	} while (ret == -EINTR);

	if (ret < 0) {
		switch (ret) {
		case -EAGAIN:
			/* No data available on a non-blocking handle — not a hard error */
			return 0;
		case -ECONNRESET:
			LOG_ERROR_MSG("RTSS_WRPR", "RX mailbox connection reset by RTSS (ECONNRESET)");
			break;
		case -EIO:
			LOG_ERROR_MSG("RTSS_WRPR", "RX mailbox I/O error (EIO)");
			break;
		case -EBADF:
			LOG_ERROR_MSG("RTSS_WRPR", "RX mailbox handle invalid (EBADF)");
			break;
		default:
			LOG_ERROR_MSG("RTSS_WRPR", "RX mailbox read failed: %s", strerror(-ret));
			break;
		}
		return ret;
	}

	LOG_DEBUG_MSG("RTSS_WRPR", "Read %d bytes from RX mailbox", ret);
	return ret;
}

int rtss_mb_close_shared(struct rtss_mb_handle **handle)
{
	int ret;

	if (!handle || !(*handle)) {
		LOG_ERROR_MSG("RTSS_WRPR", "NULL handle for mailbox close");
		return -EINVAL;
	}

	ret = rtss_mb_close(*handle);
	*handle = NULL;

	if (ret < 0) {
		switch (ret) {
		case -EBADF:
			LOG_ERROR_MSG("RTSS_WRPR", "Mailbox close: handle already invalid (EBADF)");
			break;
		case -EIO:
			LOG_ERROR_MSG("RTSS_WRPR", "Mailbox close: I/O error flushing (EIO)");
			break;
		default:
			LOG_ERROR_MSG("RTSS_WRPR", "Mailbox close failed: %s", strerror(-ret));
			break;
		}
		return ret;
	}

	LOG_INFO_MSG("RTSS_WRPR", "Mailbox closed");
	return 0;
}

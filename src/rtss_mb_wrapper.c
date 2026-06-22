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
#include "rtss_can_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/eventfd.h>

/* Forward declarations for RTSS mailbox library functions */
typedef struct mb_desc mb_desc_t;
typedef struct mbsub_rgn_desc mbsub_rgn_desc_t;

/* Get number of valid items in mailbox subregion */
int LibMB_Get_ValidItemNum(const mb_desc_t *pMbDesc, uint32_t uSubregionIndex);

/* Read items from mailbox subregion */
int LibMB_Read(mb_desc_t *pMbDesc, void *pBuffer, int nNumItems, uint32_t uSubregionIndex);

/* RTSS mailbox mode definitions */
#define RTSS_MB_MODE_TX     1
#define RTSS_MB_MODE_RX     0

/* RTSS shared channel names - only 2 channels for all controllers */
#define RTSS_CAN_TX_CHANNEL "/dev/rtss/can0"  /* Shared TX channel */
#define RTSS_CAN_RX_CHANNEL "/dev/rtss/can1"  /* Shared RX channel */

#ifndef MAX_CAN_CONTROLLERS
#define MAX_CAN_CONTROLLERS 8
#endif

/*
 * @brief Initialize shared RTSS mailbox connection for TX
 *
 * Opens the shared TX channel (/dev/rtss/can0) used by all controllers.
 *
 * @param ppClientData Pointer to pointer for client data structure (will be allocated)
 * @return 0 on success, -1 on error
 */
int rtss_mb_init_shared_tx(struct SailClientDataType **ppClientData)
{
	struct SailClientDataType *pClientData;
	int nRet;

	if (!ppClientData) {
		LOG_ERROR_MSG("RTSS_API", "Invalid parameters for shared TX init");
		return -1;
	}

	/* Allocate client data structure */
	pClientData = (struct SailClientDataType *)calloc(1, sizeof(struct SailClientDataType));
	if (!pClientData) {
		LOG_ERROR_MSG("RTSS_API", "Failed to allocate memory for TX client data");
		return -1;
	}

	/* Initialize client data structure */
	pClientData->uMode = RTSS_MB_MODE_TX;

	/* Open RTSS mailbox connection to shared TX channel */
	nRet = open_rtss_mailbox(pClientData, RTSS_CAN_TX_CHANNEL);
	if (nRet != RETURN_SUCCESS) {
		LOG_ERROR_MSG("RTSS_API", "Failed to open shared TX mailbox %s: %d",
			      RTSS_CAN_TX_CHANNEL, nRet);
		free(pClientData);
		return -1;
	}

	*ppClientData = pClientData;
	LOG_INFO_MSG("RTSS_API", "Shared TX mailbox initialized: %s", RTSS_CAN_TX_CHANNEL);
	return 0;
}

/*
 * @brief Initialize shared RTSS mailbox connection for RX
 *
 * Opens the shared RX channel (/dev/rtss/can1) used by all controllers.
 *
 * @param ppClientData Pointer to pointer for client data structure (will be allocated)
 * @return 0 on success, -1 on error
 */
int rtss_mb_init_shared_rx(struct SailClientDataType **ppClientData)
{
	struct SailClientDataType *pClientData;
	int nRet;

	if (!ppClientData) {
		LOG_ERROR_MSG("RTSS_API", "Invalid parameters for shared RX init");
		return -1;
	}

	/* Allocate client data structure */
	pClientData = (struct SailClientDataType *)calloc(1, sizeof(struct SailClientDataType));
	if (!pClientData) {
		LOG_ERROR_MSG("RTSS_API", "Failed to allocate memory for RX client data");
		return -1;
	}

	/* Initialize client data structure */
	pClientData->uMode = RTSS_MB_MODE_RX;

	/* Open RTSS mailbox connection to shared RX channel */
	nRet = open_rtss_mailbox(pClientData, RTSS_CAN_RX_CHANNEL);
	if (nRet != RETURN_SUCCESS) {
		LOG_ERROR_MSG("RTSS_API", "Failed to open shared RX mailbox %s: %d",
			      RTSS_CAN_RX_CHANNEL, nRet);
		free(pClientData);
		return -1;
	}

	*ppClientData = pClientData;
	LOG_INFO_MSG("RTSS_API", "Shared RX mailbox initialized: %s", RTSS_CAN_RX_CHANNEL);
	return 0;
}

/*
 * @brief Write data to shared RTSS mailbox
 *
 * Wrapper function for writing to the shared TX mailbox.
 *
 * @param pClientData Pointer to client data structure
 * @param pData Pointer to data buffer to write
 * @param nSize Size of data to write
 * @return Number of bytes written on success, -1 on error
 */
int rtss_mb_write_shared(struct SailClientDataType *pClientData, void *pData, size_t nSize)
{
	int nRet;

	if (!pClientData || !pData || nSize == 0) {
		LOG_ERROR_MSG("RTSS_API", "Invalid parameters for shared write");
		return -1;
	}

	if (pClientData->uMode != RTSS_MB_MODE_TX) {
		LOG_ERROR_MSG("RTSS_API", "Client not configured for TX mode");
		return -1;
	}

	/* Write data using RTSS UMD API */
	nRet = write_rtss_mailbox(pClientData, pData, nSize);
	if (nRet < 0) {
		LOG_ERROR_MSG("RTSS_API", "Failed to write to shared RTSS mailbox: %d", nRet);
		return -1;
	}

	LOG_DEBUG_MSG("RTSS_API", "Wrote %zu bytes to shared TX mailbox", nSize);
	return nRet;
}

/*
 * @brief Read data from shared RTSS mailbox
 *
 * Wrapper function for reading from the shared RX mailbox (blocking).
 *
 * @param pClientData Pointer to client data structure
 * @param pData Pointer to buffer for received data
 * @param nSize Size of buffer
 * @return Number of bytes read on success, -1 on error
 */
int rtss_mb_read_shared(struct SailClientDataType *pClientData, void *pData, size_t nSize)
{
	int nRet;

	if (!pClientData || !pData || nSize == 0) {
		LOG_ERROR_MSG("RTSS_API", "Invalid parameters for shared read");
		return -1;
	}

	if (pClientData->uMode != RTSS_MB_MODE_RX) {
		LOG_ERROR_MSG("RTSS_API", "Client not configured for RX mode");
		return -1;
	}

	/* Read data using RTSS UMD API (blocking) */
	nRet = read_rtss_mailbox(pClientData, pData, nSize);
	if (nRet < 0) {
		LOG_ERROR_MSG("RTSS_API", "Failed to read from shared RTSS mailbox: %d", nRet);
		return -1;
	}

	if (nRet == 0) {
		/* No data available */
		return -1;
	}

	LOG_DEBUG_MSG("RTSS_API", "Read %d bytes from shared RX mailbox", nRet);
	return nRet;
}

/*
 * @brief Read data from shared RTSS mailbox with timeout
 *
 * Wrapper function for reading from the shared RX mailbox with a timeout.
 * This function uses poll() with a timeout to allow the RX thread to be interruptible.
 *
 * @param pClientData Pointer to client data structure
 * @param pData Pointer to buffer for received data
 * @param nSize Size of buffer
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking, -1 for infinite)
 * @return Number of bytes read on success, 0 on timeout, -1 on error
 */
int rtss_mb_read_shared_timeout(struct SailClientDataType *pClientData, void *pData,
				 size_t nSize, int timeout_ms)
{
	int nRet;
	struct pollfd fds;
	int nNumItems, nItemsRead;
	eventfd_t EvRet = 0;
	const mb_desc_t *pMbDesc;
	int nBytesAvailable;
	int nBytesRead;

	if (!pClientData || !pData || nSize == 0) {
		LOG_ERROR_MSG("RTSS_API", "Invalid parameters for shared read with timeout");
		return -1;
	}

	if (pClientData->uMode != RTSS_MB_MODE_RX) {
		LOG_ERROR_MSG("RTSS_API", "Client not configured for RX mode");
		return -1;
	}

	if (pClientData->EventFd < 0) {
		LOG_ERROR_MSG("RTSS_API", "Invalid event FD for RX client");
		return -1;
	}

	/* Setup poll structure */
	fds.fd = pClientData->EventFd;
	fds.events = POLLIN;
	fds.revents = 0;

	/* Poll with timeout */
	nRet = poll(&fds, 1, timeout_ms);

	if (nRet < 0) {
		if (errno == EINTR) {
			/* Interrupted by signal - return timeout */
			return 0;
		}
		LOG_ERROR_MSG("RTSS_API", "Poll failed: %s", strerror(errno));
		return -1;
	}

	if (nRet == 0) {
		/* Timeout - no data available */
		return 0;
	}

	/* Data available - read the eventfd */
	if (fds.revents & POLLIN) {
		nRet = eventfd_read(fds.fd, &EvRet);
		if (nRet < 0) {
			LOG_ERROR_MSG("RTSS_API", "Eventfd read failed: %s", strerror(errno));
			return -1;
		}
	}

	/* Get mailbox descriptor */
	pMbDesc = (mb_desc_t *)pClientData->pMbBaseAddr;

	/* Check how many items are available */
	nNumItems = LibMB_Get_ValidItemNum(pMbDesc, pClientData->uSubregionIndex);
	if (nNumItems <= 0) {
		/* No data available (spurious wakeup) */
		return 0;
	}

	/* Calculate bytes to read */
	nBytesAvailable = nNumItems * pClientData->uSubChanItemSize;
	if (nBytesAvailable > (int)nSize) {
		LOG_WARN_MSG("RTSS_API", "Buffer size %zu is less than available data %d, truncating",
			     nSize, nBytesAvailable);
		nNumItems = nSize / pClientData->uSubChanItemSize;
	}

	/* Read data from mailbox */
	nItemsRead = LibMB_Read((mb_desc_t *)pMbDesc, pData, nNumItems, pClientData->uSubregionIndex);
	if (nItemsRead <= 0) {
		LOG_ERROR_MSG("RTSS_API", "LibMB_Read failed: %d", nItemsRead);
		return -1;
	}

	nBytesRead = nItemsRead * pClientData->uSubChanItemSize;
	LOG_DEBUG_MSG("RTSS_API", "Read %d bytes from shared RX mailbox (timeout=%dms)",
		      nBytesRead, timeout_ms);

	return nBytesRead;
}

/*
 * @brief Close shared RTSS mailbox connection
 *
 * Wrapper function for closing a shared mailbox client and freeing resources.
 *
 * @param ppClientData Pointer to pointer for client data structure (will be freed)
 * @return 0 on success, -1 on error
 */
int rtss_mb_close_shared(struct SailClientDataType **ppClientData)
{
	struct SailClientDataType *pClientData;
	int nRet;

	if (!ppClientData || !(*ppClientData)) {
		LOG_ERROR_MSG("RTSS_API", "Invalid parameters for shared close");
		return -1;
	}

	pClientData = *ppClientData;

	/* Close RTSS mailbox connection */
	nRet = close_rtss_mailbox(pClientData);
	if (nRet != RETURN_SUCCESS) {
		LOG_ERROR_MSG("RTSS_API", "Failed to close shared RTSS mailbox: %d", nRet);
		/* Continue to free memory even if close failed */
	}

	/* Free client data structure */
	free(pClientData);
	*ppClientData = NULL;

	LOG_INFO_MSG("RTSS_API", "Shared RTSS mailbox closed");
	return (nRet == RETURN_SUCCESS) ? 0 : -1;
}

/*
 * @brief Send CAN mailbox packet to RTSS
 *
 * @param client_data Pointer to TX client data
 * @param packet Pointer to CAN mailbox packet
 * @param controller_id Controller ID for logging
 * @return 0 on success, -1 on error
 */
int rtss_mb_send_packet(struct SailClientDataType *client_data,
			const struct can_mb_packet *packet, int controller_id)
{
	int ret;

	if (!client_data || !packet) {
		LOG_ERROR_MSG("RTSS_API", "Invalid parameters for send packet");
		return -1;
	}

	if (client_data->uMode != RTSS_MB_MODE_TX) {
		LOG_ERROR_MSG("RTSS_API", "Client not configured for TX mode");
		return -1;
	}

	/* Send packet using RTSS UMD API */
	ret = write_rtss_mailbox(client_data, (void *)packet, sizeof(struct can_mb_packet));
	if (ret < 0) {
		LOG_ERROR_MSG("RTSS_API", "Failed to write to RTSS mailbox: %d", ret);
		return -1;
	}

	LOG_DEBUG_MSG("RTSS_API", "Sent packet to RTSS for controller %d", controller_id);
	return 0;
}

/*
 * @brief Receive CAN mailbox packet from RTSS
 *
 * @param client_data Pointer to RX client data
 * @param packet Pointer to buffer for received packet
 * @param controller_id Controller ID for logging
 * @return 0 on success, -1 on error
 */
int rtss_mb_receive_packet(struct SailClientDataType *client_data,
			    struct can_mb_packet *packet, int controller_id)
{
	int ret;

	if (!client_data || !packet) {
		LOG_ERROR_MSG("RTSS_API", "Invalid parameters for receive packet");
		return -1;
	}

	if (client_data->uMode != RTSS_MB_MODE_RX) {
		LOG_ERROR_MSG("RTSS_API", "Client not configured for RX mode");
		return -1;
	}

	/* Receive packet using RTSS UMD API */
	ret = read_rtss_mailbox(client_data, (void *)packet, sizeof(struct can_mb_packet));
	if (ret < 0) {
		LOG_ERROR_MSG("RTSS_API", "Failed to read from RTSS mailbox: %d", ret);
		return -1;
	}

	if (ret == 0) {
		/* No data available */
		return -1;
	}

	LOG_DEBUG_MSG("RTSS_API", "Received packet from RTSS for controller %d", controller_id);
	return 0;
}

/*
 * @brief Close RTSS mailbox connection
 *
 * @param client_data Pointer to client data
 * @param controller_id Controller ID for logging
 * @return 0 on success, -1 on error
 */
int rtss_mb_close(struct SailClientDataType *client_data, int controller_id)
{
	int ret;

	if (!client_data) {
		LOG_ERROR_MSG("RTSS_API", "Invalid client data for close");
		return -1;
	}

	ret = close_rtss_mailbox(client_data);
	if (ret != RETURN_SUCCESS) {
		LOG_ERROR_MSG("RTSS_API", "Failed to close RTSS mailbox for controller %d: %d",
			      controller_id, ret);
		return -1;
	}

	LOG_INFO_MSG("RTSS_API", "RTSS mailbox closed for controller %d", controller_id);
	return 0;
}

/*
 * @brief Check if RTSS mailbox connection is valid
 *
 * @param client_data Pointer to client data
 * @return 1 if valid, 0 if invalid
 */
int rtss_mb_is_connected(const struct SailClientDataType *client_data)
{
	if (!client_data)
		return 0;

	/* Basic validation - in a real implementation, you might check more fields */
	return (client_data->pMbBaseAddr != NULL);
}

/*
 * @brief Get RTSS mailbox statistics
 *
 * @param client_data Pointer to client data
 * @param stats Pointer to statistics structure
 * @return 0 on success, -1 on error
 */
int rtss_mb_get_stats(const struct SailClientDataType *client_data,
		      rtss_mb_stats_t *stats)
{
	if (!client_data || !stats)
		return -1;

	/* Initialize stats structure */
	memset(stats, 0, sizeof(rtss_mb_stats_t));

	/* In a real implementation, you would gather actual statistics */
	stats->packets_sent = 0;
	stats->packets_received = 0;
	stats->errors = 0;
	stats->connection_time = 0;

	return 0;
}

/*
 * @brief Reset RTSS mailbox statistics
 *
 * @param client_data Pointer to client data
 * @return 0 on success, -1 on error
 */
int rtss_mb_reset_stats(struct SailClientDataType *client_data)
{
	if (!client_data)
		return -1;

	/* In a real implementation, you would reset actual statistics */
	LOG_INFO_MSG("RTSS_API", "Statistics reset for RTSS mailbox");
	return 0;
}

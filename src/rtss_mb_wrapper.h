// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file rtss_mb_wrapper.h
 * @brief RTSS Mailbox API Header
 * 
 * This file contains the API declarations for RTSS mailbox communication
 * used by the CAN daemon.
 */

#ifndef RTSS_MB_WRAPPER_H
#define RTSS_MB_WRAPPER_H

#include "rtss_can_structures.h"
#include "rtss_mailbox.h"
#include <stddef.h>
#include <stdint.h>

/* Return codes */
#ifndef RETURN_SUCCESS
#define RETURN_SUCCESS  0
#endif

#ifndef RETURN_ERROR
#define RETURN_ERROR   -1
#endif

/**
 * @brief RTSS mailbox statistics structure
 */
typedef struct {
    uint64_t packets_sent;      /* Number of packets sent */
    uint64_t packets_received;  /* Number of packets received */
    uint64_t errors;           /* Number of errors */
    uint64_t connection_time;  /* Connection time in seconds */
} rtss_mb_stats_t;

/* Function prototypes */

/**
 * @brief Initialize shared RTSS mailbox connection for TX
 * 
 * Opens the shared TX channel (/dev/rtss/can0) used by all controllers.
 * 
 * @param ppClientData Pointer to pointer for client data structure (will be allocated)
 * @return 0 on success, -1 on error
 */
int rtss_mb_init_shared_tx(struct SailClientDataType **ppClientData);

/**
 * @brief Initialize shared RTSS mailbox connection for RX
 * 
 * Opens the shared RX channel (/dev/rtss/can1) used by all controllers.
 * 
 * @param ppClientData Pointer to pointer for client data structure (will be allocated)
 * @return 0 on success, -1 on error
 */
int rtss_mb_init_shared_rx(struct SailClientDataType **ppClientData);

/**
 * @brief Send CAN mailbox packet to RTSS
 * 
 * @param client_data Pointer to TX client data
 * @param packet Pointer to CAN mailbox packet
 * @param controller_id Controller ID for logging
 * @return 0 on success, -1 on error
 */
int rtss_mb_send_packet(struct SailClientDataType *client_data, 
                       const struct can_mb_packet *packet, int controller_id);

/**
 * @brief Receive CAN mailbox packet from RTSS
 * 
 * @param client_data Pointer to RX client data
 * @param packet Pointer to buffer for received packet
 * @param controller_id Controller ID for logging
 * @return 0 on success, -1 on error
 */
int rtss_mb_receive_packet(struct SailClientDataType *client_data, 
                          struct can_mb_packet *packet, int controller_id);

/**
 * @brief Close RTSS mailbox connection
 * 
 * @param client_data Pointer to client data
 * @param controller_id Controller ID for logging
 * @return 0 on success, -1 on error
 */
int rtss_mb_close(struct SailClientDataType *client_data, int controller_id);

/**
 * @brief Check if RTSS mailbox connection is valid
 * 
 * @param client_data Pointer to client data
 * @return 1 if valid, 0 if invalid
 */
int rtss_mb_is_connected(const struct SailClientDataType *client_data);

/**
 * @brief Get RTSS mailbox statistics
 * 
 * @param client_data Pointer to client data
 * @param stats Pointer to statistics structure
 * @return 0 on success, -1 on error
 */
int rtss_mb_get_stats(const struct SailClientDataType *client_data, 
                     rtss_mb_stats_t *stats);

/**
 * @brief Reset RTSS mailbox statistics
 * 
 * @param client_data Pointer to client data
 * @return 0 on success, -1 on error
 */
int rtss_mb_reset_stats(struct SailClientDataType *client_data);

/**
 * @brief Write data to shared RTSS mailbox
 * 
 * Wrapper function for writing to the shared TX mailbox.
 * 
 * @param pClientData Pointer to client data structure
 * @param pData Pointer to data buffer to write
 * @param nSize Size of data to write
 * @return Number of bytes written on success, -1 on error
 */
int rtss_mb_write_shared(struct SailClientDataType *pClientData, void *pData, size_t nSize);

/**
 * @brief Read data from shared RTSS mailbox (blocking)
 * 
 * Wrapper function for reading from the shared RX mailbox (blocking, infinite timeout).
 * 
 * @param pClientData Pointer to client data structure
 * @param pData Pointer to buffer for received data
 * @param nSize Size of buffer
 * @return Number of bytes read on success, -1 on error
 */
int rtss_mb_read_shared(struct SailClientDataType *pClientData, void *pData, size_t nSize);

/**
 * @brief Read data from shared RTSS mailbox with timeout
 * 
 * Wrapper function for reading from the shared RX mailbox with a timeout.
 * This function allows the RX thread to be interruptible for graceful shutdown.
 * 
 * @param pClientData Pointer to client data structure
 * @param pData Pointer to buffer for received data
 * @param nSize Size of buffer
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking, -1 for infinite)
 * @return Number of bytes read on success, 0 on timeout, -1 on error
 */
int rtss_mb_read_shared_timeout(struct SailClientDataType *pClientData, void *pData, size_t nSize, int timeout_ms);

/**
 * @brief Close shared RTSS mailbox connection
 * 
 * Wrapper function for closing a shared mailbox client and freeing resources.
 * 
 * @param ppClientData Pointer to pointer for client data structure (will be freed)
 * @return 0 on success, -1 on error
 */
int rtss_mb_close_shared(struct SailClientDataType **ppClientData);

#endif /* RTSS_MB_WRAPPER_H */

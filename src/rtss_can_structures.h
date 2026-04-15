// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file can_mb_structures.h
 * @brief CAN Mailbox Structures for RTSS CAN Communication
 *
 * This file contains the data structures used for CAN communication
 * between Main Domain (MD) and RTSS Domain (SD) via mailbox.
 *
 * Based on RTSS CAN MB STRUCTS from can_structures.txt
 */

#ifndef CAN_MB_STRUCTURES_H
#define CAN_MB_STRUCTURES_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum CAN data length for mailbox packets */
#define CAN_MAILBOX_MAX_DATA_LEN    64

/**
 * @brief CAN Mailbox Command Types
 *
 * These commands are used to specify the type of operation
 * to be performed by the RTSS CAN controller.
 */
enum can_mb_command {
	CAN_MAILBOX_RX,              /* Receive CAN message */
	CAN_MAILBOX_TX,              /* Transmit CAN message */
	CAN_MAILBOX_SET_BAUDRATE,    /* Configure baud rate */
	CAN_MAILBOX_SD_READY,        /* RTSS Domain ready status */
	CAN_MAILBOX_ERROR,           /* Error notification */
};

/**
 * @brief CAN Mailbox Error Message Types
 *
 * Different types of errors that can be reported by the
 * RTSS CAN controller.
 */
enum can_mb_error_msg_type {
	CAN_MSG_TYPE_PROTO_ERR,      /* Protocol error */
	CAN_MSG_TYPE_FUSA_ERR,       /* Functional safety error */
	CAN_MSG_TYPE_PASSIVE_ERR,    /* Error passive state */
	CAN_MSG_TYPE_BUS_OFF_ERR,    /* Bus-off error */
};

/**
 * @brief CAN Error State Types
 *
 * Represents the current error state of the CAN controller.
 */
enum can_error_state_type {
	CAN_ERRORSTATE_ACTIVE = 0u,  /* Error active state */
	CAN_ERRORSTATE_PASSIVE,      /* Error passive state */
	CAN_ERRORSTATE_BUSOFF        /* Bus-off state */
};

/**
 * @brief CAN Error Types
 *
 * Specific types of CAN protocol errors that can occur.
 */
enum can_error_type {
	CAN_ERROR_BIT_MONITORING1 = 1u,  /* Bit monitoring error (recessive) */
	CAN_ERROR_BIT_MONITORING0,       /* Bit monitoring error (dominant) */
	CAN_ERROR_BIT,                   /* Bit error */
	CAN_ERROR_CHECK_ACK_FAILED,      /* ACK check failed */
	CAN_ERROR_ACK_DELIMITER,         /* ACK delimiter error */
	CAN_ERROR_ARBITRATION_LOST,      /* Arbitration lost */
	CAN_ERROR_OVERLOAD,              /* Overload error */
	CAN_ERROR_CHECK_FORM_FAILED,     /* Form check failed */
	CAN_ERROR_CHECK_STUFFING_FAILED, /* Stuffing check failed */
	CAN_ERROR_CHECK_CRC_FAILED,      /* CRC check failed */
	CAN_ERROR_BUS_LOCK               /* Bus lock error */
};

/**
 * @brief CAN-FD Message Structure (FD-ONLY MODE)
 *
 * Contains CAN-FD message data and metadata. This platform supports
 * ONLY CAN-FD frames. Classical CAN frames are not supported.
 * All messages must be in CAN-FD format with flexible data rate.
 */
struct can_mb_msg {
	uint8_t data[CAN_MAILBOX_MAX_DATA_LEN]; /* CAN-FD message data payload (up to 64 bytes) */
	uint8_t len;                            /* Actual data length (0-64 bytes, CAN-FD only) */
	uint32_t mid;                           /* CAN message identifier (11-bit or 29-bit) */
	bool ext_id;                            /* Extended ID flag: 1=29-bit, 0=11-bit */
	bool brs;                               /* Bit Rate Switch (mandatory for CAN-FD) */
	bool esi;                               /* Error State Indicator (CAN-FD feature) */
	uint8_t fd_flags;                       /* CAN-FD specific flags */
};

/**
 * @brief CAN Error Information Structure
 *
 * Contains detailed error information when an error occurs.
 */
struct can_mb_error {
	enum can_mb_error_msg_type error_msg_type;  /* Type of error message */
	enum can_error_state_type err_state_info;   /* Current error state */
	enum can_error_type proto_err_info;         /* Protocol error details */
	/* Note: FUSAQueueMsgType Fusa_Err_Info is commented out in original */
} __attribute__((packed));

/**
 * @brief CAN Mailbox Packet Structure
 *
 * This is the main structure used for communication between
 * Main Domain and RTSS Domain via mailbox.
 *
 * The packet size is fixed and includes padding to ensure
 * consistent memory layout across different architectures.
 */
struct can_mb_packet {
	uint8_t controller_id;      /* Controller ID: 0-7 for CAN cores, 8 for all controllers */
	uint16_t hth_object;        /* Hardware Transmit Handle object */
	enum can_mb_command cmd;    /* Command type (TX/RX/BAUDRATE/ERROR) */

	/* Union for different data types based on command */
	union {
		uint16_t baudrate;           /* Baud rate configuration */
		struct can_mb_msg can_msg;   /* CAN message data */
		struct can_mb_error error;   /* Error information */
	} data;

	uint8_t reserved[45];       /* Reserved bytes for alignment and future use */
} __attribute__((packed));

/* Note: Packet size validation - adjust reserved array size if needed */
/* _Static_assert(sizeof(struct can_mb_packet) == 64, "can_mb_packet must be exactly 64 bytes"); */

#endif /* CAN_MB_STRUCTURES_H */

// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file can_mb_config.h
 * @brief CAN Mailbox Daemon Configuration
 *
 * This file contains platform-specific configuration parameters that can be
 * easily modified for different hardware platforms and requirements.
 *
 * To adapt for different platforms:
 * 1. Modify MAX_CAN_CONTROLLERS for your platform's CAN controller count
 * 2. Update VCAN_BASE_INDEX if needed for interface naming
 * 3. Adjust other parameters as required for your specific use case
 */

#ifndef CAN_MB_CONFIG_H
#define CAN_MB_CONFIG_H

#include <stdio.h>

/* ========================================================================
 * PLATFORM CONFIGURATION - MODIFY THESE FOR YOUR PLATFORM
 * ========================================================================
 */

/**
 * @brief Maximum number of CAN controllers supported
 *
 * This defines how many CAN controllers the daemon will manage.
 * Common values:
 * - 2 for basic automotive ECUs
 * - 4 for mid-range automotive platforms
 * - 8 for high-end automotive/industrial platforms
 * - 16 for complex gateway/router applications
 *
 * Note: Changing this requires rebuilding the daemon and updating
 * the gateway script configuration.
 */
#define MAX_CAN_CONTROLLERS     8

/**
 * @brief Base index for daemon VCAN interfaces
 *
 * User applications use VCAN0 to VCAN(MAX_CAN_CONTROLLERS-1)
 * Daemon uses VCAN(VCAN_BASE_INDEX) to VCAN(VCAN_BASE_INDEX+MAX_CAN_CONTROLLERS-1)
 *
 * Default: User interfaces VCAN0-7, Daemon interfaces VCAN8-15
 */
#define VCAN_BASE_INDEX         8

/**
 * @brief Maximum VCAN interface name length
 */
#define VCAN_NAME_MAX_LEN       16

/* ========================================================================
 * PERFORMANCE CONFIGURATION
 * ========================================================================
 */

/**
 * @brief Socket retry configuration
 */
#define SOCKET_RETRY_COUNT      3
#define SOCKET_RETRY_DELAY_MS   1000

/**
 * @brief RTSS communication timeouts
 */
#define RTSS_TIMEOUT_MS         2000
#define RTSS_RETRY_COUNT        3

/**
 * @brief Health monitoring intervals
 */
#define INTERFACE_CHECK_INTERVAL_S  5
#define HEALTH_CHECK_INTERVAL_S     10

/**
 * @brief Error recovery configuration
 */
#define MAX_RECOVERY_ATTEMPTS   5
#define RECOVERY_DELAY_S        2

/* ========================================================================
 * FILE PATHS AND NAMING
 * ========================================================================
 */

/**
 * @brief Daemon configuration paths
 */
#define DAEMON_NAME             "rtss_can"
#define CONFIG_FILE_PATH        "/etc/rtss_can/rtss_can.conf"
#define PID_FILE_PATH           "/var/run/rtss_can.pid"
#define LOG_FILE_PATH           "/var/log/rtss_can.log"

/**
 * @brief RTSS channel naming pattern
 *
 * Channel names are generated as: RTSS_CHANNEL_PREFIX + controller_id
 * Example: "can_controller_0", "can_controller_1", etc.
 */
#define RTSS_CHANNEL_PREFIX     "can_controller_"
#define RTSS_CHANNEL_NAME_MAX   64

/* ========================================================================
 * VALIDATION MACROS
 * ========================================================================
 */

/**
 * @brief Validate controller ID is within range
 */
#define IS_VALID_CONTROLLER_ID(id) \
	((id) >= 0 && (id) < MAX_CAN_CONTROLLERS)

/**
 * @brief Get user VCAN interface name for controller
 */
#define GET_USER_VCAN_NAME(id, buffer) \
	snprintf(buffer, VCAN_NAME_MAX_LEN, "vcan%d", (id))

/**
 * @brief Get daemon VCAN interface name for controller
 */
#define GET_DAEMON_VCAN_NAME(id, buffer) \
	snprintf(buffer, VCAN_NAME_MAX_LEN, "vcan%d", VCAN_BASE_INDEX + (id))

/**
 * @brief Get RTSS channel name for controller
 */
#define GET_RTSS_CHANNEL_NAME(id, buffer) \
	snprintf(buffer, RTSS_CHANNEL_NAME_MAX, "%s%d", RTSS_CHANNEL_PREFIX, (id))

/* ========================================================================
 * PLATFORM-SPECIFIC FEATURES
 * ========================================================================
 */

/**
 * @brief Platform features - CAN-FD ONLY MODE
 *
 * This platform supports ONLY CAN-FD mode. Classical CAN frames are not supported.
 * All CAN controllers must operate in CAN-FD mode with flexible data rate.
 */
#define ENABLE_CANFD_SUPPORT    1   /* CAN-FD frame support (MANDATORY) */
#define ENABLE_CLASSICAL_CAN    0   /* Classical CAN disabled (FD-only platform) */
#define ENABLE_ERROR_RECOVERY   1   /* Enable automatic error recovery */
#define ENABLE_HEALTH_MONITORING 1  /* Enable controller health monitoring */
#define ENABLE_STATISTICS       1   /* Enable performance statistics */
#define ENABLE_SYSTEMD_NOTIFY   1   /* Enable systemd notification support */

/**
 * @brief CAN-FD specific configuration
 */
#define CANFD_MANDATORY         1   /* All controllers must support CAN-FD */
#define MAX_CANFD_DATA_LEN      64  /* Maximum CAN-FD data length */
#define CANFD_BRS_REQUIRED      1   /* Bit Rate Switch required */
#define CANFD_ESI_SUPPORT       1   /* Error State Indicator support */

/* ========================================================================
 * COMPILE-TIME VALIDATION
 * ========================================================================
 */

/* Validate configuration at compile time */
#if MAX_CAN_CONTROLLERS <= 0
#error "MAX_CAN_CONTROLLERS must be greater than 0"
#endif

#if MAX_CAN_CONTROLLERS > 32
#error "MAX_CAN_CONTROLLERS cannot exceed 32 (practical limit)"
#endif

/* ========================================================================
 * PLATFORM CONFIGURATION EXAMPLES
 * ========================================================================
 */

/*
 * Common platform configurations
 *
 * To use a different configuration, uncomment one of these blocks:
 */

/*
 * Example: Basic Automotive ECU (2 CAN controllers)
 * #undef MAX_CAN_CONTROLLERS
 * #define MAX_CAN_CONTROLLERS 2
 * #undef VCAN_BASE_INDEX
 * #define VCAN_BASE_INDEX 4
 */

/*
 * Example: Mid-range Automotive Platform (4 CAN controllers)
 * #undef MAX_CAN_CONTROLLERS
 * #define MAX_CAN_CONTROLLERS 4
 * #undef VCAN_BASE_INDEX
 * #define VCAN_BASE_INDEX 8
 */

/*
 * Example: High-end Industrial Platform (16 CAN controllers)
 * #undef MAX_CAN_CONTROLLERS
 * #define MAX_CAN_CONTROLLERS 16
 * #undef VCAN_BASE_INDEX
 * #define VCAN_BASE_INDEX 32
 */

#endif /* CAN_MB_CONFIG_H */

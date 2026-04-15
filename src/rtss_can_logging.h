// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file rtss_can_logging.h
 * @brief Logging System for RTSS CAN Daemon
 *
 * This file provides a comprehensive logging system with multiple
 * log levels, destinations, and beginner-friendly interfaces.
 *
 * Features:
 * - Multiple log levels (DEBUG, INFO, WARN, ERROR, FATAL)
 * - Multiple destinations (console, syslog, file)
 * - Structured logging with timestamps
 * - CAN-specific logging functions
 */

#ifndef RTSS_CAN_LOGGING_H
#define RTSS_CAN_LOGGING_H

#include <stdio.h>
#include <stdarg.h>
#include <linux/can.h>
#include "rtss_can_structures.h"

/**
 * @brief Log Level Enumeration
 *
 * Defines the severity levels for log messages.
 * Lower values indicate higher priority/severity.
 * Note: Using CAN_LOG_ prefix to avoid conflicts with syslog.h
 */
enum log_level {
	CAN_LOG_DEBUG = 0,    /* Detailed debugging information */
	CAN_LOG_INFO,         /* General information messages */
	CAN_LOG_WARN,         /* Warning messages - potential issues */
	CAN_LOG_ERROR,        /* Error messages - recoverable errors */
	CAN_LOG_FATAL         /* Fatal errors - unrecoverable conditions */
};

/**
 * @brief Log Destination Enumeration
 *
 * Specifies where log messages should be written.
 */
enum log_destination {
	LOG_DEST_CONSOLE = 1,   /* Standard output/error */
	LOG_DEST_SYSLOG = 2,    /* System log daemon */
	LOG_DEST_FILE = 4       /* Log file */
};

/**
 * @brief Logging Configuration Structure
 *
 * Contains all configuration parameters for the logging system.
 */
struct log_config {
	enum log_level min_level;       /* Minimum log level to output */
	int destinations;               /* Bitfield of log destinations */
	char log_file_path[256];        /* Path to log file */
	size_t max_file_size;           /* Maximum log file size in bytes */
	int enable_timestamps;          /* Enable/disable timestamps */
	int enable_colors;              /* Enable/disable colored output */
};

/* Global logging configuration - defined in rtss_can_logging.c */

/**
 * @brief Initialize the logging system
 *
 * Sets up the logging system with the specified configuration.
 * Must be called before any other logging functions.
 *
 * @param config Pointer to logging configuration structure
 * @return 0 on success, -1 on error
 */
int log_init(const struct log_config *config);

/**
 * @brief Cleanup the logging system
 *
 * Closes log files and cleans up resources.
 * Should be called before program exit.
 */
void log_cleanup(void);

/**
 * @brief Main logging function
 *
 * Logs a message with the specified level and component.
 * This is the core logging function used by all other log functions.
 *
 * @param level Log level (DEBUG, INFO, WARN, ERROR, FATAL)
 * @param component Component name (e.g., "DAEMON", "SOCKET", "MAILBOX")
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 */
void log_message(enum log_level level, const char *component, const char *format, ...);

/**
 * @brief Log a CAN frame with detailed information
 *
 * Logs a SocketCAN frame in a human-readable format.
 * Useful for debugging CAN communication.
 *
 * @param level Log level
 * @param frame Pointer to CAN frame structure
 * @param direction Direction string ("TX", "RX", "FORWARD")
 * @param controller_id CAN controller ID (0-7)
 */
void log_can_frame(enum log_level level, const struct can_frame *frame,
		   const char *direction, int controller_id);

/**
 * @brief Log a CAN FD frame with detailed information
 *
 * Logs a SocketCAN FD frame in a human-readable format.
 *
 * @param level Log level
 * @param frame Pointer to CAN FD frame structure
 * @param direction Direction string ("TX", "RX", "FORWARD")
 * @param controller_id CAN controller ID (0-7)
 */
void log_canfd_frame(enum log_level level, const struct canfd_frame *frame,
		     const char *direction, int controller_id);

/**
 * @brief Log a mailbox packet with detailed information
 *
 * Logs a CAN mailbox packet structure in a readable format.
 * Useful for debugging mailbox communication.
 *
 * @param level Log level
 * @param packet Pointer to mailbox packet structure
 * @param operation Operation string ("SEND", "RECV", "PROCESS")
 * @param vcan_name VCAN interface name (e.g., "vcan8")
 */
void log_mailbox_packet(enum log_level level, const struct can_mb_packet *packet,
			const char *operation, const char *vcan_name);

/**
 * @brief Log system error with errno information
 *
 * Logs a system error message along with the current errno value
 * and its string description.
 *
 * @param level Log level
 * @param component Component name
 * @param operation Operation that failed
 */
void log_system_error(enum log_level level, const char *component,
		      const char *operation);

/**
 * @brief Log network interface information
 *
 * Logs information about a network interface (e.g., VCAN interface).
 *
 * @param level Log level
 * @param interface_name Interface name (e.g., "vcan8")
 * @param operation Operation performed ("CREATE", "DELETE", "CONFIG")
 * @param status Status of operation ("SUCCESS", "FAILED")
 */
void log_interface_info(enum log_level level, const char *interface_name,
			const char *operation, const char *status);

/**
 * @brief Set the minimum log level at runtime
 *
 * Changes the minimum log level without restarting the daemon.
 * Useful for debugging.
 *
 * @param level New minimum log level
 */
void log_set_level(enum log_level level);

/**
 * @brief Get the current minimum log level
 *
 * @return Current minimum log level
 */
enum log_level log_get_level(void);

/**
 * @brief Convert log level to string
 *
 * @param level Log level
 * @return String representation of log level
 */
const char *log_level_to_string(enum log_level level);

/**
 * @brief Convert string to log level
 *
 * @param level_str String representation of log level
 * @return Log level, or LOG_INFO if string is invalid
 */
enum log_level log_string_to_level(const char *level_str);

/* Convenience macros for common logging operations */

/**
 * @brief Debug logging macro
 *
 * Logs a debug message. Only compiled in debug builds.
 */
#ifdef DEBUG
#define LOG_DEBUG_MSG(component, format, ...) \
	log_message(CAN_LOG_DEBUG, component, format, ##__VA_ARGS__)
#else
#define LOG_DEBUG_MSG(component, format, ...) do {} while (0)
#endif

/**
 * @brief Info logging macro
 */
#define LOG_INFO_MSG(component, format, ...) \
	log_message(CAN_LOG_INFO, component, format, ##__VA_ARGS__)

/**
 * @brief Warning logging macro
 */
#define LOG_WARN_MSG(component, format, ...) \
	log_message(CAN_LOG_WARN, component, format, ##__VA_ARGS__)

/**
 * @brief Error logging macro
 */
#define LOG_ERROR_MSG(component, format, ...) \
	log_message(CAN_LOG_ERROR, component, format, ##__VA_ARGS__)

/**
 * @brief Fatal error logging macro
 */
#define LOG_FATAL_MSG(component, format, ...) \
	log_message(CAN_LOG_FATAL, component, format, ##__VA_ARGS__)

/**
 * @brief System error logging macro
 *
 * Logs a system error with errno information.
 */
#define LOG_SYS_ERROR(component, operation) \
	log_system_error(CAN_LOG_ERROR, component, operation)

#endif /* RTSS_CAN_LOGGING_H */

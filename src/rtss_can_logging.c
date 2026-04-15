// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

/*
 * @file rtss_can_logging.c
 * @brief RTSS CAN Logging Implementation
 *
 * This file implements the logging functionality for the RTSS CAN daemon.
 * It provides various logging levels and output destinations.
 */

#include "rtss_can_logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <strings.h>

/* Global logging configuration */
static struct log_config g_log_config = {
	.min_level = CAN_LOG_INFO,
	.destinations = LOG_DEST_SYSLOG,
	.enable_timestamps = 1,
	.enable_colors = 0,
	.log_file_path = "/var/log/rtss_can.log",
	.max_file_size = 10 * 1024 * 1024 /* 10MB */
};

/* Additional fields not in the config structure */
static int g_log_file_fd = -1;

/* Color codes for console output */
static const char *log_colors[] = {
	[CAN_LOG_DEBUG] = "\033[36m",   /* Cyan */
	[CAN_LOG_INFO]  = "\033[32m",   /* Green */
	[CAN_LOG_WARN]  = "\033[33m",   /* Yellow */
	[CAN_LOG_ERROR] = "\033[31m",   /* Red */
	[CAN_LOG_FATAL] = "\033[35m"    /* Magenta */
};

static const char *log_reset = "\033[0m";

/* Log level names */
static const char *log_level_names[] = {
	[CAN_LOG_DEBUG] = "DEBUG",
	[CAN_LOG_INFO]  = "INFO",
	[CAN_LOG_WARN]  = "WARN",
	[CAN_LOG_ERROR] = "ERROR",
	[CAN_LOG_FATAL] = "FATAL"
};

/* Syslog priority mapping */
static const int syslog_priorities[] = {
	[CAN_LOG_DEBUG] = LOG_DEBUG,
	[CAN_LOG_INFO]  = LOG_INFO,
	[CAN_LOG_WARN]  = LOG_WARNING,
	[CAN_LOG_ERROR] = LOG_ERR,
	[CAN_LOG_FATAL] = LOG_CRIT
};

/*
 * @brief Initialize logging system
 */
int log_init(const struct log_config *config)
{
	if (config)
		memcpy(&g_log_config, config, sizeof(struct log_config));

	/* Open syslog if needed */
	if (g_log_config.destinations & LOG_DEST_SYSLOG)
		openlog("rtss_can", LOG_PID | LOG_CONS, LOG_DAEMON);

	/* Open log file if needed */
	if (g_log_config.destinations & LOG_DEST_FILE) {
		g_log_file_fd = open(g_log_config.log_file_path,
				     O_WRONLY | O_CREAT | O_APPEND, 0644);
		if (g_log_file_fd < 0) {
			fprintf(stderr, "Failed to open log file %s: %s\n",
				g_log_config.log_file_path, strerror(errno));
			return -1;
		}
	}

	return 0;
}

/*
 * @brief Cleanup logging system
 */
void log_cleanup(void)
{
	if (g_log_config.destinations & LOG_DEST_SYSLOG)
		closelog();

	if (g_log_file_fd >= 0) {
		close(g_log_file_fd);
		g_log_file_fd = -1;
	}
}

/*
 * @brief Get current timestamp string
 */
static void get_timestamp(char *buffer, size_t size)
{
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);

	strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/*
 * @brief Core logging function
 */
void log_message(enum log_level level, const char *component,
		 const char *format, ...)
{
	va_list args;
	char message[1024];
	char timestamp[32];

	if (level < g_log_config.min_level)
		return;

	/* Format the message */
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	/* Get timestamp if needed */
	if (g_log_config.enable_timestamps)
		get_timestamp(timestamp, sizeof(timestamp));

	/* Log to console */
	if (g_log_config.destinations & LOG_DEST_CONSOLE) {
		const char *color = g_log_config.enable_colors ? log_colors[level] : "";
		const char *reset = g_log_config.enable_colors ? log_reset : "";

		if (g_log_config.enable_timestamps) {
			fprintf(stderr, "%s[%s] %s %-5s %s: %s%s\n",
				color, timestamp, "rtss_can", log_level_names[level],
				component, message, reset);
		} else {
			fprintf(stderr, "%s%s %-5s %s: %s%s\n",
				color, "rtss_can", log_level_names[level],
				component, message, reset);
		}
	}

	/* Log to syslog */
	if (g_log_config.destinations & LOG_DEST_SYSLOG)
		syslog(syslog_priorities[level], "%s: %s", component, message);

	/* Log to file */
	if ((g_log_config.destinations & LOG_DEST_FILE) && g_log_file_fd >= 0) {
		char file_message[1200];

		if (g_log_config.enable_timestamps) {
			snprintf(file_message, sizeof(file_message),
				 "[%s] rtss_can %-5s %s: %s\n",
				 timestamp, log_level_names[level], component, message);
		} else {
			snprintf(file_message, sizeof(file_message),
				 "rtss_can %-5s %s: %s\n",
				 log_level_names[level], component, message);
		}

		ssize_t bytes_written = write(g_log_file_fd, file_message, strlen(file_message));

		(void)bytes_written; /* Suppress unused variable warning */
	}
}

/*
 * @brief Log CAN frame information
 */
void log_can_frame(enum log_level level, const struct can_frame *frame,
		   const char *direction, int controller_id)
{
	char data_str[32] = "";
	int i;

	if (level < g_log_config.min_level || !frame)
		return;

	for (i = 0; i < frame->can_dlc && i < 8; i++) {
		char byte_str[4];

		snprintf(byte_str, sizeof(byte_str), "%02X ", frame->data[i]);
		strlcat(data_str, byte_str,sizeof(data_str));
	}

	log_message(level, "CAN_FRAME",
		    "%s[%d] ID:0x%03X DLC:%d Data:[%s]",
		    direction, controller_id, frame->can_id & CAN_EFF_MASK,
		    frame->can_dlc, data_str);
}

/*
 * @brief Log CAN-FD frame information
 */
void log_canfd_frame(enum log_level level, const struct canfd_frame *frame,
		     const char *direction, int controller_id)
{
	char data_str[256] = "";
	int i;

	if (level < g_log_config.min_level || !frame)
		return;

	for (i = 0; i < frame->len && i < 64; i++) {
		char byte_str[4];

		snprintf(byte_str, sizeof(byte_str), "%02X ", frame->data[i]);
		strlcat(data_str, byte_str,sizeof(data_str));
	}

	log_message(level, "CANFD_FRAME",
		    "%s[%d] ID:0x%03X LEN:%d BRS:%d ESI:%d Data:[%s]",
		    direction, controller_id, frame->can_id & CAN_EFF_MASK,
		    frame->len, (frame->flags & CANFD_BRS) ? 1 : 0,
		    (frame->flags & CANFD_ESI) ? 1 : 0, data_str);
}

/*
 * @brief Log mailbox packet information
 */
void log_mailbox_packet(enum log_level level, const struct can_mb_packet *packet,
			const char *direction, const char *interface)
{
	if (level < g_log_config.min_level || !packet)
		return;

	log_message(level, "MB_PACKET",
		    "%s[%s] Ctrl:%d Cmd:%d HTH:%d ID:0x%03X Len:%d",
		    direction, interface, packet->controller_id, packet->cmd,
		    packet->hth_object, packet->data.can_msg.mid,
		    packet->data.can_msg.len);
}

/*
 * @brief Log interface information
 */
void log_interface_info(enum log_level level, const char *interface,
			const char *operation, const char *status)
{
	if (level < g_log_config.min_level)
		return;

	log_message(level, "INTERFACE", "%s %s: %s", interface, operation, status);
}

/*
 * @brief Log system error with errno information
 */
void log_system_error(enum log_level level, const char *component,
		      const char *operation)
{
	int err = errno;

	if (level < g_log_config.min_level)
		return;

	log_message(level, component, "%s: %s (errno=%d)",
		    operation, strerror(err), err);
}

/*
 * @brief Set the minimum log level at runtime
 */
void log_set_level(enum log_level level)
{
	if (level >= CAN_LOG_DEBUG && level <= CAN_LOG_FATAL)
		g_log_config.min_level = level;
}

/*
 * @brief Get the current minimum log level
 */
enum log_level log_get_level(void)
{
	return g_log_config.min_level;
}

/*
 * @brief Convert log level to string
 */
const char *log_level_to_string(enum log_level level)
{
	if (level >= CAN_LOG_DEBUG && level <= CAN_LOG_FATAL)
		return log_level_names[level];
	return "UNKNOWN";
}

/*
 * @brief Convert string to log level
 */
enum log_level log_string_to_level(const char *level_str)
{
	int i;

	if (!level_str)
		return CAN_LOG_INFO;

	for (i = CAN_LOG_DEBUG; i <= CAN_LOG_FATAL; i++) {
		if (strcasecmp(level_str, log_level_names[i]) == 0)
			return (enum log_level)i;
	}
	return CAN_LOG_INFO; /* Default fallback */
}

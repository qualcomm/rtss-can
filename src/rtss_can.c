// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

/*
 * @file rtss_can.c
 * @brief RTSS CAN Daemon - Main Implementation
 *
 * This daemon manages CAN communication between SocketCAN interfaces (VCAN8-VCAN15)
 * and RTSS mailbox. It creates virtual CAN interfaces, listens for CAN frames,
 * converts them to mailbox packets, and communicates with RTSS subsystem.
 *
 * Architecture:
 * SocketCAN Apps → VCAN0-7 → CAN Gateway → VCAN8-15 → RTSS CAN Daemon → RTSS Mailbox
 *
 * Features:
 * - Creates and manages VCAN8-VCAN15 interfaces
 * - Converts SocketCAN frames to RTSS mailbox packets
 * - Comprehensive error recovery and logging
 * - Beginner-friendly code with extensive comments
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/select.h>
#include <time.h>
#include <getopt.h>
#include <syslog.h>

#include "rtss_can_config.h"
#include "rtss_can_structures.h"
#include "rtss_can_logging.h"
#include "rtss_can_baud_config.h"
#include "rtss_mb_wrapper.h"
#include <pthread.h>

/* Constants and Configuration */
#define MAX_FRAME_SIZE          sizeof(struct canfd_frame)

/* Retry and timeout configuration */
#define SOCKET_RETRY_COUNT      3
#define SOCKET_RETRY_DELAY_MS   1000
#define RTSS_TIMEOUT_MS         2000
#define INTERFACE_CHECK_INTERVAL_S  5
#define MAX_RECOVERY_ATTEMPTS   5

/*
 * @brief CAN Controller State
 *
 * Tracks the state and resources for each CAN controller.
 */
struct can_controller_state {
	int controller_id;                      /* Controller ID (0-7) */
	int vcan_base_index;                    /* VCAN base index for daemon interfaces */
	char vcan_name[16];                     /* VCAN interface name (e.g., "vcan8") */
	int socket_fd;                          /* SocketCAN socket file descriptor */
	int is_enabled;                         /* Controller enabled flag */
	enum can_baud_config current_baud_config;  /* Current baud rate configuration */
	time_t last_error_time;                 /* Last error timestamp */
	int error_count;                        /* Consecutive error count */
	int recovery_attempts;                  /* Number of recovery attempts */
};

/*
 * @brief Daemon Global State
 *
 * Contains all global state information for the daemon.
 */
struct daemon_state {
	struct can_controller_state controllers[MAX_CAN_CONTROLLERS]; /* Controller states */
	int active_controllers;                /* Number of active controllers */
	int running;                            /* Daemon running flag */
	int debug_mode;                         /* Debug mode flag */
	struct log_config log_config;          /* Logging configuration */
	fd_set read_fds;                       /* File descriptor set for select() */
	int max_fd;                            /* Maximum file descriptor */
	time_t start_time;                     /* Daemon start time */
	int total_frames_processed;            /* Statistics counter */
	int total_errors;                      /* Error counter */
};

/* Global daemon state */
static struct daemon_state g_daemon_state;

/* Global RTSS mailbox handles - shared by all controllers */
static struct rtss_mb_handle *pTxHandle;
static struct rtss_mb_handle *pRxHandle;

/* RX thread for receiving data from RTSS */
static pthread_t RxThreadHandle;
static volatile int bRxThreadRunning;

/* Function prototypes */
static int initialize_daemon(void);
static int setup_can_gateway(void);
static int cleanup_can_gateway(void);
static int create_user_vcan_interfaces(int idx);
static int setup_gateway_rules(void);
static int setup_socket_listener(int controller_id);
static int initialize_rtss_connection(int controller_id);
static int process_incoming_can_frame(int controller_id);
static int convert_socketcan_to_mailbox_packet(const struct canfd_frame *frame,
						struct can_mb_packet *packet,
						int controller_id, ssize_t nbytes);
static int ConvertMailboxPacketToSocketcan(const struct can_mb_packet *pPacket,
					   struct canfd_frame *pFrame,
					   int nControllerId);
static int send_packet_to_rtss(const struct can_mb_packet *packet, int controller_id);
static int configure_can_baudrate(int controller_id, enum can_baud_config baud_config);
static void cleanup_daemon(void);
static void signal_handler(int sig);
static void print_usage(const char *program_name);
static int parse_command_line(int argc, char *argv[]);
static int load_configuration(const char *config_file);
static void *RxThreadHandler(void *pArg);

/*
 * @brief Main daemon entry point
 *
 * Initializes the daemon, sets up signal handlers, creates VCAN interfaces,
 * and enters the main event loop to process CAN frames.
 *
 * @param argc Command line argument count
 * @param argv Command line arguments
 * @return Exit status (0 for success, non-zero for error)
 */
int main(int argc, char *argv[])
{
	int ret;
	int i;

	/* Initialize daemon state */
	memset(&g_daemon_state, 0, sizeof(g_daemon_state));
	g_daemon_state.running = 1;
	g_daemon_state.start_time = time(NULL);

	/* Parse command line arguments */
	if (parse_command_line(argc, argv) != 0)
		return EXIT_FAILURE;

	/* Initialize logging system */
	if (log_init(&g_daemon_state.log_config) != 0) {
		fprintf(stderr, "Failed to initialize logging system\n");
		return EXIT_FAILURE;
	}

	LOG_INFO_MSG("DAEMON", "Starting %s daemon", DAEMON_NAME);
	LOG_INFO_MSG("DAEMON", "Daemon Log file path %s ",
		     g_daemon_state.log_config.log_file_path);

	/* Load configuration file for all active controllers */
	if (load_configuration(CONFIG_FILE_PATH) != 0) {
		fprintf(stderr, "Failed to load config file, exiting");
		return EXIT_FAILURE;
	}

	/* Set up signal handlers for graceful shutdown */
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGHUP, signal_handler);

	/* Initialize daemon components */
	ret = initialize_daemon();
	if (ret != 0) {
		LOG_FATAL_MSG("DAEMON", "Failed to initialize daemon: %d", ret);
		cleanup_daemon();
		return EXIT_FAILURE;
	}

	LOG_INFO_MSG("DAEMON", "Daemon initialized successfully, entering main loop");

	/* Main event loop */
	while (g_daemon_state.running) {
		struct timeval timeout;
		fd_set read_fds_copy;
		int select_ret;

		/* Copy file descriptor set for select() */
		read_fds_copy = g_daemon_state.read_fds;

		/* Set timeout for select() - allows periodic health checks */
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		/* Wait for activity on any socket */
		select_ret = select(g_daemon_state.max_fd + 1, &read_fds_copy, NULL, NULL, &timeout);

		if (select_ret < 0) {
			if (errno == EINTR)
				/* Interrupted by signal, continue */
				continue;
			LOG_SYS_ERROR("DAEMON", "select() failed");
			break;
		}

		if (select_ret == 0)
			/* Timeout - continue waiting */
			continue;

		/* Process activity on each controller socket */
		for (i = 0; i < MAX_CAN_CONTROLLERS; i++) {
			struct can_controller_state *ctrl = &g_daemon_state.controllers[i];

			if (ctrl->is_enabled && ctrl->socket_fd >= 0 &&
			    FD_ISSET(ctrl->socket_fd, &read_fds_copy)) {
				/* Process incoming CAN frame */
				ret = process_incoming_can_frame(i);
				if (ret != 0) {
					LOG_ERROR_MSG("CONTROLLER", "Failed to process frame on controller %d: %d", i, ret);
					ctrl->error_count++;
				} else {
					/* Reset error count on successful processing */
					ctrl->error_count = 0;
					g_daemon_state.total_frames_processed++;
				}
			}
		}
	}

	LOG_INFO_MSG("DAEMON", "Shutting down daemon gracefully");
	cleanup_daemon();
	return EXIT_SUCCESS;
}

/*
 * @brief Setup CAN gateway
 *
 * Sets up the complete CAN gateway including user interfaces (VCAN0-7),
 * daemon interfaces (VCAN8-15), and bidirectional gateway rules.
 *
 * @return 0 on success, -1 on error
 */
static int setup_can_gateway(void)
{
	int ret;
	int i;
	int vcan_base_idx = VCAN_BASE_INDEX;
	const char *modules[] = {"can", "can-raw", "can-gw", "vcan"};

	LOG_INFO_MSG("GATEWAY", "Setting up CAN gateway");

	/* Load required kernel modules */
	LOG_INFO_MSG("GATEWAY", "Loading required kernel modules");

	for (i = 0; i < 4; i++) {
		char command[128];

		snprintf(command, sizeof(command), "modprobe %s 2>/dev/null", modules[i]);
		if (system(command) != 0) {
			LOG_WARN_MSG("GATEWAY", "Failed to load module %s (may already be loaded)", modules[i]);
			return -1;
		}
	}

	/* Create user VCAN interfaces (VCAN0-N) */
	ret = create_user_vcan_interfaces(0);
	if (ret != 0) {
		LOG_ERROR_MSG("GATEWAY", "Failed to create user VCAN interfaces");
		return -1;
	}

	/* Create daemon VCAN interfaces using vcan_base_index for enabled controllers.
	 * All controllers share the same vcan_base_index.
	 */
	for (i = 0; i < MAX_CAN_CONTROLLERS; i++) {
		if (g_daemon_state.controllers[i].is_enabled) {
			vcan_base_idx = g_daemon_state.controllers[i].vcan_base_index;
			break;
		}
	}
	ret = create_user_vcan_interfaces(vcan_base_idx);
	if (ret != 0) {
		LOG_ERROR_MSG("GATEWAY", "Failed to create daemon VCAN interface");
		return -1;
	}

	/* Setup gateway rules (now both user and daemon vcan nodes exist) */
	ret = setup_gateway_rules();
	if (ret != 0) {
		LOG_ERROR_MSG("GATEWAY", "Failed to setup gateway rules");
		return -1;
	}

	LOG_INFO_MSG("GATEWAY", "CAN gateway setup completed successfully");
	return 0;
}

/*
 * @brief Create user VCAN interfaces (VCAN0-7 or VCAN8-15)
 *
 * Creates the VCAN interfaces for enabled controllers only.
 * For user interfaces (idx=0): creates vcan0-7 based on is_enabled flag
 * For daemon interfaces (idx=8): creates vcan8-15 based on is_enabled flag
 *
 * @param idx Base index (0 for user interfaces, VCAN_BASE_INDEX for daemon interfaces)
 * @return 0 on success, -1 on error
 */
static int create_user_vcan_interfaces(int idx)
{
	char command[256];
	int ret;
	int i;
	int created_count = 0;

	LOG_INFO_MSG("GATEWAY", "Create VCAN interfaces for enabled controllers (base index: %d)",
		     idx);

	for (i = 0; i < MAX_CAN_CONTROLLERS; i++) {
		char interface_name[16];
		int if_index = idx + i;

		/* Skip disabled controllers */
		if (!g_daemon_state.controllers[i].is_enabled) {
			LOG_DEBUG_MSG("GATEWAY", "Skipping, controller %d disabled", i);
			continue;
		}

		snprintf(interface_name, sizeof(interface_name), "vcan%d", if_index);

		/* Check if interface already exists */
		snprintf(command, sizeof(command), "ip link show %s >/dev/null 2>&1", interface_name);
		ret = system(command);

		if (ret == 0) {
			LOG_INFO_MSG("GATEWAY", "Interface %s already exists", interface_name);
		} else {
			/* Create the interface */
			snprintf(command, sizeof(command), "ip link add dev %s type vcan", interface_name);
			ret = system(command);

			if (ret != 0) {
				LOG_ERROR_MSG("GATEWAY", "Failed to create interface %s",
					      interface_name);
				return -1;
			}

			LOG_INFO_MSG("GATEWAY", "Created interface %s", interface_name);
		}

		/* Bring the interface up */
		snprintf(command, sizeof(command), "ip link set dev %s up", interface_name);
		ret = system(command);

		if (ret != 0) {
			LOG_ERROR_MSG("GATEWAY", "Failed to bring up interface %s", interface_name);
			return -1;
		}

		created_count++;
	}

	LOG_INFO_MSG("GATEWAY", "Created %d VCAN interface(s) at base index %d",
		     created_count, idx);
	return 0;
}

/*
 * @brief Setup gateway rules
 *
 * Sets up bidirectional gateway rules between user interfaces (VCAN0-7)
 * and daemon interfaces (VCAN8-15) for enabled controllers only.
 *
 * @return 0 on success, -1 on error
 */
static int setup_gateway_rules(void)
{
	char command[256];
	int ret;
	int i;
	int rules_created = 0;

	LOG_INFO_MSG("GATEWAY", "Setting up bidirectional gateway rules for enabled controllers");

	/* Clear any existing gateway rules */
	(void)system("cangw -F >/dev/null 2>&1");

	/* Setup bidirectional rules for each enabled controller */
	for (i = 0; i < MAX_CAN_CONTROLLERS; i++) {
		char user_if[16], daemon_if[16];
		struct can_controller_state *ctrl = &g_daemon_state.controllers[i];

		/* Skip disabled controllers */
		if (!ctrl->is_enabled) {
			LOG_DEBUG_MSG("GATEWAY", "Skip gateway rules for disabled controller %d",
				      i);
			continue;
		}

		snprintf(user_if, sizeof(user_if), "vcan%d", i);
		snprintf(daemon_if, sizeof(daemon_if), "vcan%d", ctrl->vcan_base_index + i);

		/* Rule 1: User interface to daemon interface (TX path) */
		snprintf(command, sizeof(command), "cangw -A -s %s -d %s -e -X", user_if, daemon_if);
		ret = system(command);
		if (ret != 0) {
			LOG_ERROR_MSG("GATEWAY", "Failed to add gateway rule: %s -> %s", user_if, daemon_if);
			return ret;
		}

		/* Rule 2: Daemon interface to user interface (RX path) */
		snprintf(command, sizeof(command), "cangw -A -s %s -d %s -e -X", daemon_if, user_if);
		ret = system(command);
		if (ret != 0) {
			LOG_ERROR_MSG("GATEWAY", "Failed to add gateway rule: %s -> %s",
				      daemon_if, user_if);
			return ret;
		}

		LOG_INFO_MSG("GATEWAY", "Setup bidirectional gateway: %s <-> %s (controller %d)",
			     user_if, daemon_if, i);
		rules_created++;
	}

	LOG_INFO_MSG("GATEWAY", "Created %d bidirectional gateway rule(s)", rules_created);
	return 0;
}

/*
 * @brief Cleanup CAN gateway
 *
 * Removes gateway rules and removes VCAN interfaces for enabled controllers only.
 *
 * @return 0 on success, -1 on error
 */
static int cleanup_can_gateway(void)
{
	char command[256];
	int i;
	int cleaned_count = 0;

	LOG_INFO_MSG("GATEWAY", "Cleaning up CAN gateway");

	/* Remove all gateway rules */
	LOG_INFO_MSG("GATEWAY", "Removing gateway rules");
	(void)system("cangw -F >/dev/null 2>&1");

	/* Bring down and remove interfaces for enabled controllers only */
	LOG_INFO_MSG("GATEWAY", "Bringing down VCAN interfaces for enabled controllers");

	for (i = 0; i < MAX_CAN_CONTROLLERS; i++) {
		struct can_controller_state *ctrl = &g_daemon_state.controllers[i];

		/* Skip disabled controllers */
		if (!ctrl->is_enabled) {
			LOG_DEBUG_MSG("GATEWAY", "Skip cleanup for controller %d (disabled)", i);
			continue;
		}

		/* Bring down user interface (vcan0-7) */
		snprintf(command, sizeof(command), "ip link set down vcan%d 2>/dev/null", i);
		(void)system(command);
		snprintf(command, sizeof(command), "ip link delete vcan%d 2>/dev/null", i);
		(void)system(command);

		/* Bring down daemon interface (vcan8-15) */
		snprintf(command, sizeof(command), "ip link set down vcan%d 2>/dev/null",
			 ctrl->vcan_base_index + i);
		(void)system(command);
		snprintf(command, sizeof(command), "ip link delete vcan%d 2>/dev/null",
			 ctrl->vcan_base_index + i);
		(void)system(command);

		LOG_DEBUG_MSG("GATEWAY", "Cleaned up interfaces for controller vcan%d, vcan%d",
			      i, i, ctrl->vcan_base_index + i);
		cleaned_count++;
	}

	LOG_INFO_MSG("GATEWAY", "CAN gateway cleanup completed (%d controller(s) cleaned)",
		     cleaned_count);
	return 0;
}

/*
 * @brief Initialize the daemon
 *
 * Sets up all daemon components including CAN gateway, VCAN interfaces, sockets,
 * and RTSS connections for each enabled controller.
 *
 * @return 0 on success, -1 on error
 */
static int initialize_daemon(void)
{
	int ret;
	int i;

	LOG_INFO_MSG("INIT", "Initializing daemon components");

	/* Check if any controllers are active */
	if (g_daemon_state.active_controllers == 0) {
		LOG_FATAL_MSG("INIT", "No active controllers configured (active_controllers=0)");
		LOG_FATAL_MSG("INIT", "Daemon requires at least 1 active controller to start");
		return -1;
	}

	/* Setup CAN gateway first */
	ret = setup_can_gateway();
	if (ret != 0) {
		LOG_ERROR_MSG("INIT", "Failed to setup CAN gateway");
		return -1;
	}

	/* Initialize file descriptor set */
	FD_ZERO(&g_daemon_state.read_fds);
	g_daemon_state.max_fd = -1;

	/* Open shared RTSS TX mailbox */
	LOG_INFO_MSG("INIT", "Initializing shared RTSS TX mailbox");
	ret = rtss_mb_init_shared_tx(&pTxHandle);
	if (ret != 0) {
		LOG_ERROR_MSG("INIT", "Failed to init shared RTSS TX mailbox: %d", ret);
		return -1;
	}
	LOG_INFO_MSG("INIT", "Shared RTSS TX mailbox init successfully");

	/* Initialize shared RTSS RX mailbox */
	LOG_INFO_MSG("INIT", "Init shared RTSS RX mailbox");
	ret = rtss_mb_init_shared_rx(&pRxHandle);
	if (ret != 0) {
		LOG_ERROR_MSG("INIT", "Failed to init shared RTSS RX mailbox: %d", ret);
		rtss_mb_close_shared(&pTxHandle);
		return -1;
	}
	LOG_INFO_MSG("INIT", "Shared RTSS RX mailbox init successfully");

	/* Start RX thread */
	LOG_INFO_MSG("INIT", "Starting RX thread for RTSS mailbox");
	bRxThreadRunning = 1;
	ret = pthread_create(&RxThreadHandle, NULL, RxThreadHandler, NULL);
	if (ret != 0) {
		LOG_ERROR_MSG("INIT", "Failed to create RX thread: %d", ret);
		rtss_mb_close_shared(&pTxHandle);
		rtss_mb_close_shared(&pRxHandle);
		return -1;
	}
	LOG_INFO_MSG("INIT", "RX thread started successfully");

	/* Initialize each CAN controller */
	for (i = 0; i < MAX_CAN_CONTROLLERS; i++) {
		struct can_controller_state *ctrl = &g_daemon_state.controllers[i];

		if (!ctrl->is_enabled) {
			LOG_INFO_MSG("INIT", "Controller %d disabled, skipping", i);
			continue;
		}
		ctrl->controller_id = i;
		snprintf(ctrl->vcan_name, sizeof(ctrl->vcan_name), "vcan%d",
			 ctrl->vcan_base_index + i);
		ctrl->socket_fd = -1;
		ctrl->current_baud_config = DEFAULT_CAN_BAUD_CONFIG;
		ctrl->error_count = 0;
		ctrl->recovery_attempts = 0;

		LOG_INFO_MSG("INIT", "Initializing controller %d (%s)", i, ctrl->vcan_name);
		/* Note: VCAN interfaces (vcan8-15) are already created by setup_can_gateway() */
		/* Set up socket listener */
		ret = setup_socket_listener(i);
		if (ret != 0) {
			LOG_ERROR_MSG("INIT", "Failed to setup socket for controller %d", i);
			return -1;
		}

		/* Initialize RTSS connection (now a no-op) */
		ret = initialize_rtss_connection(i);
		if (ret != 0) {
			LOG_ERROR_MSG("INIT", "Failed to initialize RTSS connection for controller %d", i);
			return -1;
		}

		/* Configure default baud rate */
		ret = configure_can_baudrate(i, ctrl->current_baud_config);
		if (ret != 0) {
			LOG_WARN_MSG("INIT", "Failed to configure baud rate for controller %d", i);
			/* Continue anyway - default baud rate is set anyway */
		}

		LOG_INFO_MSG("INIT", "Controller %d initialized successfully", i);
	}

	LOG_INFO_MSG("INIT", "All controllers initialized");
	return 0;
}

/*
 * @brief Set up socket listener for a controller (CAN-FD ONLY MODE)
 *
 * Creates a SocketCAN socket and binds it to the VCAN interface.
 * Configures the socket for CAN-FD ONLY operation. Classical CAN frames
 * are rejected. All frames must be CAN-FD format.
 *
 * @param controller_id Controller ID (0-7)
 * @return 0 on success, -1 on error
 */
static int setup_socket_listener(int controller_id)
{
	struct can_controller_state *ctrl = &g_daemon_state.controllers[controller_id];
	struct sockaddr_can addr;
	struct ifreq ifr;
	int canfd_on = 1;
	int ret;

	LOG_DEBUG_MSG("SOCKET", "Setting up CAN-FD ONLY socket for %s", ctrl->vcan_name);

	/* Create CAN socket */
	ctrl->socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (ctrl->socket_fd < 0) {
		LOG_SYS_ERROR("SOCKET", "Failed to create CAN socket");
		return -1;
	}

	/* Enable CAN FD support - MANDATORY for this platform */
	ret = setsockopt(ctrl->socket_fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
			 &canfd_on, sizeof(canfd_on));
	if (ret < 0) {
		LOG_SYS_ERROR("SOCKET", "Failed to enable CAN-FD (MANDATORY for this platform)");
		close(ctrl->socket_fd);
		ctrl->socket_fd = -1;
		return -1;
	}

	LOG_INFO_MSG("SOCKET", "CAN-FD mode enabled for %s (classical CAN disabled)", ctrl->vcan_name);

	/* Get interface index */
	strlcpy(ifr.ifr_name, ctrl->vcan_name, sizeof(ifr.ifr_name));
	ret = ioctl(ctrl->socket_fd, SIOCGIFINDEX, &ifr);
	if (ret < 0) {
		LOG_SYS_ERROR("SOCKET", "Failed to get interface index");
		close(ctrl->socket_fd);
		ctrl->socket_fd = -1;
		return -1;
	}

	/* Bind socket to interface */
	memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	ret = bind(ctrl->socket_fd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		LOG_SYS_ERROR("SOCKET", "Failed to bind socket");
		close(ctrl->socket_fd);
		ctrl->socket_fd = -1;
		return -1;
	}

	/* Add socket to file descriptor set */
	FD_SET(ctrl->socket_fd, &g_daemon_state.read_fds);
	if (ctrl->socket_fd > g_daemon_state.max_fd)
		g_daemon_state.max_fd = ctrl->socket_fd;

	LOG_INFO_MSG("SOCKET", "Socket setup complete for %s (fd=%d)",
		     ctrl->vcan_name, ctrl->socket_fd);
	return 0;
}

/*
 * @brief Initialize RTSS mailbox connection
 *
 * This function is now a no-op since we use shared RTSS clients.
 * Shared clients are initialized once in initialize_daemon().
 *
 * @param controller_id Controller ID (0-7)
 * @return 0 on success, -1 on error
 */
static int initialize_rtss_connection(int controller_id)
{
	/* No-op: Shared clients initialized in initialize_daemon() */
	LOG_DEBUG_MSG("RTSS", "Using shared RTSS clients for controller %d", controller_id);
	return 0;
}

/*
 * @brief Process incoming CAN frame
 *
 * Reads a CAN frame from the socket, converts it to a mailbox packet,
 * and sends it to RTSS. This is the main data path for TX frames.
 *
 * @param controller_id Controller ID (0-7)
 * @return 0 on success, -1 on error
 */
static int process_incoming_can_frame(int controller_id)
{
	struct can_controller_state *ctrl = &g_daemon_state.controllers[controller_id];
	struct canfd_frame frame;
	struct can_mb_packet packet;
	ssize_t nbytes;
	int ret;

	/* Read CAN frame from socket */
	nbytes = read(ctrl->socket_fd, &frame, sizeof(frame));
	if (nbytes < 0) {
		LOG_SYS_ERROR("SOCKET", "Failed to read CAN frame");
		return -1;
	}

	if (nbytes < (ssize_t)sizeof(struct can_frame)) {
		LOG_ERROR_MSG("SOCKET", "Received incomplete CAN frame (%zd bytes)", nbytes);
		return -1;
	}

	/* Log the received frame */
	if (nbytes == sizeof(struct canfd_frame))
		log_canfd_frame(CAN_LOG_DEBUG, &frame, "RX-FD", controller_id);
	else
		log_can_frame(CAN_LOG_DEBUG, (struct can_frame *)&frame, "RX", controller_id);

	/* Convert SocketCAN frame to mailbox packet */
	ret = convert_socketcan_to_mailbox_packet(&frame, &packet, controller_id, nbytes);
	if (ret != 0) {
		LOG_ERROR_MSG("CONVERT", "Failed to convert frame to packet: %d", ret);
		return -1;
	}

	/* Send packet to RTSS */
	ret = send_packet_to_rtss(&packet, controller_id);
	if (ret != 0) {
		LOG_ERROR_MSG("RTSS", "Failed to send packet to RTSS: %d", ret);
		return -1;
	}

	LOG_DEBUG_MSG("PROCESS", "Successfully processed frame on controller %d", controller_id);
	return 0;
}

/*
 * @brief Convert SocketCAN CAN-FD frame to mailbox packet (FD-ONLY MODE)
 *
 * Converts a SocketCAN CAN-FD frame to the RTSS mailbox packet format.
 * This platform supports ONLY CAN-FD frames. Classical CAN frames are rejected.
 * All frames must have CAN-FD format with flexible data rate capabilities.
 *
 * @param frame Pointer to SocketCAN CAN-FD frame
 * @param packet Pointer to output mailbox packet
 * @param controller_id Controller ID (0-7)
 * @return 0 on success, -1 on error
 */
static int convert_socketcan_to_mailbox_packet(const struct canfd_frame *frame,
						struct can_mb_packet *packet,
						int controller_id, ssize_t nbytes)
{
	bool is_canfd;

	/* Clear the packet structure */
	memset(packet, 0, sizeof(*packet));

	/* Check if this is a CAN-FD frame or classical CAN frame */
	is_canfd = (nbytes == sizeof(struct canfd_frame)) && (frame->flags & CANFD_BRS);

	if (!is_canfd) {
		/* For now, accept classical CAN frames but log a warning */
		LOG_WARN_MSG("CONVERT", "Classical CAN frame received on CAN-FD platform (controller %d, flags 0x%x) - converting",
			     controller_id, frame->flags);
		/* Treat as classical CAN frame with FD flags set to 0 */
	} else {
		LOG_DEBUG_MSG("CONVERT", "CAN-FD frame received (controller %d)", controller_id);
	}

	/* Set controller ID and command */
	packet->controller_id = controller_id;
	packet->cmd = CAN_MAILBOX_TX;
	/* Hardware Object ID mapping:
	 * Controller 0->0, 1->5, 2->10, 3->15,
	 * 4->20, 5->25, 6->30, 7->35
	 */
	packet->hth_object = controller_id * 5;

	/* Convert CAN ID */
	packet->data.can_msg.mid = frame->can_id & CAN_EFF_MASK;
	packet->data.can_msg.ext_id = (frame->can_id & CAN_EFF_FLAG) ? true : false;

	/* Convert CAN-FD specific flags */
	packet->data.can_msg.brs = (frame->flags & CANFD_BRS) ? true : false;
	packet->data.can_msg.esi = (frame->flags & CANFD_ESI) ? true : false;
	packet->data.can_msg.fd_flags = frame->flags;

	/* Convert data length (CAN-FD supports up to 64 bytes) */
	if (frame->len > CAN_MAILBOX_MAX_DATA_LEN) {
		LOG_ERROR_MSG("CONVERT", "CAN-FD frame data length too large: %d (max %d)",
			      frame->len, CAN_MAILBOX_MAX_DATA_LEN);
		return -1;
	}
	packet->data.can_msg.len = frame->len;

	/* Copy data */
	memcpy(packet->data.can_msg.data, frame->data, frame->len);

	/* Log the CAN-FD conversion */
	LOG_DEBUG_MSG("CONVERT", "CAN-FD frame converted: ID=0x%X, len=%d, BRS=%d, ESI=%d",
		      packet->data.can_msg.mid, packet->data.can_msg.len,
		      packet->data.can_msg.brs, packet->data.can_msg.esi);

	log_mailbox_packet(CAN_LOG_DEBUG, packet, "CONVERT_FD",
			   g_daemon_state.controllers[controller_id].vcan_name);

	return 0;
}

/*
 * @brief Send packet to RTSS mailbox
 *
 * Sends a mailbox packet to RTSS using the RTSS UMD API.
 * This is a placeholder implementation that calls the RTSS API.
 *
 * @param packet Pointer to mailbox packet
 * @param controller_id Controller ID (0-7)
 * @return 0 on success, -1 on error
 */
static int send_packet_to_rtss(const struct can_mb_packet *packet, int controller_id)
{
	int ret;

	/* Validate shared TX handle */
	if (pTxHandle == NULL) {
		LOG_ERROR_MSG("RTSS", "Shared RTSS TX handle not initialized");
		return -1;
	}

	/* Send packet using shared TX handle via rtss_mb_wrapper */
	ret = rtss_mb_write_shared(pTxHandle, (void *)packet, sizeof(*packet));
	if (ret < 0) {
		LOG_ERROR_MSG("RTSS", "Failed to write to RTSS mailbox: %d", ret);
		return -1;
	}

	/* Log successful packet transmission to RTSS */
	LOG_DEBUG_MSG("RTSS", "Sent packet to RTSS for controller %d", controller_id);
	LOG_INFO_MSG("RTSS", "RTSS write completed successfully for controller %d", controller_id);

	return 0;
}

/*
 * @brief Configure CAN baud rate
 *
 * Sends a baud rate configuration command to RTSS.
 *
 * @param controller_id Controller ID (0-7)
 * @param baud_config Baud rate configuration
 * @return 0 on success, -1 on error
 */
static int configure_can_baudrate(int controller_id, enum can_baud_config baud_config)
{
	struct can_controller_state *ctrl = &g_daemon_state.controllers[controller_id];
	struct can_mb_packet packet;
	int ret;

	if (!is_valid_baud_config(baud_config)) {
		LOG_ERROR_MSG("BAUD", "Invalid baud configuration: %d", baud_config);
		return -1;
	}

	/* Create baud rate configuration packet */
	memset(&packet, 0, sizeof(packet));
	packet.controller_id = controller_id;
	packet.cmd = CAN_MAILBOX_SET_BAUDRATE;
	packet.data.baudrate = baud_config;

	/* Send configuration to RTSS */
	ret = send_packet_to_rtss(&packet, controller_id);
	if (ret != 0) {
		LOG_ERROR_MSG("BAUD", "Failed to send baud config to RTSS: %d", ret);
		return -1;
	}

	ctrl->current_baud_config = baud_config;
	LOG_INFO_MSG("BAUD", "Configured controller %d baud rate: %s",
		     controller_id, get_baud_config_description(baud_config));
	return 0;
}

/*
 * @brief Convert mailbox packet to SocketCAN frame
 *
 * Converts a RTSS mailbox packet to SocketCAN CAN-FD frame format.
 *
 * @param pPacket Pointer to mailbox packet
 * @param pFrame Pointer to output SocketCAN frame
 * @param nControllerId Controller ID (0-7)
 * @return 0 on success, -1 on error
 */
static int ConvertMailboxPacketToSocketcan(const struct can_mb_packet *pPacket,
					   struct canfd_frame *pFrame,
					   int nControllerId)
{
	/* Clear the frame structure */
	memset(pFrame, 0, sizeof(*pFrame));

	/* Convert CAN ID */
	pFrame->can_id = pPacket->data.can_msg.mid;
	if (pPacket->data.can_msg.ext_id)
		pFrame->can_id |= CAN_EFF_FLAG;

	/* Convert CAN-FD specific flags */
	pFrame->flags = 0;
	if (pPacket->data.can_msg.brs)
		pFrame->flags |= CANFD_BRS;

	if (pPacket->data.can_msg.esi)
		pFrame->flags |= CANFD_ESI;

	/* Convert data length */
	if (pPacket->data.can_msg.len > CAN_MAILBOX_MAX_DATA_LEN) {
		LOG_ERROR_MSG("CONVERT", "Mailbox packet data length too large: %d (max %d)",
			      pPacket->data.can_msg.len, CAN_MAILBOX_MAX_DATA_LEN);
		return -1;
	}
	pFrame->len = pPacket->data.can_msg.len;

	/* Copy data */
	memcpy(pFrame->data, pPacket->data.can_msg.data, pPacket->data.can_msg.len);

	LOG_DEBUG_MSG("CONVERT", "Mailbox packet converted to CAN-FD frame: ID=0x%X, len=%d, BRS=%d, ESI=%d",
		      pFrame->can_id & CAN_EFF_MASK, pFrame->len,
		      (pFrame->flags & CANFD_BRS) ? 1 : 0,
		      (pFrame->flags & CANFD_ESI) ? 1 : 0);

	return 0;
}

/*
 * @brief RX thread handler
 *
 * Continuously reads packets from the shared RTSS RX mailbox channel and
 * forwards them to the appropriate SocketCAN interface based on controller_id.
 * This implements the RX path: RTSS → Daemon → SocketCAN.
 *
 * Uses a timeout-based read to allow graceful shutdown when Ctrl+C is pressed.
 *
 * @param pArg Thread argument
 * @return NULL
 */
static void *RxThreadHandler(void *pArg)
{
	struct can_mb_packet packet;
	struct canfd_frame frame;
	int ret;
	int controller_id;
	struct can_controller_state *ctrl;
	ssize_t nbytes;

	(void)pArg;

	LOG_INFO_MSG("RX_THREAD", "RX thread started, listening on shared RTSS RX channel");

	for (;;) {
		/* Blocking read; bRxThreadRunning checked after each return */
		ret = rtss_mb_read_shared(pRxHandle, &packet, sizeof(packet));

		if (!bRxThreadRunning)
			break;

		if (ret < 0) {
			LOG_ERROR_MSG("RX_THREAD", "Failed to read from RTSS RX mailbox: %d", ret);
			usleep(100000);
			continue;
		}

		/* Validate controller ID */
		controller_id = packet.controller_id;
		if (controller_id < 0 || controller_id >= MAX_CAN_CONTROLLERS) {
			LOG_ERROR_MSG("RX_THREAD", "Invalid controller ID in RX packet: %d", controller_id);
			continue;
		}

		ctrl = &g_daemon_state.controllers[controller_id];

		/* Check if controller is enabled */
		if (!ctrl->is_enabled) {
			LOG_WARN_MSG("RX_THREAD", "Received packet for disabled controller %d", controller_id);
			continue;
		}

		/* Log the received packet */
		log_mailbox_packet(CAN_LOG_DEBUG, &packet, "RX_FROM_RTSS", ctrl->vcan_name);

		/* Handle different packet command types */
		switch (packet.cmd) {
		case CAN_MAILBOX_SD_READY:
			LOG_INFO_MSG("RX_THREAD", "RTSS MB ready signal received for controller %d", controller_id);
			continue;  /* Don't forward control packets to SocketCAN */

		case CAN_MAILBOX_ERROR:
			LOG_ERROR_MSG("RX_THREAD", "Error packet received from RTSS for controller %d", controller_id);
			/* Could add detailed error logging here if needed */
			continue;  /* Don't forward error packets to SocketCAN */

		case CAN_MAILBOX_SET_BAUDRATE:
			LOG_INFO_MSG("RX_THREAD", "Baudrate config received from RTSS for controller %d: %d",
				     controller_id, packet.data.baudrate);
			continue;  /* Don't forward config packets to SocketCAN */

		case CAN_MAILBOX_TX:
			LOG_WARN_MSG("RX_THREAD", "Unexpected TX packet in RX path for controller %d", controller_id);
			continue;  /* TX packets shouldn't come from RTSS in RX path */

		case CAN_MAILBOX_RX:
			/* This is actual CAN data - proceed with forwarding */
			break;

		default:
			LOG_WARN_MSG("RX_THREAD", "Unknown command type %d from RTSS for controller %d",
				     packet.cmd, controller_id);
			continue;
		}

		/* Convert mailbox packet to SocketCAN frame */
		ret = ConvertMailboxPacketToSocketcan(&packet, &frame, controller_id);
		if (ret != 0) {
			LOG_ERROR_MSG("RX_THREAD", "Failed to convert mailbox packet to SocketCAN frame: %d", ret);
			continue;
		}

		/* Send frame to SocketCAN interface */
		nbytes = write(ctrl->socket_fd, &frame, sizeof(frame));
		if (nbytes < 0) {
			LOG_SYS_ERROR("RX_THREAD", "Failed to write CAN frame to socket");
			continue;
		}

		/* Log successful transmission */
		log_canfd_frame(CAN_LOG_DEBUG, &frame, "TX_TO_SOCKETCAN", controller_id);
		LOG_DEBUG_MSG("RX_THREAD", "Successfully forwarded packet from RTSS to controller %d", controller_id);
	}

	LOG_INFO_MSG("RX_THREAD", "RX thread exiting");
	return NULL;
}

/*
 * @brief Signal handler for graceful shutdown
 *
 * Handles SIGTERM, SIGINT, and SIGHUP signals.
 *
 * @param sig Signal number
 */
static void signal_handler(int sig)
{
	int i;
	int ret;

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		LOG_INFO_MSG("SIGNAL", "Received shutdown signal (%d)", sig);
		g_daemon_state.running = 0;
		break;
	case SIGHUP:
		LOG_INFO_MSG("SIGNAL", "Received SIGHUP, reloading configuration");
		/* Reload configuration file */
		ret = load_configuration(CONFIG_FILE_PATH);
		if (ret != 0)
			LOG_ERROR_MSG("SIGNAL", "cfg file parsing error, keep current settings");
		else {
			for (i = 0; i < MAX_CAN_CONTROLLERS; i++) {
				struct can_controller_state *ctrl = &g_daemon_state.controllers[i];
				if (ctrl->is_enabled) {
					if (configure_can_baudrate(i, ctrl->current_baud_config) == 0) {
						LOG_INFO_MSG("SIGNAL", "Reconfigured controller %d baud rate", i);
					} else {
						LOG_WARN_MSG("SIGNAL", "Failed to reconfigure controller %d baud rate", i);
					}
				}
			}
		}
		break;
	default:
		LOG_WARN_MSG("SIGNAL", "Received unexpected signal: %d", sig);
		break;
	}
}

/*
 * @brief Clean up daemon resources
 *
 * Closes sockets, RTSS connections, cleans up CAN gateway, and cleans up all resources.
 */
static void cleanup_daemon(void)
{
	int i;

	LOG_INFO_MSG("CLEANUP", "Cleaning up daemon resources");

	/* Stop RX thread:
	 * 1. Clear the flag so the thread exits cleanly after its current read.
	 * 2. pthread_cancel() interrupts the blocking rtss_mb_read() if the
	 *    library uses a POSIX cancellation point (e.g. read/poll syscall).
	 *    It is a no-op backstop for mailbox implementations that do not.
	 */
	if (RxThreadHandle) {
		LOG_INFO_MSG("CLEANUP", "Stopping RX thread");
		bRxThreadRunning = 0;
		pthread_cancel(RxThreadHandle);
		pthread_join(RxThreadHandle, NULL);
		RxThreadHandle = 0;
		LOG_INFO_MSG("CLEANUP", "RX thread stopped");
	}

	/* Close shared RTSS mailbox handles */
	if (pTxHandle != NULL) {
		LOG_INFO_MSG("CLEANUP", "Closing shared RTSS TX handle");
		rtss_mb_close_shared(&pTxHandle);
	}

	if (pRxHandle != NULL) {
		LOG_INFO_MSG("CLEANUP", "Closing shared RTSS RX handle");
		rtss_mb_close_shared(&pRxHandle);
	}

	/* Close controller sockets */
	for (i = 0; i < MAX_CAN_CONTROLLERS; i++) {
		struct can_controller_state *ctrl = &g_daemon_state.controllers[i];

		if (ctrl->socket_fd >= 0) {
			close(ctrl->socket_fd);
			ctrl->socket_fd = -1;
		}
	}

	/* Clean up CAN gateway */
	cleanup_can_gateway();

	LOG_INFO_MSG("CLEANUP", "Daemon cleanup complete");
	log_cleanup();
}

/*
 * @brief Parse command line arguments
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, -1 on error
 */
static int parse_command_line(int argc, char *argv[])
{
	int opt;
	static struct option long_options[] = {
		{"debug", no_argument, 0, 'd'},
		{"config", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	/* Set default logging configuration */
	g_daemon_state.log_config.min_level = CAN_LOG_INFO;
	g_daemon_state.log_config.destinations = LOG_DEST_SYSLOG;
	g_daemon_state.log_config.enable_timestamps = 1;
	g_daemon_state.log_config.enable_colors = 0;
	strlcpy(g_daemon_state.log_config.log_file_path, "/var/log/can_mb_daemon.log", sizeof(g_daemon_state.log_config.log_file_path));
	g_daemon_state.log_config.max_file_size = 10 * 1024 * 1024; /* 10MB */

	while ((opt = getopt_long(argc, argv, "dc:h", long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			g_daemon_state.debug_mode = 1;
			g_daemon_state.log_config.min_level = CAN_LOG_DEBUG;
			g_daemon_state.log_config.destinations |= LOG_DEST_CONSOLE;
			g_daemon_state.log_config.enable_colors = 1;
			break;
		case 'c':
			/* Config file will be loaded later */
			break;
		case 'h':
			print_usage(argv[0]);
			exit(EXIT_SUCCESS);
		default:
			print_usage(argv[0]);
			return -1;
		}
	}

	return 0;
}

/*
 * @brief Print usage information
 *
 * @param program_name Program name
 */
static void print_usage(const char *program_name)
{
	printf("Usage: %s [OPTIONS]\n", program_name);
	printf("CAN Mailbox Daemon - Bridges SocketCAN and RTSS mailbox.\n\n");
	printf("Options:\n");
	printf("  -d, --debug          Enable debug mode\n");
	printf("  -c, --config FILE    Configuration file path\n");
	printf("  -h, --help           Show this help message\n");
	printf("\nExample:\n");
	printf("  %s --debug\n", program_name);
	printf("  %s --config /etc/can_mb_daemon/custom.conf\n", program_name);
}

/*
 * @brief Load configuration from file
 *
 * Parses the configuration file and loads per-controller baud rate settings.
 * Simple INI-style parser that reads controller_X_baudrate=Y entries.
 *
 * @param config_file Configuration file path
 * @return 0 on success, -1 on error (non-fatal - uses defaults)
 */
static int load_configuration(const char *config_file)
{
	FILE *fp;
	char line[256];
	int controller_id;
	int baud_config;
	int configs_loaded = 0;
	int line_number = 0;
	int parse_errors = 0;
	int i;
	int active_controllers = 0;

	LOG_INFO_MSG("CONFIG", "Loading configuration from %s", config_file);

	fp = fopen(config_file, "r");
	if (!fp) {
		LOG_ERROR_MSG("CONFIG", "Config file %s open failed", config_file);
		/* Show default baud rates for all controllers */
		for (i = 0; i < MAX_CAN_CONTROLLERS; i++) {
			LOG_INFO_MSG("CONFIG", "Controller %d: using existing baud config = %d (%s)",
				      i,
				      g_daemon_state.controllers[i].current_baud_config,
				      get_baud_config_description(g_daemon_state.controllers[i].current_baud_config));
		}
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		line_number++;
		/* Skip comments and empty lines */
		if (line[0] == '#' || line[0] == '\n' || line[0] == '[')
			continue;

		/* Parse: active_controllers=N */
		if (sscanf(line, "active_controllers=%d", &active_controllers) == 1) {
			if (active_controllers < 0 || active_controllers > MAX_CAN_CONTROLLERS) {
				LOG_ERROR_MSG("CONFIG", "Invalid active_controllers=%d",
					      active_controllers, MAX_CAN_CONTROLLERS);
				parse_errors++;

			} else {
				LOG_INFO_MSG("CONFIG", "Active controllers set to %d",
					     active_controllers);
			}
			continue;
		}

		/* Parse: controller_X_baudrate=Y */
		int scan_result = sscanf(line, "controller_%d_baudrate=%d",
					 &controller_id, &baud_config);

		if (scan_result == 2) {
			/* Successfully parsed - validate controller ID */
			if (controller_id < 0 || controller_id >= MAX_CAN_CONTROLLERS) {
				LOG_ERROR_MSG("CONFIG", "Line %d: Invalid controller ID %d (valid: 0-%d), skipping",
					    line_number, controller_id, MAX_CAN_CONTROLLERS - 1);
				parse_errors++;
				continue;
			}
			/* Validate baud configuration */
			if (!is_valid_baud_config(baud_config)) {
				LOG_ERROR_MSG("CONFIG", "Line %d: Invalid baud config %d for controller %d (valid: 0-8), skipping",
					    line_number, baud_config, controller_id);
				parse_errors++;
				continue;
			}
			/* Both valid - apply configuration */
			g_daemon_state.controllers[controller_id].current_baud_config = baud_config;
			LOG_INFO_MSG("CONFIG", "Controller %d: baud config = %d (%s)",
				    controller_id, baud_config,
				    get_baud_config_description(baud_config));
			configs_loaded++;
		} else {
			/* sscanf failed - check if it's a malformed controller line */
			char *trimmed = line;
			while (*trimmed == ' ' || *trimmed == '\t')
				trimmed++;

			if (*trimmed != '\0' && *trimmed != '\n' &&
			    strstr(trimmed, "controller_") != NULL) {
				LOG_ERROR_MSG("CONFIG", "Line %d: Malformed configuration line, skipping: %.50s",
					    line_number, trimmed);
				parse_errors++;
			}
		}
	}

	fclose(fp);

	if (parse_errors != 0) {
		LOG_INFO_MSG("CONFIG", "Configuration file parsing errors occurred with %d errors",
			     parse_errors);
		return -EINVAL;
	}

	/* Store active_controllers in daemon state */
	g_daemon_state.active_controllers = active_controllers;

	/* Calculate VCAN base index based on active_controllers */
	int vcan_base_index;

	if (active_controllers > 0 && active_controllers <= MAX_CAN_CONTROLLERS) {
		vcan_base_index = active_controllers;
		LOG_INFO_MSG("CONFIG", "VCAN base index: %d active controllers: %d",
			     vcan_base_index, active_controllers);
	} else {
		/* Use default if active_controllers wasn't set or is invalid */
		vcan_base_index = VCAN_BASE_INDEX;
		LOG_INFO_MSG("CONFIG", "Using default VCAN base index %d", vcan_base_index);
	}

	/* Enable controllers up to active_controllers, disable the rest, and set vcan_base_index */
	for (i = 0; i < active_controllers && i < MAX_CAN_CONTROLLERS; i++) {
		g_daemon_state.controllers[i].is_enabled = 1;
		g_daemon_state.controllers[i].vcan_base_index = vcan_base_index;
	}
	for (i = active_controllers; i < MAX_CAN_CONTROLLERS; i++) {
		g_daemon_state.controllers[i].is_enabled = 0;
		LOG_INFO_MSG("CONFIG", "Controller %d disabled (beyond active_controllers=%d)",
			     i, active_controllers);
	}

	LOG_INFO_MSG("CONFIG", "Active controllers: %d, Disabled controllers: %d",
		     active_controllers, MAX_CAN_CONTROLLERS - active_controllers);

	return 0;
}

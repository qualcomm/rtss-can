// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file can_baud_config.h
 * @brief CAN Baud Rate Configuration Mappings
 *
 * This file contains the baud rate configuration mappings and helper functions
 * for converting between SocketCAN baud rates and RTSS mailbox baud rate indices.
 *
 * Based on the baud rate configurations from can_baud_configs.c and the
 * limited set of supported baud rates from design_requirements.txt
 */

#ifndef CAN_BAUD_CONFIG_H
#define CAN_BAUD_CONFIG_H

#include <stdint.h>

/* Helper macro to get array size */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/**
 * @brief Supported CAN Baud Rate Configurations
 *
 * These are the predefined baud rate combinations supported by the RTSS CAN controllers.
 * Each configuration specifies both nominal (arbitration) and data (payload) bit rates.
 */
enum can_baud_config {
	CAN_500KBPS_CANFD_500KBPS_WITH_BRS_DISABLE = 0,    /* 500k nominal, 500k data, BRS off */
	CAN_250KBPS_CANFD_5MBPS_WITH_BRS_ENABLE = 1,       /* 250k nominal, 5M data, BRS on */
	CAN_125KBPS_CANFD_250KBPS_WITH_BRS_DISABLE = 2,    /* 125k nominal, 250k data, BRS off */
	CAN_1MBPS_CANFD_1MBPS_WITH_BRS_DISABLE = 3,        /* 1M nominal, 1M data, BRS off */
	CAN_1MBPS_CANFD_2MBPS_WITH_BRS_ENABLE = 4,         /* 1M nominal, 2M data, BRS on */
	CAN_1MBPS_CANFD_5MBPS_WITH_BRS_ENABLE = 5,         /* 1M nominal, 5M data, BRS on */
	CAN_1MBPS_CANFD_8MBPS_WITH_BRS_ENABLE = 6,         /* 1M nominal, 8M data, BRS on */
	CAN_1MBPS_CANFD_10MBPS_WITH_BRS_ENABLE = 7,        /* 1M nominal, 10M data, BRS on */
	CAN_1MBPS_CANFD_12MBPS_WITH_BRS_ENABLE = 8,        /* 1M nominal, 12M data, BRS on */
	CAN_BAUD_CONFIG_MAX                                 /* Maximum number of configurations */
};

/**
 * @brief CAN Baud Rate Information Structure
 *
 * Contains detailed information about a baud rate configuration.
 */
struct can_baud_info {
	enum can_baud_config config_id;        /* Configuration ID */
	uint32_t nominal_bitrate;              /* Nominal (arbitration) bit rate in bps */
	uint32_t data_bitrate;                 /* Data (payload) bit rate in bps */
	int brs_enable;                        /* Bit Rate Switch enable flag */
	const char *description;               /* Human-readable description */
};

/**
 * @brief Baud Rate Configuration Table
 *
 * Maps configuration IDs to detailed baud rate information.
 * This table is used for validation and conversion between different
 * baud rate representations.
 */
static const struct can_baud_info can_baud_table[] = {
	{
		.config_id = CAN_500KBPS_CANFD_500KBPS_WITH_BRS_DISABLE,
		.nominal_bitrate = 500000,
		.data_bitrate = 500000,
		.brs_enable = 0,
		.description = "500 kbps nominal, 500 kbps data, BRS disabled"
	},
	{
		.config_id = CAN_250KBPS_CANFD_5MBPS_WITH_BRS_ENABLE,
		.nominal_bitrate = 250000,
		.data_bitrate = 5000000,
		.brs_enable = 1,
		.description = "250 kbps nominal, 5 Mbps data, BRS enabled"
	},
	{
		.config_id = CAN_125KBPS_CANFD_250KBPS_WITH_BRS_DISABLE,
		.nominal_bitrate = 125000,
		.data_bitrate = 125000,  /* Note: Description says 250k but config shows 125k */
		.brs_enable = 0,
		.description = "125 kbps nominal, 125 kbps data, BRS disabled"
	},
	{
		.config_id = CAN_1MBPS_CANFD_1MBPS_WITH_BRS_DISABLE,
		.nominal_bitrate = 1000000,
		.data_bitrate = 1000000,
		.brs_enable = 0,
		.description = "1 Mbps nominal, 1 Mbps data, BRS disabled"
	},
	{
		.config_id = CAN_1MBPS_CANFD_2MBPS_WITH_BRS_ENABLE,
		.nominal_bitrate = 1000000,
		.data_bitrate = 2000000,
		.brs_enable = 1,
		.description = "1 Mbps nominal, 2 Mbps data, BRS enabled"
	},
	{
		.config_id = CAN_1MBPS_CANFD_5MBPS_WITH_BRS_ENABLE,
		.nominal_bitrate = 1000000,
		.data_bitrate = 5000000,
		.brs_enable = 1,
		.description = "1 Mbps nominal, 5 Mbps data, BRS enabled"
	},
	{
		.config_id = CAN_1MBPS_CANFD_8MBPS_WITH_BRS_ENABLE,
		.nominal_bitrate = 1000000,
		.data_bitrate = 8000000,
		.brs_enable = 1,
		.description = "1 Mbps nominal, 8 Mbps data, BRS enabled"
	},
	{
		.config_id = CAN_1MBPS_CANFD_10MBPS_WITH_BRS_ENABLE,
		.nominal_bitrate = 1000000,
		.data_bitrate = 10000000,
		.brs_enable = 1,
		.description = "1 Mbps nominal, 10 Mbps data, BRS enabled"
	},
	{
		.config_id = CAN_1MBPS_CANFD_12MBPS_WITH_BRS_ENABLE,
		.nominal_bitrate = 1000000,
		.data_bitrate = 11428000,  /* Actual value from config: 11428 kbps */
		.brs_enable = 1,
		.description = "1 Mbps nominal, 11.428 Mbps data, BRS enabled"
	}
};

/* Number of supported baud rate configurations */
#define CAN_BAUD_TABLE_SIZE ARRAY_SIZE(can_baud_table)

/**
 * @brief Find baud rate configuration by nominal bit rate
 *
 * Searches for a baud rate configuration that matches the specified
 * nominal bit rate. If multiple configurations have the same nominal
 * bit rate, returns the first match.
 *
 * @param nominal_bitrate Nominal bit rate in bits per second
 * @return Pointer to baud rate info, or NULL if not found
 */
static inline const struct can_baud_info *find_baud_config_by_nominal(uint32_t nominal_bitrate)
{
	size_t i;

	for (i = 0; i < CAN_BAUD_TABLE_SIZE; i++) {
		if (can_baud_table[i].nominal_bitrate == nominal_bitrate)
			return &can_baud_table[i];
	}
	return NULL;
}

/**
 * @brief Find baud rate configuration by configuration ID
 *
 * @param config_id Configuration ID
 * @return Pointer to baud rate info, or NULL if invalid ID
 */
static inline const struct can_baud_info *find_baud_config_by_id(enum can_baud_config config_id)
{
	if (config_id >= 0 && config_id < CAN_BAUD_TABLE_SIZE)
		return &can_baud_table[config_id];
	return NULL;
}

/**
 * @brief Validate baud rate configuration ID
 *
 * @param config_id Configuration ID to validate
 * @return 1 if valid, 0 if invalid
 */
static inline int is_valid_baud_config(enum can_baud_config config_id)
{
	return (config_id >= 0 && config_id < CAN_BAUD_CONFIG_MAX);
}

/**
 * @brief Convert SocketCAN bitrate to RTSS baud configuration
 *
 * Converts a SocketCAN bitrate (from ip link set canX type can bitrate Y)
 * to the corresponding RTSS baud rate configuration ID.
 *
 * @param socketcan_bitrate SocketCAN bitrate in bits per second
 * @return Configuration ID, or -1 if not supported
 */
static inline int socketcan_bitrate_to_rtss_config(uint32_t socketcan_bitrate)
{
	const struct can_baud_info *info = find_baud_config_by_nominal(socketcan_bitrate);

	return info ? (int)info->config_id : -1;
}

/**
 * @brief Convert RTSS baud configuration to SocketCAN bitrate
 *
 * @param config_id RTSS baud rate configuration ID
 * @return SocketCAN bitrate in bits per second, or 0 if invalid
 */
static inline uint32_t rtss_config_to_socketcan_bitrate(enum can_baud_config config_id)
{
	const struct can_baud_info *info = find_baud_config_by_id(config_id);

	return info ? info->nominal_bitrate : 0;
}

/**
 * @brief Get baud rate description string
 *
 * @param config_id Configuration ID
 * @return Description string, or "Unknown" if invalid
 */
static inline const char *get_baud_config_description(enum can_baud_config config_id)
{
	const struct can_baud_info *info = find_baud_config_by_id(config_id);

	return info ? info->description : "Unknown baud rate configuration";
}

/* Default baud rate configuration */
#define DEFAULT_CAN_BAUD_CONFIG     CAN_500KBPS_CANFD_500KBPS_WITH_BRS_DISABLE

/* Common SocketCAN bitrates for quick reference */
#define SOCKETCAN_BITRATE_125K      125000
#define SOCKETCAN_BITRATE_250K      250000
#define SOCKETCAN_BITRATE_500K      500000
#define SOCKETCAN_BITRATE_1M        1000000

#endif /* CAN_BAUD_CONFIG_H */

#pragma once

//==================================================================================================
// Command Strings & Prefixes
//==================================================================================================
// --- General Commands ---
#define CMD_STR_DISCOVER "DISCOVER_GANTRY"
#define CMD_STR_SET_PEER_IP "SET_PEER_IP "
#define CMD_STR_CLEAR_PEER_IP "CLEAR_PEER_IP"
#define CMD_STR_ABORT "ABORT"

// --- Gantry Motion Commands ---
#define CMD_STR_MOVE_X "MOVE_X "
#define CMD_STR_MOVE_Y "MOVE_Y "
#define CMD_STR_MOVE_Z "MOVE_Z "
#define CMD_STR_HOME_X "HOME_X"
#define CMD_STR_HOME_Y "HOME_Y"
#define CMD_STR_HOME_Z "HOME_Z"
#define CMD_STR_ENABLE_X "ENABLE_X"
#define CMD_STR_DISABLE_X "DISABLE_X"
#define CMD_STR_ENABLE_Y "ENABLE_Y"
#define CMD_STR_DISABLE_Y "DISABLE_Y"
#define CMD_STR_ENABLE_Z "ENABLE_Z"
#define CMD_STR_DISABLE_Z "DISABLE_Z"

// --- Telemetry & Status Prefixes ---
#define TELEM_PREFIX "GANTRY_TELEM: "
#define STATUS_PREFIX_INFO "GANTRY_INFO: "
#define STATUS_PREFIX_START "GANTRY_START: "
#define STATUS_PREFIX_DONE "GANTRY_DONE: "
#define STATUS_PREFIX_ERROR "GANTRY_ERROR: "
#define STATUS_PREFIX_DISCOVERY "DISCOVERY: "

// Maps all command strings to a single enum for easier parsing.
typedef enum {
	CMD_UNKNOWN,
	CMD_DISCOVER,
	CMD_SET_PEER_IP,
	CMD_CLEAR_PEER_IP,
	CMD_ABORT,
	CMD_MOVE_X,
	CMD_MOVE_Y,
	CMD_MOVE_Z,
	CMD_HOME_X,
	CMD_HOME_Y,
	CMD_HOME_Z,
	CMD_ENABLE_X,
	CMD_DISABLE_X,
	CMD_ENABLE_Y,
	CMD_DISABLE_Y,
	CMD_ENABLE_Z,
	CMD_DISABLE_Z
} Command;

#pragma once

// --- Network Configuration ---
#define LOCAL_PORT 8888
#define MAX_PACKET_LENGTH 128

// --- Motor & Motion Constants ---
#define MotorX  ConnectorM0
#define MotorY1 ConnectorM1
#define MotorY2 ConnectorM2
#define MotorZ  ConnectorM3

#define MAX_VEL 10000 // sps
#define MAX_ACC 100000 // sps^2

// --- Command Definitions ---
#define CMD_STR_REQUEST_TELEM     "REQUEST_TELEM"
#define CMD_STR_DISCOVER          "DISCOVER_FILLHEAD"
#define CMD_STR_SET_PEER_IP       "SET_PEER_IP "
#define CMD_STR_CLEAR_PEER_IP     "CLEAR_PEER_IP"
#define CMD_STR_ABORT             "ABORT"
#define CMD_STR_MOVE_X            "MOVE_X "
#define CMD_STR_MOVE_Y            "MOVE_Y "
#define CMD_STR_MOVE_Z            "MOVE_Z "
#define CMD_STR_HOME_X            "HOME_X "
#define CMD_STR_HOME_Y            "HOME_Y "
#define CMD_STR_HOME_Z            "HOME_Z "

#define CMD_STR_ENABLE_X          "ENABLE_X"
#define CMD_STR_DISABLE_X         "DISABLE_X"
#define CMD_STR_ENABLE_Y          "ENABLE_Y"
#define CMD_STR_DISABLE_Y         "DISABLE_Y"
#define CMD_STR_ENABLE_Z          "ENABLE_Z"
#define CMD_STR_DISABLE_Z         "DISABLE_Z"

// --- Telemetry & Status Prefixes ---
#define TELEM_PREFIX_GUI          "FH_TELEM_GUI: "
#define STATUS_PREFIX_INFO        "INFO: "
#define STATUS_PREFIX_DONE        "DONE: "
#define STATUS_PREFIX_ERROR       "ERROR: "
#define STATUS_PREFIX_DISCOVERY   "DISCOVERY: "

// --- Physics & Control ---
#define EWMA_ALPHA 0.2f
#define STEPS_PER_MM_X 400.0f
#define STEPS_PER_MM_Y 400.0f
#define STEPS_PER_MM_Z 400.0f

// --- Type Definitions ---
typedef enum {
	STATE_STANDBY,
	STATE_STARTING_MOVE,
	STATE_MOVING,
	STATE_HOMING
} AxisState;

// CORRECTED: Removed the duplicate and outdated HomingPhase enum.
// The single source of truth is now in axis.h.
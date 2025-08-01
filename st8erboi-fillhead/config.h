#pragma once

// --- Network Configuration ---
#define LOCAL_PORT 8888
#define MAX_PACKET_LENGTH 128

// --- Motor & Motion Constants ---
#define MotorX  ConnectorM0
#define MotorY1 ConnectorM1
#define MotorY2 ConnectorM2
#define MotorZ  ConnectorM3

#define MAX_VEL 20000 // sps
#define MAX_ACC 200000 // sps^2
#define MAX_TRQ 90 // %

// --- Homing Parameters ---
#define HOMING_RAPID_VEL_MMS 50.0f
#define HOMING_BACKOFF_VEL_MMS 15.0f  // Slower speed for backing off the sensor
#define HOMING_TOUCH_VEL_MMS 5.0f   // Slowest speed for the final precise touch
#define HOMING_ACCEL_MMSS 100.0f
#define HOMING_TORQUE 25

// --- Homing Sensor Pin Configuration ---
#define SENSOR_X      ConnectorIO0
#define SENSOR_Y1     ConnectorIO1
#define SENSOR_Y2     ConnectorIO3
#define SENSOR_Z      ConnectorIO4
#define LIMIT_Y_BACK  ConnectorIO2

// --- Machine Travel Limits (mm) ---
#define X_MIN_POS 0.0f
#define X_MAX_POS 1219.0f
#define Y_MIN_POS 0.0f
#define Y_MAX_POS 410.0f
#define Z_MIN_POS -160.0f // Z=0 is top, Z=-160 is bottom
#define Z_MAX_POS 0.0f

// --- Command Definitions ---
#define CMD_STR_REQUEST_TELEM     "REQUEST_TELEM"
#define CMD_STR_DISCOVER          "DISCOVER_FILLHEAD"
#define CMD_STR_SET_PEER_IP       "SET_PEER_IP"
#define CMD_STR_CLEAR_PEER_IP     "CLEAR_PEER_IP"
#define CMD_STR_ABORT             "ABORT"
#define CMD_STR_MOVE_X            "MOVE_X"
#define CMD_STR_MOVE_Y            "MOVE_Y"
#define CMD_STR_MOVE_Z            "MOVE_Z"
#define CMD_STR_HOME_X            "HOME_X"
#define CMD_STR_HOME_Y            "HOME_Y"
#define CMD_STR_HOME_Z            "HOME_Z"

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
#define STEPS_PER_MM_X 32.0f
#define STEPS_PER_MM_Y 32.0f
#define STEPS_PER_MM_Z 80.0f

// --- Type Definitions ---
typedef enum {
	STATE_STANDBY,
	STATE_STARTING_MOVE,
	STATE_MOVING,
	STATE_HOMING
} AxisState;
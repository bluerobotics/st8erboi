#pragma once

#define DEVICE_NAME "gantry"

//==================================================================================================
// Network Configuration
//==================================================================================================
#define LOCAL_PORT 6271      // For Fillhead commands
#define MAX_PACKET_LENGTH 256  // Max UDP packet size
#define MAX_MESSAGE_LENGTH 256 // Max message length
#define RX_QUEUE_SIZE 10
#define TX_QUEUE_SIZE 10
#define TELEMETRY_INTERVAL_MS 100

//==================================================================================================
// Hardware Pin Definitions
//==================================================================================================
#define MotorX  ConnectorM0
#define MotorY1 ConnectorM1
#define MotorY2 ConnectorM2
#define MotorZ  ConnectorM3

#define SENSOR_X		ConnectorIO0
#define SENSOR_Y1		ConnectorIO1
#define SENSOR_Y2		ConnectorIO3
#define SENSOR_Z		ConnectorIO4
#define LIMIT_Y_BACK	ConnectorIO2
#define Z_BRAKE			ConnectorIO5

//==================================================================================================
// System Parameters & Conversions
//==================================================================================================
#define EWMA_ALPHA 0.2f
#define STEPS_PER_MM_X 32.0f
#define STEPS_PER_MM_Y 32.0f
#define STEPS_PER_MM_Z 80.0f

//==================================================================================================
// Motion & Operation Defaults
//==================================================================================================
#define MAX_VEL 20000 // sps
#define MAX_ACC 200000 // sps^2
#define MAX_TRQ 90 // %

#define HOMING_RAPID_VEL_MMS 20.0f
#define HOMING_BACKOFF_VEL_MMS 5.0f  // Slower speed for backing off the sensor
#define HOMING_TOUCH_VEL_MMS 1.0f   // Slowest speed for the final precise touch
#define HOMING_ACCEL_MMSS 100.0f
#define HOMING_TORQUE 25
#define HOMING_BACKOFF_MM 10

// --- Machine Travel Limits (mm) ---
#define X_MIN_POS 0.0f
#define X_MAX_POS 1219.0f
#define Y_MIN_POS 0.0f
#define Y_MAX_POS 410.0f
#define Z_MIN_POS -160.0f // Z=0 is top, Z=-160 is bottom
#define Z_MAX_POS 0.0f

// Torque Smoothing
#define EWMA_ALPHA 0.2f // Smoothing factor for torque readings. Lower is smoother.

// ===== NETWORKING =====
#define CMD_PORT 6271 // Command port for Gantry
#define RX_QUEUE_SIZE 10
#define TX_QUEUE_SIZE 10

// ===== MOTOR DEFAULTS =====
// Default maximum velocity and acceleration for gantry motors.
// These values are used during initialization and after a fault reset.
#define MOTOR_DEFAULT_VEL_MAX_SPS (10000)
#define MOTOR_DEFAULT_ACCEL_MAX_SPS2 (100000)
#pragma once

//==================================================================================================
// Network Configuration
//==================================================================================================
#define LOCAL_PORT 8888
#define MAX_PACKET_LENGTH 256 // Increased to prevent telemetry truncation
#define RX_QUEUE_SIZE 32
#define TX_QUEUE_SIZE 32
#define MAX_MESSAGE_LENGTH MAX_PACKET_LENGTH
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

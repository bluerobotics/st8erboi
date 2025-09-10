/**
 * @file config.h
 * @author Eldin Miller-Stead
 * @date September 10, 2025
 * @brief Central configuration file for the Gantry firmware.
 *
 * @details This file consolidates all compile-time constants, hardware pin definitions,
 * and default operational parameters for the entire Gantry system. By centralizing these
 * values, it simplifies tuning and maintenance, and ensures consistency across all
 * controller modules. The parameters are organized into logical sections for clarity.
 */
#pragma once

//==================================================================================================
// Network Configuration
//==================================================================================================
/**
 * @name Network and Communication Settings
 * @{
 */
#define LOCAL_PORT              8888      ///< The UDP port this device listens on for incoming commands.
#define MAX_PACKET_LENGTH       256       ///< Maximum size in bytes for a single UDP packet. Must be large enough for telemetry.
#define RX_QUEUE_SIZE           32        ///< Number of incoming messages that can be buffered before processing.
#define TX_QUEUE_SIZE           32        ///< Number of outgoing messages that can be buffered before sending.
#define MAX_MESSAGE_LENGTH      MAX_PACKET_LENGTH ///< Maximum size of a single message in the Rx/Tx queues.
#define TELEMETRY_INTERVAL_MS   100       ///< How often (in milliseconds) telemetry data is published to the GUI.
/** @} */

//==================================================================================================
// Hardware Pin Definitions
//==================================================================================================
/**
 * @name Hardware Pin and Connector Assignments
 * @{
 */
#define MotorX          ConnectorM0     ///< Motor driver for the X-axis.
#define MotorY1         ConnectorM1     ///< Primary motor driver for the Y-axis.
#define MotorY2         ConnectorM2     ///< Secondary motor driver for the Y-axis.
#define MotorZ          ConnectorM3     ///< Motor driver for the Z-axis.

#define SENSOR_X		ConnectorIO0    ///< Homing sensor for the X-axis.
#define SENSOR_Y1		ConnectorIO1    ///< Homing sensor for the primary Y-axis motor.
#define SENSOR_Y2		ConnectorIO3    ///< Homing sensor for the secondary Y-axis motor.
#define SENSOR_Z		ConnectorIO4    ///< Homing sensor for the Z-axis.
#define LIMIT_Y_BACK	ConnectorIO2    ///< Limit switch for the Y-axis.
#define Z_BRAKE			ConnectorIO5    ///< Digital output for the Z-axis brake.
/** @} */

//==================================================================================================
// System Parameters & Conversions
//==================================================================================================
/**
 * @name Core System Parameters and Unit Conversions
 * @{
 */
#define EWMA_ALPHA      0.2f            ///< Smoothing factor (alpha) for the Exponentially Weighted Moving Average filter on torque readings.
#define STEPS_PER_MM_X  32.0f           ///< Conversion factor for the X-axis: steps per millimeter.
#define STEPS_PER_MM_Y  32.0f           ///< Conversion factor for the Y-axis: steps per millimeter.
#define STEPS_PER_MM_Z  80.0f           ///< Conversion factor for the Z-axis: steps per millimeter.
/** @} */

//==================================================================================================
// Motion & Operation Defaults
//==================================================================================================
/**
 * @name Motion and Operation Parameters
 * @{
 */
#define MAX_VEL         20000           ///< Default maximum velocity for motors in steps per second.
#define MAX_ACC         200000          ///< Default maximum acceleration for motors in steps per second^2.
#define MAX_TRQ         90              ///< Default maximum torque limit for moves, as a percentage.

/**
 * @name Homing Defaults
 * @{
 */
#define HOMING_RAPID_VEL_MMS    20.0f   ///< Velocity (mm/s) for the initial high-speed search for the homing sensor.
#define HOMING_BACKOFF_VEL_MMS  5.0f    ///< Velocity (mm/s) for backing off the sensor.
#define HOMING_TOUCH_VEL_MMS    1.0f    ///< Velocity (mm/s) for the final, slow-speed precise touch-off.
#define HOMING_ACCEL_MMSS       100.0f  ///< Acceleration (mm/s^2) for all homing moves.
#define HOMING_TORQUE           25      ///< Default torque limit (%) for homing moves.
#define HOMING_BACKOFF_MM       10      ///< Distance (mm) to back off from the sensor.
/** @} */

/**
 * @name Machine Travel Limits
 * @{
 */
#define X_MIN_POS       0.0f            ///< Minimum software travel limit for the X-axis in millimeters.
#define X_MAX_POS       1219.0f         ///< Maximum software travel limit for the X-axis in millimeters.
#define Y_MIN_POS       0.0f            ///< Minimum software travel limit for the Y-axis in millimeters.
#define Y_MAX_POS       410.0f          ///< Maximum software travel limit for the Y-axis in millimeters.
#define Z_MIN_POS       -160.0f         ///< Minimum software travel limit for the Z-axis in millimeters (bottom).
#define Z_MAX_POS       0.0f            ///< Maximum software travel limit for the Z-axis in millimeters (top).
/** @} */
/** @} */

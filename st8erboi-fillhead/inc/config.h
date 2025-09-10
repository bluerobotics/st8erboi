/**
 * @file config.h
 * @author Eldin Miller-Stead
 * @date September 10, 2025
 * @brief Central configuration file for the Fillhead firmware.
 *
 * @details This file consolidates all compile-time constants, hardware pin definitions,
 * and default operational parameters for the entire Fillhead system. By centralizing these
 * values, it simplifies tuning and maintenance, and ensures consistency across all
 * controller modules. The parameters are organized into logical sections for clarity.
 */
#pragma once
#include "ClearCore.h"

//==================================================================================================
// Network Configuration
//==================================================================================================
/**
 * @name Network and Communication Settings
 * @{
 */
#define LOCAL_PORT                      8888      ///< The UDP port this device listens on for incoming commands.
#define MAX_PACKET_LENGTH               1024      ///< Maximum size in bytes for a single UDP packet. Must be large enough for the longest telemetry string.
#define RX_QUEUE_SIZE                   32        ///< Number of incoming messages that can be buffered before processing.
#define TX_QUEUE_SIZE                   32        ///< Number of outgoing messages that can be buffered before sending.
#define MAX_MESSAGE_LENGTH              MAX_PACKET_LENGTH ///< Maximum size of a single message in the Rx/Tx queues.
#define TELEMETRY_INTERVAL_MS			100       ///< How often (in milliseconds) telemetry data is published to the GUI.
/** @} */

//==================================================================================================
// System Behavior
//==================================================================================================
/**
 * @name General System Behavior
 * @{
 */
#define STATUS_MESSAGE_BUFFER_SIZE      256       ///< Standard buffer size for composing status and error messages.
#define POST_ABORT_DELAY_MS             100       ///< Delay in milliseconds after an abort command to allow motors to come to a complete stop.
/** @} */

//==================================================================================================
// System Parameters & Conversions
//==================================================================================================
/**
 * @name Core System Parameters and Unit Conversions
 * @{
 */
#define INJECTOR_PITCH_MM_PER_REV       5.0f      ///< Linear travel (in mm) of the injector plunger for one full motor revolution.
#define PINCH_PITCH_MM_PER_REV          2.0f      ///< Linear travel (in mm) of the pinch valve actuator for one full motor revolution.
#define PULSES_PER_REV                  800       ///< Number of step pulses required for one full motor revolution (dependent on microstepping settings).
#define STEPS_PER_MM_INJECTOR           (PULSES_PER_REV / INJECTOR_PITCH_MM_PER_REV)   ///< Calculated steps per millimeter for the main injector drive.
#define STEPS_PER_MM_PINCH              (PULSES_PER_REV / PINCH_PITCH_MM_PER_REV)      ///< Calculated steps per millimeter for the pinch valve mechanism.
#define MAX_HOMING_DURATION_MS          100000    ///< Maximum time (in milliseconds) a homing operation is allowed to run before timing out.
/** @} */

//==================================================================================================
// Hardware Pin Definitions
//==================================================================================================
/**
 * @name Hardware Pin and Connector Assignments
 * @{
 */
// --- Motors ---
#define MOTOR_INJECTOR_A                ConnectorM0 ///< Primary injector motor.
#define MOTOR_INJECTOR_B                ConnectorM1 ///< Secondary, ganged injector motor.
#define MOTOR_VACUUM_VALVE              ConnectorM2 ///< Motor for the pinch valve on the vacuum side.
#define MOTOR_INJECTION_VALVE           ConnectorM3 ///< Motor for the pinch valve on the injection side.

// --- Analog Sensors ---
#define PIN_THERMOCOUPLE                ConnectorA12 ///< Analog input for the heater thermocouple.
#define PIN_VACUUM_TRANSDUCER           ConnectorA11 ///< Analog input for the vacuum pressure transducer.

// --- Digital Outputs (Relays) ---
#define PIN_HEATER_RELAY                ConnectorIO1 ///< Digital output to control the heater relay.
#define PIN_VACUUM_RELAY                ConnectorIO0 ///< Digital output to control the vacuum pump relay.
#define PIN_VACUUM_VALVE_RELAY          ConnectorIO5 ///< Digital output to control the vacuum solenoid valve relay.
/** @} */

//==================================================================================================
// Sensor & Control Parameters
//==================================================================================================
/**
 * @name Sensor and Control Loop Parameters
 * @{
 */
#define SENSOR_SAMPLE_INTERVAL_MS       100       ///< How often (in ms) to sample temperature and vacuum sensors.
#define EWMA_ALPHA_SENSORS              0.5f      ///< Smoothing factor (alpha) for the Exponentially Weighted Moving Average filter on sensor readings.
#define EWMA_ALPHA_TORQUE               0.2f      ///< Smoothing factor (alpha) for the EWMA filter on motor torque readings.

/**
 * @name Heater Control Defaults
 * @{
 */
#define TC_V_REF                        10.0f     ///< ADC reference voltage (10V for ClearCore analog inputs).
#define TC_V_OFFSET                     1.25f     ///< Thermocouple sensor's output voltage at 0 degrees Celsius.
#define TC_GAIN                         200.0f    ///< Conversion factor from volts to degrees Celsius for the thermocouple sensor.
#define PID_UPDATE_INTERVAL_MS          100       ///< How often (in ms) the PID calculation for the heater is performed.
#define PID_PWM_PERIOD_MS               1000      ///< Time window (in ms) for the time-proportioned heater relay control (software PWM).
#define DEFAULT_HEATER_SETPOINT_C       70.0f     ///< Default target temperature in Celsius.
#define DEFAULT_HEATER_KP               60.0f     ///< Default Proportional (P) gain for the PID controller.
#define DEFAULT_HEATER_KI               2.5f      ///< Default Integral (I) gain for the PID controller.
#define DEFAULT_HEATER_KD               40.0f     ///< Default Derivative (D) gain for the PID controller.
/** @} */

/**
 * @name Vacuum System Defaults
 * @{
 */
#define VAC_V_OUT_MIN                   1.0f      ///< Sensor output voltage at minimum pressure (-14.7 PSIG).
#define VAC_V_OUT_MAX                   5.0f      ///< Sensor output voltage at maximum pressure (15.0 PSIG).
#define VAC_PRESSURE_MIN                -14.7f    ///< Minimum pressure in PSIG that the sensor can read.
#define VAC_PRESSURE_MAX                15.0f     ///< Maximum pressure in PSIG that the sensor can read.
#define VACUUM_PSIG_OFFSET              0.0f      ///< A calibration offset to be added to the raw vacuum sensor reading.
#define DEFAULT_VACUUM_TARGET_PSIG      -14.0f    ///< Default target pressure in PSIG for vacuum operations.
#define DEFAULT_VACUUM_RAMP_TIMEOUT_MS  30000     ///< Default time (in ms) allowed to reach the target pressure before a timeout error.
#define DEFAULT_LEAK_TEST_DELTA_PSIG    0.1f      ///< Default maximum allowed pressure drop during a leak test.
#define DEFAULT_LEAK_TEST_DURATION_MS   10000     ///< Default duration (in ms) for a leak test.
#define VACUUM_SETTLE_TIME_S            2.0f      ///< Time (in s) to let pressure stabilize before starting a leak test measurement.
/** @} */
/** @} */

//==================================================================================================
// Motion & Operation Defaults
//==================================================================================================
/**
 * @name Motion and Operation Parameters
 * @{
 */

/**
 * @name Motor Setup Defaults
 * @{
 */
#define TORQUE_SENTINEL_INVALID_VALUE		-9999.0f  ///< A special value indicating that a torque reading is invalid or not yet available.
#define MOTOR_DEFAULT_VEL_MAX_MMS           156.25f   ///< Default maximum velocity for motors in mm/s.
#define MOTOR_DEFAULT_ACCEL_MAX_MMSS        625.0f    ///< Default maximum acceleration for motors in mm/s^2.
#define MOTOR_DEFAULT_VEL_MAX_SPS           (int)(MOTOR_DEFAULT_VEL_MAX_MMS * STEPS_PER_MM_INJECTOR) ///< Default max velocity in steps/sec, derived from mm/s.
#define MOTOR_DEFAULT_ACCEL_MAX_SPS2        (int)(MOTOR_DEFAULT_ACCEL_MAX_MMSS * STEPS_PER_MM_INJECTOR) ///< Default max acceleration in steps/sec^2, derived from mm/s^2.
#define PINCH_DEFAULT_VEL_MAX_SPS           (int)(MOTOR_DEFAULT_VEL_MAX_MMS * STEPS_PER_MM_PINCH)      ///< Default max velocity for pinch valves in steps/sec.
#define PINCH_DEFAULT_ACCEL_MAX_SPS2        (int)(MOTOR_DEFAULT_ACCEL_MAX_MMSS * STEPS_PER_MM_PINCH)   ///< Default max acceleration for pinch valves in steps/sec^2.
/** @} */

/**
 * @name Timeouts
 * @{
 */
#define MOVE_START_TIMEOUT_MS           (250)     ///< Time (in ms) to wait for a move to start before flagging a motor error.
/** @} */

/**
 * @name Initialization Defaults
 * @{
 */
#define DEFAULT_INJECTOR_TORQUE_LIMIT       20.0f     ///< Default torque limit (%) for general injector motor operations.
#define DEFAULT_INJECTOR_TORQUE_OFFSET      -2.4f     ///< Default torque offset (%) to account for sensor bias or no-load friction.
/** @} */

/**
 * @name Injector Homing Defaults
 * @{
 */
#define INJECTOR_HOMING_STROKE_MM           500.0f    ///< Maximum travel distance (mm) during a homing sequence.
#define INJECTOR_HOMING_RAPID_VEL_MMS       5.0f      ///< Velocity (mm/s) for the initial high-speed search for the hard stop.
#define INJECTOR_HOMING_TOUCH_VEL_MMS       1.0f      ///< Velocity (mm/s) for the final, slow-speed precise touch-off.
#define INJECTOR_HOMING_BACKOFF_VEL_MMS     1.0f      ///< Velocity (mm/s) for backing off the hard stop.
#define INJECTOR_HOMING_ACCEL_MMSS          100.0f    ///< Acceleration (mm/s^2) for all homing moves.
#define INJECTOR_HOMING_SEARCH_TORQUE_PERCENT 10.0f   ///< Torque limit (%) used to detect the hard stop.
#define INJECTOR_HOMING_BACKOFF_TORQUE_PERCENT 40.0f  ///< Higher torque limit (%) for the back-off move to prevent stalling.
#define INJECTOR_HOMING_BACKOFF_MM          1.0f      ///< Distance (mm) to back off from the hard stop.
/** @} */

/**
 * @name Pinch Valve Homing (Untubed)
 * @{
 */
#define PINCH_HOMING_UNTUBED_STROKE_MM              50.0f  ///< Max travel (mm) for untubed homing.
#define PINCH_HOMING_UNTUBED_UNIFIED_VEL_MMS        1.0f   ///< Unified velocity (mm/s) for all moves during untubed homing.
#define PINCH_HOMING_UNTUBED_ACCEL_MMSS             100.0f ///< Acceleration (mm/s^2) for untubed homing.
#define PINCH_HOMING_UNTUBED_SEARCH_TORQUE_PERCENT  15.0f  ///< Torque limit (%) for detecting the hard stop without tubing.
#define PINCH_HOMING_UNTUBED_BACKOFF_TORQUE_PERCENT 50.0f  ///< Higher torque (%) for the back-off move.
#define PINCH_VALVE_UNTUBED_FINAL_OFFSET_MM         9.0f   ///< Final offset distance (mm) from the hard stop, defining the "open" position.
#define PINCH_VALVE_HOMING_BACKOFF_MM_UNTUBED 0.5f         ///< Back-off distance (mm) for untubed homing.
/** @} */

/**
 * @name Pinch Valve Homing (Tubed)
 * @{
 */
#define PINCH_HOMING_TUBED_STROKE_MM              50.0f  ///< Max travel (mm) for tubed homing.
#define PINCH_HOMING_TUBED_UNIFIED_VEL_MMS        1.0f   ///< Unified velocity (mm/s) for all moves during tubed homing.
#define PINCH_HOMING_TUBED_ACCEL_MMSS             100.0f ///< Acceleration (mm/s^2) for tubed homing.
#define PINCH_HOMING_TUBED_SEARCH_TORQUE_PERCENT  60.0f  ///< Higher torque limit (%) needed to fully pinch the tube.
#define PINCH_HOMING_TUBED_BACKOFF_TORQUE_PERCENT 80.0f  ///< Higher torque (%) for the back-off move.
#define PINCH_VALVE_TUBED_FINAL_OFFSET_MM         6.0f   ///< Final offset distance (mm) from the hard stop, defining the "open" position with tubing.
#define PINCH_VALVE_HOMING_BACKOFF_MM_TUBED 0.5f         ///< Back-off distance (mm) for tubed homing.
/** @} */

/**
 * @name Pinch Valve Operation Defaults
 * @{
 */
#define PINCH_VALVE_PINCH_TORQUE_PERCENT    75.0f     ///< Target torque (%) for closing the valve during normal operation.
#define PINCH_VALVE_PINCH_VEL_MMS           1.0f      ///< Speed (mm/s) for the closing (pinching) move.
#define PINCH_VALVE_OPEN_VEL_MMS            10.0f     ///< Speed (mm/s) for the opening move (returning to the homed position).
#define PINCH_VALVE_OPEN_ACCEL_MMSS         50.0f     ///< Acceleration (mm/s^2) for the opening move.
/** @} */

/**
 * @name Jogging Defaults
 * @{
 */
#define JOG_DEFAULT_TORQUE_PERCENT          30        ///< Default torque limit (%) for jog moves.
#define JOG_DEFAULT_VEL_MMS                 1.0f      ///< Default velocity (mm/s) for jog moves.
#define JOG_DEFAULT_ACCEL_MMSS              10.0f     ///< Default acceleration (mm/s^2) for jog moves.
#define PINCH_JOG_DEFAULT_VEL_MMS           5.0f      ///< Default velocity (mm/s) for pinch valve jog moves.
#define PINCH_JOG_DEFAULT_ACCEL_MMSS        25.0f     ///< Default acceleration (mm/s^2) for pinch valve jog moves.
/** @} */

/**
 * @name Injection Defaults
 * @{
 */
#define INJECT_DEFAULT_SPEED_MLS            0.5f      ///< Default injection speed in ml/s.
#define STATOR_PISTON_A_DIAMETER_MM         75.0f     ///< Diameter (mm) of piston A for stator injections.
#define STATOR_PISTON_B_DIAMETER_MM         33.0f     ///< Diameter (mm) of piston B for stator injections.
#define ROTOR_PISTON_A_DIAMETER_MM          33.0f     ///< Diameter (mm) of piston A for rotor injections.
#define ROTOR_PISTON_B_DIAMETER_MM          33.0f     ///< Diameter (mm) of piston B for rotor injections.
/** @} */

/**
 * @name Feed Defaults
 * @{
 */
#define FEED_DEFAULT_TORQUE_PERCENT         30        ///< Default torque limit (%) for feed/inject moves.
#define FEED_DEFAULT_VELOCITY_MMS           6.25f     ///< Default velocity (mm/s) for feed/inject moves.
#define FEED_DEFAULT_ACCEL_MMSS             62.5f     ///< Default acceleration (mm/s^2) for feed/inject moves.
#define FEED_DEFAULT_VELOCITY_SPS           (int)(FEED_DEFAULT_VELOCITY_MMS * STEPS_PER_MM_INJECTOR) ///< Default feed velocity in steps/sec.
#define FEED_DEFAULT_ACCEL_SPS2             (int)(FEED_DEFAULT_ACCEL_MMSS * STEPS_PER_MM_INJECTOR)   ///< Default feed acceleration in steps/sec^2.
#define INJECT_DEFAULT_VELOCITY_MMS         0.625f    ///< Fallback velocity if a command provides an invalid value.
#define INJECT_DEFAULT_VELOCITY_SPS         (int)(INJECT_DEFAULT_VELOCITY_MMS * STEPS_PER_MM_INJECTOR) ///< Fallback velocity in steps/sec.
/** @} */
/** @} */


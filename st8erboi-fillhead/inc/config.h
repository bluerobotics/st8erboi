#pragma once
// test
#include "ClearCore.h"

//==================================================================================================
// Network Configuration
//==================================================================================================
#define LOCAL_PORT                      8888      // The UDP port this device listens on for incoming commands.
#define MAX_PACKET_LENGTH               1024      // Maximum size in bytes for a single UDP packet. Should be large enough for telemetry.
#define RX_QUEUE_SIZE                   32        // Number of incoming messages that can be buffered before processing.
#define TX_QUEUE_SIZE                   32        // Number of outgoing messages that can be buffered before sending.
#define MAX_MESSAGE_LENGTH              MAX_PACKET_LENGTH // Maximum size of a single message in the Rx/Tx queues.
#define TELEMETRY_INTERVAL_MS			100

//==================================================================================================
// System Behavior
//==================================================================================================
#define STATUS_MESSAGE_BUFFER_SIZE      128       // Standard buffer size for status and error messages.
#define POST_ABORT_DELAY_MS             100       // Delay in milliseconds after an abort to allow motors to settle.

//==================================================================================================
// System Parameters & Conversions
//==================================================================================================
#define INJECTOR_PITCH_MM_PER_REV       5.0f      // Linear travel (in mm) of the injector plunger for one full motor revolution.
#define PINCH_PITCH_MM_PER_REV          2.0f      // Linear travel (in mm) of the pinch valve actuator for one full motor revolution.
#define PULSES_PER_REV                  800       // Number of step pulses required for one full motor revolution (microstepping dependent).
#define STEPS_PER_MM_INJECTOR           (PULSES_PER_REV / INJECTOR_PITCH_MM_PER_REV) // Calculated steps per millimeter for the injector axis.
#define STEPS_PER_MM_PINCH              (PULSES_PER_REV / PINCH_PITCH_MM_PER_REV)    // Calculated steps per millimeter for the pinch valve axes.
#define MAX_HOMING_DURATION_MS          100000    // Maximum time (in milliseconds) a homing operation is allowed to run before timing out.

//==================================================================================================
// Hardware Pin Definitions
//==================================================================================================
// --- Motors ---
#define MOTOR_INJECTOR_A                ConnectorM0 // Primary injector motor.
#define MOTOR_INJECTOR_B                ConnectorM1 // Secondary, ganged injector motor.
#define MOTOR_VACUUM_VALVE              ConnectorM2 // Motor for the pinch valve on the vacuum side.
#define MOTOR_INJECTION_VALVE           ConnectorM3 // Motor for the pinch valve on the injection side.


// --- Analog Sensors ---
#define PIN_THERMOCOUPLE                ConnectorA12 // Analog input for the heater thermocouple.
#define PIN_VACUUM_TRANSDUCER           ConnectorA11 // Analog input for the vacuum pressure transducer.

// --- Digital Outputs (Relays) ---
#define PIN_HEATER_RELAY                ConnectorIO1 // Digital output to control the heater relay.
#define PIN_VACUUM_RELAY                ConnectorIO0 // Digital output to control the vacuum pump relay.
#define PIN_VACUUM_VALVE_RELAY          ConnectorIO5 // Digital output to control the vacuum solenoid valve relay.

//==================================================================================================
// Sensor & Control Parameters
//==================================================================================================

#define SENSOR_SAMPLE_INTERVAL_MS       100       // How often (in ms) to sample temperature and vacuum sensors.
#define EWMA_ALPHA_SENSORS              0.5f      // Smoothing factor for the Exponentially Weighted Moving Average on sensor readings.
#define EWMA_ALPHA_TORQUE               0.2f      // Smoothing factor for the EWMA on motor torque readings.


// --- Heater Setup Defaults --
#define TC_V_REF                        10.0f     // ADC reference voltage (10V for ClearCore analog inputs).
#define TC_V_OFFSET                     1.25f     // Thermocouple sensor's output voltage at 0 degrees Celsius.
#define TC_GAIN                         200.0f    // Conversion factor from volts to degrees Celsius for the thermocouple.
#define PID_UPDATE_INTERVAL_MS          100       // How often (in ms) the PID calculation for the heater is performed.
#define PID_PWM_PERIOD_MS               1000      // Time window (in ms) for the time-proportioned heater relay control.
#define DEFAULT_HEATER_SETPOINT_C       70.0f     // Default target temperature in Celsius.
#define DEFAULT_HEATER_KP               60.0f     // Default Proportional gain.
#define DEFAULT_HEATER_KI               2.5f      // Default Integral gain.
#define DEFAULT_HEATER_KD               40.0f     // Default Derivative gain.


// --- Vacuum Setup Defaults ---
#define VAC_V_OUT_MIN                   1.0f      // Sensor voltage at minimum pressure (-14.7 PSIG).
#define VAC_V_OUT_MAX                   5.0f      // Sensor voltage at maximum pressure (15.0 PSIG).
#define VAC_PRESSURE_MIN                -14.7f    // Minimum pressure in PSIG that the sensor can read.
#define VAC_PRESSURE_MAX                15.0f     // Maximum pressure in PSIG that the sensor can read.
#define VACUUM_PSIG_OFFSET              0.0f      // A calibration offset for the vacuum sensor reading.
#define DEFAULT_VACUUM_TARGET_PSIG      -14.0f    // Default target pressure in PSIG.
#define DEFAULT_VACUUM_RAMP_TIMEOUT_MS  30000     // Default time (30s) to reach target pressure.
#define DEFAULT_LEAK_TEST_DELTA_PSIG    0.1f      // Default max allowed pressure drop during a leak test.
#define DEFAULT_LEAK_TEST_DURATION_MS   10000     // Default duration (10s) for a leak test.
#define VACUUM_SETTLE_TIME_S            2.0f      // Time (in s) to let pressure settle before a leak test.

//==================================================================================================
// Motion & Operation Defaults
//==================================================================================================
// --- Motor Setup Defaults ---
#define TORQUE_SENTINEL_INVALID_VALUE		-9999.0f  // A value indicating that the torque reading is invalid.
#define MOTOR_DEFAULT_VEL_MAX_MMS           156.25f   // Default maximum velocity for motors in mm/s.
#define MOTOR_DEFAULT_ACCEL_MAX_MMSS        625.0f    // Default maximum acceleration for motors in mm/s^2.
#define MOTOR_DEFAULT_VEL_MAX_SPS           (int)(MOTOR_DEFAULT_VEL_MAX_MMS * STEPS_PER_MM_INJECTOR)
#define MOTOR_DEFAULT_ACCEL_MAX_SPS2        (int)(MOTOR_DEFAULT_ACCEL_MAX_MMSS * STEPS_PER_MM_INJECTOR)

// --- Initialization Defaults ---
#define DEFAULT_INJECTOR_TORQUE_LIMIT       20.0f     // Default torque limit (%) for injector motors.
#define DEFAULT_INJECTOR_TORQUE_OFFSET      -2.4f     // Default torque offset (%) to account for sensor bias.

// --- Injector Homing Defaults ---
#define INJECTOR_HOMING_STROKE_MM           500.0f
#define INJECTOR_HOMING_RAPID_VEL_MMS       5.0f
#define INJECTOR_HOMING_TOUCH_VEL_MMS       1.0f
#define INJECTOR_HOMING_BACKOFF_VEL_MMS     5.0f
#define INJECTOR_HOMING_ACCEL_MMSS          100.0f
#define INJECTOR_HOMING_SEARCH_TORQUE_PERCENT 10.0f
#define INJECTOR_HOMING_BACKOFF_TORQUE_PERCENT 40.0f
#define INJECTOR_HOMING_BACKOFF_MM          1.0f

// --- Pinch Valve Homing Defaults ---
#define PINCH_HOMING_STROKE_MM              50.0f
#define PINCH_HOMING_UNIFIED_VEL_MMS        1.0f // The single speed for all homing moves.
#define PINCH_HOMING_ACCEL_MMSS             100.0f
#define PINCH_HOMING_SEARCH_TORQUE_PERCENT  10.0f
#define PINCH_HOMING_BACKOFF_TORQUE_PERCENT 50.0f
#define PINCH_VALVE_HOMING_BACKOFF_MM       0.5f      // Distance (mm) for the initial and intermediate backoffs.
#define PINCH_VALVE_FINAL_OFFSET_MM         9.0f      // Distance (mm) for the final offset from the hard stop.

// --- Pinch Valve Operation Defaults (NEW) ---
#define PINCH_VALVE_PINCH_TORQUE_PERCENT    50.0f     // Target torque for closing the valve.
#define PINCH_VALVE_PINCH_VEL_MMS           2.0f      // Speed for the closing (pinching) move.
#define PINCH_VALVE_OPEN_VEL_MMS            10.0f     // Speed for the opening move (returning to home).
#define PINCH_VALVE_OPEN_ACCEL_MMSS         50.0f     // Acceleration for the opening move.


// --- Jogging Defaults ---
#define JOG_DEFAULT_TORQUE_PERCENT          30        // Default torque limit (%) for jog moves.
#define JOG_DEFAULT_VEL_MMS                 2.0f      // Default velocity (mm/s) for jog moves.
#define JOG_DEFAULT_ACCEL_MMSS              10.0f     // Default acceleration (mm/s^2) for jog moves.
#define PINCH_JOG_DEFAULT_VEL_MMS           5.0f      // Default velocity (mm/s) for pinch valve jog moves.
#define PINCH_JOG_DEFAULT_ACCEL_MMSS        25.0f     // Default acceleration (mm/s^2) for pinch valve jog moves.

// --- Feed Defaults ---
#define FEED_DEFAULT_TORQUE_PERCENT         30        // Default torque limit (%) for feed/inject moves.
#define FEED_DEFAULT_VELOCITY_MMS           6.25f     // Default velocity (mm/s) for feed/inject moves.
#define FEED_DEFAULT_ACCEL_MMSS             62.5f     // Default acceleration (mm/s^2) for feed/inject moves.
#define FEED_DEFAULT_VELOCITY_SPS           (int)(FEED_DEFAULT_VELOCITY_MMS * STEPS_PER_MM_INJECTOR)
#define FEED_DEFAULT_ACCEL_SPS2             (int)(FEED_DEFAULT_ACCEL_MMSS * STEPS_PER_MM_INJECTOR)
#define INJECT_DEFAULT_VELOCITY_MMS         0.625f    // Fallback velocity if a command provides an invalid value.
#define INJECT_DEFAULT_VELOCITY_SPS         (int)(INJECT_DEFAULT_VELOCITY_MMS * STEPS_PER_MM_INJECTOR)


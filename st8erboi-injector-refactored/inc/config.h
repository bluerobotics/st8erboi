#pragma once

#include "ClearCore.h"

//==================================================================================================
// Network Configuration
//==================================================================================================
#define LOCAL_PORT                      8888      // The UDP port this device listens on for incoming commands.
#define MAX_PACKET_LENGTH               1024      // Maximum size in bytes for a single UDP packet. Should be large enough for telemetry.
#define RX_QUEUE_SIZE                   32        // Number of incoming messages that can be buffered before processing.
#define TX_QUEUE_SIZE                   32        // Number of outgoing messages that can be buffered before sending.
#define MAX_MESSAGE_LENGTH              MAX_PACKET_LENGTH // Maximum size of a single message in the Rx/Tx queues.

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
#define MAX_HOMING_DURATION_MS          30000     // Maximum time (in milliseconds) a homing operation is allowed to run before timing out.

//==================================================================================================
// Hardware Pin Definitions
//==================================================================================================
// --- Motors ---
#define MOTOR_INJECTOR_A                ConnectorM0 // Primary injector motor.
#define MOTOR_INJECTOR_B                ConnectorM1 // Secondary, ganged injector motor.
#define MOTOR_INJECTION_VALVE           ConnectorM2 // Motor for the pinch valve on the injection side.
#define MOTOR_VACUUM_VALVE              ConnectorM3 // Motor for the pinch valve on the vacuum side.

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
#define DEFAULT_VACUUM_TARGET_PSIG      -12.0f    // Default target pressure in PSIG.
#define DEFAULT_VACUUM_RAMP_TIMEOUT_MS  15000     // Default time (15s) to reach target pressure.
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

// --- Homing Defaults ---
#define HOMING_STROKE_MM                    500.0f    // Maximum travel distance during homing sequence.
#define HOMING_RAPID_VEL_MMS                20.0f     // Rapid velocity for seeking the hard stop.
#define HOMING_TOUCH_VEL_MMS                3.125f    // Slow velocity for precise touch-off.
#define HOMING_BACK_OFF_VEL_MMS             6.25f     // Velocity for the back-off move.
#define HOMING_RETRACT_VEL_MMS              31.25f    // Velocity for the post-touch-off retraction.
#define HOMING_ACCEL_MMSS                   100.0f    // Acceleration for homing moves.
#define HOMING_RETRACT_ACCEL_MMSS           125.0f    // Acceleration for the post-touch-off retraction.
#define HOMING_TOUCH_OFF_ACCEL_MMSS         31.25f    // Acceleration for the final touch-off move.
#define HOMING_BACK_OFF_ACCEL_MMSS          62.5f     // Acceleration for the back-off move.
#define HOMING_TORQUE_PERCENT               10.0f     // Torque limit (%) for homing moves.
#define HOMING_DEFAULT_BACKOFF_STEPS        200       // Steps to back off the hard stop after initial contact.
#define HOMING_POST_TOUCH_RETRACT_MM        12.5f     // Distance to retract after the final touch-off.

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

//==================================================================================================
// Command Strings & Prefixes
//==================================================================================================
// --- General Commands ---
#define CMD_STR_REQUEST_TELEM               "REQUEST_TELEM"
#define CMD_STR_DISCOVER                    "DISCOVER_INJECTOR"
#define CMD_STR_SET_PEER_IP                 "SET_PEER_IP "
#define CMD_STR_CLEAR_PEER_IP               "CLEAR_PEER_IP"
#define CMD_STR_ENABLE                      "ENABLE"
#define CMD_STR_DISABLE                     "DISABLE"
#define CMD_STR_ABORT                       "ABORT"
#define CMD_STR_CLEAR_ERRORS                "CLEAR_ERRORS"

// --- Injector Motion Commands ---
#define CMD_STR_SET_INJECTOR_TORQUE_OFFSET  "SET_INJECTOR_TORQUE_OFFSET "
#define CMD_STR_JOG_MOVE                    "JOG_MOVE "
#define CMD_STR_MACHINE_HOME_MOVE           "MACHINE_HOME_MOVE"
#define CMD_STR_CARTRIDGE_HOME_MOVE         "CARTRIDGE_HOME_MOVE"
#define CMD_STR_INJECT_MOVE                 "INJECT_MOVE "
#define CMD_STR_MOVE_TO_CARTRIDGE_HOME      "MOVE_TO_CARTRIDGE_HOME"
#define CMD_STR_MOVE_TO_CARTRIDGE_RETRACT   "MOVE_TO_CARTRIDGE_RETRACT "
#define CMD_STR_PAUSE_INJECTION             "PAUSE_INJECTION"
#define CMD_STR_RESUME_INJECTION            "RESUME_INJECTION"
#define CMD_STR_CANCEL_INJECTION            "CANCEL_INJECTION"

// --- Pinch Valve Commands ---
#define CMD_STR_INJECTION_VALVE_HOME        "INJECTION_VALVE_HOME"
#define CMD_STR_INJECTION_VALVE_OPEN        "INJECTION_VALVE_OPEN"
#define CMD_STR_INJECTION_VALVE_CLOSE       "INJECTION_VALVE_CLOSE"
#define CMD_STR_INJECTION_VALVE_JOG         "INJECTION_VALVE_JOG "
#define CMD_STR_VACUUM_VALVE_HOME           "VACUUM_VALVE_HOME"
#define CMD_STR_VACUUM_VALVE_OPEN           "VACUUM_VALVE_OPEN"
#define CMD_STR_VACUUM_VALVE_CLOSE          "VACUUM_VALVE_CLOSE"
#define CMD_STR_VACUUM_VALVE_JOG            "VACUUM_VALVE_JOG "

// --- Heater Commands ---
#define CMD_STR_HEATER_ON                   "HEATER_ON"
#define CMD_STR_HEATER_OFF                  "HEATER_OFF"
#define CMD_STR_SET_HEATER_GAINS            "SET_HEATER_GAINS "
#define CMD_STR_SET_HEATER_SETPOINT         "SET_HEATER_SETPOINT "

// --- Vacuum Commands ---
#define CMD_STR_VACUUM_ON                       "VACUUM_ON"
#define CMD_STR_VACUUM_OFF                      "VACUUM_OFF"
#define CMD_STR_VACUUM_LEAK_TEST                "VACUUM_LEAK_TEST"
#define CMD_STR_SET_VACUUM_TARGET               "SET_VACUUM_TARGET "
#define CMD_STR_SET_VACUUM_TIMEOUT_S            "SET_VACUUM_TIMEOUT_S "
#define CMD_STR_SET_LEAK_TEST_DELTA             "SET_LEAK_TEST_DELTA "
#define CMD_STR_SET_LEAK_TEST_DURATION_S        "SET_LEAK_TEST_DURATION_S "

// --- Telemetry & Status Prefixes ---
#define TELEM_PREFIX_GUI                    "INJ_TELEM_GUI:"
#define STATUS_PREFIX_INFO                  "INJ_INFO: "
#define STATUS_PREFIX_START                 "INJ_START: "
#define STATUS_PREFIX_DONE                  "INJ_DONE: "
#define STATUS_PREFIX_ERROR                 "INJ_ERROR: "
#define STATUS_PREFIX_DISCOVERY             "DISCOVERY: "

//==================================================================================================
// State Machine Enums
//==================================================================================================
// Defines the top-level operational state of the entire injector system.
enum MainState : uint8_t {
	STANDBY_MODE,       // System is idle and ready to accept commands.
	DISABLED_MODE       // System is disabled; motors will not move.
};

// Defines the active homing operation for the injector motors.
enum HomingState : uint8_t {
	HOMING_NONE,        // No homing operation is active.
	HOMING_MACHINE,     // Homing to the machine's physical zero point (fully retracted).
	HOMING_CARTRIDGE    // Homing to the front of the material cartridge.
};

// Defines the sub-state or "phase" within an active homing operation.
enum HomingPhase : uint8_t {
	HOMING_PHASE_IDLE,          // Homing is not active.
	HOMING_PHASE_STARTING_MOVE, // Initial delay to confirm motor movement has begun.
	HOMING_PHASE_RAPID_MOVE,    // High-speed move towards the hard stop.
	HOMING_PHASE_BACK_OFF,      // Moving away from the hard stop after initial contact.
	HOMING_PHASE_TOUCH_OFF,     // Slow-speed move to precisely locate the hard stop.
	HOMING_PHASE_RETRACT,       // Moving to a final, safe position after homing is complete.
	HOMING_PHASE_COMPLETE,      // The homing sequence finished successfully.
	HOMING_PHASE_ERROR          // The homing sequence failed.
};

// Defines the state of an injection or material feed operation.
enum FeedState : uint8_t {
	FEED_NONE,                  // No feed operation defined (should not be used).
	FEED_STANDBY,               // Ready to start a feed operation.
	FEED_INJECT_STARTING,       // An injection move has been commanded and is starting.
	FEED_INJECT_ACTIVE,         // An injection move is currently in progress.
	FEED_INJECT_PAUSED,         // An active injection has been paused.
	FEED_INJECT_RESUMING,       // A paused injection is resuming.
	FEED_MOVING_TO_HOME,        // Moving to the previously established cartridge home position.
	FEED_MOVING_TO_RETRACT,     // Moving to a retracted position relative to the cartridge home.
	FEED_INJECTION_CANCELLED,   // The injection was cancelled by the user.
	FEED_INJECTION_COMPLETED    // The injection finished successfully.
};

// Defines various error conditions the system can be in.
enum ErrorState : uint8_t {
	ERROR_NONE,                     // No error.
	ERROR_MANUAL_ABORT,             // Operation aborted by user command.
	ERROR_TORQUE_ABORT,             // Operation aborted due to exceeding torque limits.
	ERROR_MOTION_EXCEEDED_ABORT,    // Operation aborted because motion limits were exceeded.
	ERROR_NO_CARTRIDGE_HOME,        // A required cartridge home position is not set.
	ERROR_NO_MACHINE_HOME,          // A required machine home position is not set.
	ERROR_HOMING_TIMEOUT,           // Homing operation took too long to complete.
	ERROR_HOMING_NO_TORQUE_RAPID,   // Homing failed to detect torque during the rapid move.
	ERROR_HOMING_NO_TORQUE_TOUCH,   // Homing failed to detect torque during the touch-off move.
	ERROR_INVALID_INJECTION,        // An injection command was invalid.
	ERROR_NOT_HOMED,                // An operation required homing, but the system is not homed.
	ERROR_INVALID_PARAMETERS,       // A command was received with invalid parameters.
	ERROR_MOTORS_DISABLED           // An operation was blocked because motors are disabled.
};

// Defines the operational state of the heater.
enum HeaterState: uint8_t {
	HEATER_OFF,         // Heater is off.
	HEATER_PID_ACTIVE   // Heater is being controlled by the PID loop.
};

// Defines the operational state of the vacuum system.
enum VacuumState : uint8_t {
	VACUUM_OFF,             // The vacuum pump and valve are off.
	VACUUM_PULLDOWN,        // The pump is on, pulling down to the target pressure for a leak test.
	VACUUM_SETTLING,        // The pump is off, allowing pressure to stabilize before a leak test.
	VACUUM_LEAK_TESTING,    // The system is holding vacuum and monitoring for pressure changes.
	VACUUM_ACTIVE_HOLD,     // The pump is actively maintaining the target pressure.
	VACUUM_ERROR            // The system failed to reach target or failed a leak test.
};

// Maps all command strings to a single enum for easier parsing.
typedef enum {
	CMD_UNKNOWN,
	CMD_ENABLE,
	CMD_DISABLE,
	CMD_JOG_MOVE,
	CMD_MACHINE_HOME_MOVE,
	CMD_CARTRIDGE_HOME_MOVE,
	CMD_MOVE_TO_CARTRIDGE_HOME,
	CMD_MOVE_TO_CARTRIDGE_RETRACT,
	CMD_INJECT_MOVE,
	CMD_PAUSE_INJECTION,
	CMD_RESUME_INJECTION,
	CMD_CANCEL_INJECTION,
	CMD_SET_INJECTOR_TORQUE_OFFSET,
	CMD_DISCOVER,
	CMD_REQUEST_TELEM,
	CMD_ABORT,
	CMD_CLEAR_ERRORS,
	CMD_SET_PEER_IP,
	CMD_CLEAR_PEER_IP,
	CMD_VACUUM_ON,
	CMD_VACUUM_OFF,
	CMD_VACUUM_LEAK_TEST,
	CMD_SET_VACUUM_TARGET,
	CMD_SET_VACUUM_TIMEOUT_S,
	CMD_SET_LEAK_TEST_DELTA,
	CMD_SET_LEAK_TEST_DURATION_S,
	CMD_HEATER_ON,
	CMD_HEATER_OFF,
	CMD_SET_HEATER_GAINS,
	CMD_SET_HEATER_SETPOINT,
	CMD_INJECTION_VALVE_HOME,
	CMD_INJECTION_VALVE_OPEN,
	CMD_INJECTION_VALVE_CLOSE,
	CMD_INJECTION_VALVE_JOG,
	CMD_VACUUM_VALVE_HOME,
	CMD_VACUUM_VALVE_OPEN,
	CMD_VACUUM_VALVE_CLOSE,
	CMD_VACUUM_VALVE_JOG
} UserCommand;

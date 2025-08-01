#pragma once

#include <cstdint>
#include "ClearCore.h"
// Corrected TCP/IP and UDP Library Includes. These assume your IDE has the
// path to the library's 'inc' directory set up.
#include "EthernetTcp.h"
#include "EthernetTcpServer.h"
#include "EthernetTcpClient.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// --- System Parameters & Conversions ---
#define INJECTOR_PITCH_MM_PER_REV 5.0f
#define PINCH_PITCH_MM_PER_REV 2.0f
#define PULSES_PER_REV 800
#define STEPS_PER_MM_INJECTOR (PULSES_PER_REV / INJECTOR_PITCH_MM_PER_REV) // 160.0f
#define STEPS_PER_MM_PINCH (PULSES_PER_REV / PINCH_PITCH_MM_PER_REV)     // 400.0f
#define MAX_HOMING_DURATION_MS 30000 // 30 seconds

// --- Hardware Pin Definitions ---
#define PIN_THERMOCOUPLE ConnectorA12
#define PIN_HEATER_RELAY ConnectorIO1
#define PIN_VACUUM_RELAY ConnectorIO0
#define PIN_VACUUM_TRANSDUCER ConnectorA11
#define PIN_VACUUM_VALVE_RELAY ConnectorIO5

// --- Thermocouple Conversion Coefficients ---
#define TC_V_REF 10.0f
#define TC_V_OFFSET 1.25f
#define TC_GAIN 200.0f

// --- Control & Timing Parameters ---
#define PID_UPDATE_INTERVAL_MS 100
#define PID_PWM_PERIOD_MS 1000
#define SENSOR_SAMPLE_INTERVAL_MS 100
#define TELEMETRY_INTERVAL_MS 100 // 10Hz telemetry streaming

// --- Network Ports ---
#define LOCAL_PORT 8888       // Main TCP port
#define DISCOVERY_PORT 8889   // UDP port for discovery broadcasts

// --- Vacuum Transducer Coefficients ---
#define VAC_V_OUT_MIN 1.0f
#define VAC_V_OUT_MAX 5.0f
#define VAC_PRESSURE_MIN -14.7f
#define VAC_PRESSURE_MAX 15.0f
#define VACUUM_PSIG_OFFSET 0.0f

// --- Command Strings & Prefixes ---
#define CMD_STR_SET_PEER_IP "SET_PEER_IP "
#define CMD_STR_CLEAR_PEER_IP "CLEAR_PEER_IP"
#define CMD_STR_ENABLE "ENABLE"
#define CMD_STR_DISABLE "DISABLE"
#define CMD_STR_ABORT "ABORT"
#define CMD_STR_CLEAR_ERRORS "CLEAR_ERRORS"
#define CMD_STR_SET_INJECTOR_TORQUE_OFFSET "SET_INJECTOR_TORQUE_OFFSET "
#define CMD_STR_JOG_MOVE "JOG_MOVE "
#define CMD_STR_MACHINE_HOME_MOVE "MACHINE_HOME_MOVE"
#define CMD_STR_CARTRIDGE_HOME_MOVE "CARTRIDGE_HOME_MOVE"
#define CMD_STR_PINCH_HOME_MOVE "PINCH_HOME_MOVE"
#define CMD_STR_INJECT_MOVE "INJECT_MOVE "
#define CMD_STR_PURGE_MOVE "PURGE_MOVE "
#define CMD_STR_PINCH_JOG_MOVE "PINCH_JOG_MOVE "
#define CMD_STR_ENABLE_PINCH "ENABLE_PINCH"
#define CMD_STR_DISABLE_PINCH "DISABLE_PINCH"
#define CMD_STR_MOVE_TO_CARTRIDGE_HOME "MOVE_TO_CARTRIDGE_HOME"
#define CMD_STR_MOVE_TO_CARTRIDGE_RETRACT "MOVE_TO_CARTRIDGE_RETRACT "
#define CMD_STR_PAUSE_INJECTION "PAUSE_INJECTION"
#define CMD_STR_RESUME_INJECTION "RESUME_INJECTION"
#define CMD_STR_CANCEL_INJECTION "CANCEL_INJECTION"
#define CMD_STR_VACUUM_ON "VACUUM_ON"
#define CMD_STR_VACUUM_OFF "VACUUM_OFF"
#define CMD_STR_HEATER_ON "HEATER_ON"
#define CMD_STR_HEATER_OFF "HEATER_OFF"
#define CMD_STR_SET_HEATER_GAINS "SET_HEATER_GAINS "
#define CMD_STR_SET_HEATER_SETPOINT "SET_HEATER_SETPOINT "
#define CMD_STR_HEATER_PID_ON "HEATER_PID_ON"
#define CMD_STR_HEATER_PID_OFF "HEATER_PID_OFF"

#define TELEM_PREFIX_GUI "INJ_TELEM_GUI:"
#define STATUS_PREFIX_INFO "INJ_INFO: "
#define STATUS_PREFIX_START "INJ_START: "
#define STATUS_PREFIX_DONE "INJ_DONE: "
#define STATUS_PREFIX_ERROR "INJ_ERROR: "

#define EWMA_ALPHA_SENSORS 0.5f
#define EWMA_ALPHA_TORQUE 0.2f

#define TORQUE_SENTINEL_INVALID_VALUE -9999.0f
#define MAX_PACKET_LENGTH 512

// --- State Enums ---
enum MainState : uint8_t { STANDBY_MODE, DISABLED_MODE, MAIN_STATE_COUNT };
enum HomingState : uint8_t { HOMING_NONE, HOMING_MACHINE, HOMING_CARTRIDGE, HOMING_PINCH, HOMING_STATE_COUNT };
enum HomingPhase : uint8_t { HOMING_PHASE_IDLE, HOMING_PHASE_STARTING_MOVE, HOMING_PHASE_RAPID_MOVE, HOMING_PHASE_BACK_OFF, HOMING_PHASE_TOUCH_OFF, HOMING_PHASE_RETRACT, HOMING_PHASE_COMPLETE, HOMING_PHASE_ERROR, HOMING_PHASE_COUNT };
enum FeedState : uint8_t { FEED_NONE, FEED_STANDBY, FEED_INJECT_STARTING, FEED_INJECT_ACTIVE, FEED_INJECT_PAUSED, FEED_INJECT_RESUMING, FEED_PURGE_STARTING, FEED_PURGE_ACTIVE, FEED_PURGE_PAUSED, FEED_PURGE_RESUMING, FEED_MOVING_TO_HOME, FEED_MOVING_TO_RETRACT, FEED_INJECTION_CANCELLED, FEED_INJECTION_COMPLETED, FEED_STATE_COUNT };
enum ErrorState : uint8_t { ERROR_NONE, ERROR_MANUAL_ABORT, ERROR_TORQUE_ABORT, ERROR_MOTION_EXCEEDED_ABORT, ERROR_NO_CARTRIDGE_HOME, ERROR_NO_MACHINE_HOME, ERROR_HOMING_TIMEOUT, ERROR_HOMING_NO_TORQUE_RAPID, ERROR_HOMING_NO_TORQUE_TOUCH, ERROR_INVALID_INJECTION, ERROR_NOT_HOMED, ERROR_INVALID_PARAMETERS, ERROR_MOTORS_DISABLED, ERROR_STATE_COUNT };
enum HeaterState: uint8_t { HEATER_OFF, HEATER_MANUAL_ON, HEATER_PID_ACTIVE, HEATER_STATE_COUNT };

typedef enum {
	CMD_UNKNOWN, CMD_ENABLE, CMD_DISABLE, CMD_ENABLE_PINCH, CMD_DISABLE_PINCH,
	CMD_JOG_MOVE, CMD_PINCH_JOG_MOVE, CMD_MACHINE_HOME_MOVE, CMD_CARTRIDGE_HOME_MOVE,
	CMD_PINCH_HOME_MOVE, CMD_MOVE_TO_CARTRIDGE_HOME, CMD_MOVE_TO_CARTRIDGE_RETRACT,
	CMD_INJECT_MOVE, CMD_PURGE_MOVE, CMD_PAUSE_INJECTION, CMD_RESUME_INJECTION,
	CMD_CANCEL_INJECTION, CMD_SET_TORQUE_OFFSET, CMD_ABORT, CMD_CLEAR_ERRORS,
	CMD_SET_PEER_IP, CMD_CLEAR_PEER_IP, CMD_VACUUM_ON, CMD_VACUUM_OFF, CMD_HEATER_ON,
	CMD_HEATER_OFF, CMD_SET_HEATER_GAINS, CMD_SET_HEATER_SETPOINT, CMD_HEATER_PID_ON,
	CMD_HEATER_PID_OFF
} UserCommand;

class Injector {
	public:
	// State Enums
	MainState mainState;
	HomingState homingState;
	HomingPhase currentHomingPhase;
	FeedState feedState;
	ErrorState errorState;
	HeaterState heaterState;

	// TCP and UDP Communication Objects
	EthernetTcpServer server = EthernetTcpServer(LOCAL_PORT);
	EthernetTcpClient client; // CORRECTED CLASS NAME
	EthernetUdp discoveryUdp;
	IpAddress peerIp;
	bool peerDiscovered;

	// State variables
	bool homingMachineDone;
	bool homingCartridgeDone;
	bool homingPinchDone;
	bool feedingDone;
	bool jogDone;
	uint32_t homingStartTime;
	bool motorsAreEnabled;
	bool heaterOn;
	bool vacuumOn;
	float temperatureCelsius;
	float vacuumPressurePsig;
	float smoothedVacuumPsig;
	bool firstVacuumReading;
	float smoothedTemperatureCelsius;
	bool firstTempReading;
	uint32_t lastSensorSampleTime;
	bool vacuumValveOn;
	
	// PID state variables
	float pid_setpoint;
	float pid_kp, pid_ki, pid_kd;
	float pid_integral;
	float pid_last_error;
	uint32_t pid_last_time;
	float pid_output;

	// Homing parameters
	float homing_torque_percent_param;
	long homing_actual_stroke_steps;
	long homing_actual_retract_steps;
	int homing_actual_rapid_sps;
	int homing_actual_touch_sps;
	int homing_actual_accel_sps2;
	long homingDefaultBackoffSteps;
	
	// Feed parameters
	int feedDefaultTorquePercent;
	long feedDefaultVelocitySPS;
	long feedDefaultAccelSPS2;
	
	// Constructor
	Injector();

	// Core Functions
	void setup();
	void loop();
	void updateState();
	
	// Peripherals
	void updatePid();
	void resetPid();
	void updateVacuum();
	void updateTemperature();

	// Communication
	void sendGuiTelemetry(void);
	void setupSerial();
	void setupEthernet();
	void processTcp();
	void processDiscovery();
	void parseUserCommand(const char *msg);
	void sendStatus(const char* statusType, const char* message);
	UserCommand parseCommand(const char* msg);
	
	// Motor Functions
	void setupMotors(void);
	void setupPeripherals(void);
	void enableInjectorMotors(const char* reason_message);
	void disableInjectorMotors(const char* reason_message);
	void moveInjectorMotors(int stepsM0, int stepsM1, int torque_limit, int velocity, int accel);
	void movePinchMotor(int stepsM2, int torque_limit, int velocity, int accel);
	bool checkInjectorMoving(void);
	float getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead);
	bool checkInjectorTorqueLimit(void);
	void abortInjectorMove(void);

	// Command Handlers
	void handleMessage(const char* msg);
	void handleEnable();
	void handleDisable();
	void handleAbort();
	void handleClearErrors();
	void handleStandbyMode();
	void handleSetinjectorMotorsTorqueOffset(const char* msg);
	void handleJogMove(const char* msg);
	void handleMachineHomeMove();
	void handleCartridgeHomeMove();
	void handlePinchHomeMove();
	void handlePinchJogMove(const char* msg);
	void handleEnablePinch();
	void handleDisablePinch();
	void handleMoveToCartridgeHome();
	void handleMoveToCartridgeRetract(const char* msg);
	void handleInjectMove(const char* msg);
	void handlePurgeMove(const char* msg);
	void handlePauseOperation();
	void handleResumeOperation();
	void handleCancelOperation();
	void handleSetPeerIp(const char* msg);
	void handleClearPeerIp();
	void handleVacuumOn();
	void handleVacuumOff();
	void handleHeaterOn();
	void handleHeaterOff();
	void handleSetHeaterGains(const char* msg);
	void handleSetHeaterSetpoint(const char* msg);
	void handleHeaterPidOn();
	void handleHeaterPidOff();
	// ADDED MISSING DECLARATIONS
	void handlePinchOpen();
	void handlePinchClose();

	// State Trigger Functions
	void onHomingMachineDone(void);
	void onHomingCartridgeDone(void);
	void onHomingPinchDone(void);
	void onFeedingDone(void);
	void onJogDone(void);
	
	// Reset Functions
	void resetMotors(void);
	void finalizeAndResetActiveDispenseOperation(bool operationCompletedSuccessfully);
	void fullyResetActiveDispenseOperation(void);
	void resetActiveDispenseOp(void);

	private:
	// String converters for enums
	const char* mainStateStr() const;
	const char* homingStateStr() const;
	const char* homingPhaseStr() const;
	const char* feedStateStr() const;
	const char* errorStateStr() const;
	const char* heaterStateStr() const;

	// Timers and buffers
	uint32_t lastGuiTelemetryTime;
	uint32_t lastPidUpdateTime;
	char telemetryBuffer[512];
	char command_buffer[MAX_PACKET_LENGTH];
	int command_buffer_index;

	// Motor-specific variables
	float injectorMotorsTorqueLimit;
	float injectorMotorsTorqueOffset;
	float smoothedTorqueValue0, smoothedTorqueValue1, smoothedTorqueValue2;
	bool firstTorqueReading0, firstTorqueReading1, firstTorqueReading2;
	int32_t machineHomeReferenceSteps, cartridgeHomeReferenceSteps, pinchHomeReferenceSteps;
	
	// Active command tracking
	const char* activeFeedCommand;
	const char* activeJogCommand;

	// Dispense operation variables
	float active_op_target_ml, active_op_total_dispensed_ml, active_op_steps_per_ml, last_completed_dispense_ml;
	long active_op_total_target_steps, active_op_remaining_steps, active_op_segment_initial_axis_steps, active_op_initial_axis_steps;
	bool active_dispense_INJECTION_ongoing;
	int active_op_velocity_sps, active_op_accel_sps2, active_op_torque_percent;

	// Static arrays for enum to string conversion
	static const char *MainStateNames[MAIN_STATE_COUNT];
	static const char *HomingStateNames[HOMING_STATE_COUNT];
	static const char *HomingPhaseNames[HOMING_PHASE_COUNT];
	static const char *FeedStateNames[FEED_STATE_COUNT];
	static const char *ErrorStateNames[ERROR_STATE_COUNT];
	static const char *HeaterStateNames[HEATER_STATE_COUNT];
};

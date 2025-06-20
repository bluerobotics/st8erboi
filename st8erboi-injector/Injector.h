// ============================================================================
// injector.h
//
// Public interface for the Injector subsystem.
//
// Declares the Injector class and related enums that manage system states,
// error tracking, motion phases, and command handling for a ClearCore-driven
// fluid injector system.
//
// Exposes methods for external communication (UDP), motor control,
// operational state transitions (e.g., homing, jog, feed), and runtime telemetry.
//
// All logic implementations reside in the `injector_*.cpp` source files.
//
// Copyright 2025 Blue Robotics Inc.
// Author: Eldin Miller-Stead <eldin@bluerobotics.com>
// ============================================================================

#pragma once

#include <cstdint>
#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- State Enums ---
enum MainState : uint8_t {
	STANDBY_MODE,
	TEST_MODE,
	JOG_MODE,
	HOMING_MODE,
	FEED_MODE,
	DISABLED_MODE,
	MAIN_STATE_COUNT
};

enum HomingState : uint8_t {
	HOMING_NONE,
	HOMING_MACHINE,
	HOMING_CARTRIDGE,
	HOMING_STATE_COUNT
};

enum HomingPhase : uint8_t {
	HOMING_PHASE_IDLE,
	HOMING_PHASE_RAPID_MOVE,
	HOMING_PHASE_BACK_OFF,
	HOMING_PHASE_TOUCH_OFF,
	HOMING_PHASE_RETRACT,
	HOMING_PHASE_COMPLETE,
	HOMING_PHASE_ERROR,
	HOMING_PHASE_COUNT
};

enum FeedState : uint8_t {
	FEED_NONE,
	FEED_STANDBY,
	FEED_INJECT_STARTING,   // Brief state while initiating inject
	FEED_INJECT_ACTIVE,     // Actively injecting
	FEED_INJECT_PAUSED,     // Injection is paused
	FEED_INJECT_RESUMING,   // Brief state while initiating resume
	FEED_PURGE_STARTING,    // Brief state while initiating purge
	FEED_PURGE_ACTIVE,      // Actively purging
	FEED_PURGE_PAUSED,      // Purge is paused
	FEED_PURGE_RESUMING,    // Brief state while initiating resume
	FEED_MOVING_TO_HOME,
	FEED_MOVING_TO_RETRACT,
	FEED_INJECTION_CANCELLED,
	FEED_INJECTION_COMPLETED, // Intermediate state before going to STANDBY
	FEED_STATE_COUNT
};

enum ErrorState : uint8_t {
	ERROR_NONE,
	ERROR_MANUAL_ABORT,
	ERROR_TORQUE_ABORT,
	ERROR_MOTION_EXCEEDED_ABORT,
	ERROR_NO_CARTRIDGE_HOME,
	ERROR_NO_MACHINE_HOME,
	ERROR_HOMING_TIMEOUT,
	ERROR_HOMING_NO_TORQUE_RAPID,
	ERROR_HOMING_NO_TORQUE_TOUCH,
	ERROR_INVALID_INJECTION,
	ERROR_NOT_HOMED,
	ERROR_STATE_COUNT
};

//--- User Command Enum---//
typedef enum {
	USER_CMD_UNKNOWN = 0,
	USER_CMD_DISCOVER_TELEM,
	USER_CMD_ENABLE,
	USER_CMD_DISABLE,
	USER_CMD_ABORT,
	USER_CMD_CLEAR_ERRORS,
	USER_CMD_STANDBY_MODE,
	USER_CMD_TEST_MODE,
	USER_CMD_JOG_MODE,
	USER_CMD_HOMING_MODE,
	USER_CMD_FEED_MODE,
	USER_CMD_SET_TORQUE_OFFSET,
	USER_CMD_JOG_MOVE,
	USER_CMD_MACHINE_HOME_MOVE,
	USER_CMD_CARTRIDGE_HOME_MOVE,
	USER_CMD_INJECT_MOVE,
	USER_CMD_PURGE_MOVE,
	USER_CMD_MOVE_TO_CARTRIDGE_HOME,
	USER_CMD_MOVE_TO_CARTRIDGE_RETRACT,
	USER_CMD_PAUSE_INJECTION,
	USER_CMD_RESUME_INJECTION,
	USER_CMD_CANCEL_INJECTION,
	USER_CMD_PINCH_HOME_MOVE,
	USER_CMD_PINCH_OPEN,
	USER_CMD_PINCH_CLOSE,
	USER_CMD_HOME_X,
	USER_CMD_HOME_Y,
	USER_CMD_HOME_Z,
	USER_CMD_START_CYCLE,
	USER_CMD_PAUSE_CYCLE,
	USER_CMD_RESUME_CYCLE,
	USER_CMD_CANCEL_CYCLE,
	USER_CMD_COUNT
} UserCommand;

//--- Fillhead Command Enum---//
typedef enum {
	FILLHEAD_CMD_UNKNOWN = 0,
	FILLHEAD_CMD_HOME_X,
	FILLHEAD_CMD_HOME_Y,
	FILLHEAD_CMD_HOME_Z,
	FILLHEAD_CMD_MOVE_X,
	FILLHEAD_CMD_MOVE_Y,
	FILLHEAD_CMD_MOVE_Z,
	FILLHEAD_CMD_COUNT
} FillheadCommand;

extern const char *MainStateNames[MAIN_STATE_COUNT];
extern const char *HomingStateNames[HOMING_STATE_COUNT];
extern const char *HomingPhaseNames[HOMING_PHASE_COUNT];
extern const char *FeedStateNames[FEED_STATE_COUNT];
extern const char *ErrorStateNames[ERROR_STATE_COUNT];

class Injector {
	public:
	
	// State Variables
	MainState mainState;
	HomingState homingState;
	HomingPhase currentHomingPhase;
	FeedState feedState;
	ErrorState errorState;
	
	//--- Ethernet Variables ---//
	EthernetUdp Udp;
	const uint16_t MAX_PACKET_LENGTH;
	const uint8_t LOCAL_PORT;
	unsigned char packetBuffer[];
	IpAddress terminalIp;
	uint16_t terminalPort;
	bool terminalDiscovered;

	//--- System Flags ---//
	bool homingMachineDone;
	bool homingCartridgeDone;
	bool feedingDone;
	bool jogDone;
	bool pinchPosition; // 0 = open, 1 = close
	uint32_t homingStartTime;
	
	//--- Motor Variables ---//
	int velocityLimit = 10000;
	int accelerationLimit = 1000000;
	float injectorMotorsTorqueLimit;    
	float injectorMotorsTorqueOffset;
	float smoothedTorqueValue1;
	float smoothedTorqueValue2;
	bool firstTorqueReading1;
	bool firstTorqueReading2;
	bool motorsAreEnabled;
	uint32_t pulsesPerRev;
	int32_t machineStepCounter;
	int32_t cartridgeStepCounter;
	int32_t machineHomeReferenceSteps;
	int32_t cartridgeHomeReferenceSteps;
	float PITCH_MM_PER_REV_CONST = 5.0f; // 5mm
	float STEPS_PER_MM_CONST = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;

	extern int32_t machineHomeReferenceSteps;
	extern int32_t cartridgeHomeReferenceSteps;

	const int FEED_GOTO_VELOCITY_SPS = 2000;  // Example speed for positioning moves
	const int FEED_GOTO_ACCEL_SPS2 = 10000; // Example acceleration
	const int FEED_GOTO_TORQUE_PERCENT = 40; // Example torque

	//--- Feed Operation Parameters ---//
	float homing_stroke_mm_param;
	float homing_rapid_vel_mm_s_param;
	float homing_touch_vel_mm_s_param;
	float homing_acceleration_param;
	float homing_retract_mm_param;
	float homing_torque_percent_param;
	long homing_actual_stroke_steps;
	long homing_actual_retract_steps;
	int homing_actual_rapid_sps;
	int homing_actual_touch_sps;
	int homing_actual_accel_sps2;
	static const long HOMING_DEFAULT_BACK_OFF_STEPS = 200;

	//--- Feed Operation Variables ---//
	float active_op_target_ml;
	float active_op_total_dispensed_ml; // Total dispensed for the current multi-segment operation
	long active_op_total_target_steps;  // Original total steps for the operation
	long active_op_remaining_steps;     // Remaining steps when paused
	long active_op_segment_initial_axis_steps; // Motor steps at start of current segment (after resume)
	float active_op_steps_per_ml;
	bool active_dispense_INJECTION_ongoing; // Overall flag for inject/purge sequence
	long active_op_initial_axis_steps;
	float last_completed_dispense_ml;
	int active_op_velocity_sps;
	int active_op_accel_sps2;
	int active_op_torque_percent;


	Injector();
	void reset();
	
	//--- Ethernet Functions ---//
	UserCommand parseUserCommand(const char *msg);
	void setupEthernet(void);
	void setupUsbSerial(void);
	void checkUdpBuffer(void);
	void sendToPC(const char *msg);
	void handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp);
	void sendTelem(Injector *injector);
	void handleMessage(const char *msg);
	
	//--- State Trigger Functions ---//	
	void onHomingMachineDone();
	void onHomingCartridgeDone();
	void onFeedingDone();
	void onJogDone();
	
	//--- Motor Functions ---//	
	void setupInjectorMotors(void); 																	// Initializes and enables both motors for use
	void enableInjectorMotors(const char* reason_message); 												// Enables both motors and waits for HLFB asserted
	void disableInjectorMotors(const char* reason_message); 											// Disables both motors and resets torque smoothing
	void moveInjectorMotors(int stepsM0, int stepsM1, int torque_limit, int velocity, int accel); 		// The 'torque_limit' parameter will set the injectorMotorsTorqueLimit for checkInjectorTorqueLimit.
	bool checkInjectorMoving(void); 																	// Returns true if either motor is currently moving or not in-position
	float getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead);				// Returns smoothed, offset torque value (EWMA filter)
	bool checkInjectorTorqueLimit(void);																// Checks if the output of getSmoothedTorqueEWMA exceeds injectorMotorsTorqueLimit
	void abortInjectorMove(void);																		// Abruptly stops all motor motion
	
	//--- Message Handler Functions ---//
	void handleEnable();
	void handleDisable();
	void handleAbort(); // Will call finalizeAndResetActiveDispenseOperation
	void handleClearErrors();
	void handleStandbyMode();
	void handleTestMode();
	void handleJogMode();
	void handleHomingMode();
	void handleFeedMode();
	void handleSetinjectorMotorsTorqueOffset(const char *msg);
	void handleJogMove(const char *msg);
	void handleMachineHomeMove(const char *msg);
	void handleCartridgeHomeMove(const char *msg);
	void handleInjectMove(const char *msg);
	void handlePurgeMove(const char *msg);
	void handleMoveToCartridgeHome();
	void handleMoveToCartridgeRetract(const char *msg);
	void handlePauseOperation();
	void handleResumeOperation();
	void handleCancelOperation();
	
	//--- Reset Functions ---//
	void resetMotors(void); 	// Disables, clears alerts, resets counters, and re-enables both motors
	void finalizeAndResetActiveDispenseOperation(bool operationCompletedSuccessfully);
	void fullyResetActiveDispenseOperation();

	//--- Pinch Valve ---//
	void handlePinchHomeMove();
	void handlePinchOpen();
	void handlePinchClose();

	const char* maininjectortr() const;
	const char* hominginjectortr() const;
	const char* homingPhaseStr() const;
	const char* feedinjectortr() const;
	const char* errorinjectortr() const;
};
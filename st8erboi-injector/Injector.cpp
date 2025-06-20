// ============================================================================
// injector.cpp
//
// Core initialization and runtime state management for the Injector class.
//
// Implements constructor logic, state tracking, and string mapping utilities
// for translating internal enum states into telemetry and UI-friendly outputs.
//
// Contains basic system reset logic and internal flag transitions
// for mode entry/exit coordination.
//
// Corresponds to declarations in `injector.h`.
//
// Copyright 2025 Blue Robotics Inc.
// Author: Eldin Miller-Stead <eldin@bluerobotics.com>
// ============================================================================

#include "injector.h"
#include <math.h>

const char *MainStateNames[MAIN_STATE_COUNT] = {
	"STANDBY_MODE",
	"TEST_MODE",
	"JOG_MODE",
	"HOMING_MODE",
	"FEED_MODE",
	"DISABLED_MODE"
};

const char *HomingStateNames[HOMING_STATE_COUNT] = {
	"HOMING_NONE",
	"HOMING_MACHINE_ACTIVE",
	"HOMING_CARTRIDGE_ACTIVE"
};

const char *HomingPhaseNames[HOMING_PHASE_COUNT] = {
	"IDLE",
	"RAPID_MOVE",
	"BACK_OFF",
	"TOUCH_OFF",
	"RETRACT",
	"COMPLETE",
	"ERROR"
};

const char *FeedStateNames[FEED_STATE_COUNT] = {
	"FEED_NONE",
	"FEED_STANDBY",
	"FEED_INJECT_STARTING",
	"FEED_INJECT_ACTIVE",
	"FEED_INJECT_PAUSED",
	"FEED_INJECT_RESUMING",
	"FEED_PURGE_STARTING",
	"FEED_PURGE_ACTIVE",
	"FEED_PURGE_PAUSED",
	"FEED_PURGE_RESUMING",
	"FEED_MOVING_TO_HOME",
	"FEED_MOVING_TO_RETRACT",
	"FEED_OP_CANCELLED",
	"FEED_OP_COMPLETED"
};

const char *ErrorStateNames[ERROR_STATE_COUNT] = {
	"ERROR_NONE",
	"ERROR_MANUAL_ABORT",
	"ERROR_TORQUE_ABORT",
	"ERROR_MOTION_EXCEEDED_ABORT",
	"ERROR_NO_CARTRIDGE_HOME",
	"ERROR_NO_MACHINE_HOME",
	"ERROR_HOMING_TIMEOUT",
	"ERROR_HOMING_NO_TORQUE_RAPID",
	"ERROR_HOMING_NO_TORQUE_TOUCH",
	"ERROR_INVALID_INJECTION",
	"ERROR_NOT_HOMED"
};

Injector::Injector(){
	reset();
}

void Injector::reset() {
	setupEthernet();
	setupInjectorMotors();
	//setupUsbSerial();
	//Delay_ms(5000);
	//ConnectorUsb.SendLine("main: Delay finished. Proceeding with setup...");
	
	//--- State Variables ---//
	mainState = STANDBY_MODE;
	homingState = HOMING_NONE;
	currentHomingPhase = HOMING_PHASE_IDLE;
	feedState = FEED_STANDBY;
	errorState = ERROR_NONE;
	homingMachineDone = false;
	homingCartridgeDone = false;
	feedingDone = false;
	jogDone = false;
	homingStartTime = 0;
	
	//--- Ethernet Variables ---//
	MAX_PACKET_LENGTH = 100;
	LOCAL_PORT = 8888;
	EthernetUdp Udp;
	unsigned char packetBuffer[MAX_PACKET_LENGTH];
	IpAddress terminalIp;
	uint16_t terminalPort = 0;
	bool terminalDiscovered = false;
	const uint32_t telemInterval = 50; // ms
	
	// Motor Variables
	float injectorMotorsTorqueLimit		= 2.0f;   // Default overall limit, overridden by moveInjectorMotors's param
	float injectorMotorsTorqueOffset	= -2.4f;  // Global offset, settable by USER_CMD_SET_TORQUE_OFFSET
	float smoothedTorqueValue1			= 0.0f;
	float smoothedTorqueValue2			= 0.0f;
	float smoothedTorqueValue3			= 0.0f;
	bool firstTorqueReading1			= true;
	bool firstTorqueReading2			= true;
	bool firstTorqueReading3			= true;
	bool motorsAreEnabled				= false;
	uint32_t pulsesPerRev				= 800;
	int32_t machineStepCounter			= 0;
	int32_t cartridgeStepCounter		= 0;
	int32_t velocityLimit				= 10000;  // pulses/sec
	int32_t accelerationLimit			= 100000; // pulses/sec^2

	int32_t machineHomeReferenceSteps = 0;
	int32_t cartridgeHomeReferenceSteps = 0;


	// Homing params
	homing_stroke_mm_param = 0.0f; homing_rapid_vel_mm_s_param = 0.0f; homing_touch_vel_mm_s_param = 0.0f;
	homing_acceleration_param = 0.0f; homing_retract_mm_param = 0.0f; homing_torque_percent_param = 0.0f;
	homing_actual_stroke_steps = 0; homing_actual_retract_steps = 0; homing_actual_rapid_sps = 0;
	homing_actual_touch_sps = 0; homing_actual_accel_sps2 = 0;

	// Dispensing / Feed Operation Tracking
	active_op_target_ml = 0.0f;
	active_op_total_dispensed_ml = 0.0f;
	active_op_total_target_steps = 0;
	active_op_remaining_steps = 0;
	active_op_segment_initial_axis_steps = 0;
	active_op_steps_per_ml = 0.0f;
	active_dispense_INJECTION_ongoing = false;
	active_op_velocity_sps = 0;
	active_op_accel_sps2 = 0;
	active_op_torque_percent = 0;
	active_op_initial_axis_steps = 0;
	last_completed_dispense_ml = 0;
}

void Injector::onHomingMachineDone(){
	homingMachineDone = true;
}

void Injector::onHomingCartridgeDone(){
	homingCartridgeDone = true;
}

void Injector::onFeedingDone(){
	feedingDone = true;
}

void Injector::onJogDone(){
	jogDone = true;
}

const char* Injector::maininjectortr()   const { return MainStateNames[mainState]; }
const char* Injector::hominginjectortr() const { return HomingStateNames[homingState]; }
const char* Injector::homingPhaseStr() const { return HomingPhaseNames[currentHomingPhase]; }
const char* Injector::feedinjectortr()   const { return FeedStateNames[feedState]; }
const char* Injector::errorinjectortr()  const { return ErrorStateNames[errorState]; }
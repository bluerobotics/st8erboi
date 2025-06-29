#pragma once

#include <cstdint>
#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define USER_CMD_STR_REQUEST_TELEM  "REQUEST_TELEM"
#define USER_CMD_STR_DISCOVER_TELEM					"DISCOVER_INJECTOR"
#define USER_CMD_STR_ENABLE							"ENABLE"
#define USER_CMD_STR_DISABLE						"DISABLE"
#define USER_CMD_STR_ABORT							"ABORT"
#define USER_CMD_STR_CLEAR_ERRORS					"CLEAR_ERRORS"
#define USER_CMD_STR_STANDBY_MODE					"STANDBY_MODE"
#define USER_CMD_STR_JOG_MODE						"JOG_MODE"
#define USER_CMD_STR_HOMING_MODE					"HOMING_MODE"
#define USER_CMD_STR_FEED_MODE						"FEED_MODE"
#define USER_CMD_STR_SET_INJECTOR_TORQUE_OFFSET		"SET_INJECTOR_TORQUE_OFFSET "
#define USER_CMD_STR_SET_PINCH_TORQUE_OFFSET		"SET_PINCH_TORQUE_OFFSET "
#define USER_CMD_STR_JOG_MOVE						"JOG_MOVE "
#define USER_CMD_STR_MACHINE_HOME_MOVE				"MACHINE_HOME_MOVE "
#define USER_CMD_STR_CARTRIDGE_HOME_MOVE			"CARTRIDGE_HOME_MOVE "
#define USER_CMD_STR_PINCH_HOME_MOVE				"PINCH_HOME_MOVE"
#define USER_CMD_STR_INJECT_MOVE					"INJECT_MOVE "
#define USER_CMD_STR_PURGE_MOVE						"PURGE_MOVE "
#define USER_CMD_STR_PINCH_OPEN						"PINCH_OPEN"
#define USER_CMD_STR_PINCH_CLOSE					"PINCH_CLOSE"
#define USER_CMD_STR_PINCH_JOG_MOVE				    "PINCH_JOG_MOVE "
#define USER_CMD_STR_ENABLE_PINCH					"ENABLE_PINCH"
#define USER_CMD_STR_DISABLE_PINCH					"DISABLE_PINCH"
#define USER_CMD_STR_MOVE_TO_CARTRIDGE_HOME			"MOVE_TO_CARTRIDGE_HOME"
#define USER_CMD_STR_MOVE_TO_CARTRIDGE_RETRACT		"MOVE_TO_CARTRIDGE_RETRACT "
#define USER_CMD_STR_PAUSE_INJECTION				"PAUSE_INJECTION"
#define USER_CMD_STR_RESUME_INJECTION				"RESUME_INJECTION"
#define USER_CMD_STR_CANCEL_INJECTION				"CANCEL_INJECTION"
#define TELEM_PREFIX_GUI "INJ_TELEM_GUI:"

#define EWMA_ALPHA 0.2f
#define TORQUE_SENTINEL_INVALID_VALUE -9999.0f
#define MAX_PACKET_LENGTH 100
#define LOCAL_PORT 8888

// --- State Enums ---
enum MainState : uint8_t {
	STANDBY_MODE,
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
	HOMING_PHASE_STARTING_MOVE,
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
	FEED_INJECT_STARTING,
	FEED_INJECT_ACTIVE,
	FEED_INJECT_PAUSED,
	FEED_INJECT_RESUMING,
	FEED_PURGE_STARTING,
	FEED_PURGE_ACTIVE,
	FEED_PURGE_PAUSED,
	FEED_PURGE_RESUMING,
	FEED_MOVING_TO_HOME,
	FEED_MOVING_TO_RETRACT,
	FEED_INJECTION_CANCELLED,
	FEED_INJECTION_COMPLETED,
	FEED_STATE_COUNT
};

// --- MODIFIED ENUM ---
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
	ERROR_INVALID_PARAMETERS, // ADDED
	ERROR_MOTORS_DISABLED,    // ADDED
	ERROR_STATE_COUNT
};

//--- User Command Enum---//
typedef enum {
	USER_CMD_UNKNOWN = 0,
	USER_CMD_REQUEST_TELEM,
	USER_CMD_DISCOVER_TELEM,
	USER_CMD_ENABLE,
	USER_CMD_DISABLE,
	USER_CMD_ABORT,
	USER_CMD_CLEAR_ERRORS,
	USER_CMD_STANDBY_MODE,
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
	USER_CMD_PINCH_JOG_MOVE,
	USER_CMD_ENABLE_PINCH,
	USER_CMD_DISABLE_PINCH,
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
	IpAddress terminalIp;
	uint16_t terminalPort;
	bool terminalDiscovered;

	//--- System Flags ---//
	bool homingMachineDone;
	bool homingCartridgeDone;
	bool homingPinchDone;
	bool feedingDone;
	bool jogDone;
	bool pinchPosition; // 0 = open, 1 = close
	uint32_t homingStartTime;
	
	//--- Motor Variables ---//
	int velocityLimit;
	int accelerationLimit;
	float injectorMotorsTorqueLimit;
	float injectorMotorsTorqueOffset;
	float smoothedTorqueValue0;
	float smoothedTorqueValue1;
	float smoothedTorqueValue2;
	bool firstTorqueReading0;
	bool firstTorqueReading1;
	bool firstTorqueReading2;
	bool motorsAreEnabled;
	uint32_t pulsesPerRev;
	int32_t machineStepCounter;
	int32_t cartridgeStepCounter;
	int32_t machineHomeReferenceSteps;
	int32_t cartridgeHomeReferenceSteps;
	float PITCH_MM_PER_REV;
	float STEPS_PER_MM = (float)pulsesPerRev / PITCH_MM_PER_REV;
	int feedDefaultTorquePercent;
	int feedDefaultVelocitySPS;
	int feedDefaultAccelSPS2;
	uint32_t telemInterval;

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
	long homingDefaultBackoffSteps;

	//--- Feed Operation Variables ---//
	float active_op_target_ml;
	float active_op_total_dispensed_ml;
	long active_op_total_target_steps;
	long active_op_remaining_steps;
	long active_op_segment_initial_axis_steps;
	float active_op_steps_per_ml;
	bool active_dispense_INJECTION_ongoing;
	long active_op_initial_axis_steps;
	float last_completed_dispense_ml;
	int active_op_velocity_sps;
	int active_op_accel_sps2;
	int active_op_torque_percent;


	Injector();
	void reset(void);
	
	//--- Ethernet Functions ---//
	UserCommand parseUserCommand(const char *msg);
	void setupEthernet(void);
	void setupUsbSerial(void);
	void checkUdpBuffer(void);
	void sendToPC(const char *msg);
	void handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp);
	void sendTelem(void);
	void handleMessage(const char *msg);
	
	//--- State Trigger Functions ---//
	void onHomingMachineDone(void);
	void onHomingCartridgeDone(void);
	void onFeedingDone(void);
	void onJogDone(void);
	
	//--- Motor Functions ---//
	void setupInjectorMotors(void);
	void enableInjectorMotors(const char* reason_message);
	void disableInjectorMotors(const char* reason_message);
	void moveInjectorMotors(int stepsM0, int stepsM1, int torque_limit, int velocity, int accel);
	void movePinchMotor(int stepsM3, int torque_limit, int velocity, int accel);
	bool checkInjectorMoving(void);
	float getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead);
	bool checkInjectorTorqueLimit(void);
	void abortInjectorMove(void);
	
	//--- Message Handler Functions ---//
	void handleEnable(void);
	void handleDisable(void);
	void handleAbort(void);
	void handleClearErrors(void);
	void handleStandbyMode(void);
	void handleJogMode(void);
	void handleHomingMode(void);
	void handleFeedMode(void);
	void handleSetinjectorMotorsTorqueOffset(const char *msg);
	void handleJogMove(const char *msg);
	void handleMachineHomeMove(const char *msg);
	void handleCartridgeHomeMove(const char *msg);
	void handleInjectMove(const char *msg);
	void handlePurgeMove(const char *msg);
	void handleMoveToCartridgeHome();
	void handleMoveToCartridgeRetract(const char *msg);
	void handlePauseOperation(void);
	void handleResumeOperation(void);
	void handleCancelOperation(void);
	
	//--- Reset Functions ---//
	void resetMotors(void);
	void finalizeAndResetActiveDispenseOperation(bool operationCompletedSuccessfully);
	void fullyResetActiveDispenseOperation(void);
	void resetActiveDispenseOp(void);

	//--- Pinch Valve ---//
	void handlePinchHomeMove(void);
	void handlePinchJogMove(const char *msg);
	void handlePinchOpen(void);
	void handlePinchClose(void);
	void handleEnablePinch(void);
	void handleDisablePinch(void);
	
	static const char *MainStateNames[MAIN_STATE_COUNT];
	static const char *HomingStateNames[HOMING_STATE_COUNT];
	static const char *HomingPhaseNames[HOMING_PHASE_COUNT];
	static const char *FeedStateNames[FEED_STATE_COUNT];
	static const char *ErrorStateNames[ERROR_STATE_COUNT];

	const char* mainStateStr() const;
	const char* homingStateStr() const;
	const char* homingPhaseStr() const;
	const char* feedStateStr() const;
	const char* errorStateStr() const;
	
	unsigned char packetBuffer[MAX_PACKET_LENGTH];
};
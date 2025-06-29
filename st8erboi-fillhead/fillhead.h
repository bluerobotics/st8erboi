#pragma once

#include <cstdint>
#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Network Configuration ---
#define LOCAL_PORT 8888
#define MAX_PACKET_LENGTH 128

// --- Motor Configuration ---
#define MotorX  ConnectorM0
#define MotorY1 ConnectorM1
#define MotorY2 ConnectorM2
#define MotorZ  ConnectorM3
#define ALL_FILLHEAD_MOTORS MotorManager::MOTOR_ALL

// --- Command Definitions ---
#define CMD_STR_REQUEST_TELEM     "REQUEST_TELEM"
#define CMD_STR_DISCOVER          "DISCOVER_FILLHEAD"
#define CMD_STR_SET_PEER_IP       "SET_PEER_IP "
#define CMD_STR_CLEAR_PEER_IP     "CLEAR_PEER_IP"
#define CMD_STR_ABORT             "ABORT"
#define CMD_STR_MOVE_X            "MOVE_X "
#define CMD_STR_MOVE_Y            "MOVE_Y "
#define CMD_STR_MOVE_Z            "MOVE_Z "
#define CMD_STR_HOME_X            "HOME_X "
#define CMD_STR_HOME_Y            "HOME_Y "
#define CMD_STR_HOME_Z            "HOME_Z "

// --- Telemetry & Status Prefixes ---
#define TELEM_PREFIX_GUI      "FH_TELEM_GUI:"
#define STATUS_PREFIX_INFO    "INFO:"
#define STATUS_PREFIX_DONE    "DONE:"
#define STATUS_PREFIX_ERROR   "ERROR:"
#define EWMA_ALPHA 0.2f
#define SLOW_CODE_INTERVAL_MS 50

// Conversion factors for real-world units
// 800 steps/rev / 2mm/rev = 400 steps/mm
#define STEPS_PER_MM_X 400.0f
#define STEPS_PER_MM_Y 400.0f
#define STEPS_PER_MM_Z 400.0f

typedef enum { STATE_STANDBY, STATE_STARTING_MOVE, STATE_MOVING, STATE_HOMING } FillheadState;
typedef enum { CMD_UNKNOWN, CMD_REQUEST_TELEM, CMD_DISCOVER, CMD_SET_PEER_IP, CMD_CLEAR_PEER_IP, CMD_ABORT, CMD_MOVE_X, CMD_MOVE_Y, CMD_MOVE_Z, CMD_HOME_X, CMD_HOME_Y, CMD_HOME_Z } FillheadCommand;

// --- MODIFIED ENUM ---
// Added specific wait states for robust homing logic
typedef enum {
    HOMING_IDLE,
    HOMING_START,
    HOMING_WAIT_FOR_RAPID_START,
    HOMING_RAPID,
    HOMING_BACK_OFF,
    HOMING_WAIT_FOR_TOUCH_START,
    HOMING_TOUCH,
    HOMING_COMPLETE,
    HOMING_ERROR
} HomingPhase;

class Fillhead {
	public:
	EthernetUdp Udp;
	IpAddress guiIp;
	uint16_t guiPort;
	bool guiDiscovered;
	IpAddress peerIp;
	bool peerDiscovered;

	FillheadState state;
	HomingPhase homingPhase;
	MotorDriver* activeMotor1;
	MotorDriver* activeMotor2;

	int homingTorque;
	int homingRapidSps;
	int homingTouchSps;
	long homingBackoffSteps;
	long homingDistanceSteps;

	float torqueLimit;
	float torqueOffset;
	float smoothedTorqueM0, smoothedTorqueM1, smoothedTorqueM2, smoothedTorqueM3;
	bool firstTorqueReadM0, firstTorqueReadM1, firstTorqueReadM2, firstTorqueReadM3;

	bool homedX, homedY1, homedY2, homedZ;

	Fillhead();

	void setup();
	void loop();
	void setupEthernet();
	void setupMotors();
	void setupUsbSerial();

	void processUdp();
	void updateState();
	void sendGuiTelemetry();
	void sendInternalTelemetry();
	
	void handleMessage(const char* msg);
	void handleMove(const char* msg);
	void handleHome(const char* msg);
	void handleSetPeerIp(const char* msg);
	void handleClearPeerIp();
	void handleAbort();

	private:
	FillheadCommand parseCommand(const char* msg);
	bool isMoving();
	void sendStatus(const char* statusType, const char* message);
	const char* stateToString();
	void abortAllMoves();
	void moveAxis(MotorDriver* motor, long steps, int vel, int accel, int torque);
	float getSmoothedTorque(MotorDriver* motor, float* smoothedValue, bool* firstRead);
	bool checkTorqueLimit(MotorDriver* motor);
	
	// interval
	bool checkSlowCodeInterval();
	uint32_t lastGuiTelemetryTime;
	uint32_t lastGuiMessageTime;
	char telemetryBuffer[256]; // Moved from stack to static memory

	unsigned char packetBuffer[MAX_PACKET_LENGTH];
};
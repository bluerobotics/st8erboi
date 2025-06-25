#pragma once

#include <cstdint>
#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Network Configuration ---
#define INJECTOR_IP "192.168.1.114" // The "brain" controller IP
#define LOCAL_PORT 8888
#define MAX_PACKET_LENGTH 100

// --- Motor Configuration ---
#define MotorX ConnectorM0
#define MotorY ConnectorM1
#define MotorZ ConnectorM2
// --- MODIFIED: Corrected enum for specifying multiple motors ---
#define ALL_FILLHEAD_MOTORS MotorManager::MOTOR_ALL

// --- Command Definitions ---
#define CMD_STR_DISCOVER        "DISCOVER_TELEM"
#define CMD_STR_MOVE_X          "MOVE_X "
#define CMD_STR_MOVE_Y          "MOVE_Y "
#define CMD_STR_MOVE_Z          "MOVE_Z "
#define CMD_STR_HOME_X          "HOME_X"
#define CMD_STR_HOME_Y          "HOME_Y"
#define CMD_STR_HOME_Z          "HOME_Z"

// --- Telemetry Prefixes ---
#define TELEM_PREFIX_GUI        "FH_TELEM_GUI:" // Added colon for easier parsing
#define TELEM_PREFIX_INTERNAL   "FH_TELEM_INT:" // Added colon

typedef enum {
	CMD_UNKNOWN,
	CMD_DISCOVER,
	CMD_MOVE_X,
	CMD_MOVE_Y,
	CMD_MOVE_Z,
	CMD_HOME_X,
	CMD_HOME_Y,
	CMD_HOME_Z
} FillheadCommand;

class Fillhead {
	public:
	EthernetUdp Udp;
	IpAddress guiIp;
	uint16_t guiPort;
	bool guiDiscovered;
	IpAddress injectorIp;

	bool homedX, homedY, homedZ;

	Fillhead();

	void setup();
	void setupEthernet();
	void setupMotors();
	void setupUsbSerial();

	void processUdp();
	void sendInternalTelemetry();
	void sendGuiTelemetry();
	
	void handleMessage(const char* msg);
	void handleMove(MotorDriver* motor, const char* msg);
	void handleHome(MotorDriver* motor, bool* homedFlag);

	private:
	FillheadCommand parseCommand(const char* msg);
	bool isMoving();
	unsigned char packetBuffer[MAX_PACKET_LENGTH];
};
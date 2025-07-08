#pragma once

#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include "config.h"
#include "axis.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- UPDATED: Command parsing enum ---
typedef enum {
	CMD_UNKNOWN, CMD_REQUEST_TELEM, CMD_DISCOVER, CMD_SET_PEER_IP,
	CMD_CLEAR_PEER_IP, CMD_ABORT, CMD_MOVE_X, CMD_MOVE_Y, CMD_MOVE_Z,
	CMD_HOME_X, CMD_HOME_Y, CMD_HOME_Z,
	// Add the new command identifiers here
	CMD_ENABLE_X, CMD_DISABLE_X, CMD_ENABLE_Y, CMD_DISABLE_Y,
	CMD_ENABLE_Z, CMD_DISABLE_Z
} FillheadCommand;

class Fillhead {
	public:
	// Constructor
	Fillhead();

	// Core Functions
	void setup();
	void update();

	// Public for Axis to use
	void sendStatus(const char* statusType, const char* message);

	private:
	// Setup
	void setupEthernet();
	void setupUsbSerial();

	// Communication
	void processUdp();
	void handleMessage(const char* msg);
	void sendGuiTelemetry();

	// Command Handlers
	void handleSetPeerIp(const char* msg);
	void handleClearPeerIp();
	void abortAll();
	
	// Helper
	FillheadCommand parseCommand(const char* msg);

	// Member Variables
	EthernetUdp m_udp;
	IpAddress m_guiIp;
	uint16_t m_guiPort;
	bool m_guiDiscovered;
	IpAddress m_peerIp;
	bool m_peerDiscovered;
	
	// Axis objects
	Axis xAxis;
	Axis yAxis;
	Axis zAxis;

	// Buffers
	char m_telemetryBuffer[300];
	unsigned char m_packetBuffer[MAX_PACKET_LENGTH];
};
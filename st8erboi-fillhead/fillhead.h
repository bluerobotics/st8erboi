#pragma once

#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include "config.h"
#include "axis.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Command parsing enum ---
typedef enum {
	CMD_UNKNOWN, CMD_REQUEST_TELEM, CMD_DISCOVER, CMD_SET_PEER_IP,
	CMD_CLEAR_PEER_IP, CMD_ABORT, CMD_MOVE_X, CMD_MOVE_Y, CMD_MOVE_Z,
	CMD_HOME_X, CMD_HOME_Y, CMD_HOME_Z,
	CMD_ENABLE_X, CMD_DISABLE_X, CMD_ENABLE_Y, CMD_DISABLE_Y,
	CMD_ENABLE_Z, CMD_DISABLE_Z, CMD_START_DEMO
} FillheadCommand;

// UPDATED: State machine for the demo routine now includes centering and edge-finding states.
typedef enum {
    FH_STATE_IDLE,
    FH_STATE_DEMO_START,
    FH_STATE_DEMO_CENTERING,
    FH_STATE_DEMO_MOVING_TO_EDGE,
    FH_STATE_DEMO_RUNNING
} FillheadState;


class Fillhead {
	public:
	Fillhead();
	void setup();
	void update();
	void sendStatus(const char* statusType, const char* message);

	private:
	void setupEthernet();
	void setupUsbSerial();
	void processUdp();
	void handleMessage(const char* msg);
	void sendGuiTelemetry();

	// Command Handlers
	void handleSetPeerIp(const char* msg);
	void handleClearPeerIp();
	void abortAll();
    void handleStartDemo();

    // State updaters
    void updateDemoState();
	
	FillheadCommand parseCommand(const char* msg);

	// Member Variables
	EthernetUdp m_udp;
	IpAddress m_guiIp;
	uint16_t m_guiPort;
	bool m_guiDiscovered;
	IpAddress m_peerIp;
	bool m_peerDiscovered;
	
	Axis xAxis;
	Axis yAxis;
	Axis zAxis;

    FillheadState m_fillheadState;
    float m_demoAngleRad;

	char m_telemetryBuffer[300];
	unsigned char m_packetBuffer[MAX_PACKET_LENGTH];
};

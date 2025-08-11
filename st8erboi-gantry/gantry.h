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
	CMD_ENABLE_Z, CMD_DISABLE_Z
} GantryCommand;

// Structure to hold a single message for the queues
struct Message {
	char buffer[MAX_MESSAGE_LENGTH];
	IpAddress remoteIp;
	uint16_t remotePort;
};

class Gantry {
	public:
	Gantry();
	void setup();
	void update();
	void sendStatus(const char* statusType, const char* message);

	private:
	void setupEthernet();
	void setupUsbSerial();
	
	// UDP and Message Handling
	void processUdp();
	void handleMessage(const Message& msg);
	void sendGuiTelemetry();
	GantryCommand parseCommand(const char* msg);

	// Queue Processing
	void processRxQueue();
	void processTxQueue();

	// Queue Management
	bool enqueueRx(const char* msg, const IpAddress& ip, uint16_t port);
	bool dequeueRx(Message& msg);
	bool enqueueTx(const char* msg, const IpAddress& ip, uint16_t port);
	bool dequeueTx(Message& msg);

	// Command Handlers
	void handleSetPeerIp(const char* msg);
	void handleClearPeerIp();
	void abortAll();
	
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

	char m_telemetryBuffer[300];
	unsigned char m_packetBuffer[MAX_PACKET_LENGTH];
	
	uint32_t m_lastTelemetryTime;
	
	// --- Message Queues ---
	// Receive (Rx) Queue
	Message m_rxQueue[RX_QUEUE_SIZE];
	volatile int m_rxQueueHead;
	volatile int m_rxQueueTail;

	// Transmit (Tx) Queue
	Message m_txQueue[TX_QUEUE_SIZE];
	volatile int m_txQueueHead;
	volatile int m_txQueueTail;
};

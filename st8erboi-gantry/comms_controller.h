#pragma once

#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Command parsing enum (Moved from gantry.h) ---
typedef enum {
	CMD_UNKNOWN, CMD_REQUEST_TELEM, CMD_DISCOVER, CMD_SET_PEER_IP,
	CMD_CLEAR_PEER_IP, CMD_ABORT, CMD_MOVE_X, CMD_MOVE_Y, CMD_MOVE_Z,
	CMD_HOME_X, CMD_HOME_Y, CMD_HOME_Z,
	CMD_ENABLE_X, CMD_DISABLE_X, CMD_ENABLE_Y, CMD_DISABLE_Y,
	CMD_ENABLE_Z, CMD_DISABLE_Z
} GantryCommand;

// --- Message struct (Moved from gantry.h) ---
struct Message {
	char buffer[MAX_MESSAGE_LENGTH];
	IpAddress remoteIp;
	uint16_t remotePort;
};

class CommsController {
	public:
	CommsController();
	void setup();
	void update();

	// Queue Management
	bool dequeueRx(Message& msg);
	void sendStatus(const char* statusType, const char* message);
	bool enqueueTx(const char* msg, const IpAddress& ip, uint16_t port);
	GantryCommand parseCommand(const char* msg);

	// Getters
	bool isGuiDiscovered() const { return m_guiDiscovered; }
	IpAddress getGuiIp() const { return m_guiIp; }
	uint16_t getGuiPort() const { return m_guiPort; }
	bool isPeerDiscovered() const { return m_peerDiscovered; }
	IpAddress getPeerIp() const { return m_peerIp; }

	// Setters
	void setGuiDiscovered(bool discovered) { m_guiDiscovered = discovered; }
	void setGuiIp(const IpAddress& ip) { m_guiIp = ip; }
	void setGuiPort(uint16_t port) { m_guiPort = port; }
	void setPeerIp(const IpAddress& ip);
	void clearPeerIp();

	private:
	void processUdp();
	void processTxQueue();
	void setupEthernet();
	bool enqueueRx(const char* msg, const IpAddress& ip, uint16_t port);

	EthernetUdp m_udp;
	IpAddress m_guiIp;
	uint16_t m_guiPort;
	bool m_guiDiscovered;
	IpAddress m_peerIp;
	bool m_peerDiscovered;

	unsigned char m_packetBuffer[MAX_PACKET_LENGTH];

	// Message Queues
	Message m_rxQueue[RX_QUEUE_SIZE];
	volatile int m_rxQueueHead;
	volatile int m_rxQueueTail;

	Message m_txQueue[TX_QUEUE_SIZE];
	volatile int m_txQueueHead;
	volatile int m_txQueueTail;
};
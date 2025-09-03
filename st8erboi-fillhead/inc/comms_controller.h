#pragma once

#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include "config.h"
#include "commands.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Structure to hold a single message for the queues
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
	bool enqueueRx(const char* msg, const IpAddress& ip, uint16_t port);
	bool dequeueRx(Message& msg);
	bool enqueueTx(const char* msg, const IpAddress& ip, uint16_t port);
	
	void reportEvent(const char* statusType, const char* message);
	Command parseCommand(const char* msg);

	// Getters
	bool isGuiDiscovered() const { return m_guiDiscovered; }
	IpAddress getGuiIp() const { return m_guiIp; }
	uint16_t getGuiPort() const { return m_guiPort; }

	// Setters
	void setGuiDiscovered(bool discovered) { m_guiDiscovered = discovered; }
	void setGuiIp(const IpAddress& ip) { m_guiIp = ip; }
	void setGuiPort(uint16_t port) { m_guiPort = port; }

	private:
	void processUdp();
	void processTxQueue();
	void setupEthernet();
	void setupUsbSerial();

	EthernetUdp m_udp;
	IpAddress m_guiIp;
	uint16_t m_guiPort;
	bool m_guiDiscovered;

	unsigned char m_packetBuffer[MAX_PACKET_LENGTH];
	
	// Message Queues
	Message m_rxQueue[RX_QUEUE_SIZE];
	volatile int m_rxQueueHead;
	volatile int m_rxQueueTail;

	Message m_txQueue[TX_QUEUE_SIZE];
	volatile int m_txQueueHead;
	volatile int m_txQueueTail;
};

#include "comms_controller.h"

CommsController::CommsController() {
	m_guiDiscovered = false;
	m_guiPort = 0;

	m_rxQueueHead = 0;
	m_rxQueueTail = 0;
	m_txQueueHead = 0;
	m_txQueueTail = 0;
}

void CommsController::setup() {
	setupEthernet();
}

void CommsController::update() {
	processUdp();
	processTxQueue();
}

void CommsController::setupEthernet() {
	EthernetMgr.Setup();
	if (!EthernetMgr.DhcpBegin()) {
		while (1); // Hang if DHCP fails
	}
	while (!EthernetMgr.PhyLinkActive()) {
		Delay_ms(100);
	}
	m_udp.Begin(LOCAL_PORT);
}

void CommsController::processUdp() {
	while (m_udp.PacketParse()) {
		IpAddress remoteIp = m_udp.RemoteIp();
		uint16_t remotePort = m_udp.RemotePort();
		int32_t bytesRead = m_udp.PacketRead(m_packetBuffer, MAX_PACKET_LENGTH - 1);
		if (bytesRead > 0) {
			m_packetBuffer[bytesRead] = '\0';
			if (!enqueueRx((char*)m_packetBuffer, remoteIp, remotePort)) {
				// Error is handled inside enqueueRx
			}
		}
	}
}

void CommsController::processTxQueue() {
	if (m_txQueueHead != m_txQueueTail) {
		Message msg;
		msg = m_txQueue[m_txQueueTail];
		m_txQueueTail = (m_txQueueTail + 1) % TX_QUEUE_SIZE;

		m_udp.Connect(msg.remoteIp, msg.remotePort);
		m_udp.PacketWrite(msg.buffer);
		m_udp.PacketSend();
	}
}

bool CommsController::enqueueRx(const char* msg, const IpAddress& ip, uint16_t port) {
	int next_head = (m_rxQueueHead + 1) % RX_QUEUE_SIZE;
	if (next_head == m_rxQueueTail) {
		// Rx Queue is full. Send an immediate error back to the GUI.
		if(m_guiDiscovered) {
			char errorMsg[] = "INJ_ERROR: RX QUEUE OVERFLOW - COMMAND DROPPED";
			m_udp.Connect(m_guiIp, m_guiPort);
			m_udp.PacketWrite(errorMsg);
			m_udp.PacketSend();
		}
		return false;
	}
	strncpy(m_rxQueue[m_rxQueueHead].buffer, msg, MAX_MESSAGE_LENGTH);
	m_rxQueue[m_rxQueueHead].buffer[MAX_MESSAGE_LENGTH - 1] = '\0';
	m_rxQueue[m_rxQueueHead].remoteIp = ip;
	m_rxQueue[m_rxQueueHead].remotePort = port;
	m_rxQueueHead = next_head;
	return true;
}

bool CommsController::dequeueRx(Message& msg) {
	if (m_rxQueueHead == m_rxQueueTail) {
		return false;
	}
	msg = m_rxQueue[m_rxQueueTail];
	m_rxQueueTail = (m_rxQueueTail + 1) % RX_QUEUE_SIZE;
	return true;
}

bool CommsController::enqueueTx(const char* msg, const IpAddress& ip, uint16_t port) {
	int next_head = (m_txQueueHead + 1) % TX_QUEUE_SIZE;
	if (next_head == m_txQueueTail) {
		if(m_guiDiscovered) {
			char errorMsg[] = "INJ_ERROR: TX QUEUE OVERFLOW - MESSAGE DROPPED";
			m_udp.Connect(m_guiIp, m_guiPort);
			m_udp.PacketWrite(errorMsg);
			m_udp.PacketSend();
		}
		return false;
	}
	strncpy(m_txQueue[m_txQueueHead].buffer, msg, MAX_MESSAGE_LENGTH);
	m_txQueue[m_txQueueHead].buffer[MAX_MESSAGE_LENGTH - 1] = '\0';
	m_txQueue[m_txQueueHead].remoteIp = ip;
	m_txQueue[m_txQueueHead].remotePort = port;
	m_txQueueHead = next_head;
	return true;
}

void CommsController::sendStatus(const char* statusType, const char* message) {
	char fullMsg[MAX_MESSAGE_LENGTH];
	snprintf(fullMsg, sizeof(fullMsg), "%s%s", statusType, message);

	if (m_guiDiscovered) {
		enqueueTx(fullMsg, m_guiIp, m_guiPort);
	}
}

Command CommsController::parseCommand(const char* msg) {
	// Trim leading whitespace
	while (isspace((unsigned char)*msg)) {
		msg++;
	}

	// If the message is empty after trimming, return UNKNOWN.
	if (*msg == '\0') {
		return CMD_UNKNOWN;
	}

	if (strncmp(msg, CMD_STR_DISCOVER, strlen(CMD_STR_DISCOVER)) == 0) return CMD_DISCOVER;
	if (strcmp(msg, CMD_STR_ABORT) == 0) return CMD_ABORT;
	if (strncmp(msg, CMD_STR_MOVE_X, strlen(CMD_STR_MOVE_X)) == 0) return CMD_MOVE_X;
	if (strncmp(msg, CMD_STR_MOVE_Y, strlen(CMD_STR_MOVE_Y)) == 0) return CMD_MOVE_Y;
	if (strncmp(msg, CMD_STR_MOVE_Z, strlen(CMD_STR_MOVE_Z)) == 0) return CMD_MOVE_Z;
	if (strncmp(msg, CMD_STR_HOME_X, strlen(CMD_STR_HOME_X)) == 0) return CMD_HOME_X;
	if (strncmp(msg, CMD_STR_HOME_Y, strlen(CMD_STR_HOME_Y)) == 0) return CMD_HOME_Y;
	if (strncmp(msg, CMD_STR_HOME_Z, strlen(CMD_STR_HOME_Z)) == 0) return CMD_HOME_Z;
	if (strcmp(msg, CMD_STR_ENABLE_X) == 0) return CMD_ENABLE_X;
	if (strcmp(msg, CMD_STR_DISABLE_X) == 0) return CMD_DISABLE_X;
	if (strcmp(msg, CMD_STR_ENABLE_Y) == 0) return CMD_ENABLE_Y;
	if (strcmp(msg, CMD_STR_DISABLE_Y) == 0) return CMD_DISABLE_Y;
	if (strcmp(msg, CMD_STR_ENABLE_Z) == 0) return CMD_ENABLE_Z;
	if (strcmp(msg, CMD_STR_DISABLE_Z) == 0) return CMD_DISABLE_Z;
	return CMD_UNKNOWN;
}
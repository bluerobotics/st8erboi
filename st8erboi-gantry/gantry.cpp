#include "gantry.h"
#include <math.h>

Gantry::Gantry() :
// X-Axis: Single motor, single homing sensor, no limit switch
xAxis(this, "X", &MotorX, nullptr, STEPS_PER_MM_X, X_MIN_POS, X_MAX_POS, &SENSOR_X, nullptr, nullptr, nullptr),
// Y-Axis: Dual motor gantry, two homing sensors, one rear limit switch
yAxis(this, "Y", &MotorY1, &MotorY2, STEPS_PER_MM_Y, Y_MIN_POS, Y_MAX_POS, &SENSOR_Y1, &SENSOR_Y2, &LIMIT_Y_BACK, nullptr),
// Z-Axis: Single motor, single homing sensor, no limit switch
zAxis(this, "Z", &MotorZ, nullptr, STEPS_PER_MM_Z, Z_MIN_POS, Z_MAX_POS, &SENSOR_Z, nullptr, nullptr, &Z_BRAKE)
{
	m_guiDiscovered = false;
	m_guiPort = 0;
	m_peerDiscovered = false;
	m_lastTelemetryTime = 0;
    m_state = GANTRY_STANDBY;
	
	// Initialize queue pointers
	m_rxQueueHead = 0;
	m_rxQueueTail = 0;
	m_txQueueHead = 0;
	m_txQueueTail = 0;
}

void Gantry::setup() {
	MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);
	
	xAxis.setupMotors();
	yAxis.setupMotors();
	zAxis.setupMotors();
	
	uint32_t timeout = Milliseconds() + 2000;
	while(Milliseconds() < timeout) {
		if (MotorX.StatusReg().bit.Enabled &&
		MotorY1.StatusReg().bit.Enabled &&
		MotorY2.StatusReg().bit.Enabled &&
		MotorZ.StatusReg().bit.Enabled) {
			break;
		}
	}

	setupEthernet();
}

void Gantry::update() {
	// 1. Read incoming UDP packets and place them in the Rx queue
	processUdp();
	
	// 2. Process ONE message from the Rx queue
	processRxQueue();
	
	// 3. Send ONE message from the Tx queue to keep the loop fast
	processTxQueue();
	
	// 4. Update state machines for each axis
	xAxis.updateState();
	yAxis.updateState();
	zAxis.updateState();

    // 5. Update the overall gantry state based on axis states
    updateGantryState();
	
	// 6. Enqueue telemetry data at regular intervals
	uint32_t now = Milliseconds();
	if (m_guiDiscovered && (now - m_lastTelemetryTime >= TELEMETRY_INTERVAL_MS)) {
		m_lastTelemetryTime = now;
		sendGuiTelemetry();
	}
}

// -----------------------------------------------------------------------------
// State Management
// -----------------------------------------------------------------------------

void Gantry::updateGantryState() {
    // Homing has the highest priority. If any axis is homing, the whole gantry is homing.
    if (xAxis.getState() == Axis::STATE_HOMING || yAxis.getState() == Axis::STATE_HOMING || zAxis.getState() == Axis::STATE_HOMING) {
        m_state = GANTRY_HOMING;
        return;
    }

    // Any other movement means the system is moving.
    if (xAxis.isMoving() || yAxis.isMoving() || zAxis.isMoving()) {
        m_state = GANTRY_MOVING;
        return;
    }

    // If nothing else is happening, the system is in standby.
    m_state = GANTRY_STANDBY;
}

const char* Gantry::getGantryStateString() {
    switch (m_state) {
        case GANTRY_STANDBY: return "STANDBY";
        case GANTRY_HOMING:  return "HOMING";
        case GANTRY_MOVING:  return "MOVING";
        default:             return "UNKNOWN";
    }
}


// -----------------------------------------------------------------------------
// Queue Management
// -----------------------------------------------------------------------------

bool Gantry::enqueueRx(const char* msg, const IpAddress& ip, uint16_t port) {
	int next_head = (m_rxQueueHead + 1) % RX_QUEUE_SIZE;
	if (next_head == m_rxQueueTail) {
		// Rx Queue is full. Send an immediate error back to the GUI.
		if(m_guiDiscovered) {
			char errorMsg[] = "ERROR: RX QUEUE OVERFLOW - COMMAND DROPPED";
			m_udp.Connect(m_guiIp, m_guiPort);
			m_udp.PacketWrite(errorMsg);
			m_udp.PacketSend();
		}
		return false;
	}
	strncpy(m_rxQueue[m_rxQueueHead].buffer, msg, MAX_MESSAGE_LENGTH);
	m_rxQueue[m_rxQueueHead].buffer[MAX_MESSAGE_LENGTH - 1] = '\0'; // Ensure null termination
	m_rxQueue[m_rxQueueHead].remoteIp = ip;
	m_rxQueue[m_rxQueueHead].remotePort = port;
	m_rxQueueHead = next_head;
	return true;
}

bool Gantry::dequeueRx(Message& msg) {
	if (m_rxQueueHead == m_rxQueueTail) {
		// Queue is empty
		return false;
	}
	msg = m_rxQueue[m_rxQueueTail];
	m_rxQueueTail = (m_rxQueueTail + 1) % RX_QUEUE_SIZE;
	return true;
}

bool Gantry::enqueueTx(const char* msg, const IpAddress& ip, uint16_t port) {
	int next_head = (m_txQueueHead + 1) % TX_QUEUE_SIZE;
	if (next_head == m_txQueueTail) {
		// Tx Queue is full. Send an immediate error message, bypassing the queue.
        // This is a critical error, as status messages are being dropped.
		if(m_guiDiscovered) {
			char errorMsg[] = "ERROR: TX QUEUE OVERFLOW - MESSAGE DROPPED";
			m_udp.Connect(m_guiIp, m_guiPort);
			m_udp.PacketWrite(errorMsg);
			m_udp.PacketSend();
		}
		return false;
	}
	strncpy(m_txQueue[m_txQueueHead].buffer, msg, MAX_MESSAGE_LENGTH);
	m_txQueue[m_txQueueHead].buffer[MAX_MESSAGE_LENGTH - 1] = '\0'; // Ensure null termination
	m_txQueue[m_txQueueHead].remoteIp = ip;
	m_txQueue[m_txQueueHead].remotePort = port;
	m_txQueueHead = next_head;
	return true;
}

bool Gantry::dequeueTx(Message& msg) {
	if (m_txQueueHead == m_txQueueTail) {
		// Queue is empty
		return false;
	}
	msg = m_txQueue[m_txQueueTail];
	m_txQueueTail = (m_txQueueTail + 1) % TX_QUEUE_SIZE;
	return true;
}

// -----------------------------------------------------------------------------
// Network and Message Processing
// -----------------------------------------------------------------------------

void Gantry::processUdp() {
	// Process all available packets in the UDP hardware buffer to avoid dropping any during a burst.
	while (m_udp.PacketParse()) {
		IpAddress remoteIp = m_udp.RemoteIp();
		uint16_t remotePort = m_udp.RemotePort();
		int32_t bytesRead = m_udp.PacketRead(m_packetBuffer, MAX_PACKET_LENGTH - 1);
		if (bytesRead > 0) {
			m_packetBuffer[bytesRead] = '\0';
			// Enqueue the received message. If the queue is full, enqueueRx will return false.
			if (!enqueueRx((char*)m_packetBuffer, remoteIp, remotePort)) {
				// The Rx queue is full, which is now handled inside enqueueRx.
			}
		}
	}
}

void Gantry::processRxQueue() {
	// Process only ONE message per loop. This keeps the main loop consistently fast,
	// ensuring the processUdp() function is called frequently enough to prevent the
	// low-level hardware network buffer from overflowing during a command burst.
	Message msg;
	if (dequeueRx(msg)) {
		handleMessage(msg);
	}
}

void Gantry::processTxQueue() {
	Message msg;
	// Send only one message per update loop to keep the loop fast and non-blocking.
	// This prevents stalling, which could cause incoming UDP packets to be dropped.
	if (dequeueTx(msg)) {
		m_udp.Connect(msg.remoteIp, msg.remotePort);
		m_udp.PacketWrite(msg.buffer);
		m_udp.PacketSend();
	}
}

void Gantry::sendStatus(const char* statusType, const char* message) {
	char fullMsg[MAX_MESSAGE_LENGTH];
	snprintf(fullMsg, sizeof(fullMsg), "%s%s", statusType, message);
	
	// Enqueue the message for the GUI
	if (m_guiDiscovered) {
		enqueueTx(fullMsg, m_guiIp, m_guiPort);
	}
	// Enqueue the message for the peer
	if (m_peerDiscovered) {
		enqueueTx(fullMsg, m_peerIp, LOCAL_PORT);
	}
}

void Gantry::sendGuiTelemetry() {
	if (!m_guiDiscovered) return;

	snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
	"%s"
    "gantry_state:%s,"
	"x_p:%.2f,x_t:%.2f,x_e:%d,x_h:%d,"
	"y_p:%.2f,y_t:%.2f,y_e:%d,y_h:%d,"
	"z_p:%.2f,z_t:%.2f,z_e:%d,z_h:%d,"
	"pd:%d,pip:%s",
	TELEM_PREFIX_GUI,
    getGantryStateString(),
	xAxis.getPositionMm(), xAxis.getSmoothedTorque(), xAxis.isEnabled(), xAxis.isHomed(),
	yAxis.getPositionMm(), yAxis.getSmoothedTorque(), yAxis.isEnabled(), yAxis.isHomed(),
	zAxis.getPositionMm(), zAxis.getSmoothedTorque(), zAxis.isEnabled(), zAxis.isHomed(),
	(int)m_peerDiscovered, m_peerIp.StringValue());

	// Enqueue the telemetry message instead of sending directly
	enqueueTx(m_telemetryBuffer, m_guiIp, m_guiPort);
}

void Gantry::handleMessage(const Message& msg) {
	GantryCommand cmd = parseCommand(msg.buffer);
	const char* args = strchr(msg.buffer, ' ');
	if(args) args++;

	switch(cmd) {
		case CMD_SET_PEER_IP: handleSetPeerIp(msg.buffer); break;
		case CMD_CLEAR_PEER_IP: handleClearPeerIp(); break;
		case CMD_ABORT: abortAll(); break;
		
		case CMD_MOVE_X: xAxis.handleMove(args); break;
		case CMD_MOVE_Y: yAxis.handleMove(args); break;
		case CMD_MOVE_Z: zAxis.handleMove(args); break;
		
		case CMD_HOME_X: xAxis.handleHome(args); break;
		case CMD_HOME_Y: yAxis.handleHome(args); break;
		case CMD_HOME_Z: zAxis.handleHome(args); break;
		
		case CMD_ENABLE_X:
		xAxis.enable();
		sendStatus(STATUS_PREFIX_DONE, "ENABLE_X complete.");
		break;
		case CMD_DISABLE_X:
		xAxis.disable();
		sendStatus(STATUS_PREFIX_DONE, "DISABLE_X complete.");
		break;
		case CMD_ENABLE_Y:
		yAxis.enable();
		sendStatus(STATUS_PREFIX_DONE, "ENABLE_Y complete.");
		break;
		case CMD_DISABLE_Y:
		yAxis.disable();
		sendStatus(STATUS_PREFIX_DONE, "DISABLE_Y complete.");
		break;
		case CMD_ENABLE_Z:
		zAxis.enable();
		sendStatus(STATUS_PREFIX_DONE, "ENABLE_Z complete.");
		break;
		case CMD_DISABLE_Z:
		zAxis.disable();
		sendStatus(STATUS_PREFIX_DONE, "DISABLE_Z complete.");
		break;

		case CMD_REQUEST_TELEM: sendGuiTelemetry(); break;
		case CMD_DISCOVER: {
			char* portStr = strstr(msg.buffer, "PORT=");
			if (portStr) {
				m_guiIp = msg.remoteIp; // Use the IP from the message
				m_guiPort = atoi(portStr + 5);
				m_guiDiscovered = true;
				sendStatus(STATUS_PREFIX_DISCOVERY, "GANTRY DISCOVERED");
			}
			break;
		}
		case CMD_UNKNOWN:
		default:
		// Optionally send an error for unknown commands
		// sendStatus(STATUS_PREFIX_ERROR, "Unknown command received.");
		break;
	}
}

// -----------------------------------------------------------------------------
// Unchanged Helper Functions
// -----------------------------------------------------------------------------

void Gantry::abortAll() {
	xAxis.abort();
	yAxis.abort();
	zAxis.abort();
	sendStatus(STATUS_PREFIX_DONE, "ABORT complete.");
}

void Gantry::handleSetPeerIp(const char* msg) {
	const char* ipStr = msg + strlen(CMD_STR_SET_PEER_IP);
	IpAddress newPeerIp(ipStr);
	if (strcmp(newPeerIp.StringValue(), "0.0.0.0") == 0) {
		m_peerDiscovered = false;
		sendStatus(STATUS_PREFIX_ERROR, "Failed to parse peer IP address");
		} else {
		m_peerIp = newPeerIp;
		m_peerDiscovered = true;
		char response[100];
		snprintf(response, sizeof(response), "Peer IP set to %s", ipStr);
		sendStatus(STATUS_PREFIX_INFO, response);
	}
}

void Gantry::handleClearPeerIp() {
	m_peerDiscovered = false;
	sendStatus(STATUS_PREFIX_INFO, "Peer IP cleared");
}

void Gantry::setupUsbSerial(void) {
	ConnectorUsb.Mode(Connector::USB_CDC);
	ConnectorUsb.Speed(9600);
	ConnectorUsb.PortOpen();
	uint32_t timeout = 5000;
	uint32_t start = Milliseconds();
	while (!ConnectorUsb && Milliseconds() - start < timeout);
}

void Gantry::setupEthernet() {
	EthernetMgr.Setup();

	if (!EthernetMgr.DhcpBegin()) {
		// This will hang if DHCP fails. Consider a timeout or fallback.
		while (1);
	}
	
	while (!EthernetMgr.PhyLinkActive()) {
		Delay_ms(100);
	}

	m_udp.Begin(LOCAL_PORT);
}

GantryCommand Gantry::parseCommand(const char* msg) {
	if (strcmp(msg, CMD_STR_REQUEST_TELEM) == 0) return CMD_REQUEST_TELEM;
	if (strncmp(msg, CMD_STR_DISCOVER, strlen(CMD_STR_DISCOVER)) == 0) return CMD_DISCOVER;
	if (strncmp(msg, CMD_STR_SET_PEER_IP, strlen(CMD_STR_SET_PEER_IP)) == 0) return CMD_SET_PEER_IP;
	if (strcmp(msg, CMD_STR_CLEAR_PEER_IP) == 0) return CMD_CLEAR_PEER_IP;
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

// Global instance of our Gantry controller class
Gantry gantry;


/*
    Main application entry point.
*/
int main(void) {
    // Perform one-time setup
    gantry.setup();

    // Main non-blocking application loop
    while (true) {
        // This single call now processes communications and updates all axis state machines.
        gantry.update();
    }
}

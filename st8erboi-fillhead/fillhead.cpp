#include "fillhead.h"
#include <math.h>

Fillhead::Fillhead() :
// CORRECTED: Pass positive steps/mm. Direction is handled in the Axis class.
// X-Axis: Single motor, single homing sensor, no limit switch
xAxis(this, "X", &MotorX, nullptr, STEPS_PER_MM_X, X_MIN_POS, X_MAX_POS, &SENSOR_X, nullptr, nullptr),

// Y-Axis: Dual motor gantry, two homing sensors, one rear limit switch
yAxis(this, "Y", &MotorY1, &MotorY2, STEPS_PER_MM_Y, Y_MIN_POS, Y_MAX_POS, &SENSOR_Y1, &SENSOR_Y2, &LIMIT_Y_BACK),

// Z-Axis: Single motor, single homing sensor, no limit switch
zAxis(this, "Z", &MotorZ, nullptr, STEPS_PER_MM_Z, Z_MIN_POS, Z_MAX_POS, &SENSOR_Z, nullptr, nullptr)
{
	m_guiDiscovered = false;
	m_guiPort = 0;
	m_peerDiscovered = false;
}

void Fillhead::setup() {
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

void Fillhead::update() {
	processUdp();
	xAxis.updateState();
	yAxis.updateState();
	zAxis.updateState();
}

void Fillhead::abortAll() {
	xAxis.abort();
	yAxis.abort();
	zAxis.abort();
	sendStatus(STATUS_PREFIX_DONE, "ABORT complete.");
}

void Fillhead::sendStatus(const char* statusType, const char* message) {
	char msg[128];
	snprintf(msg, sizeof(msg), "%s%s", statusType, message);
	
	if (m_guiDiscovered) {
		m_udp.Connect(m_guiIp, m_guiPort);
		m_udp.PacketWrite(msg);
		m_udp.PacketSend();
	}
	if (m_peerDiscovered) {
		m_udp.Connect(m_peerIp, LOCAL_PORT);
		m_udp.PacketWrite(msg);
		m_udp.PacketSend();
	}
}

void Fillhead::sendGuiTelemetry() {
	if (!m_guiDiscovered) return;

	snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
	"%s"
	"x_s:%s,x_p:%.2f,x_t:%.2f,x_e:%d,x_h:%d,"
	"y_s:%s,y_p:%.2f,y_t:%.2f,y_e:%d,y_h:%d,"
	"z_s:%s,z_p:%.2f,z_t:%.2f,z_e:%d,z_h:%d,"
	"pd:%d,pip:%s",
	TELEM_PREFIX_GUI,
	xAxis.getStateString(), xAxis.getPositionMm(), xAxis.getSmoothedTorque(), xAxis.isEnabled(), xAxis.isHomed(),
	yAxis.getStateString(), yAxis.getPositionMm(), yAxis.getSmoothedTorque(), yAxis.isEnabled(), yAxis.isHomed(),
	zAxis.getStateString(), zAxis.getPositionMm(), zAxis.getSmoothedTorque(), zAxis.isEnabled(), zAxis.isHomed(),
	(int)m_peerDiscovered, m_peerIp.StringValue());

	m_udp.Connect(m_guiIp, m_guiPort);
	m_udp.PacketWrite(m_telemetryBuffer);
	m_udp.PacketSend();
}

void Fillhead::processUdp() {
	if (m_udp.PacketParse()) {
		int32_t bytesRead = m_udp.PacketRead(m_packetBuffer, MAX_PACKET_LENGTH - 1);
		if (bytesRead > 0) {
			m_packetBuffer[bytesRead] = '\0';
			handleMessage((char*)m_packetBuffer);
		}
	}
}

void Fillhead::handleSetPeerIp(const char* msg) {
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

void Fillhead::handleClearPeerIp() {
	m_peerDiscovered = false;
	sendStatus(STATUS_PREFIX_INFO, "Peer IP cleared");
}

void Fillhead::setupUsbSerial(void) {
	ConnectorUsb.Mode(Connector::USB_CDC);
	ConnectorUsb.Speed(9600);
	ConnectorUsb.PortOpen();
	uint32_t timeout = 5000;
	uint32_t start = Milliseconds();
	while (!ConnectorUsb && Milliseconds() - start < timeout);
}

void Fillhead::setupEthernet() {
	EthernetMgr.Setup();

	if (!EthernetMgr.DhcpBegin()) {
		while (1);
	}
	
	while (!EthernetMgr.PhyLinkActive()) {
		Delay_ms(100);
	}

	m_udp.Begin(LOCAL_PORT);
}

void Fillhead::handleMessage(const char* msg) {
	FillheadCommand cmd = parseCommand(msg);
	const char* args = strchr(msg, ' ');
	if(args) args++;

	switch(cmd) {
		case CMD_SET_PEER_IP: handleSetPeerIp(msg); break;
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
			char* portStr = strstr(msg, "PORT=");
			if (portStr) {
				m_guiIp = m_udp.RemoteIp();
				m_guiPort = atoi(portStr + 5);
				m_guiDiscovered = true;
				sendStatus(STATUS_PREFIX_DISCOVERY, "FILLHEAD DISCOVERED");
			}
			break;
		}
		case CMD_UNKNOWN:
		default:
		break;
	}
}

FillheadCommand Fillhead::parseCommand(const char* msg) {
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
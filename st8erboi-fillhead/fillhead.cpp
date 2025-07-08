#include "fillhead.h"
#include <math.h> // Needed for sin() and cos()

Fillhead::Fillhead() :
    xAxis(this, "X", &MotorX, nullptr, -STEPS_PER_MM_X, X_MIN_POS, X_MAX_POS),
    yAxis(this, "Y", &MotorY1, &MotorY2, STEPS_PER_MM_Y, Y_MIN_POS, Y_MAX_POS),
    zAxis(this, "Z", &MotorZ, nullptr, STEPS_PER_MM_Z, Z_MIN_POS, Z_MAX_POS)
{
    m_guiDiscovered = false;
    m_guiPort = 0;
    m_peerDiscovered = false;
    m_fillheadState = FH_STATE_IDLE; // Initialize state
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
    updateDemoState(); // Update the demo state machine
}

void Fillhead::abortAll() {
    xAxis.abort();
    yAxis.abort();
    zAxis.abort();
    m_fillheadState = FH_STATE_IDLE; // Also stop the demo routine
    sendStatus(STATUS_PREFIX_INFO, "ABORT Received. All motion stopped.");
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

void Fillhead::handleStartDemo() {
    if (m_fillheadState != FH_STATE_IDLE) {
        sendStatus(STATUS_PREFIX_ERROR, "Another operation is already in progress.");
        return;
    }
    if (xAxis.isMoving() || yAxis.isMoving() || zAxis.isMoving()) {
        sendStatus(STATUS_PREFIX_ERROR, "Cannot start demo: Axes are busy.");
        return;
    }
    if (!xAxis.isHomed() || !yAxis.isHomed() || !zAxis.isHomed()) {
        sendStatus(STATUS_PREFIX_ERROR, "Cannot start demo: All axes must be homed.");
        return;
    }

    sendStatus(STATUS_PREFIX_INFO, "Starting circle demo routine.");
    m_fillheadState = FH_STATE_DEMO_START;
}

// UPDATED: The demo state machine is now more complex to handle the new requirements.
void Fillhead::updateDemoState() {
    switch (m_fillheadState) {
        case FH_STATE_IDLE:
            break;

        case FH_STATE_DEMO_START:
            // This state is just a trigger. Move immediately to the first action state.
            sendStatus(STATUS_PREFIX_INFO, "Demo: Moving to center.");
            m_fillheadState = FH_STATE_DEMO_CENTERING;
            break;

        case FH_STATE_DEMO_CENTERING: {
            if (xAxis.isMoving() || yAxis.isMoving() || zAxis.isMoving()) {
                return; // Wait for any prior moves to finish.
            }

            // Calculate the center point of all axes
            float centerX = (X_MAX_POS + X_MIN_POS) / 2.0f;
            float centerY = (Y_MAX_POS + Y_MIN_POS) / 2.0f;
            float centerZ = (DEMO_Z_MAX_POS + DEMO_Z_MIN_POS) / 2.0f;

            // Calculate the required move distance for each axis
            float deltaX = centerX - xAxis.getPositionMm();
            float deltaY = centerY - yAxis.getPositionMm();
            float deltaZ = centerZ - zAxis.getPositionMm();

            // Command all three axes to move to the center simultaneously
            xAxis.startMove(deltaX, DEMO_XY_VEL_MMS, DEMO_ACCEL_MMSS, DEMO_TORQUE);
            yAxis.startMove(deltaY, DEMO_XY_VEL_MMS, DEMO_ACCEL_MMSS, DEMO_TORQUE);
            zAxis.startMove(deltaZ, DEMO_Z_VEL_MMS, DEMO_ACCEL_MMSS, DEMO_TORQUE);
            
            // Transition to the next state, which waits for this move to complete
            m_fillheadState = FH_STATE_DEMO_MOVING_TO_EDGE;
            break;
        }

        case FH_STATE_DEMO_MOVING_TO_EDGE: {
            // Wait until the move to the center is complete
            if (xAxis.isMoving() || yAxis.isMoving() || zAxis.isMoving()) {
                return;
            }
            
            sendStatus(STATUS_PREFIX_INFO, "Demo: Centered. Moving to circle edge.");

            // Now, from the center, move to the starting point of the circle (angle = 0)
            float startX_offset = DEMO_CIRCLE_RADIUS; // Move along X axis by the radius
            float startY_offset = 0;                  // No initial Y move
            
            xAxis.startMove(startX_offset, DEMO_XY_VEL_MMS, DEMO_ACCEL_MMSS, DEMO_TORQUE);
            yAxis.startMove(startY_offset, DEMO_XY_VEL_MMS, DEMO_ACCEL_MMSS, DEMO_TORQUE);

            m_demoAngleRad = 0; // Reset angle for the main circle routine
            m_fillheadState = FH_STATE_DEMO_RUNNING;
            break;
        }

        case FH_STATE_DEMO_RUNNING: {
            // Wait for the previous segment of the circle to finish moving
            if (xAxis.isMoving() || yAxis.isMoving() || zAxis.isMoving()) {
                return;
            }

            // Increment the angle for the next point on the circle
            m_demoAngleRad += DEMO_ANGLE_STEP_RAD;

            // Check if we've completed a full circle
            if (m_demoAngleRad >= (2.0 * M_PI)) {
                m_fillheadState = FH_STATE_IDLE;
                sendStatus(STATUS_PREFIX_DONE, "Circle demo complete.");
                return;
            }

            // Calculate the absolute target coordinates for the next point
            float centerX = (X_MAX_POS + X_MIN_POS) / 2.0f;
            float centerY = (Y_MAX_POS + Y_MIN_POS) / 2.0f;
            float zCenter = (DEMO_Z_MAX_POS + DEMO_Z_MIN_POS) / 2.0f;
            float zAmplitude = (DEMO_Z_MAX_POS - DEMO_Z_MIN_POS) / 2.0f;

            float targetX = centerX + DEMO_CIRCLE_RADIUS * cos(m_demoAngleRad);
            float targetY = centerY + DEMO_CIRCLE_RADIUS * sin(m_demoAngleRad);
            // Z oscillates twice for every one XY circle
            float targetZ = zCenter + zAmplitude * sin(2.0 * m_demoAngleRad);

            // Calculate the relative move needed from the current position
            float deltaX = targetX - xAxis.getPositionMm();
            float deltaY = targetY - yAxis.getPositionMm();
            float deltaZ = targetZ - zAxis.getPositionMm();

            // Command the moves for the next segment
            xAxis.startMove(deltaX, DEMO_XY_VEL_MMS, DEMO_ACCEL_MMSS, DEMO_TORQUE);
            yAxis.startMove(deltaY, DEMO_XY_VEL_MMS, DEMO_ACCEL_MMSS, DEMO_TORQUE);
            zAxis.startMove(deltaZ, DEMO_Z_VEL_MMS, DEMO_ACCEL_MMSS, DEMO_TORQUE);
            break;
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
        case CMD_START_DEMO: handleStartDemo(); break;
		
		case CMD_MOVE_X: xAxis.handleMove(args); break;
		case CMD_MOVE_Y: yAxis.handleMove(args); break;
		case CMD_MOVE_Z: zAxis.handleMove(args); break;
		
		case CMD_HOME_X: xAxis.handleHome(args); break;
		case CMD_HOME_Y: yAxis.handleHome(args); break;
		case CMD_HOME_Z: zAxis.handleHome(args); break;
		
		case CMD_ENABLE_X: xAxis.enable(); break;
		case CMD_DISABLE_X: xAxis.disable(); break;
		case CMD_ENABLE_Y: yAxis.enable(); break;
		case CMD_DISABLE_Y: yAxis.disable(); break;
		case CMD_ENABLE_Z: zAxis.enable(); break;
		case CMD_DISABLE_Z: zAxis.disable(); break;

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
    if (strcmp(msg, CMD_STR_START_DEMO) == 0) return CMD_START_DEMO;
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

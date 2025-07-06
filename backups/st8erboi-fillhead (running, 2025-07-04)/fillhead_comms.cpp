#include "fillhead.h"

// --- REPLACED FUNCTION ---
// Now sends status updates to both the GUI and the connected peer.
void Fillhead::sendStatus(const char* statusType, const char* message) {
	char msg[100];
	snprintf(msg, sizeof(msg), "%s%s", statusType, message);
	
	if (guiDiscovered) {
		udp.Connect(guiIp, guiPort);
		udp.PacketWrite(msg);
		udp.PacketSend();
	}

    if (peerDiscovered) {
        udp.Connect(peerIp, LOCAL_PORT); 
        udp.PacketWrite(msg);
        udp.PacketSend();
    }
}

void Fillhead::sendGuiTelemetry() {
	if (!guiDiscovered) return;
	
	snprintf(telemetryBuffer, sizeof(telemetryBuffer),
	    "%s,s:%s,"
	    "p0:%.2f,t0:%.2f,e0:%d,h0:%d,"
	    "p1:%.2f,t1:%.2f,e1:%d,h1:%d,"
	    "p2:%.2f,t2:%.2f,e2:%d,h2:%d,"
	    "p3:%.2f,t3:%.2f,e3:%d,h3:%d,"
        "pd:%d,pip:%s", // <-- NEW FIELDS
	    TELEM_PREFIX_GUI,
	    stateToString(),
	    (float)MotorX.PositionRefCommanded() / STEPS_PER_MM_X,
	    (MotorX.StatusReg().bit.StepsActive ? getSmoothedTorque(&MotorX, &smoothedTorqueM0, &firstTorqueReadM0) : 0.0f),
	    (int)MotorX.StatusReg().bit.Enabled, (int)homedX,
	    (float)MotorY1.PositionRefCommanded() / STEPS_PER_MM_Y,
	    (MotorY1.StatusReg().bit.StepsActive ? getSmoothedTorque(&MotorY1, &smoothedTorqueM1, &firstTorqueReadM1) : 0.0f),
	    (int)MotorY1.StatusReg().bit.Enabled, (int)homedY1,
	    (float)MotorY2.PositionRefCommanded() / STEPS_PER_MM_Y,
	    (MotorY2.StatusReg().bit.StepsActive ? getSmoothedTorque(&MotorY2, &smoothedTorqueM2, &firstTorqueReadM2) : 0.0f),
	    (int)MotorY2.StatusReg().bit.Enabled, (int)homedY2,
	    (float)MotorZ.PositionRefCommanded() / STEPS_PER_MM_Z,
	    (MotorZ.StatusReg().bit.StepsActive ? getSmoothedTorque(&MotorZ, &smoothedTorqueM3, &firstTorqueReadM3) : 0.0f),
	    (int)MotorZ.StatusReg().bit.Enabled, (int)homedZ,
        (int)peerDiscovered, peerIp.StringValue()); // <-- NEW VALUES

	udp.Connect(guiIp, guiPort);
	udp.PacketWrite(telemetryBuffer);
	udp.PacketSend();
}

// --- REPLACED FUNCTION ---
// Removed the line that overwrites the GUI's IP on every message.
void Fillhead::handleMessage(const char* msg) {
	FillheadCommand cmd = parseCommand(msg);
	switch(cmd) {
		case CMD_SET_PEER_IP: handleSetPeerIp(msg); break;
		case CMD_CLEAR_PEER_IP: handleClearPeerIp(); break;
		case CMD_ABORT: handleAbort(); break;
		case CMD_MOVE_X:
		case CMD_MOVE_Y:
		case CMD_MOVE_Z:
		    handleMove(msg);
		    break;
		case CMD_HOME_X:
		case CMD_HOME_Y:
		case CMD_HOME_Z:
		    handleHome(msg);
		    break;
		case CMD_REQUEST_TELEM:
			sendGuiTelemetry();
			break;
		case CMD_DISCOVER: {
			char* portStr = strstr(msg, "PORT=");
			if (portStr) {
				guiIp = udp.RemoteIp();
				guiPort = atoi(portStr + 5);
				guiDiscovered = true;
				sendStatus(STATUS_PREFIX_DISCOVERY, "FILLHEAD DISCOVERED");
			}
			break;
		}
		case CMD_UNKNOWN:
		default:
		    break;
	}
}

void Fillhead::processUdp() {
	if (udp.PacketParse()) {
		int32_t bytesRead = udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
		if (bytesRead > 0) {
			packetBuffer[bytesRead] = '\0';
			handleMessage((char*)packetBuffer);
		}
	}
}

// NOTE: sendInternalTelemetry is now effectively replaced by the upgraded sendStatus.
// This function can be deprecated or removed if no longer needed.
void Fillhead::sendInternalTelemetry() {
	// This function is no longer the primary way to send peer status.
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
	ConnectorUsb.SendLine("Fillhead: Starting Ethernet setup...");
	EthernetMgr.Setup();

	if (!EthernetMgr.DhcpBegin()) {
		ConnectorUsb.SendLine("Fillhead: DHCP FAILED. System Halted.");
		while (1);
	}
	
	char ipAddrStr[100];
	snprintf(ipAddrStr, sizeof(ipAddrStr), "Fillhead: DHCP successful. IP: %s", EthernetMgr.LocalIp().StringValue());
	ConnectorUsb.SendLine(ipAddrStr);

	while (!EthernetMgr.PhyLinkActive()) {
		Delay_ms(100);
	}
	ConnectorUsb.SendLine("Fillhead: PhyLink is Active.");

	udp.Begin(LOCAL_PORT);
	ConnectorUsb.SendLine("Fillhead: UDP Port 8888 is open. Setup complete.");
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
	return CMD_UNKNOWN;
}
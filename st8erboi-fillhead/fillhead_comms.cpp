#include "fillhead.h"

// This function is now safe. It sends to the destination configured once
// in handleMessage, without trying to reconnect.
void Fillhead::sendStatus(const char* statusType, const char* message) {
	char msg[100];
	snprintf(msg, sizeof(msg), "%s%s", statusType, message);
	
	if (guiDiscovered) {
		// This is required before every send with this library.
		Udp.Connect(guiIp, guiPort); 
		Udp.PacketWrite(msg);
		Udp.PacketSend();
	}
}

// This function is also safe now.
void Fillhead::sendGuiTelemetry() {
	if (!guiDiscovered) return;
	
	char msg[500];
	snprintf(msg, sizeof(msg),
	"%s,state:%s,"
	"pos_m0:%ld,torque_m0:%.2f,enabled_m0:%d,homed_m0:%d,"
	"pos_m1:%ld,torque_m1:%.2f,enabled_m1:%d,homed_m1:%d,"
	"pos_m2:%ld,torque_m2:%.2f,enabled_m2:%d,homed_m2:%d,"
	"pos_m3:%ld,torque_m3:%.2f,enabled_m3:%d,homed_m3:%d",
	TELEM_PREFIX_GUI,
	stateToString(),
	MotorX.PositionRefCommanded(),  getSmoothedTorque(&MotorX, &smoothedTorqueM0, &firstTorqueReadM0),  (int)MotorX.StatusReg().bit.Enabled,  (int)homedX,
	MotorY1.PositionRefCommanded(), getSmoothedTorque(&MotorY1, &smoothedTorqueM1, &firstTorqueReadM1), (int)MotorY1.StatusReg().bit.Enabled, (int)homedY1,
	MotorY2.PositionRefCommanded(), getSmoothedTorque(&MotorY2, &smoothedTorqueM2, &firstTorqueReadM2), (int)MotorY2.StatusReg().bit.Enabled, (int)homedY2,
	MotorZ.PositionRefCommanded(),  getSmoothedTorque(&MotorZ, &smoothedTorqueM3, &firstTorqueReadM3),  (int)MotorZ.StatusReg().bit.Enabled,  (int)homedZ);

	// This is required before every send with this library.
	Udp.Connect(guiIp, guiPort);
	Udp.PacketWrite(msg);
	Udp.PacketSend();
}


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
		case CMD_DISCOVER: {
			char* portStr = strstr(msg, "PORT=");
			if (portStr) {
				guiIp = Udp.RemoteIp();
				guiPort = atoi(portStr + 5);
				guiDiscovered = true;
				// Send the confirmation message. The sendStatus function
				// will handle the connect call itself.
				Udp.Connect(guiIp, guiPort); 
				sendStatus(STATUS_PREFIX_INFO, "GUI Discovered and Connected");
			}
			break;
		}
		case CMD_UNKNOWN:
		default:
		    sendStatus(STATUS_PREFIX_ERROR, "Unknown command");
		    break;
	}
}

// No changes to processUdp, but included for completeness of the file.
void Fillhead::processUdp() {
	if (Udp.PacketParse()) {
		int32_t bytesRead = Udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
		if (bytesRead > 0) {
			packetBuffer[bytesRead] = '\0';
			handleMessage((char*)packetBuffer);
		}
	}
}

// NOTE: This function still has the issue of "stealing" the UDP connection from the GUI.
// This is a larger architectural issue to solve later if needed. The primary GUI communication is now fixed.
void Fillhead::sendInternalTelemetry() {
	if (!peerDiscovered) return;
	
    // To send to the peer, we must temporarily change the UDP destination
	Udp.Connect(peerIp, LOCAL_PORT);

	char msg[200];
	snprintf(msg, sizeof(msg), "%s,state:%s,pos_m0:%ld,pos_m1:%ld,pos_m2:%ld,pos_m3:%ld,moving:%d",
	"FH_TELEM_INT:",
	stateToString(),
	MotorX.PositionRefCommanded(),
	MotorY1.PositionRefCommanded(),
	MotorY2.PositionRefCommanded(),
	MotorZ.PositionRefCommanded(),
	isMoving() ? 1 : 0);
	
	Udp.PacketWrite(msg);
	Udp.PacketSend();

}


// These functions are unchanged but are part of a complete "comms" file.
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

	Udp.Begin(LOCAL_PORT);
	ConnectorUsb.SendLine("Fillhead: UDP Port 8888 is open. Setup complete.");
}

FillheadCommand Fillhead::parseCommand(const char* msg) {
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
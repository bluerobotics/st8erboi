#include "fillhead.h"

// Adding a USB serial setup is very useful for debugging
void Fillhead::setupUsbSerial(void) {
	ConnectorUsb.Mode(Connector::USB_CDC);
	ConnectorUsb.Speed(9600);
	ConnectorUsb.PortOpen();
	uint32_t timeout = 5000;
	uint32_t start = Milliseconds();
	while (!ConnectorUsb && Milliseconds() - start < timeout);
}

// --- MODIFIED: Reverted to robust DHCP method ---
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
	if (strncmp(msg, CMD_STR_MOVE_X, strlen(CMD_STR_MOVE_X)) == 0) return CMD_MOVE_X;
	if (strncmp(msg, CMD_STR_MOVE_Y, strlen(CMD_STR_MOVE_Y)) == 0) return CMD_MOVE_Y;
	if (strncmp(msg, CMD_STR_MOVE_Z, strlen(CMD_STR_MOVE_Z)) == 0) return CMD_MOVE_Z;
	if (strcmp(msg, CMD_STR_HOME_X) == 0) return CMD_HOME_X;
	if (strcmp(msg, CMD_STR_HOME_Y) == 0) return CMD_HOME_Y;
	if (strcmp(msg, CMD_STR_HOME_Z) == 0) return CMD_HOME_Z;
	return CMD_UNKNOWN;
}

void Fillhead::handleMessage(const char* msg) {
	FillheadCommand cmd = parseCommand(msg);
	switch(cmd) {
		case CMD_MOVE_X: handleMove(&MotorX, msg); break;
		case CMD_MOVE_Y: handleMove(&MotorY, msg); break;
		case CMD_MOVE_Z: handleMove(&MotorZ, msg); break;
		case CMD_HOME_X: handleHome(&MotorX, &homedX); break;
		case CMD_HOME_Y: handleHome(&MotorY, &homedY); break;
		case CMD_HOME_Z: handleHome(&MotorZ, &homedZ); break;
		case CMD_DISCOVER: {
			char* portStr = strstr(msg, "PORT=");
			if (portStr) {
				guiIp = Udp.RemoteIp();
				guiPort = atoi(portStr + 5);
				guiDiscovered = true;
			}
			break;
		}
		case CMD_UNKNOWN:
		default:
		break;
	}
}

void Fillhead::processUdp() {
	if (Udp.PacketParse()) {
		int32_t bytesRead = Udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
		if (bytesRead > 0) {
			packetBuffer[bytesRead] = '\0';
			handleMessage((char*)packetBuffer);
		}
	}
}

void Fillhead::sendInternalTelemetry() {
	static uint32_t lastSendTime = 0;
	if (Milliseconds() - lastSendTime < 50) return;
	lastSendTime = Milliseconds();

	char msg[200];
	snprintf(msg, sizeof(msg), "%s,pos_x:%ld,pos_y:%ld,pos_z:%ld,moving:%d,homed:%d",
	TELEM_PREFIX_INTERNAL,
	MotorX.PositionRefCommanded(),
	MotorY.PositionRefCommanded(),
	MotorZ.PositionRefCommanded(),
	isMoving() ? 1 : 0,
	(homedX && homedY && homedZ) ? 1 : 0);
	
	Udp.Connect(injectorIp, LOCAL_PORT);
	Udp.PacketWrite(msg);
	Udp.PacketSend();
}

void Fillhead::sendGuiTelemetry() {
	if (!guiDiscovered) return;

	static uint32_t lastSendTime = 0;
	if (Milliseconds() - lastSendTime < 100) return;
	lastSendTime = Milliseconds();

	char msg[500];
	// --- MODIFIED: No warnings, as enabled status is a boolean (int 0 or 1) ---
	snprintf(msg, sizeof(msg), "%s,STATE:IDLE,pos_x:%ld,torque_x:%.2f,enabled_x:%d,homed_x:%d,pos_y:%ld,torque_y:%.2f,enabled_y:%d,homed_y:%d,pos_z:%ld,torque_z:%.2f,enabled_z:%d,homed_z:%d",
	TELEM_PREFIX_GUI,
	MotorX.PositionRefCommanded(), MotorX.HlfbPercent(), MotorX.StatusReg().bit.Enabled, homedX,
	MotorY.PositionRefCommanded(), MotorY.HlfbPercent(), MotorY.StatusReg().bit.Enabled, homedY,
	MotorZ.PositionRefCommanded(), MotorZ.HlfbPercent(), MotorZ.StatusReg().bit.Enabled, homedZ);

	Udp.Connect(guiIp, guiPort);
	Udp.PacketWrite(msg);
	Udp.PacketSend();
}
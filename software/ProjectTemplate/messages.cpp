#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include "messages.h"
#include "motor.h"
#include "states.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define LOCAL_PORT 8888
const uint16_t MAX_PACKET_LENGTH = 100;

EthernetUdp Udp;
unsigned char packetBuffer[MAX_PACKET_LENGTH];
IpAddress terminalIp;
uint16_t terminalPort = 0;
bool terminalDiscovered = false;

// Externs from other modules (motors, etc)
extern float globalTorqueLimit;
extern float torqueOffset;
extern float smoothedTorqueValue1;
extern float smoothedTorqueValue2;
extern bool firstTorqueReading1;
extern bool firstTorqueReading2;
extern bool motorsAreEnabled;
extern int32_t machineStepCounter;
extern int32_t cartridgeStepCounter;

void sendToPC(const char *msg)
{
	if (!terminalDiscovered) return;
	Udp.Connect(terminalIp, terminalPort);
	Udp.PacketWrite(msg);
	Udp.PacketSend();
}

void SetupUsbSerial(void)
{
	ConnectorUsb.Mode(Connector::USB_CDC);
	ConnectorUsb.Speed(9600);
	ConnectorUsb.PortOpen();
	uint32_t timeout = 5000;
	uint32_t start = Milliseconds();
	while (!ConnectorUsb && Milliseconds() - start < timeout);
}

void SetupEthernet(void)
{
	EthernetMgr.Setup();
	if (!EthernetMgr.DhcpBegin()) while (1);
	while (!EthernetMgr.PhyLinkActive()) Delay_ms(1000);
	Udp.Begin(LOCAL_PORT);
}

MessageCommand parseMessageCommand(const char *msg)
{
	if (strncmp(msg, "DISCOVER_TELEM", 14) == 0) return CMD_DISCOVER_TELEM;
	if (strncmp(msg, "SET_TORQUE_LIMIT ", 17) == 0) return CMD_SET_TORQUE_LIMIT;
	if (strncmp(msg, "SET_TORQUE_OFFSET", 17) == 0) return CMD_SET_TORQUE_OFFSET;
	if (strcmp(msg, "DISABLE") == 0) return CMD_DISABLE;
	if (strcmp(msg, "ENABLE") == 0) return CMD_ENABLE;
	if (strcmp(msg, "ABORT") == 0) return CMD_ABORT;
	if (strcmp(msg, "JOG_MODE") == 0) return CMD_JOG_MODE;
	if (strncmp(msg, "JOG_UP ", 7) == 0) return CMD_JOG_UP;
	if (strncmp(msg, "JOG_DOWN ", 9) == 0) return CMD_JOG_DOWN;
	if (strcmp(msg, "STANDBY") == 0) return CMD_STANDBY;
	if (strcmp(msg, "HOMING_MACHINE") == 0) return CMD_HOMING_MACHINE;
	if (strcmp(msg, "HOMING_CARTRIDGE") == 0) return CMD_HOMING_CARTRIDGE;
	if (strcmp(msg, "FEED") == 0) return CMD_FEED;
	if (strcmp(msg, "MACHINE_HOME") == 0) return CMD_MACHINE_HOME;
	if (strcmp(msg, "CARTRIDGE_HOME") == 0) return CMD_CARTRIDGE_HOME;
	return CMD_UNKNOWN;
}

void handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp, SystemStates *states)
{
	char *portStr = strstr(msg, "PORT=");
	if (portStr) {
		terminalPort = atoi(portStr + 5);
		terminalIp = senderIp;
		terminalDiscovered = true;
	}
	sendTelem(states); // Always reply with telemetry!
}

void handleSetTorqueLimit(const char *msg)
{
	float newLimit = atof(msg + 17);
	if (newLimit >= 0.0f && newLimit <= 100.0f) {
		globalTorqueLimit = newLimit;
		char response[64];
		snprintf(response, sizeof(response), "torque limit set %.1f", globalTorqueLimit);
		sendToPC(response);
		} else {
		sendToPC("invalid torque limit value");
	}
}

void handleSetTorqueOffset(const char *msg)
{
	float newOffset = atof(msg + 17);
	torqueOffset = newOffset;
	char response[64];
	snprintf(response, sizeof(response), "torque offset set %.2f", torqueOffset);
	sendToPC(response);
}

void handleDisable(SystemStates *states)
{
	states->mainState = DISABLED;
	disableMotors("MOTORS DISABLED (entered DISABLED state)");
	sendToPC("System disabled: must ENABLE to return to standby.");
}

void handleEnable(SystemStates *states)
{
	if (states->mainState == DISABLED) {
		enableMotors("MOTORS ENABLED (returned to STANDBY)");
		states->mainState = STANDBY;
		states->movingState = MOVING_NONE;
		states->homingState = MOVING_HOMING_NONE;
		states->errorState = ERROR_NONE;
		sendToPC("System enabled: state is now STANDBY.");
		} else {
		enableMotors("MOTORS ENABLED (state unchanged)");
		sendToPC("Motors enabled, but system was not in DISABLED state.");
	}
}

void handleAbort(SystemStates *states)
{
	if (states->mainState == MOVING) {
		states->mainState = ERROR;
		states->errorState = ERROR_MANUAL_ABORT;
		states->movingState = MOVING_NONE;
		states->homingState = MOVING_HOMING_NONE;
		sendToPC("ABORT: All motion stopped, state set to ERROR_MANUAL_ABORT");
		} else {
		disableMotors("MOTORS DISABLED VIA ABORT");
		Delay_ms(200);
		enableMotors("MOTORS RE-ENABLED AFTER MANUAL ABORT");
	}
}

void handleJogMode(SystemStates *states)
{
	if (!(states->mainState == MOVING && states->movingState == MOVING_JOG)) {
		states->mainState = MOVING;
		states->movingState = MOVING_JOG;
		sendToPC("Entering JOG mode");
		} else {
		sendToPC("Already in JOG mode");
	}
}

void handleJogUp(const char *msg, SystemStates *states)
{
	if (states->mainState == MOVING && states->movingState == MOVING_JOG) {
		int steps = atoi(msg + 7);
		moveMotors(steps, 0, (int)globalTorqueLimit, FAST, MOTOR_M0);
		sendToPC("Jog UP command executed");
		} else {
		sendToPC("JOG_UP ignored: Not in JOG mode");
	}
}

void handleJogDown(const char *msg, SystemStates *states)
{
	if (states->mainState == MOVING && states->movingState == MOVING_JOG) {
		int steps = atoi(msg + 9);
		moveMotors(-steps, 0, (int)globalTorqueLimit, FAST, MOTOR_M0);
		sendToPC("Jog DOWN command executed");
		} else {
		sendToPC("JOG_DOWN ignored: Not in JOG mode");
	}
}

void handleStandby(SystemStates *states)
{
	bool wasError = (states->mainState == ERROR);
	states->mainState = STANDBY;
	states->movingState = MOVING_NONE;
	states->homingState = MOVING_HOMING_NONE;
	states->errorState = ERROR_NONE;
	if (wasError) {
		sendToPC("Exited ERROR: State set to STANDBY and error cleared");
		} else {
		sendToPC("State set to STANDBY");
	}
}

void handleHomingMachine(SystemStates *states)
{
	if (!(states->mainState == MOVING && states->movingState == MOVING_HOMING && states->homingState == MOVING_HOMING_MACHINE)) {
		states->mainState = MOVING;
		states->movingState = MOVING_HOMING;
		states->homingState = MOVING_HOMING_MACHINE;
		sendToPC("Transition to HOMING_MACHINE");
		} else {
		sendToPC("Already in HOMING_MACHINE");
	}
}

void handleHomingCartridge(SystemStates *states)
{
	if (!(states->mainState == MOVING && states->movingState == MOVING_HOMING && states->homingState == MOVING_HOMING_CARTRIDGE)) {
		states->mainState = MOVING;
		states->movingState = MOVING_HOMING;
		states->homingState = MOVING_HOMING_CARTRIDGE;
		sendToPC("Transition to HOMING_CARTRIDGE");
		} else {
		sendToPC("Already in HOMING_CARTRIDGE");
	}
}

void handleFeed(SystemStates *states)
{
	if (!(states->mainState == MOVING && states->movingState == MOVING_FEEDING)) {
		states->mainState = MOVING;
		states->movingState = MOVING_FEEDING;
		sendToPC("Transition to FEED");
		} else {
		sendToPC("Already in FEED");
	}
}

void handleMachineHome(void)
{
	machineStepCounter = 0;
	sendToPC("MACHINE steps reset");
}

void handleCartridgeHome(void)
{
	cartridgeStepCounter = 0;
	sendToPC("CARTRIDGE steps reset");
}

void handleMessage(const char *msg, SystemStates *states)
{
	switch (parseMessageCommand(msg)) {
		case CMD_DISCOVER_TELEM:   handleDiscoveryTelemPacket(msg, Udp.RemoteIp(), states); break;
		case CMD_SET_TORQUE_LIMIT: handleSetTorqueLimit(msg); break;
		case CMD_SET_TORQUE_OFFSET: handleSetTorqueOffset(msg); break;
		case CMD_DISABLE:          handleDisable(states); break;
		case CMD_ENABLE:           handleEnable(states); break;
		case CMD_ABORT:            handleAbort(states); break;
		case CMD_JOG_MODE:         handleJogMode(states); break;
		case CMD_JOG_UP:           handleJogUp(msg, states); break;
		case CMD_JOG_DOWN:         handleJogDown(msg, states); break;
		case CMD_STANDBY:          handleStandby(states); break;
		case CMD_HOMING_MACHINE:   handleHomingMachine(states); break;
		case CMD_HOMING_CARTRIDGE: handleHomingCartridge(states); break;
		case CMD_FEED:             handleFeed(states); break;
		case CMD_MACHINE_HOME:     handleMachineHome(); break;
		case CMD_CARTRIDGE_HOME:   handleCartridgeHome(); break;
		default:                   sendToPC("Unknown command"); break;
	}
}

void checkUdpBuffer(SystemStates *states)
{
	uint16_t packetSize = Udp.PacketParse();
	if (packetSize > 0) {
		int32_t bytesRead = Udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
		packetBuffer[bytesRead] = '\0';
		handleMessage((char *)packetBuffer, states);
	}
}

void sendTelem(SystemStates *states)
{
	float displayTorque1 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue1, &firstTorqueReading1);
	float displayTorque2 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue2, &firstTorqueReading2);
	int hlfb1 = (int)ConnectorM0.HlfbState();
	int hlfb2 = (int)ConnectorM1.HlfbState();

	char torque1Str[16], torque2Str[16];
	if (displayTorque1 == TORQUE_SENTINEL_INVALID_VALUE) strcpy(torque1Str, "---");
	else snprintf(torque1Str, sizeof(torque1Str), "%.2f", displayTorque1);
	if (displayTorque2 == TORQUE_SENTINEL_INVALID_VALUE) strcpy(torque2Str, "---");
	else snprintf(torque2Str, sizeof(torque2Str), "%.2f", displayTorque2);

	char msg[256];
	snprintf(msg, sizeof(msg),
	"MAIN_STATE: %s, MOVING_STATE: %s, ERROR_STATE: %s, SPEED_STATE: %s, "
	"torque1: %s, hlfb1: %d, enabled1: %d, pos_cmd1: %ld, "
	"torque2: %s, hlfb2: %d, enabled2: %d, pos_cmd2: %ld, "
	"machine_steps: %ld, cartridge_steps: %ld",
	MainStateNames[states->mainState],
	MovingStateNames[states->movingState],
	ErrorStateNames[states->errorState],
	SimpleSpeedStateNames[states->simpleSpeedState],
	torque1Str,
	hlfb1,
	motorsAreEnabled ? 1 : 0,
	ConnectorM0.PositionRefCommanded(),
	torque2Str,
	hlfb2,
	motorsAreEnabled ? 1 : 0,
	ConnectorM1.PositionRefCommanded(),
	(long)machineStepCounter,
	(long)cartridgeStepCounter
	);
	sendToPC(msg);
}

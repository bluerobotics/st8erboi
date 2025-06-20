// ============================================================================
// injector_comms.cpp
//
// UDP communication and telemetry interface for the Injector class.
//
// Handles discovery, message reception, parsing, routing, and response logic
// for communication with the remote terminal. Encodes system state into
// structured telemetry messages and manages connection state.
//
// Also defines command string constants and maps them to `UserCommand` enums
// for interpretation within the handler layer.
//
// Corresponds to declarations in `injector.h`.
//
// Copyright 2025 Blue Robotics Inc.
// Author: Eldin Miller-Stead <eldin@bluerobotics.com>
// ============================================================================

#include "injector.h"

#define USER_CMD_STR_DISCOVER_TELEM					"DISCOVER_TELEM"
#define USER_CMD_STR_ENABLE							"ENABLE"
#define USER_CMD_STR_DISABLE						"DISABLE"
#define USER_CMD_STR_ABORT							"ABORT"
#define USER_CMD_STR_CLEAR_ERRORS					"CLEAR_ERRORS"
#define USER_CMD_STR_STANDBY_MODE					"STANDBY_MODE"
#define USER_CMD_STR_TEST_MODE						"TEST_MODE"
#define USER_CMD_STR_JOG_MODE						"JOG_MODE"
#define USER_CMD_STR_HOMING_MODE					"HOMING_MODE"
#define USER_CMD_STR_FEED_MODE						"FEED_MODE"
#define USER_CMD_STR_SET_INJECTOR_TORQUE_OFFSET		"SET_INJECTOR_TORQUE_OFFSET "
#define USER_CMD_STR_SET_PINCH_TORQUE_OFFSET		"SET_PINCH_TORQUE_OFFSET "
#define USER_CMD_STR_JOG_MOVE						"JOG_MOVE "
#define USER_CMD_STR_MACHINE_HOME_MOVE				"MACHINE_HOME_MOVE "
#define USER_CMD_STR_CARTRIDGE_HOME_MOVE			"CARTRIDGE_HOME_MOVE "
#define USER_CMD_STR_PINCH_HOME_MOVE				"PINCH_HOME_MOVE "
#define USER_CMD_STR_INJECT_MOVE					"INJECT_MOVE "
#define USER_CMD_STR_PURGE_MOVE						"PURGE_MOVE "
#define USER_CMD_STR_PINCH_OPEN						"PINCH_OPEN"
#define USER_CMD_STR_PINCH_CLOSE					"PINCH_CLOSE"
#define USER_CMD_STR_MOVE_TO_CARTRIDGE_HOME			"MOVE_TO_CARTRIDGE_HOME"
#define USER_CMD_STR_MOVE_TO_CARTRIDGE_RETRACT		"MOVE_TO_CARTRIDGE_RETRACT "
#define USER_CMD_STR_PAUSE_INJECTION				"PAUSE_INJECTION"
#define USER_CMD_STR_RESUME_INJECTION				"RESUME_INJECTION"
#define USER_CMD_STR_CANCEL_INJECTION				"CANCEL_INJECTION"

void Injector::sendToPC(const char *msg)
{
	if (!terminalDiscovered) return;
	Udp.Connect(terminalIp, terminalPort);
	Udp.PacketWrite(msg);
	Udp.PacketSend();
}

void Injector::setupUsbSerial(void)
{
	ConnectorUsb.Mode(Connector::USB_CDC);
	ConnectorUsb.Speed(9600);
	ConnectorUsb.PortOpen();
	uint32_t timeout = 5000;
	uint32_t start = Milliseconds();
	while (!ConnectorUsb && Milliseconds() - start < timeout);
}

void Injector::setupEthernet(void)
{
	ConnectorUsb.SendLine("SetupEthernet: Starting...");
	EthernetMgr.Setup();
	ConnectorUsb.SendLine("SetupEthernet: EthernetMgr.Setup() done.");

	if (!EthernetMgr.DhcpBegin()) {
		ConnectorUsb.SendLine("SetupEthernet: DhcpBegin() FAILED. System Halted.");
		while (1);
	}
	
	char ipAddrStr[100];
	// CORRECTED LINE: Using StringValue() which returns char*
	snprintf(ipAddrStr, sizeof(ipAddrStr), "SetupEthernet: DhcpBegin() successful. IP: %s", EthernetMgr.LocalIp().StringValue());
	ConnectorUsb.SendLine(ipAddrStr);

	ConnectorUsb.SendLine("SetupEthernet: Waiting for PhyLinkActive...");
	while (!EthernetMgr.PhyLinkActive()) {
		Delay_ms(1000);
		ConnectorUsb.SendLine("SetupEthernet: Still waiting for PhyLinkActive...");
	}
	ConnectorUsb.SendLine("SetupEthernet: PhyLinkActive is active.");

	Udp.Begin(LOCAL_PORT);
	ConnectorUsb.SendLine("SetupEthernet: Udp.Begin() called for port 8888. Setup Complete.");
}

void Injector::handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp)
{
	char dbgBuffer[256];
	// CORRECTED LINE: Using StringValue() for senderIp
	snprintf(dbgBuffer, sizeof(dbgBuffer), "Discovery: RX from %s:%u, Msg: '%.*s'",
	senderIp.StringValue(), Udp.RemotePort(), MAX_PACKET_LENGTH-1, msg);
	ConnectorUsb.SendLine(dbgBuffer);

	char *portStr = strstr(msg, "PORT=");
	if (portStr) {
		ConnectorUsb.SendLine("Discovery: Found 'PORT=' in message.");
		terminalPort = atoi(portStr + 5);
		terminalIp = senderIp; // terminalIp is also an IpAddress object
		terminalDiscovered = true;
		// CORRECTED LINE: Using StringValue() for terminalIp
		snprintf(dbgBuffer, sizeof(dbgBuffer), "Discovery: Set terminalPort=%u, terminalIp=%s, terminalDiscovered=true",
		terminalPort, terminalIp.StringValue());
		ConnectorUsb.SendLine(dbgBuffer);
		} else {
		ConnectorUsb.SendLine("Discovery Error: 'PORT=' NOT found in message. Cannot discover.");
	}

	ConnectorUsb.SendLine("Discovery: Attempting to send telemetry reply...");
	ConnectorUsb.SendLine("Discovery: Called sendTelem.");
	
	char msg[500]; // Ensure this buffer is large enough
	snprintf(msg, sizeof(msg),
	"DISCOVERED: Set terminalPort=%u, terminalIp=%s, terminalDiscovered=true", terminalPort, terminalIp.StringValue());
	sendToPC(msg);
}

void Injector::checkUdpBuffer()
{
    memset(packetBuffer, 0, MAX_PACKET_LENGTH);
    uint16_t packetSize = Udp.PacketParse();

    if (packetSize > 0) {
        char dbgBuffer[MAX_PACKET_LENGTH + 70]; 
        snprintf(dbgBuffer, sizeof(dbgBuffer), "checkUdpBuffer: Udp.PacketParse() found a packet! Size: %u", packetSize);

        IpAddress remote_ip = Udp.RemoteIp();
        uint16_t remote_port = Udp.RemotePort();
        snprintf(dbgBuffer, sizeof(dbgBuffer), "checkUdpBuffer: Packet is from IP: %s, Port: %u", remote_ip.StringValue(), remote_port);

        int32_t bytesRead = Udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
        
        if (bytesRead > 0) {
            packetBuffer[bytesRead] = '\0'; // Null-terminate the received data
            snprintf(dbgBuffer, sizeof(dbgBuffer), "checkUdpBuffer: Udp.PacketRead() %ld bytes. Msg: '%s'", bytesRead, (char*)packetBuffer);
            handleMessage((char *)packetBuffer, injector);
        } else if (bytesRead == 0) {
        } else {
            // bytesRead < 0 usually indicates an error
        }
    }
}


void Injector::sendTelem(Injector *injector) {
	static uint32_t lastTelemTime = 0;
	uint32_t now = Milliseconds();

	if (!terminalDiscovered || terminalPort == 0) return;
	if (now - lastTelemTime < telemInterval && lastTelemTime != 0) return; // telemInterval is 50ms
	lastTelemTime = now;

	float displayTorque1 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue1, &firstTorqueReading1);
	float displayTorque2 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue2, &firstTorqueReading2);
	int hlfb1_val = (int)ConnectorM0.HlfbState();
	int hlfb2_val = (int)ConnectorM1.HlfbState();
	long current_pos_m0 = ConnectorM0.PositionRefCommanded();
	long current_pos_m1 = ConnectorM1.PositionRefCommanded();

	long machine_pos_from_home = current_pos_m0 - machineHomeReferenceSteps;
	long cartridge_pos_from_home = current_pos_m0 - cartridgeHomeReferenceSteps;

	int machine_is_homed_flag = injector->homingMachineDone ? 1 : 0;
	int cartridge_is_homed_flag = injector->homingCartridgeDone ? 1 : 0;

	float current_dispensed_for_telemetry = 0.0f;
	float current_target_for_telemetry = 0.0f;

	if (injector->active_dispense_INJECTION_ongoing) {
		current_target_for_telemetry = injector->active_op_target_ml;
		if (injector->feedState == FEED_INJECT_ACTIVE || injector->feedState == FEED_PURGE_ACTIVE ||
		injector->feedState == FEED_INJECT_RESUMING || injector->feedState == FEED_PURGE_RESUMING ) {
			// Live calculation based on initial position of the entire operation
			if (injector->active_op_steps_per_ml > 0.0001f) {
				long total_steps_moved_for_op = current_pos_m0 - injector->active_op_initial_axis_steps;
				current_dispensed_for_telemetry = fabs(total_steps_moved_for_op) / injector->active_op_steps_per_ml;
			}
			} else if (injector->feedState == FEED_INJECT_PAUSED || injector->feedState == FEED_PURGE_PAUSED) {
			// When paused, use the accumulated total from before pausing
			current_dispensed_for_telemetry = injector->active_op_total_dispensed_ml;
		}
		// For STARTING injector, current_dispensed_for_telemetry will be 0 as active_op_total_dispensed_ml is 0
		// and active_op_initial_axis_steps equals current_pos_m0.
		} else { // Not active_dispense_INJECTION_ongoing (i.e., idle, completed, or cancelled)
		current_dispensed_for_telemetry = injector->last_completed_dispense_ml;
		current_target_for_telemetry = 0.0f; // No active target
	}

	char torque1Str[16], torque2Str[16];
	if (displayTorque1 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque1Str, "---"); }
	else { snprintf(torque1Str, sizeof(torque1Str), "%.2f", displayTorque1); }
	if (displayTorque2 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque2Str, "---"); }
	else { snprintf(torque2Str, sizeof(torque2Str), "%.2f", displayTorque2); }

	char msg[500]; // Ensure this buffer is large enough
	snprintf(msg, sizeof(msg),
	"MAIN_STATE:%s,HOMING_STATE:%s,HOMING_PHASE:%s,FEED_STATE:%s,ERROR_STATE:%s,"
	"torque1:%s,hlfb1:%d,enabled1:%d,pos_cmd1:%ld,"
	"torque2:%s,hlfb2:%d,enabled2:%d,pos_cmd2:%ld,"
	"machine_steps:%ld,machine_homed:%d,"
	"cartridge_steps:%ld,cartridge_homed:%d,"
	"dispensed_ml:%.2f,target_ml:%.2f", // Changed to : for easier parsing
	injector->maininjectortr(), injector->hominginjectortr(), injector->homingPhaseStr(),
	injector->feedinjectortr(), injector->errorinjectortr(),
	torque1Str, hlfb1_val, motorsAreEnabled ? 1 : 0, current_pos_m0,
	torque2Str, hlfb2_val, motorsAreEnabled ? 1 : 0, current_pos_m1,
	machine_pos_from_home, machine_is_homed_flag,
	cartridge_pos_from_home, cartridge_is_homed_flag,
	current_dispensed_for_telemetry,
	current_target_for_telemetry);

	sendToPC(msg);
}

UserCommand Injector::parseUserCommand(const char *msg) {
	if (strncmp(msg, USER_CMD_STR_DISCOVER_TELEM, strlen(USER_CMD_STR_DISCOVER_TELEM)) == 0) return USER_CMD_DISCOVER_TELEM;

	if (strcmp(msg, USER_CMD_STR_ENABLE) == 0) return USER_CMD_ENABLE;
	if (strcmp(msg, USER_CMD_STR_DISABLE) == 0) return USER_CMD_DISABLE;
	if (strcmp(msg, USER_CMD_STR_ABORT) == 0) return USER_CMD_ABORT;
	if (strcmp(msg, USER_CMD_STR_CLEAR_ERRORS) == 0) return USER_CMD_CLEAR_ERRORS; // New

	if (strcmp(msg, USER_CMD_STR_STANDBY_MODE) == 0) return USER_CMD_STANDBY_MODE;
	if (strcmp(msg, USER_CMD_STR_TEST_MODE) == 0) return USER_CMD_TEST_MODE;
	if (strcmp(msg, USER_CMD_STR_JOG_MODE) == 0) return USER_CMD_JOG_MODE;
	if (strcmp(msg, USER_CMD_STR_HOMING_MODE) == 0) return USER_CMD_HOMING_MODE;
	if (strcmp(msg, USER_CMD_STR_FEED_MODE) == 0) return USER_CMD_FEED_MODE;

	if (strncmp(msg, USER_CMD_STR_SET_INJECTOR_TORQUE_OFFSET, strlen(USER_CMD_STR_SET_INJECTOR_TORQUE_OFFSET)) == 0) return USER_CMD_SET_TORQUE_OFFSET;
	if (strncmp(msg, USER_CMD_STR_JOG_MOVE, strlen(USER_CMD_STR_JOG_MOVE)) == 0) return USER_CMD_JOG_MOVE;
	if (strncmp(msg, USER_CMD_STR_MACHINE_HOME_MOVE, strlen(USER_CMD_STR_MACHINE_HOME_MOVE)) == 0) return USER_CMD_MACHINE_HOME_MOVE;
	if (strncmp(msg, USER_CMD_STR_CARTRIDGE_HOME_MOVE, strlen(USER_CMD_STR_CARTRIDGE_HOME_MOVE)) == 0) return USER_CMD_CARTRIDGE_HOME_MOVE;
	if (strncmp(msg, USER_CMD_STR_INJECT_MOVE, strlen(USER_CMD_STR_INJECT_MOVE)) == 0) return USER_CMD_INJECT_MOVE;
	if (strncmp(msg, USER_CMD_STR_PURGE_MOVE, strlen(USER_CMD_STR_PURGE_MOVE)) == 0) return USER_CMD_PURGE_MOVE;
	// if (strncmp(msg, USER_CMD_STR_RETRACT_MOVE, strlen(USER_CMD_STR_RETRACT_MOVE)) == 0) return USER_CMD_RETRACT_MOVE;

	if (strcmp(msg, USER_CMD_STR_MOVE_TO_CARTRIDGE_HOME) == 0) return USER_CMD_MOVE_TO_CARTRIDGE_HOME;
	if (strncmp(msg, USER_CMD_STR_MOVE_TO_CARTRIDGE_RETRACT, strlen(USER_CMD_STR_MOVE_TO_CARTRIDGE_RETRACT)) == 0) return USER_CMD_MOVE_TO_CARTRIDGE_RETRACT;

	if (strcmp(msg, USER_CMD_STR_PAUSE_INJECTION) == 0) return USER_CMD_PAUSE_INJECTION;
	if (strcmp(msg, USER_CMD_STR_RESUME_INJECTION) == 0) return USER_CMD_RESUME_INJECTION;
	if (strcmp(msg, USER_CMD_STR_CANCEL_INJECTION) == 0) return USER_CMD_CANCEL_INJECTION;

	if (strcmp(msg, USER_CMD_STR_PINCH_HOME_MOVE) == 0) return USER_CMD_PINCH_HOME_MOVE;	
	if (strcmp(msg, USER_CMD_STR_PINCH_OPEN) == 0) return USER_CMD_PINCH_OPEN;
	if (strcmp(msg, USER_CMD_STR_PINCH_CLOSE) == 0) return USER_CMD_PINCH_CLOSE;

	return USER_CMD_UNKNOWN;
}

void Injector::handleMessage(const char *msg, Injector *injector) {
	UserCommand command = parseUserCommand(msg);

	switch (command) {
		case USER_CMD_DISCOVER_TELEM:        handleDiscoveryTelemPacket(msg, Udp.RemoteIp()); break;
		case USER_CMD_ENABLE:                handleEnable(injector); break;
		case USER_CMD_DISABLE:               handleDisable(injector); break;
		case USER_CMD_ABORT:                 handleAbort(injector); break;
		case USER_CMD_CLEAR_ERRORS:          handleClearErrors(injector); break;
		case USER_CMD_STANDBY_MODE:          handleStandbyMode(injector); break;
		case USER_CMD_TEST_MODE:             handleTestMode(injector); break;
		case USER_CMD_JOG_MODE:              handleJogMode(injector); break;
		case USER_CMD_HOMING_MODE:           handleHomingMode(injector); break;
		case USER_CMD_FEED_MODE:             handleFeedMode(injector); break;
		case USER_CMD_SET_TORQUE_OFFSET:     handleSetinjectorMotorsTorqueOffset(msg); break;
		case USER_CMD_JOG_MOVE:              handleJogMove(msg, injector); break;
		case USER_CMD_MACHINE_HOME_MOVE:     handleMachineHomeMove(msg, injector); break;
		case USER_CMD_CARTRIDGE_HOME_MOVE:   handleCartridgeHomeMove(msg, injector); break;
		case USER_CMD_INJECT_MOVE:           handleInjectMove(msg, injector); break;
		case USER_CMD_PURGE_MOVE:            handlePurgeMove(msg, injector); break;
		case USER_CMD_MOVE_TO_CARTRIDGE_HOME:    handleMoveToCartridgeHome(injector); break;
		case USER_CMD_MOVE_TO_CARTRIDGE_RETRACT: handleMoveToCartridgeRetract(msg, injector); break;
		case USER_CMD_PAUSE_INJECTION:       handlePauseOperation(injector); break;
		case USER_CMD_RESUME_INJECTION:      handleResumeOperation(injector); break;
		case USER_CMD_CANCEL_INJECTION:      handleCancelOperation(injector); break;
		case USER_CMD_PINCH_HOME_MOVE:       handlePinchHomeMove(injector); break;
		case USER_CMD_PINCH_OPEN:			handlePinchOpen(injector); break;
		case USER_CMD_PINCH_CLOSE:			handlePinchClose(injector); break;

		case USER_CMD_UNKNOWN:
		default:
		char unknownCmdMsg[128];
		int msg_len = strlen(msg);
		int max_len_to_copy = (sizeof(unknownCmdMsg) - strlen("Unknown cmd: ''") - 2);
		if (max_len_to_copy < 0) max_len_to_copy = 0;
		if (msg_len > max_len_to_copy) msg_len = max_len_to_copy;
		snprintf(unknownCmdMsg, sizeof(unknownCmdMsg), "Unknown cmd: '%.*s'", msg_len, msg);
		sendToPC(unknownCmdMsg);
		break;
	}
}
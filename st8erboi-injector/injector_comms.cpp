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

void Injector::handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp) {
	char dbgBuffer[256];
	snprintf(dbgBuffer, sizeof(dbgBuffer), "Discovery: RX from %s:%u, Msg: '%.*s'",
	senderIp.StringValue(), Udp.RemotePort(), MAX_PACKET_LENGTH - 1, msg);
	ConnectorUsb.SendLine(dbgBuffer);

	char *portStr = strstr(msg, "PORT=");
	if (portStr) {
		ConnectorUsb.SendLine("Discovery: Found 'PORT=' in message.");
		terminalPort = atoi(portStr + 5);
		terminalIp = senderIp;
		terminalDiscovered = true;

		snprintf(dbgBuffer, sizeof(dbgBuffer),
		"Discovery: Set terminalPort=%u, terminalIp=%s, terminalDiscovered=true",
		terminalPort, terminalIp.StringValue());
		ConnectorUsb.SendLine(dbgBuffer);
		} else {
		ConnectorUsb.SendLine("Discovery Error: 'PORT=' NOT found in message. Cannot discover.");
	}

	sendToPC("DISCOVERY: INJECTOR DISCOVERED");
}

void Injector::checkUdpBuffer(void){
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
            handleMessage((char *)packetBuffer);
        } else if (bytesRead == 0) {
        } else {
            // bytesRead < 0 usually indicates an error
        }
    }
}


// This is the full, updated function for injector_comms.cpp

void Injector::sendTelem(void) {
	static uint32_t lastTelemTime = 0;
	uint32_t now = Milliseconds();

	if (!terminalDiscovered || terminalPort == 0) return;
	if (now - lastTelemTime < telemInterval && lastTelemTime != 0) return;
	lastTelemTime = now;

	// --- Data gathering for zero-indexed motors 0, 1, 2 ---
	float displayTorque0 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue0, &firstTorqueReading0);
	float displayTorque1 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue1, &firstTorqueReading1);
	float displayTorque2 = getSmoothedTorqueEWMA(&ConnectorM2, &smoothedTorqueValue2, &firstTorqueReading2);
	
	int hlfb0_val = (int)ConnectorM0.HlfbState();
	int hlfb1_val = (int)ConnectorM1.HlfbState();
	int hlfb2_val = (int)ConnectorM2.HlfbState();

	long current_pos_m0 = ConnectorM0.PositionRefCommanded();
	long current_pos_m1 = ConnectorM1.PositionRefCommanded();
	long current_pos_m2 = ConnectorM2.PositionRefCommanded();

	// Homing status for each motor
	int is_homed0 = homingMachineDone ? 1 : 0; // Motor 0 is tied to machine home
	int is_homed1 = homingMachineDone ? 1 : 0; // Motor 1 is also tied to machine home
	int is_homed2 = homingPinchDone ? 1 : 0;   // Motor 2 is tied to its own pinch home status

	// Legacy relative position values
	long machine_pos_from_home = current_pos_m0 - machineHomeReferenceSteps;
	long cartridge_pos_from_home = current_pos_m0 - cartridgeHomeReferenceSteps;

	// Dispense status logic
	float current_dispensed_for_telemetry = 0.0f;
	float current_target_for_telemetry = 0.0f;

	if (active_dispense_INJECTION_ongoing) {
		current_target_for_telemetry = active_op_target_ml;
		if (feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE ||
		feedState == FEED_INJECT_RESUMING || feedState == FEED_PURGE_RESUMING ) {
			if (active_op_steps_per_ml > 0.0001f) {
				long total_steps_moved_for_op = current_pos_m0 - active_op_initial_axis_steps;
				current_dispensed_for_telemetry = fabs(total_steps_moved_for_op) / active_op_steps_per_ml;
			}
			} else if (feedState == FEED_INJECT_PAUSED || feedState == FEED_PURGE_PAUSED) {
			current_dispensed_for_telemetry = active_op_total_dispensed_ml;
		}
		} else {
		current_dispensed_for_telemetry = last_completed_dispense_ml;
		current_target_for_telemetry = 0.0f;
	}

	// --- Prepare Torque Strings for motors 0, 1, 2 ---
	char torque0Str[16], torque1Str[16], torque2Str[16];
	if (displayTorque0 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque0Str, "---"); } else { snprintf(torque0Str, sizeof(torque0Str), "%.2f", displayTorque0); }
	if (displayTorque1 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque1Str, "---"); } else { snprintf(torque1Str, sizeof(torque1Str), "%.2f", displayTorque1); }
	if (displayTorque2 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque2Str, "---"); } else { snprintf(torque2Str, sizeof(torque2Str), "%.2f", displayTorque2); }

	// --- Assemble new zero-indexed telemetry packet ---
	char msg[600];
	snprintf(msg, sizeof(msg),
	"%s,MAIN_STATE:%s,HOMING_STATE:%s,HOMING_PHASE:%s,FEED_STATE:%s,ERROR_STATE:%s,"
	"torque0:%s,hlfb0:%d,enabled0:%d,pos_cmd0:%ld,homed0:%d,"
	"torque1:%s,hlfb1:%d,enabled1:%d,pos_cmd1:%ld,homed1:%d,"
	"torque2:%s,hlfb2:%d,enabled2:%d,pos_cmd2:%ld,homed2:%d,"
	"machine_steps:%ld,cartridge_steps:%ld,"
	"dispensed_ml:%.2f,target_ml:%.2f",
	TELEM_PREFIX_GUI, // The new prefix
	// State strings
	mainStateStr(), homingStateStr(), homingPhaseStr(),
	feedStateStr(), errorStateStr(),
	// Motor 0 Data
	torque0Str, hlfb0_val, (int)(ConnectorM0.HlfbState() == MotorDriver::HLFB_ASSERTED), current_pos_m0, is_homed0,
	// Motor 1 Data
	torque1Str, hlfb1_val, (int)(ConnectorM1.HlfbState() == MotorDriver::HLFB_ASSERTED), current_pos_m1, is_homed1,
	// Motor 2 Data
	torque2Str, hlfb2_val, (int)(ConnectorM2.HlfbState() == MotorDriver::HLFB_ASSERTED), current_pos_m2, is_homed2,
	// Legacy Homing and dispense data
	machine_pos_from_home, cartridge_pos_from_home,
	current_dispensed_for_telemetry,
	current_target_for_telemetry);

	sendToPC(msg);
}

UserCommand Injector::parseUserCommand(const char *msg) {
	if (strcmp(msg, USER_CMD_STR_REQUEST_TELEM) == 0) return USER_CMD_REQUEST_TELEM; // ADD THIS LINE
	if (strncmp(msg, USER_CMD_STR_DISCOVER_TELEM, strlen(USER_CMD_STR_DISCOVER_TELEM)) == 0) return USER_CMD_DISCOVER_TELEM;

	if (strcmp(msg, USER_CMD_STR_ENABLE) == 0) return USER_CMD_ENABLE;
	if (strcmp(msg, USER_CMD_STR_DISABLE) == 0) return USER_CMD_DISABLE;
	if (strcmp(msg, USER_CMD_STR_ABORT) == 0) return USER_CMD_ABORT;
	if (strcmp(msg, USER_CMD_STR_CLEAR_ERRORS) == 0) return USER_CMD_CLEAR_ERRORS;

	if (strcmp(msg, USER_CMD_STR_STANDBY_MODE) == 0) return USER_CMD_STANDBY_MODE;
	if (strcmp(msg, USER_CMD_STR_JOG_MODE) == 0) return USER_CMD_JOG_MODE;
	if (strcmp(msg, USER_CMD_STR_HOMING_MODE) == 0) return USER_CMD_HOMING_MODE;
	if (strcmp(msg, USER_CMD_STR_FEED_MODE) == 0) return USER_CMD_FEED_MODE;

	if (strncmp(msg, USER_CMD_STR_SET_INJECTOR_TORQUE_OFFSET, strlen(USER_CMD_STR_SET_INJECTOR_TORQUE_OFFSET)) == 0) return USER_CMD_SET_TORQUE_OFFSET;
	if (strncmp(msg, USER_CMD_STR_JOG_MOVE, strlen(USER_CMD_STR_JOG_MOVE)) == 0) return USER_CMD_JOG_MOVE;
	if (strncmp(msg, USER_CMD_STR_MACHINE_HOME_MOVE, strlen(USER_CMD_STR_MACHINE_HOME_MOVE)) == 0) return USER_CMD_MACHINE_HOME_MOVE;
	if (strncmp(msg, USER_CMD_STR_CARTRIDGE_HOME_MOVE, strlen(USER_CMD_STR_CARTRIDGE_HOME_MOVE)) == 0) return USER_CMD_CARTRIDGE_HOME_MOVE;
	if (strncmp(msg, USER_CMD_STR_INJECT_MOVE, strlen(USER_CMD_STR_INJECT_MOVE)) == 0) return USER_CMD_INJECT_MOVE;
	if (strncmp(msg, USER_CMD_STR_PURGE_MOVE, strlen(USER_CMD_STR_PURGE_MOVE)) == 0) return USER_CMD_PURGE_MOVE;
	
	if (strcmp(msg, USER_CMD_STR_MOVE_TO_CARTRIDGE_HOME) == 0) return USER_CMD_MOVE_TO_CARTRIDGE_HOME;
	if (strncmp(msg, USER_CMD_STR_MOVE_TO_CARTRIDGE_RETRACT, strlen(USER_CMD_STR_MOVE_TO_CARTRIDGE_RETRACT)) == 0) return USER_CMD_MOVE_TO_CARTRIDGE_RETRACT;

	if (strcmp(msg, USER_CMD_STR_PAUSE_INJECTION) == 0) return USER_CMD_PAUSE_INJECTION;
	if (strcmp(msg, USER_CMD_STR_RESUME_INJECTION) == 0) return USER_CMD_RESUME_INJECTION;
	if (strcmp(msg, USER_CMD_STR_CANCEL_INJECTION) == 0) return USER_CMD_CANCEL_INJECTION;

    // --- ADDED THIS LINE BACK ---
	if (strcmp(msg, USER_CMD_STR_PINCH_HOME_MOVE) == 0) return USER_CMD_PINCH_HOME_MOVE;
	if (strncmp(msg, USER_CMD_STR_PINCH_JOG_MOVE, strlen(USER_CMD_STR_PINCH_JOG_MOVE)) == 0) return USER_CMD_PINCH_JOG_MOVE;
	if (strcmp(msg, USER_CMD_STR_PINCH_OPEN) == 0) return USER_CMD_PINCH_OPEN;
	if (strcmp(msg, USER_CMD_STR_PINCH_CLOSE) == 0) return USER_CMD_PINCH_CLOSE;
    if (strcmp(msg, USER_CMD_STR_ENABLE_PINCH) == 0) return USER_CMD_ENABLE_PINCH;
    if (strcmp(msg, USER_CMD_STR_DISABLE_PINCH) == 0) return USER_CMD_DISABLE_PINCH;

	return USER_CMD_UNKNOWN;
}


/**
 * @brief Routes a parsed command to the appropriate handler function.
 * @param msg The raw command string, needed for handlers that parse arguments.
 */
void Injector::handleMessage(const char *msg) {
	UserCommand command = parseUserCommand(msg);

	switch (command) {
		case USER_CMD_REQUEST_TELEM:             sendTelem(); break;
		case USER_CMD_DISCOVER_TELEM:            handleDiscoveryTelemPacket(msg, Udp.RemoteIp()); break;
		case USER_CMD_ENABLE:                    handleEnable(); break;
		case USER_CMD_DISABLE:                   handleDisable(); break;
		case USER_CMD_ABORT:                     handleAbort(); break;
		case USER_CMD_CLEAR_ERRORS:              handleClearErrors(); break;
		case USER_CMD_STANDBY_MODE:              handleStandbyMode(); break;
		case USER_CMD_JOG_MODE:                  handleJogMode(); break;
		case USER_CMD_HOMING_MODE:               handleHomingMode(); break;
		case USER_CMD_FEED_MODE:                 handleFeedMode(); break;
		case USER_CMD_SET_TORQUE_OFFSET:         handleSetinjectorMotorsTorqueOffset(msg); break;
		case USER_CMD_JOG_MOVE:                  handleJogMove(msg); break;
		case USER_CMD_MACHINE_HOME_MOVE:         handleMachineHomeMove(msg); break;
		case USER_CMD_CARTRIDGE_HOME_MOVE:       handleCartridgeHomeMove(msg); break;
		case USER_CMD_INJECT_MOVE:               handleInjectMove(msg); break;
		case USER_CMD_PURGE_MOVE:                handlePurgeMove(msg); break;
		case USER_CMD_MOVE_TO_CARTRIDGE_HOME:    handleMoveToCartridgeHome(); break;
		case USER_CMD_MOVE_TO_CARTRIDGE_RETRACT: handleMoveToCartridgeRetract(msg); break;
		case USER_CMD_PAUSE_INJECTION:           handlePauseOperation(); break;
		case USER_CMD_RESUME_INJECTION:          handleResumeOperation(); break;
		case USER_CMD_CANCEL_INJECTION:          handleCancelOperation(); break;
		
        // --- ADDED THIS CASE BACK ---
        case USER_CMD_PINCH_HOME_MOVE:           handlePinchHomeMove(); break;
		case USER_CMD_PINCH_JOG_MOVE:            handlePinchJogMove(msg); break;
		case USER_CMD_PINCH_OPEN:                handlePinchOpen(); break;
		case USER_CMD_PINCH_CLOSE:               handlePinchClose(); break;
        case USER_CMD_ENABLE_PINCH:              handleEnablePinch(); break;
        case USER_CMD_DISABLE_PINCH:             handleDisablePinch(); break;

		case USER_CMD_UNKNOWN:
		default:
			// be silent
			break;
	}
}
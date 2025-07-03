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

void Injector::sendStatus(const char* statusType, const char* message) {
	char msg[512];
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

void Injector::setupSerial(void)
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

	udp.Begin(LOCAL_PORT);
	ConnectorUsb.SendLine("SetupEthernet: udp.Begin() called for port 8888. Setup Complete.");
}

void Injector::processUdp() {
	if (udp.PacketParse()) {
		int32_t bytesRead = udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
		if (bytesRead > 0) {
			packetBuffer[bytesRead] = '\0';
			handleMessage((char*)packetBuffer);
		}
	}
}


// This is the full, updated function for injector_comms.cpp
void Injector::sendGuiTelemetry(void){
	
	// --- Data gathering for motors 0, 1, 2 ---
	float displayTorque0 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue0, &firstTorqueReading0);
	float displayTorque1 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue1, &firstTorqueReading1);
	float displayTorque2 = getSmoothedTorqueEWMA(&ConnectorM2, &smoothedTorqueValue2, &firstTorqueReading2);
	
	int hlfb0_val = (int)ConnectorM0.HlfbState();
	int hlfb1_val = (int)ConnectorM1.HlfbState();
	int hlfb2_val = (int)ConnectorM2.HlfbState();

	long current_pos_steps_m0 = ConnectorM0.PositionRefCommanded();
	long current_pos_steps_m1 = ConnectorM1.PositionRefCommanded();
	long current_pos_steps_m2 = ConnectorM2.PositionRefCommanded();

    // --- NEW: Convert absolute positions to millimeters ---
    float pos_mm_m0 = (float)current_pos_steps_m0 / STEPS_PER_MM_M0;
    float pos_mm_m1 = (float)current_pos_steps_m1 / STEPS_PER_MM_M1;
    float pos_mm_m2 = (float)current_pos_steps_m2 / STEPS_PER_MM_M2;

	// Homing status for each motor
	int is_homed0 = homingMachineDone ? 1 : 0;
	int is_homed1 = homingMachineDone ? 1 : 0;
	int is_homed2 = homingPinchDone ? 1 : 0;

	// --- MODIFIED: Calculate relative positions in millimeters ---
	float machine_pos_mm = (float)(current_pos_steps_m0 - machineHomeReferenceSteps) / STEPS_PER_MM_M0;
	float cartridge_pos_mm = (float)(current_pos_steps_m0 - cartridgeHomeReferenceSteps) / STEPS_PER_MM_M0;

	// Dispense status logic (unchanged)
	float current_dispensed_for_telemetry = 0.0f;
	float current_target_for_telemetry = 0.0f;
	if (active_dispense_INJECTION_ongoing) {
		current_target_for_telemetry = active_op_target_ml;
		if (feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE ||
		    feedState == FEED_INJECT_RESUMING || feedState == FEED_PURGE_RESUMING ) {
			if (active_op_steps_per_ml > 0.0001f) {
				long total_steps_moved_for_op = current_pos_steps_m0 - active_op_initial_axis_steps;
				current_dispensed_for_telemetry = fabs(total_steps_moved_for_op) / active_op_steps_per_ml;
			}
		} else if (feedState == FEED_INJECT_PAUSED || feedState == FEED_PURGE_PAUSED) {
			current_dispensed_for_telemetry = active_op_total_dispensed_ml;
		}
	} else {
		current_dispensed_for_telemetry = last_completed_dispense_ml;
		current_target_for_telemetry = 0.0f;
	}

	char torque0Str[16], torque1Str[16], torque2Str[16];
	if (displayTorque0 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque0Str, "---"); } else { snprintf(torque0Str, sizeof(torque0Str), "%.2f", displayTorque0); }
	if (displayTorque1 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque1Str, "---"); } else { snprintf(torque1Str, sizeof(torque1Str), "%.2f", displayTorque1); }
	if (displayTorque2 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque2Str, "---"); } else { snprintf(torque2Str, sizeof(torque2Str), "%.2f", displayTorque2); }

	// --- MODIFIED: Assemble new telemetry packet with mm units and peer status ---
	char msg[512];
	snprintf(msg, sizeof(msg),
	    "MAIN_STATE:%s,HOMING_STATE:%s,HOMING_PHASE:%s,FEED_STATE:%s,ERROR_STATE:%s,"
	    "torque0:%s,hlfb0:%d,enabled0:%d,pos_mm0:%.2f,homed0:%d,"      // pos_cmd -> pos_mm
	    "torque1:%s,hlfb1:%d,enabled1:%d,pos_mm1:%.2f,homed1:%d,"      // pos_cmd -> pos_mm
	    "torque2:%s,hlfb2:%d,enabled2:%d,pos_mm2:%.2f,homed2:%d,"      // pos_cmd -> pos_mm
	    "machine_mm:%.2f,cartridge_mm:%.2f,"                         // machine_steps -> machine_mm, etc.
	    "dispensed_ml:%.2f,target_ml:%.2f,"
        "peer_disc:%d,peer_ip:%s",                                   // NEW peer status fields
	    mainStateStr(), homingStateStr(), homingPhaseStr(), feedStateStr(), errorStateStr(),
	    // Motor 0 Data
	    torque0Str, hlfb0_val, (int)ConnectorM0.StatusReg().bit.Enabled, pos_mm_m0, is_homed0,
	    // Motor 1 Data
	    torque1Str, hlfb1_val, (int)ConnectorM1.StatusReg().bit.Enabled, pos_mm_m1, is_homed1,
	    // Motor 2 Data
	    torque2Str, hlfb2_val, (int)ConnectorM2.StatusReg().bit.Enabled, pos_mm_m2, is_homed2,
	    // Relative positions in mm
	    machine_pos_mm, cartridge_pos_mm,
	    // Dispense data
	    current_dispensed_for_telemetry, current_target_for_telemetry,
        // Peer data
        (int)peerDiscovered, peerIp.StringValue());

	sendStatus(TELEM_PREFIX_GUI, msg);
}

UserCommand Injector::parseCommand(const char *msg) {
	if (strcmp(msg, CMD_STR_REQUEST_TELEM) == 0) return CMD_REQUEST_TELEM; // ADD THIS LINE
	if (strncmp(msg, CMD_STR_DISCOVER, strlen(CMD_STR_DISCOVER)) == 0) return CMD_DISCOVER;

	if (strcmp(msg, CMD_STR_ENABLE) == 0) return CMD_ENABLE;
	if (strcmp(msg, CMD_STR_DISABLE) == 0) return CMD_DISABLE;
	if (strcmp(msg, CMD_STR_ABORT) == 0) return CMD_ABORT;
	if (strcmp(msg, CMD_STR_CLEAR_ERRORS) == 0) return CMD_CLEAR_ERRORS;

	if (strcmp(msg, CMD_STR_STANDBY_MODE) == 0) return CMD_STANDBY_MODE;
	if (strcmp(msg, CMD_STR_JOG_MODE) == 0) return CMD_JOG_MODE;
	if (strcmp(msg, CMD_STR_HOMING_MODE) == 0) return CMD_HOMING_MODE;
	if (strcmp(msg, CMD_STR_FEED_MODE) == 0) return CMD_FEED_MODE;

	if (strncmp(msg, CMD_STR_SET_INJECTOR_TORQUE_OFFSET, strlen(CMD_STR_SET_INJECTOR_TORQUE_OFFSET)) == 0) return CMD_SET_TORQUE_OFFSET;
	if (strncmp(msg, CMD_STR_JOG_MOVE, strlen(CMD_STR_JOG_MOVE)) == 0) return CMD_JOG_MOVE;
	if (strncmp(msg, CMD_STR_MACHINE_HOME_MOVE, strlen(CMD_STR_MACHINE_HOME_MOVE)) == 0) return CMD_MACHINE_HOME_MOVE;
	if (strncmp(msg, CMD_STR_CARTRIDGE_HOME_MOVE, strlen(CMD_STR_CARTRIDGE_HOME_MOVE)) == 0) return CMD_CARTRIDGE_HOME_MOVE;
	if (strncmp(msg, CMD_STR_INJECT_MOVE, strlen(CMD_STR_INJECT_MOVE)) == 0) return CMD_INJECT_MOVE;
	if (strncmp(msg, CMD_STR_PURGE_MOVE, strlen(CMD_STR_PURGE_MOVE)) == 0) return CMD_PURGE_MOVE;
	
	if (strcmp(msg, CMD_STR_MOVE_TO_CARTRIDGE_HOME) == 0) return CMD_MOVE_TO_CARTRIDGE_HOME;
	if (strncmp(msg, CMD_STR_MOVE_TO_CARTRIDGE_RETRACT, strlen(CMD_STR_MOVE_TO_CARTRIDGE_RETRACT)) == 0) return CMD_MOVE_TO_CARTRIDGE_RETRACT;

	if (strcmp(msg, CMD_STR_PAUSE_INJECTION) == 0) return CMD_PAUSE_INJECTION;
	if (strcmp(msg, CMD_STR_RESUME_INJECTION) == 0) return CMD_RESUME_INJECTION;
	if (strcmp(msg, CMD_STR_CANCEL_INJECTION) == 0) return CMD_CANCEL_INJECTION;

    // --- ADDED THIS LINE BACK ---
	if (strcmp(msg, CMD_STR_PINCH_HOME_MOVE) == 0) return CMD_PINCH_HOME_MOVE;
	if (strncmp(msg, CMD_STR_PINCH_JOG_MOVE, strlen(CMD_STR_PINCH_JOG_MOVE)) == 0) return CMD_PINCH_JOG_MOVE;
	if (strcmp(msg, CMD_STR_PINCH_OPEN) == 0) return CMD_PINCH_OPEN;
	if (strcmp(msg, CMD_STR_PINCH_CLOSE) == 0) return CMD_PINCH_CLOSE;
    if (strcmp(msg, CMD_STR_ENABLE_PINCH) == 0) return CMD_ENABLE_PINCH;
    if (strcmp(msg, CMD_STR_DISABLE_PINCH) == 0) return CMD_DISABLE_PINCH;
	
	if (strncmp(msg, CMD_STR_SET_PEER_IP, strlen(CMD_STR_SET_PEER_IP)) == 0) return CMD_SET_PEER_IP;
	if (strcmp(msg, CMD_STR_CLEAR_PEER_IP) == 0) return CMD_CLEAR_PEER_IP;

	return CMD_UNKNOWN;
}


/**
 * @brief Routes a parsed command to the appropriate handler function.
 * @param msg The raw command string, needed for handlers that parse arguments.
 */
void Injector::handleMessage(const char *msg) {
	UserCommand command = parseCommand(msg);

	switch (command) {
		case CMD_REQUEST_TELEM:             sendGuiTelemetry(); break;
		case CMD_DISCOVER: {
			char* portStr = strstr(msg, "PORT=");
			if (portStr) {
				guiIp = udp.RemoteIp();
				guiPort = atoi(portStr + 5);
				guiDiscovered = true;
				sendStatus(STATUS_PREFIX_DISCOVERY, "INJECTOR DISCOVERED");
			}
			break;
		}
		case CMD_ENABLE:                    handleEnable(); break;
		case CMD_DISABLE:                   handleDisable(); break;
		case CMD_ABORT:                     handleAbort(); break;
		case CMD_CLEAR_ERRORS:              handleClearErrors(); break;
		case CMD_STANDBY_MODE:              handleStandbyMode(); break;
		case CMD_JOG_MODE:                  handleJogMode(); break;
		case CMD_HOMING_MODE:               handleHomingMode(); break;
		case CMD_FEED_MODE:                 handleFeedMode(); break;
		case CMD_SET_TORQUE_OFFSET:         handleSetinjectorMotorsTorqueOffset(msg); break;
		case CMD_JOG_MOVE:                  handleJogMove(msg); break;
		case CMD_MACHINE_HOME_MOVE:         handleMachineHomeMove(msg); break;
		case CMD_CARTRIDGE_HOME_MOVE:       handleCartridgeHomeMove(msg); break;
		case CMD_INJECT_MOVE:               handleInjectMove(msg); break;
		case CMD_PURGE_MOVE:                handlePurgeMove(msg); break;
		case CMD_MOVE_TO_CARTRIDGE_HOME:    handleMoveToCartridgeHome(); break;
		case CMD_MOVE_TO_CARTRIDGE_RETRACT: handleMoveToCartridgeRetract(msg); break;
		case CMD_PAUSE_INJECTION:           handlePauseOperation(); break;
		case CMD_RESUME_INJECTION:          handleResumeOperation(); break;
		case CMD_CANCEL_INJECTION:          handleCancelOperation(); break;
		
        // --- ADDED THIS CASE BACK ---
        case CMD_PINCH_HOME_MOVE:           handlePinchHomeMove(); break;
		case CMD_PINCH_JOG_MOVE:            handlePinchJogMove(msg); break;
		case CMD_PINCH_OPEN:                handlePinchOpen(); break;
		case CMD_PINCH_CLOSE:               handlePinchClose(); break;
        case CMD_ENABLE_PINCH:              handleEnablePinch(); break;
        case CMD_DISABLE_PINCH:             handleDisablePinch(); break;
		case CMD_SET_PEER_IP:
            handleSetPeerIp(msg);
            break;
        case CMD_CLEAR_PEER_IP:
            handleClearPeerIp();
            break;

		case CMD_UNKNOWN:
		default:
			// be silent
			break;
	}
}

void Injector::handleSetPeerIp(const char* msg) {
    const char* ipStr = msg + strlen(CMD_STR_SET_PEER_IP);
    IpAddress newPeerIp(ipStr);
    if (strcmp(newPeerIp.StringValue(), "0.0.0.0") == 0) {
        peerDiscovered = false;
        sendStatus(STATUS_PREFIX_ERROR, "Failed to parse peer IP address");
    } else {
        peerIp = newPeerIp;
        peerDiscovered = true;
        char response[100];
        snprintf(response, sizeof(response), "Peer IP set to %s", ipStr);
        sendStatus(STATUS_PREFIX_INFO, response);
    }
}

void Injector::handleClearPeerIp() {
    peerDiscovered = false;
    sendStatus(STATUS_PREFIX_INFO, "Peer IP cleared");
}
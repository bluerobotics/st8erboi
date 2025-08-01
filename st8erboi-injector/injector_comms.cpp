// ============================================================================
// injector_comms.cpp (TCP Version with UDP Discovery)
//
// Handles TCP communication for commands/telemetry and UDP for discovery.
// ============================================================================

#include "injector.h"

// Sends a status message to the currently connected TCP client.
void Injector::sendStatus(const char* statusType, const char* message) {
	if (client.Connected()) {
		char msg[512];
		snprintf(msg, sizeof(msg), "%s%s\n", statusType, message);
		client.Send(msg);
	}
}

// Sets up USB serial for debugging.
void Injector::setupSerial(void) {
	ConnectorUsb.Mode(Connector::USB_CDC);
	ConnectorUsb.Speed(9600);
	ConnectorUsb.PortOpen();
	uint32_t timeout = 5000;
	uint32_t start = Milliseconds();
	while (!ConnectorUsb && Milliseconds() - start < timeout);
}

// Initializes Ethernet, starts the TCP server, and begins listening for UDP discovery packets.
void Injector::setupEthernet(void) {
	ConnectorUsb.SendLine("SetupEthernet: Starting...");
	EthernetMgr.Setup();
	ConnectorUsb.SendLine("SetupEthernet: EthernetMgr.Setup() done.");

	if (!EthernetMgr.DhcpBegin()) {
		ConnectorUsb.SendLine("SetupEthernet: DhcpBegin() FAILED. System Halted.");
		while (1);
	}
	
	char ipAddrStr[100];
	snprintf(ipAddrStr, sizeof(ipAddrStr), "SetupEthernet: IP: %s", EthernetMgr.LocalIp().StringValue());
	ConnectorUsb.SendLine(ipAddrStr);

	ConnectorUsb.SendLine("SetupEthernet: Waiting for PhyLinkActive...");
	while (!EthernetMgr.PhyLinkActive()) {
		Delay_ms(1000);
	}
	ConnectorUsb.SendLine("SetupEthernet: PhyLinkActive is active.");

	// Start the TCP server for main communication
	server.Begin();
	char serverMsg[100];
	snprintf(serverMsg, sizeof(serverMsg), "SetupEthernet: TCP server started on port %d.", LOCAL_PORT);
	ConnectorUsb.SendLine(serverMsg);

	// Begin listening on the discovery port for UDP broadcasts
	discoveryUdp.Begin(DISCOVERY_PORT);
	char discoveryMsg[100];
	snprintf(discoveryMsg, sizeof(discoveryMsg), "SetupEthernet: UDP Discovery listening on port %d.", DISCOVERY_PORT);
	ConnectorUsb.SendLine(discoveryMsg);
}

// New function to listen for and respond to discovery broadcasts.
void Injector::processDiscovery() {
	if (discoveryUdp.PacketParse()) {
		char packetBuffer[64];
		int32_t bytesRead = discoveryUdp.PacketRead(packetBuffer, 63);
		if (bytesRead > 0) {
			packetBuffer[bytesRead] = '\0';

			if (strcmp(packetBuffer, "DISCOVER_DEVICES_V2") == 0) {
				IpAddress remoteIp = discoveryUdp.RemoteIp();
				uint16_t remotePort = discoveryUdp.RemotePort();
				
				char response[64];
				snprintf(response, sizeof(response), "INJECTOR_IP_IS %s", EthernetMgr.LocalIp().StringValue());
				
				discoveryUdp.Connect(remoteIp, remotePort);
				discoveryUdp.PacketWrite(response);
				discoveryUdp.PacketSend();
			}
		}
	}
}

// Processes incoming TCP traffic from a connected client.
void Injector::processTcp() {
	if (!client.Connected()) {
		client = server.Available();
		if (client.Connected()) {
			ConnectorUsb.SendLine("TCP: Client connected.");
			command_buffer_index = 0;
			memset(command_buffer, 0, MAX_PACKET_LENGTH);
		}
	}
	else {
		if (!client.Connected()) {
			ConnectorUsb.SendLine("TCP: Client disconnected.");
			client.Stop();
			return;
		}

		while (client.Available()) {
			char c = client.Read();
			if (c == '\n' || c == '\r') {
				if (command_buffer_index > 0) {
					command_buffer[command_buffer_index] = '\0';
					handleMessage(command_buffer);
					command_buffer_index = 0;
					memset(command_buffer, 0, MAX_PACKET_LENGTH);
				}
			}
			else {
				if (command_buffer_index < MAX_PACKET_LENGTH - 1) {
					command_buffer[command_buffer_index++] = c;
					} else {
					sendStatus(STATUS_PREFIX_ERROR, "Command too long.");
					command_buffer_index = 0;
					memset(command_buffer, 0, MAX_PACKET_LENGTH);
				}
			}
		}
	}
}

// Sends the main telemetry string to the connected client.
void Injector::sendGuiTelemetry(void){
	if (!client.Connected()) return;

	float displayTorque0 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue0, &firstTorqueReading0);
	float displayTorque1 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue1, &firstTorqueReading1);
	float displayTorque2 = getSmoothedTorqueEWMA(&ConnectorM2, &smoothedTorqueValue2, &firstTorqueReading2);
	
	long current_pos_steps_m0 = ConnectorM0.PositionRefCommanded();
	long current_pos_steps_m1 = ConnectorM1.PositionRefCommanded();
	long current_pos_steps_m2 = ConnectorM2.PositionRefCommanded();

	float pos_mm_m0 = (float)current_pos_steps_m0 / STEPS_PER_MM_INJECTOR;
	float pos_mm_m1 = (float)current_pos_steps_m1 / STEPS_PER_MM_INJECTOR;
	float pos_mm_m2 = (float)current_pos_steps_m2 / STEPS_PER_MM_PINCH;

	int is_homed0 = homingMachineDone ? 1 : 0;
	int is_homed1 = homingMachineDone ? 1 : 0;
	int is_homed2 = homingPinchDone ? 1 : 0;

	float machine_pos_mm = (float)(current_pos_steps_m0 - machineHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;
	float cartridge_pos_mm = (float)(current_pos_steps_m0 - cartridgeHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;

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
	if (displayTorque0 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque0Str, "0.0"); } else { snprintf(torque0Str, sizeof(torque0Str), "%.2f", displayTorque0); }
	if (displayTorque1 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque1Str, "0.0"); } else { snprintf(torque1Str, sizeof(torque1Str), "%.2f", displayTorque1); }
	if (displayTorque2 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque2Str, "0.0"); } else { snprintf(torque2Str, sizeof(torque2Str), "%.2f", displayTorque2); }

	snprintf(telemetryBuffer, sizeof(telemetryBuffer),
	"MAIN_STATE:%s,HOMING_STATE:%s,HOMING_PHASE:%s,FEED_STATE:%s,ERROR_STATE:%s,"
	"torque0:%s,hlfb0:%d,enabled0:%d,pos_mm0:%.2f,homed0:%d,"
	"torque1:%s,hlfb1:%d,enabled1:%d,pos_mm1:%.2f,homed1:%d,"
	"torque2:%s,hlfb2:%d,enabled2:%d,pos_mm2:%.2f,homed2:%d,"
	"machine_mm:%.2f,cartridge_mm:%.2f,"
	"dispensed_ml:%.2f,target_ml:%.2f,"
	"peer_disc:%d,peer_ip:%s,"
	"temp_c:%.1f,vacuum:%d,vacuum_psig:%.2f,heater_mode:%s,"
	"pid_setpoint:%.1f,pid_kp:%.2f,pid_ki:%.2f,pid_kd:%.2f,pid_output:%.1f",
	mainStateStr(), homingStateStr(), homingPhaseStr(), feedStateStr(), errorStateStr(),
	torque0Str, (int)ConnectorM0.HlfbState(), (int)ConnectorM0.StatusReg().bit.Enabled, pos_mm_m0, is_homed0,
	torque1Str, (int)ConnectorM1.HlfbState(), (int)ConnectorM1.StatusReg().bit.Enabled, pos_mm_m1, is_homed1,
	torque2Str, (int)ConnectorM2.HlfbState(), (int)ConnectorM2.StatusReg().bit.Enabled, pos_mm_m2, is_homed2,
	machine_pos_mm, cartridge_pos_mm,
	current_dispensed_for_telemetry, current_target_for_telemetry,
	(int)peerDiscovered, peerIp.StringValue(),
	temperatureCelsius, (int)vacuumOn, vacuumPressurePsig, heaterStateStr(),
	pid_setpoint, pid_kp, pid_ki, pid_kd, pid_output);

	sendStatus(TELEM_PREFIX_GUI, telemetryBuffer);
}

// Parses a command string into a UserCommand enum.
UserCommand Injector::parseCommand(const char *msg) {
	if (strcmp(msg, CMD_STR_ENABLE) == 0) return CMD_ENABLE;
	if (strcmp(msg, CMD_STR_DISABLE) == 0) return CMD_DISABLE;
	if (strcmp(msg, CMD_STR_ABORT) == 0) return CMD_ABORT;
	if (strcmp(msg, CMD_STR_CLEAR_ERRORS) == 0) return CMD_CLEAR_ERRORS;
	if (strncmp(msg, CMD_STR_SET_INJECTOR_TORQUE_OFFSET, strlen(CMD_STR_SET_INJECTOR_TORQUE_OFFSET)) == 0) return CMD_SET_TORQUE_OFFSET;
	if (strncmp(msg, CMD_STR_JOG_MOVE, strlen(CMD_STR_JOG_MOVE)) == 0) return CMD_JOG_MOVE;
	if (strcmp(msg, CMD_STR_MACHINE_HOME_MOVE) == 0) return CMD_MACHINE_HOME_MOVE;
	if (strcmp(msg, CMD_STR_CARTRIDGE_HOME_MOVE) == 0) return CMD_CARTRIDGE_HOME_MOVE;
	if (strncmp(msg, CMD_STR_INJECT_MOVE, strlen(CMD_STR_INJECT_MOVE)) == 0) return CMD_INJECT_MOVE;
	if (strncmp(msg, CMD_STR_PURGE_MOVE, strlen(CMD_STR_PURGE_MOVE)) == 0) return CMD_PURGE_MOVE;
	if (strcmp(msg, CMD_STR_MOVE_TO_CARTRIDGE_HOME) == 0) return CMD_MOVE_TO_CARTRIDGE_HOME;
	if (strncmp(msg, CMD_STR_MOVE_TO_CARTRIDGE_RETRACT, strlen(CMD_STR_MOVE_TO_CARTRIDGE_RETRACT)) == 0) return CMD_MOVE_TO_CARTRIDGE_RETRACT;
	if (strcmp(msg, CMD_STR_PAUSE_INJECTION) == 0) return CMD_PAUSE_INJECTION;
	if (strcmp(msg, CMD_STR_RESUME_INJECTION) == 0) return CMD_RESUME_INJECTION;
	if (strcmp(msg, CMD_STR_CANCEL_INJECTION) == 0) return CMD_CANCEL_INJECTION;
	if (strcmp(msg, CMD_STR_PINCH_HOME_MOVE) == 0) return CMD_PINCH_HOME_MOVE;
	if (strncmp(msg, CMD_STR_PINCH_JOG_MOVE, strlen(CMD_STR_PINCH_JOG_MOVE)) == 0) return CMD_PINCH_JOG_MOVE;
	if (strcmp(msg, CMD_STR_ENABLE_PINCH) == 0) return CMD_ENABLE_PINCH;
	if (strcmp(msg, CMD_STR_DISABLE_PINCH) == 0) return CMD_DISABLE_PINCH;
	if (strncmp(msg, CMD_STR_SET_PEER_IP, strlen(CMD_STR_SET_PEER_IP)) == 0) return CMD_SET_PEER_IP;
	if (strcmp(msg, CMD_STR_CLEAR_PEER_IP) == 0) return CMD_CLEAR_PEER_IP;
	if (strcmp(msg, CMD_STR_HEATER_ON) == 0) return CMD_HEATER_ON;
	if (strcmp(msg, CMD_STR_HEATER_OFF) == 0) return CMD_HEATER_OFF;
	if (strcmp(msg, CMD_STR_VACUUM_ON) == 0) return CMD_VACUUM_ON;
	if (strcmp(msg, CMD_STR_VACUUM_OFF) == 0) return CMD_VACUUM_OFF;
	if (strncmp(msg, CMD_STR_SET_HEATER_GAINS, strlen(CMD_STR_SET_HEATER_GAINS)) == 0) return CMD_SET_HEATER_GAINS;
	if (strncmp(msg, CMD_STR_SET_HEATER_SETPOINT, strlen(CMD_STR_SET_HEATER_SETPOINT)) == 0) return CMD_SET_HEATER_SETPOINT;
	if (strcmp(msg, CMD_STR_HEATER_PID_ON) == 0) return CMD_HEATER_PID_ON;
	if (strcmp(msg, CMD_STR_HEATER_PID_OFF) == 0) return CMD_HEATER_PID_OFF;
	return CMD_UNKNOWN;
}

// Routes a received message to the appropriate handler function.
void Injector::handleMessage(const char *msg) {
	UserCommand command = parseCommand(msg);

	switch (command) {
		case CMD_ENABLE:                    handleEnable(); break;
		case CMD_DISABLE:                   handleDisable(); break;
		case CMD_ABORT:                     handleAbort(); break;
		case CMD_CLEAR_ERRORS:              handleClearErrors(); break;
		case CMD_SET_TORQUE_OFFSET:         handleSetinjectorMotorsTorqueOffset(msg); break;
		case CMD_JOG_MOVE:                  handleJogMove(msg); break;
		case CMD_MACHINE_HOME_MOVE:         handleMachineHomeMove(); break;
		case CMD_CARTRIDGE_HOME_MOVE:       handleCartridgeHomeMove(); break;
		case CMD_INJECT_MOVE:               handleInjectMove(msg); break;
		case CMD_PURGE_MOVE:                handlePurgeMove(msg); break;
		case CMD_MOVE_TO_CARTRIDGE_HOME:    handleMoveToCartridgeHome(); break;
		case CMD_MOVE_TO_CARTRIDGE_RETRACT: handleMoveToCartridgeRetract(msg); break;
		case CMD_PAUSE_INJECTION:           handlePauseOperation(); break;
		case CMD_RESUME_INJECTION:          handleResumeOperation(); break;
		case CMD_CANCEL_INJECTION:          handleCancelOperation(); break;
		case CMD_PINCH_HOME_MOVE:           handlePinchHomeMove(); break;
		case CMD_PINCH_JOG_MOVE:            handlePinchJogMove(msg); break;
		case CMD_ENABLE_PINCH:              handleEnablePinch(); break;
		case CMD_DISABLE_PINCH:             handleDisablePinch(); break;
		case CMD_SET_PEER_IP:               handleSetPeerIp(msg); break;
		case CMD_CLEAR_PEER_IP:             handleClearPeerIp(); break;
		case CMD_HEATER_ON:                 handleHeaterOn(); break;
		case CMD_HEATER_OFF:                handleHeaterOff(); break;
		case CMD_VACUUM_ON:                 handleVacuumOn(); break;
		case CMD_VACUUM_OFF:                handleVacuumOff(); break;
		case CMD_SET_HEATER_GAINS:          handleSetHeaterGains(msg); break;
		case CMD_SET_HEATER_SETPOINT:       handleSetHeaterSetpoint(msg); break;
		case CMD_HEATER_PID_ON:             handleHeaterPidOn(); break;
		case CMD_HEATER_PID_OFF:            handleHeaterPidOff(); break;
		case CMD_UNKNOWN:
		default:
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

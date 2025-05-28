#include "messages.h"
#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include "motor.h"
#include "states.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define LOCAL_PORT 8888
const uint16_t MAX_PACKET_LENGTH = 100;

#define CMD_STR_DISCOVER_TELEM       "DISCOVER_TELEM"
#define CMD_STR_ENABLE               "ENABLE"
#define CMD_STR_DISABLE              "DISABLE"
#define CMD_STR_ABORT                "ABORT"
#define CMD_STR_CLEAR_ERRORS         "CLEAR_ERRORS" // New
#define CMD_STR_STANDBY_MODE         "STANDBY_MODE"
#define CMD_STR_TEST_MODE            "TEST_MODE"
#define CMD_STR_JOG_MODE             "JOG_MODE"
#define CMD_STR_HOMING_MODE          "HOMING_MODE"
#define CMD_STR_FEED_MODE            "FEED_MODE"
#define CMD_STR_SET_TORQUE_OFFSET    "SET_TORQUE_OFFSET "
#define CMD_STR_JOG_MOVE             "JOG_MOVE "
#define CMD_STR_MACHINE_HOME_MOVE    "MACHINE_HOME_MOVE "
#define CMD_STR_CARTRIDGE_HOME_MOVE  "CARTRIDGE_HOME_MOVE "
#define CMD_STR_INJECT_MOVE          "INJECT_MOVE "
#define CMD_STR_PURGE_MOVE           "PURGE_MOVE "
#define CMD_STR_MOVE_TO_CARTRIDGE_HOME "MOVE_TO_CARTRIDGE_HOME"
#define CMD_STR_MOVE_TO_CARTRIDGE_RETRACT "MOVE_TO_CARTRIDGE_RETRACT "
#define CMD_STR_PAUSE_OPERATION "PAUSE_OPERATION"
#define CMD_STR_RESUME_OPERATION "RESUME_OPERATION"
#define CMD_STR_CANCEL_OPERATION "CANCEL_OPERATION"

EthernetUdp Udp;
unsigned char packetBuffer[MAX_PACKET_LENGTH];
IpAddress terminalIp;
uint16_t terminalPort = 0;
bool terminalDiscovered = false;
const uint32_t telemInterval = 50; // ms

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
const float PITCH_MM_PER_REV_CONST = 5.0f; // 5mm
const float STEPS_PER_MM_CONST = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;

extern int32_t machineHomeReferenceSteps;
extern int32_t cartridgeHomeReferenceSteps;

const int FEED_GOTO_VELOCITY_SPS = 2000;  // Example speed for positioning moves
const int FEED_GOTO_ACCEL_SPS2 = 10000; // Example acceleration
const int FEED_GOTO_TORQUE_PERCENT = 40; // Example torque

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

void handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp, SystemStates *states)
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
	sendTelem(states);
	ConnectorUsb.SendLine("Discovery: Called sendTelem.");
}

void checkUdpBuffer(SystemStates *states)
{
    // This static variable and check can be used to reduce spam if needed,
    // but for now, let's see every potential parse.
    /*
    static uint32_t lastCheckTime = 0;
    if (Milliseconds() - lastCheckTime < 200) { // Check every 200ms
        return;
    }
    lastCheckTime = Milliseconds();
    ConnectorUsb.SendLine("checkUdpBuffer: Polling..."); // Indicates function is being called
    */

    memset(packetBuffer, 0, MAX_PACKET_LENGTH);
    uint16_t packetSize = Udp.PacketParse(); // Attempt to see if a packet is waiting

    if (packetSize > 0) {
        // If we get here, a UDP packet has been detected by the hardware/stack!
        char dbgBuffer[MAX_PACKET_LENGTH + 70]; 
        snprintf(dbgBuffer, sizeof(dbgBuffer), "checkUdpBuffer: Udp.PacketParse() found a packet! Size: %u", packetSize);
        ConnectorUsb.SendLine(dbgBuffer);

        IpAddress remote_ip = Udp.RemoteIp();
        uint16_t remote_port = Udp.RemotePort();
        snprintf(dbgBuffer, sizeof(dbgBuffer), "checkUdpBuffer: Packet is from IP: %s, Port: %u", remote_ip.StringValue(), remote_port);
        ConnectorUsb.SendLine(dbgBuffer);

        int32_t bytesRead = Udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
        
        if (bytesRead > 0) {
            packetBuffer[bytesRead] = '\0'; // Null-terminate the received data
            snprintf(dbgBuffer, sizeof(dbgBuffer), "checkUdpBuffer: Udp.PacketRead() %ld bytes. Msg: '%s'", bytesRead, (char*)packetBuffer);
            ConnectorUsb.SendLine(dbgBuffer);
            
            // Now, let's see if handleMessage gets called
            ConnectorUsb.SendLine("checkUdpBuffer: Calling handleMessage...");
            handleMessage((char *)packetBuffer, states);
        } else if (bytesRead == 0) {
            ConnectorUsb.SendLine("checkUdpBuffer: Udp.PacketRead() read 0 bytes, though parse indicated a packet.");
        } else {
            // bytesRead < 0 usually indicates an error
            ConnectorUsb.SendLine("checkUdpBuffer: Udp.PacketRead() returned an error or no data.");
        }
    }
    // No 'else' here; it's normal for no packet to be available during many checks.
    // The "sendTelem check..." messages will continue to show the main loop is active.
}

void finalizeAndResetActiveDispenseOperation(SystemStates *states, bool operationCompletedSuccessfully) {
	if (states->active_dispense_operation_ongoing) {
		// Calculate final dispensed amount for this operation segment
		if (states->active_op_steps_per_ml > 0.0001f) {
			long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - states->active_op_segment_initial_axis_steps;
			float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / states->active_op_steps_per_ml;
			states->active_op_total_dispensed_ml += segment_dispensed_ml;
		}
		states->last_completed_dispense_ml = states->active_op_total_dispensed_ml; // Store final total
	}
	
	states->active_dispense_operation_ongoing = false;
	states->active_op_target_ml = 0.0f; // Clear target for next op
	states->active_op_remaining_steps = 0;
	// active_op_total_dispensed_ml is now in last_completed_dispense_ml
	// active_op_segment_initial_axis_steps will be reset by the next START command
}

void fullyResetActiveDispenseOperation(SystemStates *states) {
	states->active_dispense_operation_ongoing = false;
	states->active_op_target_ml = 0.0f;
	states->active_op_total_dispensed_ml = 0.0f; // Reset total for this op
	states->active_op_total_target_steps = 0;
	states->active_op_remaining_steps = 0;
	states->active_op_segment_initial_axis_steps = 0;
	states->active_op_steps_per_ml = 0.0f;
	// states->last_completed_dispense_ml remains to show the result of the last FINISHED op
}

void sendTelem(SystemStates *states) {
	static uint32_t lastTelemTime = 0;
	uint32_t now = Milliseconds();
	
	char dbgBuffer[100];
	snprintf(dbgBuffer, sizeof(dbgBuffer), "sendTelem check: discovered=%d, port=%u\r\n", terminalDiscovered, terminalPort);
	//ConnectorUsb.SendLine(dbgBuffer);

	if (!terminalDiscovered || terminalPort == 0) return;
	if (now - lastTelemTime < telemInterval && lastTelemTime != 0) return;
	lastTelemTime = now;

	// ... (motor data fetching: torque, hlfb, pos_cmd) ...
	float displayTorque1 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue1, &firstTorqueReading1);
	float displayTorque2 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue2, &firstTorqueReading2);
	int hlfb1_val = (int)ConnectorM0.HlfbState();
	int hlfb2_val = (int)ConnectorM1.HlfbState();
	long current_pos_m0 = ConnectorM0.PositionRefCommanded();
	long current_pos_m1 = ConnectorM1.PositionRefCommanded();

	long machine_pos_from_home = current_pos_m0 - machineHomeReferenceSteps;
	long cartridge_pos_from_home = current_pos_m0 - cartridgeHomeReferenceSteps; // Single axis system
	
	int machine_is_homed_flag = states->homingMachineDone ? 1 : 0;
	int cartridge_is_homed_flag = states->homingCartridgeDone ? 1 : 0;

	float current_dispensed_for_telemetry = 0.0f;
	float current_target_for_telemetry = 0.0f;

	if (states->active_dispense_operation_ongoing &&
	(states->feedState == FEED_INJECT_ACTIVE || states->feedState == FEED_PURGE_ACTIVE ||
	states->feedState == FEED_INJECT_RESUMING || states->feedState == FEED_PURGE_RESUMING) ) {
		if (states->active_op_steps_per_ml > 0.0001f) {
			long steps_moved_this_segment = current_pos_m0 - states->active_op_segment_initial_axis_steps;
			float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / states->active_op_steps_per_ml;
			// active_op_total_dispensed_ml should be updated when PAUSING or COMPLETING a segment/operation.
			// For real-time display, it's total_dispensed_so_far + current_segment_dispense.
			// Let's simplify: sendTelem calculates based on initial start of the whole operation for ongoing.
			long total_steps_moved_for_op = current_pos_m0 - states->active_op_initial_axis_steps; // initial_axis_steps set at START of op
			current_dispensed_for_telemetry = (float)fabs(total_steps_moved_for_op) / states->active_op_steps_per_ml;

		}
		current_target_for_telemetry = states->active_op_target_ml;
		} else if (states->feedState == FEED_INJECT_PAUSED || states->feedState == FEED_PURGE_PAUSED) {
		// Show the total dispensed so far when paused
		current_dispensed_for_telemetry = states->active_op_total_dispensed_ml;
		current_target_for_telemetry = states->active_op_target_ml;
	}
	else { // Not actively dispensing or paused, show last completed op's info
		current_dispensed_for_telemetry = states->last_completed_dispense_ml;
		current_target_for_telemetry = 0.0f; // Or last target, depends on desired display
	}


	char torque1Str[16], torque2Str[16];
	if (displayTorque1 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque1Str, "---"); }
	else { snprintf(torque1Str, sizeof(torque1Str), "%.2f", displayTorque1); }
	if (displayTorque2 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque2Str, "---"); }
	else { snprintf(torque2Str, sizeof(torque2Str), "%.2f", displayTorque2); }

	char msg[500];
	snprintf(msg, sizeof(msg),
	"MAIN_STATE: %s, HOMING_STATE: %s, HOMING_PHASE: %s, FEED_STATE: %s, ERROR_STATE: %s, "
	"torque1: %s, hlfb1: %d, enabled1: %d, pos_cmd1: %ld, "
	"torque2: %s, hlfb2: %d, enabled2: %d, pos_cmd2: %ld, "
	"machine_steps: %ld, machine_homed: %d, "
	"cartridge_steps: %ld, cartridge_homed: %d, "
	"dispensed_ml:%.2f, target_ml:%.2f", // New fields
	states->mainStateStr(), states->homingStateStr(), states->homingPhaseStr(),
	states->feedStateStr(), states->errorStateStr(),
	torque1Str, hlfb1_val, motorsAreEnabled ? 1 : 0, current_pos_m0,
	torque2Str, hlfb2_val, motorsAreEnabled ? 1 : 0, current_pos_m1,
	machine_pos_from_home, machine_is_homed_flag,
	cartridge_pos_from_home, cartridge_is_homed_flag,
	current_dispensed_for_telemetry,
	current_target_for_telemetry);
	sendToPC(msg);
}

MessageCommand parseMessageCommand(const char *msg) {
	if (strncmp(msg, CMD_STR_DISCOVER_TELEM, strlen(CMD_STR_DISCOVER_TELEM)) == 0) return CMD_DISCOVER_TELEM;

	if (strcmp(msg, CMD_STR_ENABLE) == 0) return CMD_ENABLE;
	if (strcmp(msg, CMD_STR_DISABLE) == 0) return CMD_DISABLE;
	if (strcmp(msg, CMD_STR_ABORT) == 0) return CMD_ABORT;
	if (strcmp(msg, CMD_STR_CLEAR_ERRORS) == 0) return CMD_CLEAR_ERRORS; // New

	if (strcmp(msg, CMD_STR_STANDBY_MODE) == 0) return CMD_STANDBY_MODE;
	if (strcmp(msg, CMD_STR_TEST_MODE) == 0) return CMD_TEST_MODE;
	if (strcmp(msg, CMD_STR_JOG_MODE) == 0) return CMD_JOG_MODE;
	if (strcmp(msg, CMD_STR_HOMING_MODE) == 0) return CMD_HOMING_MODE;
	if (strcmp(msg, CMD_STR_FEED_MODE) == 0) return CMD_FEED_MODE;

	if (strncmp(msg, CMD_STR_SET_TORQUE_OFFSET, strlen(CMD_STR_SET_TORQUE_OFFSET)) == 0) return CMD_SET_TORQUE_OFFSET;
	if (strncmp(msg, CMD_STR_JOG_MOVE, strlen(CMD_STR_JOG_MOVE)) == 0) return CMD_JOG_MOVE;
	if (strncmp(msg, CMD_STR_MACHINE_HOME_MOVE, strlen(CMD_STR_MACHINE_HOME_MOVE)) == 0) return CMD_MACHINE_HOME_MOVE;
	if (strncmp(msg, CMD_STR_CARTRIDGE_HOME_MOVE, strlen(CMD_STR_CARTRIDGE_HOME_MOVE)) == 0) return CMD_CARTRIDGE_HOME_MOVE;
	if (strncmp(msg, CMD_STR_INJECT_MOVE, strlen(CMD_STR_INJECT_MOVE)) == 0) return CMD_INJECT_MOVE;
	if (strncmp(msg, CMD_STR_PURGE_MOVE, strlen(CMD_STR_PURGE_MOVE)) == 0) return CMD_PURGE_MOVE;
	// if (strncmp(msg, CMD_STR_RETRACT_MOVE, strlen(CMD_STR_RETRACT_MOVE)) == 0) return CMD_RETRACT_MOVE;

	if (strcmp(msg, CMD_STR_MOVE_TO_CARTRIDGE_HOME) == 0) return CMD_MOVE_TO_CARTRIDGE_HOME;
	if (strncmp(msg, CMD_STR_MOVE_TO_CARTRIDGE_RETRACT, strlen(CMD_STR_MOVE_TO_CARTRIDGE_RETRACT)) == 0) return CMD_MOVE_TO_CARTRIDGE_RETRACT;

	if (strcmp(msg, CMD_STR_PAUSE_OPERATION) == 0) return CMD_PAUSE_OPERATION;
	if (strcmp(msg, CMD_STR_RESUME_OPERATION) == 0) return CMD_RESUME_OPERATION;
	if (strcmp(msg, CMD_STR_CANCEL_OPERATION) == 0) return CMD_CANCEL_OPERATION;

	return CMD_UNKNOWN;
}

void handleMessage(const char *msg, SystemStates *states) {
	MessageCommand command = parseMessageCommand(msg);

	switch (command) {
		case CMD_DISCOVER_TELEM:        handleDiscoveryTelemPacket(msg, Udp.RemoteIp(), states); break;
		case CMD_ENABLE:                handleEnable(states); break;
		case CMD_DISABLE:               handleDisable(states); break;
		case CMD_ABORT:                 handleAbort(states); break;
		case CMD_CLEAR_ERRORS:          handleClearErrors(states); break;
		case CMD_STANDBY_MODE:          handleStandbyMode(states); break;
		case CMD_TEST_MODE:             handleTestMode(states); break;
		case CMD_JOG_MODE:              handleJogMode(states); break;
		case CMD_HOMING_MODE:           handleHomingMode(states); break;
		case CMD_FEED_MODE:             handleFeedMode(states); break;
		case CMD_SET_TORQUE_OFFSET:     handleSetTorqueOffset(msg); break;
		case CMD_JOG_MOVE:              handleJogMove(msg, states); break;
		case CMD_MACHINE_HOME_MOVE:     handleMachineHomeMove(msg, states); break;
		case CMD_CARTRIDGE_HOME_MOVE:   handleCartridgeHomeMove(msg, states); break;
		case CMD_INJECT_MOVE:           handleInjectMove(msg, states); break;
		case CMD_PURGE_MOVE:            handlePurgeMove(msg, states); break;
		case CMD_MOVE_TO_CARTRIDGE_HOME:    handleMoveToCartridgeHome(states); break;
		case CMD_MOVE_TO_CARTRIDGE_RETRACT: handleMoveToCartridgeRetract(msg, states); break;
		case CMD_PAUSE_OPERATION:       handlePauseOperation(states); break;
		case CMD_RESUME_OPERATION:      handleResumeOperation(states); break;
		case CMD_CANCEL_OPERATION:      handleCancelOperation(states); break;

		case CMD_UNKNOWN:
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

// --- System Command Handlers ---

void handleEnable(SystemStates *states) {
	if (states->mainState == DISABLED_MODE) {
		enableMotors("MOTORS ENABLED (transitioned to STANDBY_MODE)");
		states->mainState = STANDBY_MODE;
		states->homingState = HOMING_NONE;
		states->feedState = FEED_NONE; 
		states->errorState = ERROR_NONE;
		sendToPC("System enabled: state is now STANDBY_MODE.");
		} else {
		// If already enabled, or in a state other than DISABLED_MODE,
		// just ensure motors are enabled without changing main state.
		if (!motorsAreEnabled) {
			enableMotors("MOTORS re-enabled (state unchanged)");
			} else {
			sendToPC("Motors already enabled, system not in DISABLED_MODE.");
		}
	}
}

void handleDisable(SystemStates *states)
{
	abortMove();
	Delay_ms(200);
	states->mainState = DISABLED_MODE;
	states->errorState = ERROR_NONE;
	disableMotors("MOTORS DISABLED (entered DISABLED state)");
	sendToPC("System disabled: must ENABLE to return to standby.");
}

void handleAbort(SystemStates *states) {
	// This function is mostly from your uploaded messages.cpp, updated for new state model
	// Abort should work in most operational states, not just a generic "MOVING"
	if (states->mainState == JOG_MODE || states->mainState == HOMING_MODE ||
	states->mainState == FEED_MODE || states->mainState == TEST_MODE) {
		
		abortMove(); // Your function from motor.cpp
		Delay_ms(200);
		// Log the specific operation that was aborted if possible
		// For example, if (states->mainState == HOMING_MODE) { log sub-state }

		sendToPC("ABORT command received: All motion stopped.");
		states->errorState = ERROR_MANUAL_ABORT;
		states->mainState = STANDBY_MODE; // Or a specific ERROR_MAIN_STATE if you add one
		states->homingState = HOMING_NONE;
		states->feedState = FEED_NONE;
		} else {
		sendToPC("ABORT command ignored: System not in an abortable state.");
	}
}

void handleStandbyMode(SystemStates *states) {
	// This function is mostly from your uploaded messages.cpp (handleStandby), updated
	bool wasError = (states->errorState != ERROR_NONE); // Check if error was active
	
	if (states->mainState != STANDBY_MODE) { // Only log/act if changing state
		abortMove(); // Good practice to stop motion when changing to standby
		Delay_ms(200);
		states->mainState = STANDBY_MODE;
		states->homingState = HOMING_NONE;
		states->feedState = FEED_NONE;
		states->errorState = ERROR_NONE; // Clear errors when explicitly going to standby

		if (wasError) {
			sendToPC("Exited previous state: State set to STANDBY_MODE and error cleared.");
			} else {
			sendToPC("State set to STANDBY_MODE.");
		}
		} else {
		if (states->errorState != ERROR_NONE) { // If already in standby but error occurs and then standby is requested again
			states->errorState = ERROR_NONE;
			sendToPC("Still in STANDBY_MODE, error cleared.");
			} else {
			sendToPC("Already in STANDBY_MODE.");
		}
	}
}

void handleTestMode(SystemStates *states) {
	// This function is from your uploaded messages.cpp (handleTestState)
	if (states->mainState != TEST_MODE) {
		abortMove(); // Stop other activities
		states->mainState = TEST_MODE;
		states->homingState = HOMING_NONE; // Reset other mode sub-states
		states->feedState = FEED_NONE;
		states->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered TEST_MODE.");
		} else {
		sendToPC("Already in TEST_MODE.");
	}
}

void handleJogMode(SystemStates *states) {
	// Your uploaded messages.cpp handleJogMode uses an old 'MOVING' and 'MOVING_JOG' concept.
	// This should now simply set the main state. Actual jog moves are handled by CMD_JOG_MOVE.
	if (states->mainState != JOG_MODE) {
		abortMove(); // Stop other activities
		Delay_ms(200);
		states->mainState = JOG_MODE;
		states->homingState = HOMING_NONE; // Reset other mode sub-states
		states->feedState = FEED_NONE;
		states->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered JOG_MODE. Ready for JOG_MOVE commands.");
		} else {
		sendToPC("Already in JOG_MODE.");
	}
}

void handleHomingMode(SystemStates *states) {
	if (states->mainState != HOMING_MODE) {
		abortMove(); // Stop other activities
		Delay_ms(200);
		states->mainState = HOMING_MODE;
		states->homingState = HOMING_NONE; // Initialize to no specific homing action
		states->feedState = FEED_NONE;     // Reset other mode sub-states
		states->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered HOMING_MODE. Ready for homing operations.");
		} else {
		sendToPC("Already in HOMING_MODE.");
	}
}

void handleFeedMode(SystemStates *states) {
	if (states->mainState != FEED_MODE) {
		abortMove(); // Stop other activities
		Delay_ms(200);
		states->mainState = FEED_MODE;
		states->feedState = FEED_STANDBY;  // Default sub-state for feed operations
		states->homingState = HOMING_NONE; // Reset other mode sub-states
		states->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered FEED_MODE. Ready for inject/purge/retract.");
		} else {
		sendToPC("Already in FEED_MODE.");
	}
}

void handleSetTorqueOffset(const char *msg) {
	float newOffset = atof(msg + strlen(CMD_STR_SET_TORQUE_OFFSET)); // Use defined string length
	torqueOffset = newOffset; // This directly updates the global from motor.cpp
	char response[64];
	snprintf(response, sizeof(response), "Global torque offset set to %.2f", torqueOffset);
	sendToPC(response);
}

void handleJogMove(const char *msg, SystemStates *states) {
	if (states->mainState != JOG_MODE) {
		sendToPC("JOG_MOVE ignored: Not in JOG_MODE.");
		return;
	}

	long s0 = 0, s1 = 0;
	int torque_percent = 0, velocity_sps = 0, accel_sps2 = 0;

	// Format: JOG_MOVE <s0_steps> <s1_steps> <torque_%> <vel_sps> <accel_sps2>
	int parsed_count = sscanf(msg + strlen(CMD_STR_JOG_MOVE), "%ld %ld %d %d %d",
	&s0, &s1,
	&torque_percent,
	&velocity_sps,
	&accel_sps2);

	if (parsed_count == 5) {
		char response[150];
		snprintf(response, sizeof(response),
		"JOG_MOVE RX: s0:%ld s1:%ld TqL:%d%% Vel:%d Acc:%d",
		s0, s1, torque_percent, velocity_sps, accel_sps2);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("JOG_MOVE blocked: Motors are disabled.");
			return;
		}

		// Validate torque_percent, velocity_sps, accel_sps2 if necessary
		if (torque_percent <= 0 || torque_percent > 100) {
			// extern float jogTorqueLimit; // from motor.h (ensure it's properly externed/defined)
			// torque_percent = (int)jogTorqueLimit; // Use a default if parsed is invalid
			torque_percent = 30; // Fallback to a sensible default like 30%
			char torqueWarn[80];
			snprintf(torqueWarn, sizeof(torqueWarn), "Warning: Invalid jog torque in command, using %d%%.", torque_percent);
			sendToPC(torqueWarn);
		}
		if (velocity_sps <= 0) {
			// extern int32_t velocityLimit; // from motor.cpp
			// velocity_sps = velocityLimit;
			velocity_sps = 800; // Fallback
			sendToPC("Warning: Invalid jog velocity, using default.");
		}
		if (accel_sps2 <= 0) {
			// extern int32_t accelerationLimit; // from motor.cpp
			// accel_sps2 = accelerationLimit;
			accel_sps2 = 5000; // Fallback
			sendToPC("Warning: Invalid jog acceleration, using default.");
		}


		// Directly call your existing moveMotors function
		moveMotors((int)s0, (int)s1, torque_percent, velocity_sps, accel_sps2);

		states->jogDone = false; // Indicate a jog operation has started (if you use this flag)

		} else {
		char errorMsg[100];
		snprintf(errorMsg, sizeof(errorMsg), "Invalid JOG_MOVE format. Expected 5 params, got %d.", parsed_count);
		sendToPC(errorMsg);
	}
}

void handleMachineHomeMove(const char *msg, SystemStates *states) {
	if (states->mainState != HOMING_MODE) {
		sendToPC("MACHINE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (states->currentHomingPhase != HOMING_PHASE_IDLE && states->currentHomingPhase != HOMING_PHASE_COMPLETE && states->currentHomingPhase != HOMING_PHASE_ERROR) {
		sendToPC("MACHINE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}

	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent;
	// Format: <CMD_STR> <stroke_mm> <rapid_vel_mm_s> <touch_vel_mm_s> <acceleration_mm_s2> <retract_dist_mm> <torque_%>
	
	int parsed_count = sscanf(msg + strlen(CMD_STR_MACHINE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration_mm_s2, &retract_mm, &torque_percent);

	if (parsed_count == 6) {
		char response[250]; // Increased for acceleration
		snprintf(response, sizeof(response), "MACHINE_HOME_MOVE RX: Strk:%.1f RpdV:%.1f TchV:%.1f Acc:%.1f Ret:%.1f Tq:%.1f%%",
		stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("MACHINE_HOME_MOVE blocked: Motors disabled. Set to HOMING_PHASE_ERROR.");
			states->homingState = HOMING_MACHINE; // Indicate it was attempted
			states->currentHomingPhase = HOMING_PHASE_ERROR;
			states->errorState = ERROR_MANUAL_ABORT; // Or a more specific error
			return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Machine Home. Using default 20%.");
			torque_percent = 20.0f;
		}
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendToPC("Error: Invalid parameters for Machine Home (must be positive, retract >= 0). Set to HOMING_PHASE_ERROR.");
			states->homingState = HOMING_MACHINE;
			states->currentHomingPhase = HOMING_PHASE_ERROR;
			states->errorState = ERROR_MANUAL_ABORT; // Or specific param error
			return;
		}


		// Store parameters in SystemStates
		states->homing_stroke_mm_param = stroke_mm;
		states->homing_rapid_vel_mm_s_param = rapid_vel_mm_s;
		states->homing_touch_vel_mm_s_param = touch_vel_mm_s;
		states->homing_acceleration_param = acceleration_mm_s2;
		states->homing_retract_mm_param = retract_mm;
		states->homing_torque_percent_param = torque_percent;

		// Calculate and store step/sps values
		// STEPS_PER_MM_CONST needs pulsesPerRev which is from motor.cpp
		const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
		states->homing_actual_stroke_steps = (long)(stroke_mm * current_steps_per_mm);
		states->homing_actual_rapid_sps = (int)(rapid_vel_mm_s * current_steps_per_mm);
		states->homing_actual_touch_sps = (int)(touch_vel_mm_s * current_steps_per_mm);
		states->homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * current_steps_per_mm);
		states->homing_actual_retract_steps = (long)(retract_mm * current_steps_per_mm);

		// Initialize homing state machine
		states->homingState = HOMING_MACHINE;
		states->currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
		states->homingStartTime = Milliseconds(); // For timeout tracking
		states->errorState = ERROR_NONE; // Clear previous errors

		sendToPC("Initiating Machine Homing: RAPID_MOVE phase.");
		// Machine homing is typically in the negative direction
		long initial_move_steps = states->homing_actual_stroke_steps;
		moveMotors(initial_move_steps, initial_move_steps, (int)states->homing_torque_percent_param, states->homing_actual_rapid_sps, states->homing_actual_accel_sps2);
		
		} else {
		sendToPC("Invalid MACHINE_HOME_MOVE format. Expected 6 parameters.");
		states->homingState = HOMING_MACHINE; // Indicate it was attempted
		states->currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void handleCartridgeHomeMove(const char *msg, SystemStates *states) {
	if (states->mainState != HOMING_MODE) {
		sendToPC("CARTRIDGE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (states->currentHomingPhase != HOMING_PHASE_IDLE && states->currentHomingPhase != HOMING_PHASE_COMPLETE && states->currentHomingPhase != HOMING_PHASE_ERROR) {
		sendToPC("CARTRIDGE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}
	
	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent;
	int parsed_count = sscanf(msg + strlen(CMD_STR_CARTRIDGE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration_mm_s2, &retract_mm, &torque_percent);

	if (parsed_count == 6) {
		char response[250];
		snprintf(response, sizeof(response), "CARTRIDGE_HOME_MOVE RX: Strk:%.1f RpdV:%.1f TchV:%.1f Acc:%.1f Ret:%.1f Tq:%.1f%%",
		stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("CARTRIDGE_HOME_MOVE blocked: Motors disabled. Set to HOMING_PHASE_ERROR.");
			states->homingState = HOMING_CARTRIDGE;
			states->currentHomingPhase = HOMING_PHASE_ERROR;
			states->errorState = ERROR_MANUAL_ABORT; // Or a more specific error
			return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Cartridge Home. Using default 20%.");
			torque_percent = 20.0f;
		}
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendToPC("Error: Invalid parameters for Cartridge Home (must be positive, retract >= 0). Set to HOMING_PHASE_ERROR.");
			states->homingState = HOMING_CARTRIDGE;
			states->currentHomingPhase = HOMING_PHASE_ERROR;
			states->errorState = ERROR_MANUAL_ABORT; // Or specific param error
			return;
		}


		states->homing_stroke_mm_param = stroke_mm;
		states->homing_rapid_vel_mm_s_param = rapid_vel_mm_s;
		states->homing_touch_vel_mm_s_param = touch_vel_mm_s;
		states->homing_acceleration_param = acceleration_mm_s2;
		states->homing_retract_mm_param = retract_mm;
		states->homing_torque_percent_param = torque_percent;

		const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
		states->homing_actual_stroke_steps = (long)(stroke_mm * current_steps_per_mm);
		states->homing_actual_rapid_sps = (int)(rapid_vel_mm_s * current_steps_per_mm);
		states->homing_actual_touch_sps = (int)(touch_vel_mm_s * current_steps_per_mm);
		states->homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * current_steps_per_mm);
		states->homing_actual_retract_steps = (long)(retract_mm * current_steps_per_mm);

		states->homingState = HOMING_CARTRIDGE;
		states->currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
		states->homingStartTime = Milliseconds();
		states->errorState = ERROR_NONE;

		sendToPC("Initiating Cartridge Homing: RAPID_MOVE phase.");
		// Cartridge homing is typically in the positive direction
		long initial_move_steps = -1 * states->homing_actual_stroke_steps;
		moveMotors(initial_move_steps, initial_move_steps, (int)states->homing_torque_percent_param, states->homing_actual_rapid_sps, states->homing_actual_accel_sps2);

		} else {
		sendToPC("Invalid CARTRIDGE_HOME_MOVE format. Expected 6 parameters.");
		states->homingState = HOMING_CARTRIDGE;
		states->currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void resetActiveDispenseOp(SystemStates *states) {
    states->active_op_target_ml = 0.0f;
    states->active_op_total_dispensed_ml = 0.0f;
    states->active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
    states->active_op_steps_per_ml = 0.0f;
    states->active_dispense_operation_ongoing = false;
}

void handleClearErrors(SystemStates *states) {
	if (states->errorState != ERROR_NONE) {
		states->errorState = ERROR_NONE;
		sendToPC("Errors cleared.");
		// If an error interrupted an operation, that operation should already be stopped/reset.
		// Consider if returning to STANDBY_MODE is appropriate if not in an active state.
		// For now, just clearing the error flag.
		} else {
		sendToPC("No active errors to clear.");
	}
}

void handleMoveToCartridgeHome(SystemStates *states) {
	if (states->mainState != FEED_MODE) { /* ... */ return; }
	if (!states->homingCartridgeDone) { states->errorState = ERROR_NO_CARTRIDGE_HOME; sendToPC("Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { sendToPC("Err: Motors disabled."); return; }
	if (checkMoving() || states->feedState == FEED_INJECT_ACTIVE || states->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Err: Busy. Cannot move to cart home now.");
		states->errorState = ERROR_INVALID_OPERATION; return;
	}

	sendToPC("Cmd: Move to Cartridge Home...");
	fullyResetActiveDispenseOperation(states); // Reset any prior dispense op
	states->feedState = FEED_MOVING_TO_HOME;
	states->feedingDone = false;
	
	long current_axis_pos = ConnectorM0.PositionRefCommanded();
	long steps_to_move_axis = cartridgeHomeReferenceSteps - current_axis_pos;

	moveMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	FEED_GOTO_TORQUE_PERCENT, FEED_GOTO_VELOCITY_SPS, FEED_GOTO_ACCEL_SPS2);
}

void handleMoveToCartridgeRetract(const char *msg, SystemStates *states) {
	if (states->mainState != FEED_MODE) { /* ... */ return; }
	if (!states->homingCartridgeDone) { /* ... */ states->errorState = ERROR_NO_CARTRIDGE_HOME; sendToPC("Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { /* ... */ sendToPC("Err: Motors disabled."); return; }
	if (checkMoving() || states->feedState == FEED_INJECT_ACTIVE || states->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Err: Busy. Cannot move to cart retract now.");
		states->errorState = ERROR_INVALID_OPERATION; return;
	}

	float offset_mm = 0.0f;
	if (sscanf(msg + strlen(CMD_STR_MOVE_TO_CARTRIDGE_RETRACT), "%f", &offset_mm) != 1 || offset_mm < 0) {
		sendToPC("Err: Invalid offset for MOVE_TO_CARTRIDGE_RETRACT."); return;
	}
	
	fullyResetActiveDispenseOperation(states);
	states->feedState = FEED_MOVING_TO_RETRACT;
	states->feedingDone = false;

	const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
	long offset_steps = (long)(offset_mm * current_steps_per_mm);
	long target_absolute_axis_position = cartridgeHomeReferenceSteps + offset_steps;

	char response[150];
	snprintf(response, sizeof(response), "Cmd: Move to Cart Retract (Offset: %.1fmm, Target: %ld steps)", offset_mm, target_absolute_axis_position);
	sendToPC(response);

	long current_axis_pos = ConnectorM0.PositionRefCommanded();
	long steps_to_move_axis = target_absolute_axis_position - current_axis_pos;

	moveMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	FEED_GOTO_TORQUE_PERCENT, FEED_GOTO_VELOCITY_SPS, FEED_GOTO_ACCEL_SPS2);
}


void handleInjectMove(const char *msg, SystemStates *states) {
	if (states->mainState != FEED_MODE) {
		sendToPC("INJECT ignored: Not in FEED_MODE.");
		return;
	}
	if (checkMoving() || states->active_dispense_operation_ongoing) {
		sendToPC("Error: Operation already in progress or motors busy.");
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2, steps_per_ml_val, torque_percent;
	if (sscanf(msg + strlen(CMD_STR_INJECT_MOVE), "%f %f %f %f %f",
		&volume_ml, &speed_ml_s, &acceleration_sps2, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!motorsAreEnabled) { sendToPC("INJECT blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0) { sendToPC("Error: Steps/ml must be positive."); return; }
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f;
		if (volume_ml <= 0) { sendToPC("Error: Inject volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendToPC("Error: Inject speed must be positive."); return; }
		if (acceleration_sps2 <= 0) { sendToPC("Error: Inject acceleration must be positive."); return; }

		states->feedState = FEED_INJECT_STARTING;
		states->feedingDone = false;
		states->active_dispense_operation_ongoing = true;
		states->active_op_target_ml = volume_ml;
		states->active_op_steps_per_ml = steps_per_ml_val;
		states->active_op_total_target_steps = (long)(volume_ml * steps_per_ml_val);
		states->active_op_remaining_steps = states->active_op_total_target_steps;

		long current_pos = ConnectorM0.PositionRefCommanded();
		states->active_op_initial_axis_steps = current_pos;                // <== REQUIRED
		states->active_op_segment_initial_axis_steps = current_pos;        // <== USED for pause tracking
		states->active_op_total_dispensed_ml = 0.0f;

		states->active_op_velocity_sps = (int)(speed_ml_s * steps_per_ml_val);
		states->active_op_accel_sps2 = (int)acceleration_sps2;
		states->active_op_torque_percent = (int)torque_percent;
		if (states->active_op_velocity_sps <= 0) states->active_op_velocity_sps = 100;

		char response[200];
		snprintf(response, sizeof(response), "RX INJECT: Vol:%.2fml, Speed:%.2fml/s, Acc:%.0f, Steps/ml:%.2f, Tq:%.0f%%",
			volume_ml, speed_ml_s, acceleration_sps2, steps_per_ml_val, torque_percent);
		sendToPC(response);
		sendToPC("Starting Inject operation...");

		moveMotors(states->active_op_remaining_steps, states->active_op_remaining_steps,
			states->active_op_torque_percent, states->active_op_velocity_sps, states->active_op_accel_sps2);
		states->feedState = FEED_INJECT_ACTIVE;
	} else {
		sendToPC("Invalid INJECT_MOVE format.");
	}
}

void handlePurgeMove(const char *msg, SystemStates *states) { // This is effectively START_PURGE
	// Similar logic to handleInjectMove
	if (states->mainState != FEED_MODE) { sendToPC("PURGE ignored: Not in FEED_MODE."); return; }
	if (checkMoving() || states->active_dispense_operation_ongoing) { sendToPC("Error: Operation already in progress or motors busy."); return; }

	float volume_ml, speed_ml_s, acceleration_sps2, steps_per_ml_val, torque_percent;
	if (sscanf(msg + strlen(CMD_STR_PURGE_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration_sps2, &steps_per_ml_val, &torque_percent) == 5) {
			
		if (!motorsAreEnabled) { sendToPC("PURGE blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0) { sendToPC("Error: Steps/ml must be positive."); return;}
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f;
		if (volume_ml <= 0) { sendToPC("Error: Purge volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendToPC("Error: Purge speed must be positive."); return; }
		if (acceleration_sps2 <= 0) { sendToPC("Error: Purge acceleration must be positive."); return; }

		states->feedState = FEED_PURGE_STARTING;
		states->feedingDone = false;
		states->active_dispense_operation_ongoing = true;
		states->active_op_target_ml = volume_ml;
		states->active_op_steps_per_ml = steps_per_ml_val;
		states->active_op_total_target_steps = (long)(volume_ml * steps_per_ml_val);
		states->active_op_remaining_steps = states->active_op_total_target_steps;
		states->active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
		states->active_op_total_dispensed_ml = 0.0f;

		states->active_op_velocity_sps = (int)(speed_ml_s * steps_per_ml_val);
		states->active_op_accel_sps2 = (int)acceleration_sps2;
		states->active_op_torque_percent = (int)torque_percent;
		if (states->active_op_velocity_sps <= 0) states->active_op_velocity_sps = 200;

		char response[200];
		snprintf(response, sizeof(response), "RX PURGE: Vol:%.2fml, Speed:%.2fml/s, Acc:%.0f, Steps/ml:%.2f, Tq:%.0f%%",
		volume_ml, speed_ml_s, acceleration_sps2, steps_per_ml_val, torque_percent);
		sendToPC(response);
		sendToPC("Starting Purge operation...");
		moveMotors(states->active_op_remaining_steps, states->active_op_remaining_steps,
		states->active_op_torque_percent, states->active_op_velocity_sps, states->active_op_accel_sps2);
		states->feedState = FEED_PURGE_ACTIVE;
		} else { sendToPC("Invalid PURGE_MOVE format."); }
}

void handlePauseOperation(SystemStates *states) {
	if (!states->active_dispense_operation_ongoing) {
		sendToPC("PAUSE ignored: No active inject/purge operation.");
		return;
	}
	if (states->feedState == FEED_INJECT_ACTIVE || states->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Cmd: Pausing operation...");
		ConnectorM0.MoveStopDecel(); // Use decelerated stop
		ConnectorM1.MoveStopDecel();
		// The actual calculation of remaining steps and state change to PAUSED
		// will happen in main.cpp once motors confirm they've stopped.
		// For now, we just signal the intent.
		// We need a way for main.cpp to know a pause was requested.
		if(states->feedState == FEED_INJECT_ACTIVE) states->feedState = FEED_INJECT_PAUSED; // Tentative state
		if(states->feedState == FEED_PURGE_ACTIVE) states->feedState = FEED_PURGE_PAUSED;   // Tentative state
		// The main loop will see motors stopping and then finalize the pause.
		} else {
		sendToPC("PAUSE ignored: Not in an active inject/purge state.");
	}
}

void handleResumeOperation(SystemStates *states) {
	if (!states->active_dispense_operation_ongoing) {
		sendToPC("RESUME ignored: No operation was ongoing or paused.");
		return;
	}
	if (states->feedState == FEED_INJECT_PAUSED || states->feedState == FEED_PURGE_PAUSED) {
		if (states->active_op_remaining_steps <= 0) {
			sendToPC("RESUME ignored: No remaining volume/steps to dispense.");
			fullyResetActiveDispenseOperation(states);
			states->feedState = FEED_STANDBY;
			return;
		}
		sendToPC("Cmd: Resuming operation...");
		states->active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded(); // Update base for this segment
		states->feedingDone = false; // New segment starting

		moveMotors(states->active_op_remaining_steps, states->active_op_remaining_steps,
		states->active_op_torque_percent, states->active_op_velocity_sps, states->active_op_accel_sps2);
				
		if(states->feedState == FEED_INJECT_PAUSED) states->feedState = FEED_INJECT_RESUMING; // Or directly to _ACTIVE
		if(states->feedState == FEED_PURGE_PAUSED) states->feedState = FEED_PURGE_RESUMING;  // Or directly to _ACTIVE
		// main.cpp will transition from RESUMING to ACTIVE once motors are moving, or directly here.
		// For simplicity let's assume it goes active if move command is successful.
		if(states->feedState == FEED_INJECT_RESUMING) states->feedState = FEED_INJECT_ACTIVE;
		if(states->feedState == FEED_PURGE_RESUMING) states->feedState = FEED_PURGE_ACTIVE;


		} else {
		sendToPC("RESUME ignored: Operation not paused.");
	}
}

void handleCancelOperation(SystemStates *states) {
	if (!states->active_dispense_operation_ongoing) {
		sendToPC("CANCEL ignored: No active operation to cancel.");
		return;
	}
	sendToPC("Cmd: Cancelling operation...");
	abortMove(); // Stop motors abruptly
			
	// Calculate final dispensed amount before resetting
	if (states->active_dispense_operation_ongoing && states->active_op_steps_per_ml > 0.0001f) {
		long steps_moved_on_axis = ConnectorM0.PositionRefCommanded() - states->active_op_segment_initial_axis_steps;
		float segment_dispensed_ml = (float)fabs(steps_moved_on_axis) / states->active_op_steps_per_ml;
		states->active_op_total_dispensed_ml += segment_dispensed_ml; // Add last segment's dispense
	}
	states->last_completed_dispense_ml = states->active_op_total_dispensed_ml; // Store total dispensed before cancel

	fullyResetActiveDispenseOperation(states);
	states->feedState = FEED_OPERATION_CANCELLED; // Or directly to FEED_STANDBY
	states->feedingDone = true; // Mark as "done" via cancellation
	sendToPC("Operation cancelled.");
}




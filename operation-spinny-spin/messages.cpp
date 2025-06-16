#include "messages.h"
#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include "motor.h"
#include "machine_1.h"
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
#define CMD_STR_DAVID_STATE "DAVID_STATE"

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

void handleDavidMode(Spinny_Spin_Machine *machine_1)
{
	machine_1->mainState = DAVID_STATE;
	machine_1->davidState = HUNGRY_FOR_TACOS;
}

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

void handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp, Spinny_Spin_Machine *machine_1)
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
	sendTelem(machine_1);
	ConnectorUsb.SendLine("Discovery: Called sendTelem.");
}

void checkUdpBuffer(Spinny_Spin_Machine *machine_1)
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
            handleMessage((char *)packetBuffer, machine_1);
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

void finalizeAndResetActiveDispenseOperation(Spinny_Spin_Machine *machine_1, bool operationCompletedSuccessfully) {
	if (machine_1->active_dispense_operation_ongoing) {
		// Calculate final dispensed amount for this operation segment
		if (machine_1->active_op_steps_per_ml > 0.0001f) {
			long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - machine_1->active_op_segment_initial_axis_steps;
			float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / machine_1->active_op_steps_per_ml;
			machine_1->active_op_total_dispensed_ml += segment_dispensed_ml;
		}
		machine_1->last_completed_dispense_ml = machine_1->active_op_total_dispensed_ml; // Store final total
	}
	
	// This part is similar to fullyReset, but called after a successful/finalized op.
	machine_1->active_dispense_operation_ongoing = false;
	machine_1->active_op_target_ml = 0.0f;
	// machine_1->active_op_total_dispensed_ml is now in last_completed_dispense_ml. If you want it zero for next op,
	// fullyReset should be called by the START of the next op.
	machine_1->active_op_remaining_steps = 0;
}

void fullyResetActiveDispenseOperation(Spinny_Spin_Machine *machine_1) {
	machine_1->active_dispense_operation_ongoing = false;
	machine_1->active_op_target_ml = 0.0f;
	machine_1->active_op_total_dispensed_ml = 0.0f; // Crucial for starting fresh
	machine_1->active_op_total_target_steps = 0;
	machine_1->active_op_remaining_steps = 0;
	machine_1->active_op_segment_initial_axis_steps = 0; // Reset for next segment start
	machine_1->active_op_initial_axis_steps = 0;       // Reset for the very start of an operation
	machine_1->active_op_steps_per_ml = 0.0f;
	// Note: machine_1->last_completed_dispense_ml is now explicitly managed by callers (start, cancel, complete)
	// to reflect the outcome of the operation for idle telemetry.
}

void sendTelem(Spinny_Spin_Machine *machine_1) {
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

	int machine_is_homed_flag = machine_1->homingMachineDone ? 1 : 0;
	int cartridge_is_homed_flag = machine_1->homingCartridgeDone ? 1 : 0;

	float current_dispensed_for_telemetry = 0.0f;
	float current_target_for_telemetry = 0.0f;

	if (machine_1->active_dispense_operation_ongoing) {
		current_target_for_telemetry = machine_1->active_op_target_ml;
		if (machine_1->feedState == FEED_INJECT_ACTIVE || machine_1->feedState == FEED_PURGE_ACTIVE ||
		machine_1->feedState == FEED_INJECT_RESUMING || machine_1->feedState == FEED_PURGE_RESUMING ) {
			// Live calculation based on initial position of the entire operation
			if (machine_1->active_op_steps_per_ml > 0.0001f) {
				long total_steps_moved_for_op = current_pos_m0 - machine_1->active_op_initial_axis_steps;
				current_dispensed_for_telemetry = fabs(total_steps_moved_for_op) / machine_1->active_op_steps_per_ml;
			}
			} else if (machine_1->feedState == FEED_INJECT_PAUSED || machine_1->feedState == FEED_PURGE_PAUSED) {
			// When paused, use the accumulated total from before pausing
			current_dispensed_for_telemetry = machine_1->active_op_total_dispensed_ml;
		}
		// For STARTING machine_1, current_dispensed_for_telemetry will be 0 as active_op_total_dispensed_ml is 0
		// and active_op_initial_axis_steps equals current_pos_m0.
		} else { // Not active_dispense_operation_ongoing (i.e., idle, completed, or cancelled)
		current_dispensed_for_telemetry = machine_1->last_completed_dispense_ml;
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
	machine_1->mainmachine_1tr(), machine_1->homingmachine_1tr(), machine_1->homingPhaseStr(),
	machine_1->feedmachine_1tr(), machine_1->errormachine_1tr(),
	torque1Str, hlfb1_val, motorsAreEnabled ? 1 : 0, current_pos_m0,
	torque2Str, hlfb2_val, motorsAreEnabled ? 1 : 0, current_pos_m1,
	machine_pos_from_home, machine_is_homed_flag,
	cartridge_pos_from_home, cartridge_is_homed_flag,
	current_dispensed_for_telemetry,
	current_target_for_telemetry);

	sendToPC(msg);
}


//parser
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
	if (strcmp(msg, CMD_STR_DAVID_STATE) == 0) return CMD_SDAVID_STATE;
	

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

//dispatcher
void handleMessage(const char *msg, Spinny_Spin_Machine *machine_1) {
	MessageCommand command = parseMessageCommand(msg);

	switch (command) {
		case CMD_DISCOVER_TELEM:        handleDiscoveryTelemPacket(msg, Udp.RemoteIp(), machine_1); break;
		case CMD_ENABLE:                handleEnable(machine_1); break;
		case CMD_DISABLE:               handleDisable(machine_1); break;
		case CMD_ABORT:                 handleAbort(machine_1); break;
		case CMD_CLEAR_ERRORS:          handleClearErrors(machine_1); break;
		case CMD_STANDBY_MODE:          handleStandbyMode(machine_1); break;
		case CMD_TEST_MODE:             handleTestMode(machine_1); break;
		case CMD_JOG_MODE:              handleJogMode(machine_1); break;
		case CMD_HOMING_MODE:           handleHomingMode(machine_1); break;
		case CMD_FEED_MODE:             handleFeedMode(machine_1); break;
		case CMD_SET_TORQUE_OFFSET:     handleSetTorqueOffset(msg); break;
		case CMD_JOG_MOVE:              handleJogMove(msg, machine_1); break;
		case CMD_MACHINE_HOME_MOVE:     handleMachineHomeMove(msg, machine_1); break;
		case CMD_CARTRIDGE_HOME_MOVE:   handleCartridgeHomeMove(msg, machine_1); break;
		case CMD_INJECT_MOVE:           handleInjectMove(msg, machine_1); break;
		case CMD_PURGE_MOVE:            handlePurgeMove(msg, machine_1); break;
		case CMD_MOVE_TO_CARTRIDGE_HOME:    handleMoveToCartridgeHome(machine_1); break;
		case CMD_MOVE_TO_CARTRIDGE_RETRACT: handleMoveToCartridgeRetract(msg, machine_1); break;
		case CMD_PAUSE_OPERATION:       handlePauseOperation(machine_1); break;
		case CMD_RESUME_OPERATION:      handleResumeOperation(machine_1); break;
		case CMD_CANCEL_OPERATION:      handleCancelOperation(machine_1); break;
		case CMD_DAVID_STATE:			handleDavidMode(machine_1); break;

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

void handleEnable(Spinny_Spin_Machine *machine_1) {
	if (machine_1->mainState == DISABLED_MODE) {
		enableMotors("MOTORS ENABLED (transitioned to STANDBY_MODE)");
		machine_1->mainState = STANDBY_MODE;
		machine_1->homingState = HOMING_NONE;
		machine_1->feedState = FEED_NONE; 
		machine_1->errorState = ERROR_NONE;
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

void handleDisable(Spinny_Spin_Machine *machine_1)
{
	abortMove();
	Delay_ms(200);
	machine_1->mainState = DISABLED_MODE;
	machine_1->errorState = ERROR_NONE;
	disableMotors("MOTORS DISABLED (entered DISABLED state)");
	sendToPC("System disabled: must ENABLE to return to standby.");
}

void handleAbort(Spinny_Spin_Machine *machine_1) {
	sendToPC("ABORT received. Stopping motion and resetting...");

	abortMove();  // Stop motors
	Delay_ms(200);

	handleStandbyMode(machine_1);  // Fully resets system
}


void handleStandbyMode(Spinny_Spin_Machine *machine_1) {
	// This function is mostly from your uploaded messages.cpp (handleStandby), updated
	bool wasError = (machine_1->errorState != ERROR_NONE); // Check if error was active
	
	if (machine_1->mainState != STANDBY_MODE) { // Only log/act if changing state
		abortMove(); // Good practice to stop motion when changing to standby
		Delay_ms(200);
		machine_1->reset(); 

		if (wasError) {
			sendToPC("Exited previous state: State set to STANDBY_MODE and error cleared.");
			} else {
			sendToPC("State set to STANDBY_MODE.");
		}
		} else {
		if (machine_1->errorState != ERROR_NONE) { // If already in standby but error occurs and then standby is requested again
			machine_1->errorState = ERROR_NONE;
			sendToPC("Still in STANDBY_MODE, error cleared.");
			} else {
			sendToPC("Already in STANDBY_MODE.");
		}
	}
}

void handleTestMode(Spinny_Spin_Machine *machine_1) {
	// This function is from your uploaded messages.cpp (handleTestState)
	if (machine_1->mainState != TEST_MODE) {
		abortMove(); // Stop other activities
		machine_1->mainState = TEST_MODE;
		machine_1->homingState = HOMING_NONE; // Reset other mode sub-machine_1
		machine_1->feedState = FEED_NONE;
		machine_1->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered TEST_MODE.");
		} else {
		sendToPC("Already in TEST_MODE.");
	}
}

void handleJogMode(Spinny_Spin_Machine *machine_1) {
	// Your uploaded messages.cpp handleJogMode uses an old 'MOVING' and 'MOVING_JOG' concept.
	// This should now simply set the main state. Actual jog moves are handled by CMD_JOG_MOVE.
	if (machine_1->mainState != JOG_MODE) {
		abortMove(); // Stop other activities
		Delay_ms(200);
		machine_1->mainState = JOG_MODE;
		machine_1->homingState = HOMING_NONE; // Reset other mode sub-machine_1
		machine_1->feedState = FEED_NONE;
		machine_1->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered JOG_MODE. Ready for JOG_MOVE commands.");
		} else {
		sendToPC("Already in JOG_MODE.");
	}
}

void handleHomingMode(Spinny_Spin_Machine *machine_1) {
	if (machine_1->mainState != HOMING_MODE) {
		abortMove(); // Stop other activities
		Delay_ms(200);
		machine_1->mainState = HOMING_MODE;
		machine_1->homingState = HOMING_NONE; // Initialize to no specific homing action
		machine_1->feedState = FEED_NONE;     // Reset other mode sub-machine_1
		machine_1->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered HOMING_MODE. Ready for homing operations.");
		} else {
		sendToPC("Already in HOMING_MODE.");
	}
}

void handleFeedMode(Spinny_Spin_Machine *machine_1) {
	if (machine_1->mainState != FEED_MODE) {
		abortMove(); // Stop other activities
		Delay_ms(200);
		machine_1->mainState = FEED_MODE;
		machine_1->feedState = FEED_STANDBY;  // Default sub-state for feed operations
		machine_1->homingState = HOMING_NONE; // Reset other mode sub-machine_1
		machine_1->errorState = ERROR_NONE;   // Clear errors
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

void handleJogMove(const char *msg, Spinny_Spin_Machine *machine_1) {
	if (machine_1->mainState != JOG_MODE) {
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

		machine_1->jogDone = false; // Indicate a jog operation has started (if you use this flag)

		} else {
		char errorMsg[100];
		snprintf(errorMsg, sizeof(errorMsg), "Invalid JOG_MOVE format. Expected 5 params, got %d.", parsed_count);
		sendToPC(errorMsg);
	}
}

void handleMachineHomeMove(const char *msg, Spinny_Spin_Machine *machine_1) {
	if (machine_1->mainState != HOMING_MODE) {
		sendToPC("MACHINE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (machine_1->currentHomingPhase != HOMING_PHASE_IDLE && machine_1->currentHomingPhase != HOMING_PHASE_COMPLETE && machine_1->currentHomingPhase != HOMING_PHASE_ERROR) {
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
			machine_1->homingState = HOMING_MACHINE; // Indicate it was attempted
			machine_1->currentHomingPhase = HOMING_PHASE_ERROR;
			machine_1->errorState = ERROR_MANUAL_ABORT; // Or a more specific error
			return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Machine Home. Using default 20%.");
			torque_percent = 20.0f;
		}
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendToPC("Error: Invalid parameters for Machine Home (must be positive, retract >= 0). Set to HOMING_PHASE_ERROR.");
			machine_1->homingState = HOMING_MACHINE;
			machine_1->currentHomingPhase = HOMING_PHASE_ERROR;
			machine_1->errorState = ERROR_MANUAL_ABORT; // Or specific param error
			return;
		}


		// Store parameters in Systemmachine_1
		machine_1->homing_stroke_mm_param = stroke_mm;
		machine_1->homing_rapid_vel_mm_s_param = rapid_vel_mm_s;
		machine_1->homing_touch_vel_mm_s_param = touch_vel_mm_s;
		machine_1->homing_acceleration_param = acceleration_mm_s2;
		machine_1->homing_retract_mm_param = retract_mm;
		machine_1->homing_torque_percent_param = torque_percent;

		// Calculate and store step/sps values
		// STEPS_PER_MM_CONST needs pulsesPerRev which is from motor.cpp
		const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
		machine_1->homing_actual_stroke_steps = (long)(stroke_mm * current_steps_per_mm);
		machine_1->homing_actual_rapid_sps = (int)(rapid_vel_mm_s * current_steps_per_mm);
		machine_1->homing_actual_touch_sps = (int)(touch_vel_mm_s * current_steps_per_mm);
		machine_1->homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * current_steps_per_mm);
		machine_1->homing_actual_retract_steps = (long)(retract_mm * current_steps_per_mm);

		// Initialize homing state machine
		machine_1->homingState = HOMING_MACHINE;
		machine_1->currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
		machine_1->homingStartTime = Milliseconds(); // For timeout tracking
		machine_1->errorState = ERROR_NONE; // Clear previous errors

		sendToPC("Initiating Machine Homing: RAPID_MOVE phase.");
		// Machine homing is typically in the negative direction
		long initial_move_steps = -1 * machine_1->homing_actual_stroke_steps;
		moveMotors(initial_move_steps, initial_move_steps, (int)machine_1->homing_torque_percent_param, machine_1->homing_actual_rapid_sps, machine_1->homing_actual_accel_sps2);
		
		} else {
		sendToPC("Invalid MACHINE_HOME_MOVE format. Expected 6 parameters.");
		machine_1->homingState = HOMING_MACHINE; // Indicate it was attempted
		machine_1->currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void handleCartridgeHomeMove(const char *msg, Spinny_Spin_Machine *machine_1) {
	if (machine_1->mainState != HOMING_MODE) {
		sendToPC("CARTRIDGE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (machine_1->currentHomingPhase != HOMING_PHASE_IDLE && machine_1->currentHomingPhase != HOMING_PHASE_COMPLETE && machine_1->currentHomingPhase != HOMING_PHASE_ERROR) {
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
			machine_1->homingState = HOMING_CARTRIDGE;
			machine_1->currentHomingPhase = HOMING_PHASE_ERROR;
			machine_1->errorState = ERROR_MANUAL_ABORT; // Or a more specific error
			return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Cartridge Home. Using default 20%.");
			torque_percent = 20.0f;
		}
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendToPC("Error: Invalid parameters for Cartridge Home (must be positive, retract >= 0). Set to HOMING_PHASE_ERROR.");
			machine_1->homingState = HOMING_CARTRIDGE;
			machine_1->currentHomingPhase = HOMING_PHASE_ERROR;
			machine_1->errorState = ERROR_MANUAL_ABORT; // Or specific param error
			return;
		}


		machine_1->homing_stroke_mm_param = stroke_mm;
		machine_1->homing_rapid_vel_mm_s_param = rapid_vel_mm_s;
		machine_1->homing_touch_vel_mm_s_param = touch_vel_mm_s;
		machine_1->homing_acceleration_param = acceleration_mm_s2;
		machine_1->homing_retract_mm_param = retract_mm;
		machine_1->homing_torque_percent_param = torque_percent;

		const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
		machine_1->homing_actual_stroke_steps = (long)(stroke_mm * current_steps_per_mm);
		machine_1->homing_actual_rapid_sps = (int)(rapid_vel_mm_s * current_steps_per_mm);
		machine_1->homing_actual_touch_sps = (int)(touch_vel_mm_s * current_steps_per_mm);
		machine_1->homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * current_steps_per_mm);
		machine_1->homing_actual_retract_steps = (long)(retract_mm * current_steps_per_mm);

		machine_1->homingState = HOMING_CARTRIDGE;
		machine_1->currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
		machine_1->homingStartTime = Milliseconds();
		machine_1->errorState = ERROR_NONE;

		sendToPC("Initiating Cartridge Homing: RAPID_MOVE phase.");
		// Cartridge homing is typically in the positive direction
		long initial_move_steps = 1 * machine_1->homing_actual_stroke_steps;
		moveMotors(initial_move_steps, initial_move_steps, (int)machine_1->homing_torque_percent_param, machine_1->homing_actual_rapid_sps, machine_1->homing_actual_accel_sps2);

		} else {
		sendToPC("Invalid CARTRIDGE_HOME_MOVE format. Expected 6 parameters.");
		machine_1->homingState = HOMING_CARTRIDGE;
		machine_1->currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void resetActiveDispenseOp(Spinny_Spin_Machine *machine_1) {
    machine_1->active_op_target_ml = 0.0f;
    machine_1->active_op_total_dispensed_ml = 0.0f;
    machine_1->active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
    machine_1->active_op_steps_per_ml = 0.0f;
    machine_1->active_dispense_operation_ongoing = false;
}

void handleClearErrors(Spinny_Spin_Machine *machine_1) {
	sendToPC("CLEAR_ERRORS received. Resetting system...");
	handleStandbyMode(machine_1);  // Goes to standby and clears errors
}

void handleMoveToCartridgeHome(Spinny_Spin_Machine *machine_1) {
	if (machine_1->mainState != FEED_MODE) { /* ... */ return; }
	if (!machine_1->homingCartridgeDone) { machine_1->errorState = ERROR_NO_CARTRIDGE_HOME; sendToPC("Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { sendToPC("Err: Motors disabled."); return; }
	if (checkMoving() || machine_1->feedState == FEED_INJECT_ACTIVE || machine_1->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Err: Busy. Cannot move to cart home now.");
		machine_1->errorState = ERROR_INVALID_OPERATION; return;
	}

	sendToPC("Cmd: Move to Cartridge Home...");
	fullyResetActiveDispenseOperation(machine_1); // Reset any prior dispense op
	machine_1->feedState = FEED_MOVING_TO_HOME;
	machine_1->feedingDone = false;
	
	long current_axis_pos = ConnectorM0.PositionRefCommanded();
	long steps_to_move_axis = cartridgeHomeReferenceSteps - current_axis_pos;

	moveMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	FEED_GOTO_TORQUE_PERCENT, FEED_GOTO_VELOCITY_SPS, FEED_GOTO_ACCEL_SPS2);
}

void handleMoveToCartridgeRetract(const char *msg, Spinny_Spin_Machine *machine_1) {
	if (machine_1->mainState != FEED_MODE) { /* ... */ return; }
	if (!machine_1->homingCartridgeDone) { /* ... */ machine_1->errorState = ERROR_NO_CARTRIDGE_HOME; sendToPC("Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { /* ... */ sendToPC("Err: Motors disabled."); return; }
	if (checkMoving() || machine_1->feedState == FEED_INJECT_ACTIVE || machine_1->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Err: Busy. Cannot move to cart retract now.");
		machine_1->errorState = ERROR_INVALID_OPERATION; return;
	}

	float offset_mm = 0.0f;
	if (sscanf(msg + strlen(CMD_STR_MOVE_TO_CARTRIDGE_RETRACT), "%f", &offset_mm) != 1 || offset_mm < 0) {
		sendToPC("Err: Invalid offset for MOVE_TO_CARTRIDGE_RETRACT."); return;
	}
	
	fullyResetActiveDispenseOperation(machine_1);
	machine_1->feedState = FEED_MOVING_TO_RETRACT;
	machine_1->feedingDone = false;

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


void handleInjectMove(const char *msg, Spinny_Spin_Machine *machine_1) {
	if (machine_1->mainState != FEED_MODE) {
		sendToPC("INJECT ignored: Not in FEED_MODE.");
		return;
	}
	if (checkMoving() || machine_1->active_dispense_operation_ongoing) {
		sendToPC("Error: Operation already in progress or motors busy.");
		machine_1->errorState = ERROR_INVALID_OPERATION; // Set an error state
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2_param, steps_per_ml_val, torque_percent;
	// Format: INJECT_MOVE <volume_ml> <speed_ml_s> <feed_accel_sps2> <feed_steps_per_ml> <feed_torque_percent>
	if (sscanf(msg + strlen(CMD_STR_INJECT_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration_sps2_param, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!motorsAreEnabled) { sendToPC("INJECT blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0.0001f) { sendToPC("Error: Steps/ml must be positive and non-zero."); return; }
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f; // Default
		if (volume_ml <= 0) { sendToPC("Error: Inject volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendToPC("Error: Inject speed must be positive."); return; }
		if (acceleration_sps2_param <= 0) { sendToPC("Error: Inject acceleration must be positive."); return; }

		// Clear previous operation's trackers and set last completed to 0 for this new context
		fullyResetActiveDispenseOperation(machine_1);
		machine_1->last_completed_dispense_ml = 0.0f;

		machine_1->feedState = FEED_INJECT_STARTING; // Will transition to ACTIVE in main loop
		machine_1->feedingDone = false; // Mark that a feed operation is starting
		machine_1->active_dispense_operation_ongoing = true;
		machine_1->active_op_target_ml = volume_ml;
		machine_1->active_op_steps_per_ml = steps_per_ml_val;
		machine_1->active_op_total_target_steps = (long)(volume_ml * machine_1->active_op_steps_per_ml);
		machine_1->active_op_remaining_steps = machine_1->active_op_total_target_steps;
		
		// Capture motor position at the very start of the entire multi-segment operation
		machine_1->active_op_initial_axis_steps = ConnectorM0.PositionRefCommanded();
		// Also set for the first segment
		machine_1->active_op_segment_initial_axis_steps = machine_1->active_op_initial_axis_steps;
		
		machine_1->active_op_total_dispensed_ml = 0.0f; // Explicitly reset for the new operation

		machine_1->active_op_velocity_sps = (int)(speed_ml_s * machine_1->active_op_steps_per_ml);
		machine_1->active_op_accel_sps2 = (int)acceleration_sps2_param; // Use param directly
		machine_1->active_op_torque_percent = (int)torque_percent;
		if (machine_1->active_op_velocity_sps <= 0) machine_1->active_op_velocity_sps = 100; // Fallback velocity

		char response[200];
		snprintf(response, sizeof(response), "RX INJECT: Vol:%.2fml, Speed:%.2fml/s (calc_vel_sps:%d), Acc:%.0f, Steps/ml:%.2f, Tq:%.0f%%",
		volume_ml, speed_ml_s, machine_1->active_op_velocity_sps, acceleration_sps2_param, steps_per_ml_val, torque_percent);
		sendToPC(response);
		sendToPC("Starting Inject operation...");

		// Positive steps for inject/purge (assuming forward motion dispenses)
		moveMotors(machine_1->active_op_remaining_steps, machine_1->active_op_remaining_steps,
		machine_1->active_op_torque_percent, machine_1->active_op_velocity_sps, machine_1->active_op_accel_sps2);
		// machine_1->feedState will be set to FEED_INJECT_ACTIVE in main.cpp loop once motion starts
		} else {
		sendToPC("Invalid INJECT_MOVE format. Expected 5 params.");
	}
}

void handlePurgeMove(const char *msg, Spinny_Spin_Machine *machine_1) {
	if (machine_1->mainState != FEED_MODE) { sendToPC("PURGE ignored: Not in FEED_MODE."); return; }
	if (checkMoving() || machine_1->active_dispense_operation_ongoing) {
		sendToPC("Error: Operation already in progress or motors busy.");
		machine_1->errorState = ERROR_INVALID_OPERATION;
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2_param, steps_per_ml_val, torque_percent;
	// Format: PURGE_MOVE <volume_ml> <speed_ml_s> <feed_accel_sps2> <feed_steps_per_ml> <feed_torque_percent>
	if (sscanf(msg + strlen(CMD_STR_PURGE_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration_sps2_param, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!motorsAreEnabled) { sendToPC("PURGE blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0.0001f) { sendToPC("Error: Steps/ml must be positive and non-zero."); return;}
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f; // Default
		if (volume_ml <= 0) { sendToPC("Error: Purge volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendToPC("Error: Purge speed must be positive."); return; }
		if (acceleration_sps2_param <= 0) { sendToPC("Error: Purge acceleration must be positive."); return; }

		// Clear previous operation's trackers and set last completed to 0 for this new context
		fullyResetActiveDispenseOperation(machine_1);
		machine_1->last_completed_dispense_ml = 0.0f;

		machine_1->feedState = FEED_PURGE_STARTING; // Will transition to ACTIVE in main loop
		machine_1->feedingDone = false;
		machine_1->active_dispense_operation_ongoing = true;
		machine_1->active_op_target_ml = volume_ml;
		machine_1->active_op_steps_per_ml = steps_per_ml_val;
		machine_1->active_op_total_target_steps = (long)(volume_ml * machine_1->active_op_steps_per_ml);
		machine_1->active_op_remaining_steps = machine_1->active_op_total_target_steps;
		
		machine_1->active_op_initial_axis_steps = ConnectorM0.PositionRefCommanded();
		machine_1->active_op_segment_initial_axis_steps = machine_1->active_op_initial_axis_steps;
		
		machine_1->active_op_total_dispensed_ml = 0.0f; // Explicitly reset

		machine_1->active_op_velocity_sps = (int)(speed_ml_s * machine_1->active_op_steps_per_ml);
		machine_1->active_op_accel_sps2 = (int)acceleration_sps2_param;
		machine_1->active_op_torque_percent = (int)torque_percent;
		if (machine_1->active_op_velocity_sps <= 0) machine_1->active_op_velocity_sps = 200; // Fallback

		char response[200];
		snprintf(response, sizeof(response), "RX PURGE: Vol:%.2fml, Speed:%.2fml/s (calc_vel_sps:%d), Acc:%.0f, Steps/ml:%.2f, Tq:%.0f%%",
		volume_ml, speed_ml_s, machine_1->active_op_velocity_sps, acceleration_sps2_param, steps_per_ml_val, torque_percent);
		sendToPC(response);
		sendToPC("Starting Purge operation...");
		
		moveMotors(machine_1->active_op_remaining_steps, machine_1->active_op_remaining_steps,
		machine_1->active_op_torque_percent, machine_1->active_op_velocity_sps, machine_1->active_op_accel_sps2);
		// machine_1->feedState will be set to FEED_PURGE_ACTIVE in main.cpp loop once motion starts
		} else {
		sendToPC("Invalid PURGE_MOVE format. Expected 5 params.");
	}
}

void handlePauseOperation(Spinny_Spin_Machine *machine_1) {
	if (!machine_1->active_dispense_operation_ongoing) {
		sendToPC("PAUSE ignored: No active inject/purge operation.");
		return;
	}

	if (machine_1->feedState == FEED_INJECT_ACTIVE || machine_1->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Cmd: Pausing operation...");
		ConnectorM0.MoveStopDecel();
		ConnectorM1.MoveStopDecel();

		// Immediately set paused state, main loop will finalize steps
		if (machine_1->feedState == FEED_INJECT_ACTIVE) machine_1->feedState = FEED_INJECT_PAUSED;
		if (machine_1->feedState == FEED_PURGE_ACTIVE) machine_1->feedState = FEED_PURGE_PAUSED;
		} else {
		sendToPC("PAUSE ignored: Not in an active inject/purge state.");
	}
}


void handleResumeOperation(Spinny_Spin_Machine *machine_1) {
	if (!machine_1->active_dispense_operation_ongoing) {
		sendToPC("RESUME ignored: No operation was ongoing or paused.");
		return;
	}
	if (machine_1->feedState == FEED_INJECT_PAUSED || machine_1->feedState == FEED_PURGE_PAUSED) {
		if (machine_1->active_op_remaining_steps <= 0) {
			sendToPC("RESUME ignored: No remaining volume/steps to dispense.");
			fullyResetActiveDispenseOperation(machine_1);
			machine_1->feedState = FEED_STANDBY;
			return;
		}
		sendToPC("Cmd: Resuming operation...");
		machine_1->active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded(); // Update base for this segment
		machine_1->feedingDone = false; // New segment starting

		moveMotors(machine_1->active_op_remaining_steps, machine_1->active_op_remaining_steps,
		machine_1->active_op_torque_percent, machine_1->active_op_velocity_sps, machine_1->active_op_accel_sps2);
				
		if(machine_1->feedState == FEED_INJECT_PAUSED) machine_1->feedState = FEED_INJECT_RESUMING; // Or directly to _ACTIVE
		if(machine_1->feedState == FEED_PURGE_PAUSED) machine_1->feedState = FEED_PURGE_RESUMING;  // Or directly to _ACTIVE
		// main.cpp will transition from RESUMING to ACTIVE once motors are moving, or directly here.
		// For simplicity let's assume it goes active if move command is successful.
		if(machine_1->feedState == FEED_INJECT_RESUMING) machine_1->feedState = FEED_INJECT_ACTIVE;
		if(machine_1->feedState == FEED_PURGE_RESUMING) machine_1->feedState = FEED_PURGE_ACTIVE;


		} else {
		sendToPC("RESUME ignored: Operation not paused.");
	}
}

void handleCancelOperation(Spinny_Spin_Machine *machine_1) {
	if (!machine_1->active_dispense_operation_ongoing) {
		sendToPC("CANCEL ignored: No active inject/purge operation to cancel.");
		return;
	}

	sendToPC("Cmd: Cancelling current feed operation...");
	abortMove(); // Stop motors abruptly
	Delay_ms(100); // Give motors a moment to fully stop if needed

	// Even though cancelled, calculate what was dispensed before reset for logging/internal use if needed.
	// However, for the purpose of telemetry display when idle, a cancelled op means 0 "completed" dispense.
	if (machine_1->active_op_steps_per_ml > 0.0001f && machine_1->active_dispense_operation_ongoing) {
		long steps_moved_on_axis = ConnectorM0.PositionRefCommanded() - machine_1->active_op_segment_initial_axis_steps;
		float segment_dispensed_ml = (float)fabs(steps_moved_on_axis) / machine_1->active_op_steps_per_ml;
		machine_1->active_op_total_dispensed_ml += segment_dispensed_ml;
		// This active_op_total_dispensed_ml will be reset by fullyResetActiveDispenseOperation shortly.
		// If you needed to log the actual amount dispensed before cancel, do it here.
	}
	
	machine_1->last_completed_dispense_ml = 0.0f; // Set "completed" amount to 0 for a cancelled operation

	fullyResetActiveDispenseOperation(machine_1); // Clears all active_op_ trackers
	
	machine_1->feedState = FEED_OPERATION_CANCELLED; // A specific state for cancelled
	// Main loop should transition this to FEED_STANDBY
	machine_1->feedingDone = true; // Mark as "done" (via cancellation)
	machine_1->errorState = ERROR_NONE; // Typically, a user cancel is not an "error" condition.
	// If it should be, set machine_1->errorState = ERROR_MANUAL_ABORT;

	sendToPC("Operation Cancelled. Dispensed value for this attempt will show as 0 ml in idle telemetry.");
}




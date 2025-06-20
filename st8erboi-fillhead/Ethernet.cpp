#include "messages.h"
#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include "motor.h"
#include "InjectorStates.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define LOCAL_PORT 8888
const uint16_t MAX_PACKET_LENGTH = 100;

#define USER_CMD_STR_DISCOVER_TELEM       "DISCOVER_TELEM"
#define USER_CMD_STR_ENABLE               "ENABLE"
#define USER_CMD_STR_DISABLE              "DISABLE"
#define USER_CMD_STR_ABORT                "ABORT"
#define USER_CMD_STR_CLEAR_ERRORS         "CLEAR_ERRORS" // New
#define USER_CMD_STR_STANDBY_MODE         "STANDBY_MODE"
#define USER_CMD_STR_TEST_MODE            "TEST_MODE"
#define USER_CMD_STR_JOG_MODE             "JOG_MODE"
#define USER_CMD_STR_HOMING_MODE          "HOMING_MODE"
#define USER_CMD_STR_FEED_MODE            "FEED_MODE"
#define USER_CMD_STR_SET_TORQUE_OFFSET    "SET_TORQUE_OFFSET "
#define USER_CMD_STR_JOG_MOVE             "JOG_MOVE "
#define USER_CMD_STR_MACHINE_HOME_MOVE    "MACHINE_HOME_MOVE "
#define USER_CMD_STR_CARTRIDGE_HOME_MOVE  "CARTRIDGE_HOME_MOVE "
#define USER_CMD_STR_PINCH_HOME_MOVE  "PINCH_HOME_MOVE "
#define USER_CMD_STR_INJECT_MOVE          "INJECT_MOVE "
#define USER_CMD_STR_PURGE_MOVE           "PURGE_MOVE "
#define USER_CMD_STR_PINCH_OPEN			 "PINCH_OPEN"
#define USER_CMD_STR_PINCH_CLOSE			 "PINCH_CLOSE"
#define USER_CMD_STR_MOVE_TO_CARTRIDGE_HOME "MOVE_TO_CARTRIDGE_HOME"
#define USER_CMD_STR_MOVE_TO_CARTRIDGE_RETRACT "MOVE_TO_CARTRIDGE_RETRACT "
#define USER_CMD_STR_PAUSE_INJECTION "PAUSE_INJECTION"
#define USER_CMD_STR_RESUME_INJECTION "RESUME_INJECTION"
#define USER_CMD_STR_CANCEL_INJECTION "CANCEL_INJECTION"

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

void handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp)
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

void checkUdpBuffer(Injector *injector)
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

void finalizeAndResetActiveDispenseOperation(Injector *injector, bool operationCompletedSuccessfully) {
	if (injector->active_dispense_INJECTION_ongoing) {
		// Calculate final dispensed amount for this operation segment
		if (injector->active_op_steps_per_ml > 0.0001f) {
			long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - injector->active_op_segment_initial_axis_steps;
			float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / injector->active_op_steps_per_ml;
			injector->active_op_total_dispensed_ml += segment_dispensed_ml;
		}
		injector->last_completed_dispense_ml = injector->active_op_total_dispensed_ml; // Store final total
	}
	
	// This part is similar to fullyReset, but called after a successful/finalized op.
	injector->active_dispense_INJECTION_ongoing = false;
	injector->active_op_target_ml = 0.0f;
	// injector->active_op_total_dispensed_ml is now in last_completed_dispense_ml. If you want it zero for next op,
	// fullyReset should be called by the START of the next op.
	injector->active_op_remaining_steps = 0;
}

void fullyResetActiveDispenseOperation(Injector *injector) {
	injector->active_dispense_INJECTION_ongoing = false;
	injector->active_op_target_ml = 0.0f;
	injector->active_op_total_dispensed_ml = 0.0f; // Crucial for starting fresh
	injector->active_op_total_target_steps = 0;
	injector->active_op_remaining_steps = 0;
	injector->active_op_segment_initial_axis_steps = 0; // Reset for next segment start
	injector->active_op_initial_axis_steps = 0;       // Reset for the very start of an operation
	injector->active_op_steps_per_ml = 0.0f;
	// Note: injector->last_completed_dispense_ml is now explicitly managed by callers (start, cancel, complete)
	// to reflect the outcome of the operation for idle telemetry.
}

void sendTelem(Injector *injector) {
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




UserCommand parseUserCommand(const char *msg) {
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

	if (strncmp(msg, USER_CMD_STR_SET_TORQUE_OFFSET, strlen(USER_CMD_STR_SET_TORQUE_OFFSET)) == 0) return USER_CMD_SET_TORQUE_OFFSET;
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

void handleMessage(const char *msg, Injector *injector) {
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
		case USER_CMD_SET_TORQUE_OFFSET:     handleSetTorqueOffset(msg); break;
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

// --- System Command Handlers ---

void handleEnable(Injector *injector) {
	if (injector->mainState == DISABLED_MODE) {
		enableInjectorMotors("MOTORS ENABLED (transitioned to STANDBY_MODE)");
		injector->mainState = STANDBY_MODE;
		injector->homingState = HOMING_NONE;
		injector->feedState = FEED_NONE; 
		injector->errorState = ERROR_NONE;
		sendToPC("System enabled: state is now STANDBY_MODE.");
		} else {
		// If already enabled, or in a state other than DISABLED_MODE,
		// just ensure motors are enabled without changing main state.
		if (!motorsAreEnabled) {
			enableInjectorMotors("MOTORS re-enabled (state unchanged)");
			} else {
			sendToPC("Motors already enabled, system not in DISABLED_MODE.");
		}
	}
}

void handleDisable(Injector *injector)
{
	abortInjectorMove();
	Delay_ms(200);
	injector->mainState = DISABLED_MODE;
	injector->errorState = ERROR_NONE;
	disableInjectorMotors("MOTORS DISABLED (entered DISABLED state)");
	sendToPC("System disabled: must ENABLE to return to standby.");
}

void handleAbort(Injector *injector) {
	sendToPC("ABORT received. Stopping motion and resetting...");

	abortInjectorMove();  // Stop motors
	Delay_ms(200);

	handleStandbyMode(injector);  // Fully resets system
}


void handleStandbyMode(Injector *injector) {
	// This function is mostly from your uploaded messages.cpp (handleStandby), updated
	bool wasError = (injector->errorState != ERROR_NONE); // Check if error was active
	
	if (injector->mainState != STANDBY_MODE) { // Only log/act if changing state
		abortInjectorMove(); // Good practice to stop motion when changing to standby
		Delay_ms(200);
		injector->reset(); 

		if (wasError) {
			sendToPC("Exited previous state: State set to STANDBY_MODE and error cleared.");
			} else {
			sendToPC("State set to STANDBY_MODE.");
		}
		} else {
		if (injector->errorState != ERROR_NONE) { // If already in standby but error occurs and then standby is requested again
			injector->errorState = ERROR_NONE;
			sendToPC("Still in STANDBY_MODE, error cleared.");
			} else {
			sendToPC("Already in STANDBY_MODE.");
		}
	}
}

void handleTestMode(Injector *injector) {
	// This function is from your uploaded messages.cpp (handleTestState)
	if (injector->mainState != TEST_MODE) {
		abortInjectorMove(); // Stop other activities
		injector->mainState = TEST_MODE;
		injector->homingState = HOMING_NONE; // Reset other mode sub-injector
		injector->feedState = FEED_NONE;
		injector->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered TEST_MODE.");
		} else {
		sendToPC("Already in TEST_MODE.");
	}
}

void handleJogMode(Injector *injector) {
	// Your uploaded messages.cpp handleJogMode uses an old 'MOVING' and 'MOVING_JOG' concept.
	// This should now simply set the main state. Actual jog moves are handled by USER_CMD_JOG_MOVE.
	if (injector->mainState != JOG_MODE) {
		abortInjectorMove(); // Stop other activities
		Delay_ms(200);
		injector->mainState = JOG_MODE;
		injector->homingState = HOMING_NONE; // Reset other mode sub-injector
		injector->feedState = FEED_NONE;
		injector->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered JOG_MODE. Ready for JOG_MOVE commands.");
		} else {
		sendToPC("Already in JOG_MODE.");
	}
}

void handleHomingMode(Injector *injector) {
	if (injector->mainState != HOMING_MODE) {
		abortInjectorMove(); // Stop other activities
		Delay_ms(200);
		injector->mainState = HOMING_MODE;
		injector->homingState = HOMING_NONE; // Initialize to no specific homing action
		injector->feedState = FEED_NONE;     // Reset other mode sub-injector
		injector->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered HOMING_MODE. Ready for homing operations.");
		} else {
		sendToPC("Already in HOMING_MODE.");
	}
}

void handleFeedMode(Injector *injector) {
	if (injector->mainState != FEED_MODE) {
		abortInjectorMove(); // Stop other activities
		Delay_ms(200);
		injector->mainState = FEED_MODE;
		injector->feedState = FEED_STANDBY;  // Default sub-state for feed operations
		injector->homingState = HOMING_NONE; // Reset other mode sub-injector
		injector->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered FEED_MODE. Ready for inject/purge/retract.");
		} else {
		sendToPC("Already in FEED_MODE.");
	}
}

void handleSetTorqueOffset(const char *msg) {
	float newOffset = atof(msg + strlen(USER_CMD_STR_SET_TORQUE_OFFSET)); // Use defined string length
	torqueOffset = newOffset; // This directly updates the global from motor.cpp
	char response[64];
	snprintf(response, sizeof(response), "Global torque offset set to %.2f", torqueOffset);
	sendToPC(response);
}

void handleJogMove(const char *msg, Injector *injector) {
	if (injector->mainState != JOG_MODE) {
		sendToPC("JOG_MOVE ignored: Not in JOG_MODE.");
		return;
	}

	long s0 = 0, s1 = 0;
	int torque_percent = 0, velocity_sps = 0, accel_sps2 = 0;

	// Format: JOG_MOVE <s0_steps> <s1_steps> <torque_%> <vel_sps> <accel_sps2>
	int parsed_count = sscanf(msg + strlen(USER_CMD_STR_JOG_MOVE), "%ld %ld %d %d %d",
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


		// Directly call your existing moveInjectorMotors function
		moveInjectorMotors((int)s0, (int)s1, torque_percent, velocity_sps, accel_sps2);

		injector->jogDone = false; // Indicate a jog operation has started (if you use this flag)

		} else {
		char errorMsg[100];
		snprintf(errorMsg, sizeof(errorMsg), "Invalid JOG_MOVE format. Expected 5 params, got %d.", parsed_count);
		sendToPC(errorMsg);
	}
}

void handleMachineHomeMove(const char *msg, Injector *injector) {
	if (injector->mainState != HOMING_MODE) {
		sendToPC("MACHINE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (injector->currentHomingPhase != HOMING_PHASE_IDLE && injector->currentHomingPhase != HOMING_PHASE_COMPLETE && injector->currentHomingPhase != HOMING_PHASE_ERROR) {
		sendToPC("MACHINE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}

	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent;
	// Format: <USER_CMD_STR> <stroke_mm> <rapid_vel_mm_s> <touch_vel_mm_s> <acceleration_mm_s2> <retract_dist_mm> <torque_%>
	
	int parsed_count = sscanf(msg + strlen(USER_CMD_STR_MACHINE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration_mm_s2, &retract_mm, &torque_percent);

	if (parsed_count == 6) {
		char response[250]; // Increased for acceleration
		snprintf(response, sizeof(response), "MACHINE_HOME_MOVE RX: Strk:%.1f RpdV:%.1f TchV:%.1f Acc:%.1f Ret:%.1f Tq:%.1f%%",
		stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("MACHINE_HOME_MOVE blocked: Motors disabled. Set to HOMING_PHASE_ERROR.");
			injector->homingState = HOMING_MACHINE; // Indicate it was attempted
			injector->currentHomingPhase = HOMING_PHASE_ERROR;
			injector->errorState = ERROR_MANUAL_ABORT; // Or a more specific error
			return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Machine Home. Using default 20%.");
			torque_percent = 20.0f;
		}
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendToPC("Error: Invalid parameters for Machine Home (must be positive, retract >= 0). Set to HOMING_PHASE_ERROR.");
			injector->homingState = HOMING_MACHINE;
			injector->currentHomingPhase = HOMING_PHASE_ERROR;
			injector->errorState = ERROR_MANUAL_ABORT; // Or specific param error
			return;
		}


		// Store parameters in Injector
		injector->homing_stroke_mm_param = stroke_mm;
		injector->homing_rapid_vel_mm_s_param = rapid_vel_mm_s;
		injector->homing_touch_vel_mm_s_param = touch_vel_mm_s;
		injector->homing_acceleration_param = acceleration_mm_s2;
		injector->homing_retract_mm_param = retract_mm;
		injector->homing_torque_percent_param = torque_percent;

		// Calculate and store step/sps values
		// STEPS_PER_MM_CONST needs pulsesPerRev which is from motor.cpp
		const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
		injector->homing_actual_stroke_steps = (long)(stroke_mm * current_steps_per_mm);
		injector->homing_actual_rapid_sps = (int)(rapid_vel_mm_s * current_steps_per_mm);
		injector->homing_actual_touch_sps = (int)(touch_vel_mm_s * current_steps_per_mm);
		injector->homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * current_steps_per_mm);
		injector->homing_actual_retract_steps = (long)(retract_mm * current_steps_per_mm);

		// Initialize homing state machine
		injector->homingState = HOMING_MACHINE;
		injector->currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
		injector->homingStartTime = Milliseconds(); // For timeout tracking
		injector->errorState = ERROR_NONE; // Clear previous errors

		sendToPC("Initiating Machine Homing: RAPID_MOVE phase.");
		// Machine homing is typically in the negative direction
		long initial_move_steps = -1 * injector->homing_actual_stroke_steps;
		moveInjectorMotors(initial_move_steps, initial_move_steps, (int)injector->homing_torque_percent_param, injector->homing_actual_rapid_sps, injector->homing_actual_accel_sps2);
		
		} else {
		sendToPC("Invalid MACHINE_HOME_MOVE format. Expected 6 parameters.");
		injector->homingState = HOMING_MACHINE; // Indicate it was attempted
		injector->currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void handleCartridgeHomeMove(const char *msg, Injector *injector) {
	if (injector->mainState != HOMING_MODE) {
		sendToPC("CARTRIDGE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (injector->currentHomingPhase != HOMING_PHASE_IDLE && injector->currentHomingPhase != HOMING_PHASE_COMPLETE && injector->currentHomingPhase != HOMING_PHASE_ERROR) {
		sendToPC("CARTRIDGE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}
	
	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent;
	int parsed_count = sscanf(msg + strlen(USER_CMD_STR_CARTRIDGE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration_mm_s2, &retract_mm, &torque_percent);

	if (parsed_count == 6) {
		char response[250];
		snprintf(response, sizeof(response), "CARTRIDGE_HOME_MOVE RX: Strk:%.1f RpdV:%.1f TchV:%.1f Acc:%.1f Ret:%.1f Tq:%.1f%%",
		stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("CARTRIDGE_HOME_MOVE blocked: Motors disabled. Set to HOMING_PHASE_ERROR.");
			injector->homingState = HOMING_CARTRIDGE;
			injector->currentHomingPhase = HOMING_PHASE_ERROR;
			injector->errorState = ERROR_MANUAL_ABORT; // Or a more specific error
			return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Cartridge Home. Using default 20%.");
			torque_percent = 20.0f;
		}
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendToPC("Error: Invalid parameters for Cartridge Home (must be positive, retract >= 0). Set to HOMING_PHASE_ERROR.");
			injector->homingState = HOMING_CARTRIDGE;
			injector->currentHomingPhase = HOMING_PHASE_ERROR;
			injector->errorState = ERROR_MANUAL_ABORT; // Or specific param error
			return;
		}


		injector->homing_stroke_mm_param = stroke_mm;
		injector->homing_rapid_vel_mm_s_param = rapid_vel_mm_s;
		injector->homing_touch_vel_mm_s_param = touch_vel_mm_s;
		injector->homing_acceleration_param = acceleration_mm_s2;
		injector->homing_retract_mm_param = retract_mm;
		injector->homing_torque_percent_param = torque_percent;

		const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
		injector->homing_actual_stroke_steps = (long)(stroke_mm * current_steps_per_mm);
		injector->homing_actual_rapid_sps = (int)(rapid_vel_mm_s * current_steps_per_mm);
		injector->homing_actual_touch_sps = (int)(touch_vel_mm_s * current_steps_per_mm);
		injector->homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * current_steps_per_mm);
		injector->homing_actual_retract_steps = (long)(retract_mm * current_steps_per_mm);

		injector->homingState = HOMING_CARTRIDGE;
		injector->currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
		injector->homingStartTime = Milliseconds();
		injector->errorState = ERROR_NONE;

		sendToPC("Initiating Cartridge Homing: RAPID_MOVE phase.");
		// Cartridge homing is typically in the positive direction
		long initial_move_steps = 1 * injector->homing_actual_stroke_steps;
		moveInjectorMotors(initial_move_steps, initial_move_steps, (int)injector->homing_torque_percent_param, injector->homing_actual_rapid_sps, injector->homing_actual_accel_sps2);

		} else {
		sendToPC("Invalid CARTRIDGE_HOME_MOVE format. Expected 6 parameters.");
		injector->homingState = HOMING_CARTRIDGE;
		injector->currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void handlePinchHomeMove(Injector *injector) {
	if (injector->mainState != HOMING_MODE) {
		sendToPC("MACHINE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (injector->currentHomingPhase != HOMING_PHASE_IDLE && injector->currentHomingPhase != HOMING_PHASE_COMPLETE && injector->currentHomingPhase != HOMING_PHASE_ERROR) {
		sendToPC("MACHINE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}

	float velocity = 200; //steps per second
	float acceleration = 5000; //steps per second squared
	float torque_percent = 30;
	
	if (!motorsAreEnabled) {
		sendToPC("MACHINE_HOME_MOVE blocked: Motors disabled. Set to HOMING_PHASE_ERROR.");
		injector->homingState = HOMING_MACHINE; // Indicate it was attempted
		injector->currentHomingPhase = HOMING_PHASE_ERROR;
		injector->errorState = ERROR_MANUAL_ABORT; // Or a more specific error
		return;
	}

		// Store parameters in Injector
		injector->homing_stroke_mm_param = stroke_mm;
		injector->homing_rapid_vel_mm_s_param = rapid_vel_mm_s;
		injector->homing_touch_vel_mm_s_param = touch_vel_mm_s;
		injector->homing_acceleration_param = acceleration_mm_s2;
		injector->homing_retract_mm_param = retract_mm;
		injector->homing_torque_percent_param = torque_percent;

		// Calculate and store step/sps values
		// STEPS_PER_MM_CONST needs pulsesPerRev which is from motor.cpp
		const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
		injector->homing_actual_stroke_steps = (long)(stroke_mm * current_steps_per_mm);
		injector->homing_actual_rapid_sps = (int)(rapid_vel_mm_s * current_steps_per_mm);
		injector->homing_actual_touch_sps = (int)(touch_vel_mm_s * current_steps_per_mm);
		injector->homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * current_steps_per_mm);
		injector->homing_actual_retract_steps = (long)(retract_mm * current_steps_per_mm);

		// Initialize homing state machine
		injector->homingState = HOMING_MACHINE;
		injector->currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
		injector->homingStartTime = Milliseconds(); // For timeout tracking
		injector->errorState = ERROR_NONE; // Clear previous errors

		sendToPC("Initiating Machine Homing: RAPID_MOVE phase.");
		// Machine homing is typically in the negative direction
		long initial_move_steps = -1 * injector->homing_actual_stroke_steps;
		moveInjectorMotors(initial_move_steps, initial_move_steps, (int)injector->homing_torque_percent_param, injector->homing_actual_rapid_sps, injector->homing_actual_accel_sps2);
		
		} else {
		sendToPC("Invalid MACHINE_HOME_MOVE format. Expected 6 parameters.");
		injector->homingState = HOMING_MACHINE; // Indicate it was attempted
		injector->currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void resetActiveDispenseOp(Injector *injector) {
    injector->active_op_target_ml = 0.0f;
    injector->active_op_total_dispensed_ml = 0.0f;
    injector->active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
    injector->active_op_steps_per_ml = 0.0f;
    injector->active_dispense_INJECTION_ongoing = false;
}

void handleClearErrors(Injector *injector) {
	sendToPC("CLEAR_ERRORS received. Resetting system...");
	handleStandbyMode(injector);  // Goes to standby and clears errors
}

void handleMoveToCartridgeHome(Injector *injector) {
	if (injector->mainState != FEED_MODE) { /* ... */ return; }
	if (!injector->homingCartridgeDone) { injector->errorState = ERROR_NO_CARTRIDGE_HOME; sendToPC("Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { sendToPC("Err: Motors disabled."); return; }
	if (checkInjectorMoving() || injector->feedState == FEED_INJECT_ACTIVE || injector->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Err: Busy. Cannot move to cart home now.");
		injector->errorState = ERROR_INVALID_INJECTION; return;
	}

	sendToPC("Cmd: Move to Cartridge Home...");
	fullyResetActiveDispenseOperation(injector); // Reset any prior dispense op
	injector->feedState = FEED_MOVING_TO_HOME;
	injector->feedingDone = false;
	
	long current_axis_pos = ConnectorM0.PositionRefCommanded();
	long steps_to_move_axis = cartridgeHomeReferenceSteps - current_axis_pos;

	moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	FEED_GOTO_TORQUE_PERCENT, FEED_GOTO_VELOCITY_SPS, FEED_GOTO_ACCEL_SPS2);
}

void handleMoveToCartridgeRetract(const char *msg, Injector *injector) {
	if (injector->mainState != FEED_MODE) { /* ... */ return; }
	if (!injector->homingCartridgeDone) { /* ... */ injector->errorState = ERROR_NO_CARTRIDGE_HOME; sendToPC("Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { /* ... */ sendToPC("Err: Motors disabled."); return; }
	if (checkInjectorMoving() || injector->feedState == FEED_INJECT_ACTIVE || injector->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Err: Busy. Cannot move to cart retract now.");
		injector->errorState = ERROR_INVALID_INJECTION; return;
	}

	float offset_mm = 0.0f;
	if (sscanf(msg + strlen(USER_CMD_STR_MOVE_TO_CARTRIDGE_RETRACT), "%f", &offset_mm) != 1 || offset_mm < 0) {
		sendToPC("Err: Invalid offset for MOVE_TO_CARTRIDGE_RETRACT."); return;
	}
	
	fullyResetActiveDispenseOperation(injector);
	injector->feedState = FEED_MOVING_TO_RETRACT;
	injector->feedingDone = false;

	const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
	long offset_steps = (long)(offset_mm * current_steps_per_mm);
	long target_absolute_axis_position = cartridgeHomeReferenceSteps + offset_steps;

	char response[150];
	snprintf(response, sizeof(response), "Cmd: Move to Cart Retract (Offset: %.1fmm, Target: %ld steps)", offset_mm, target_absolute_axis_position);
	sendToPC(response);

	long current_axis_pos = ConnectorM0.PositionRefCommanded();
	long steps_to_move_axis = target_absolute_axis_position - current_axis_pos;

	moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	FEED_GOTO_TORQUE_PERCENT, FEED_GOTO_VELOCITY_SPS, FEED_GOTO_ACCEL_SPS2);
}


void handleInjectMove(const char *msg, Injector *injector) {
	if (injector->mainState != FEED_MODE) {
		sendToPC("INJECT ignored: Not in FEED_MODE.");
		return;
	}
	if (checkInjectorMoving() || injector->active_dispense_INJECTION_ongoing) {
		sendToPC("Error: Operation already in progress or motors busy.");
		injector->errorState = ERROR_INVALID_INJECTION; // Set an error state
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2_param, steps_per_ml_val, torque_percent;
	// Format: INJECT_MOVE <volume_ml> <speed_ml_s> <feed_accel_sps2> <feed_steps_per_ml> <feed_torque_percent>
	if (sscanf(msg + strlen(USER_CMD_STR_INJECT_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration_sps2_param, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!motorsAreEnabled) { sendToPC("INJECT blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0.0001f) { sendToPC("Error: Steps/ml must be positive and non-zero."); return; }
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f; // Default
		if (volume_ml <= 0) { sendToPC("Error: Inject volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendToPC("Error: Inject speed must be positive."); return; }
		if (acceleration_sps2_param <= 0) { sendToPC("Error: Inject acceleration must be positive."); return; }

		// Clear previous operation's trackers and set last completed to 0 for this new context
		fullyResetActiveDispenseOperation(injector);
		injector->last_completed_dispense_ml = 0.0f;

		injector->feedState = FEED_INJECT_STARTING; // Will transition to ACTIVE in main loop
		injector->feedingDone = false; // Mark that a feed operation is starting
		injector->active_dispense_INJECTION_ongoing = true;
		injector->active_op_target_ml = volume_ml;
		injector->active_op_steps_per_ml = steps_per_ml_val;
		injector->active_op_total_target_steps = (long)(volume_ml * injector->active_op_steps_per_ml);
		injector->active_op_remaining_steps = injector->active_op_total_target_steps;
		
		// Capture motor position at the very start of the entire multi-segment operation
		injector->active_op_initial_axis_steps = ConnectorM0.PositionRefCommanded();
		// Also set for the first segment
		injector->active_op_segment_initial_axis_steps = injector->active_op_initial_axis_steps;
		
		injector->active_op_total_dispensed_ml = 0.0f; // Explicitly reset for the new operation

		injector->active_op_velocity_sps = (int)(speed_ml_s * injector->active_op_steps_per_ml);
		injector->active_op_accel_sps2 = (int)acceleration_sps2_param; // Use param directly
		injector->active_op_torque_percent = (int)torque_percent;
		if (injector->active_op_velocity_sps <= 0) injector->active_op_velocity_sps = 100; // Fallback velocity

		char response[200];
		snprintf(response, sizeof(response), "RX INJECT: Vol:%.2fml, Speed:%.2fml/s (calc_vel_sps:%d), Acc:%.0f, Steps/ml:%.2f, Tq:%.0f%%",
		volume_ml, speed_ml_s, injector->active_op_velocity_sps, acceleration_sps2_param, steps_per_ml_val, torque_percent);
		sendToPC(response);
		sendToPC("Starting Inject operation...");

		// Positive steps for inject/purge (assuming forward motion dispenses)
		moveInjectorMotors(injector->active_op_remaining_steps, injector->active_op_remaining_steps,
		injector->active_op_torque_percent, injector->active_op_velocity_sps, injector->active_op_accel_sps2);
		// injector->feedState will be set to FEED_INJECT_ACTIVE in main.cpp loop once motion starts
		} else {
		sendToPC("Invalid INJECT_MOVE format. Expected 5 params.");
	}
}

void handlePurgeMove(const char *msg, Injector *injector) {
	if (injector->mainState != FEED_MODE) { sendToPC("PURGE ignored: Not in FEED_MODE."); return; }
	if (checkInjectorMoving() || injector->active_dispense_INJECTION_ongoing) {
		sendToPC("Error: Operation already in progress or motors busy.");
		injector->errorState = ERROR_INVALID_INJECTION;
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2_param, steps_per_ml_val, torque_percent;
	// Format: PURGE_MOVE <volume_ml> <speed_ml_s> <feed_accel_sps2> <feed_steps_per_ml> <feed_torque_percent>
	if (sscanf(msg + strlen(USER_CMD_STR_PURGE_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration_sps2_param, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!motorsAreEnabled) { sendToPC("PURGE blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0.0001f) { sendToPC("Error: Steps/ml must be positive and non-zero."); return;}
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f; // Default
		if (volume_ml <= 0) { sendToPC("Error: Purge volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendToPC("Error: Purge speed must be positive."); return; }
		if (acceleration_sps2_param <= 0) { sendToPC("Error: Purge acceleration must be positive."); return; }

		// Clear previous operation's trackers and set last completed to 0 for this new context
		fullyResetActiveDispenseOperation(injector);
		injector->last_completed_dispense_ml = 0.0f;

		injector->feedState = FEED_PURGE_STARTING; // Will transition to ACTIVE in main loop
		injector->feedingDone = false;
		injector->active_dispense_INJECTION_ongoing = true;
		injector->active_op_target_ml = volume_ml;
		injector->active_op_steps_per_ml = steps_per_ml_val;
		injector->active_op_total_target_steps = (long)(volume_ml * injector->active_op_steps_per_ml);
		injector->active_op_remaining_steps = injector->active_op_total_target_steps;
		
		injector->active_op_initial_axis_steps = ConnectorM0.PositionRefCommanded();
		injector->active_op_segment_initial_axis_steps = injector->active_op_initial_axis_steps;
		
		injector->active_op_total_dispensed_ml = 0.0f; // Explicitly reset

		injector->active_op_velocity_sps = (int)(speed_ml_s * injector->active_op_steps_per_ml);
		injector->active_op_accel_sps2 = (int)acceleration_sps2_param;
		injector->active_op_torque_percent = (int)torque_percent;
		if (injector->active_op_velocity_sps <= 0) injector->active_op_velocity_sps = 200; // Fallback

		char response[200];
		snprintf(response, sizeof(response), "RX PURGE: Vol:%.2fml, Speed:%.2fml/s (calc_vel_sps:%d), Acc:%.0f, Steps/ml:%.2f, Tq:%.0f%%",
		volume_ml, speed_ml_s, injector->active_op_velocity_sps, acceleration_sps2_param, steps_per_ml_val, torque_percent);
		sendToPC(response);
		sendToPC("Starting Purge operation...");
		
		moveInjectorMotors(injector->active_op_remaining_steps, injector->active_op_remaining_steps,
		injector->active_op_torque_percent, injector->active_op_velocity_sps, injector->active_op_accel_sps2);
		// injector->feedState will be set to FEED_PURGE_ACTIVE in main.cpp loop once motion starts
		} else {
		sendToPC("Invalid PURGE_MOVE format. Expected 5 params.");
	}
}

void handlePauseOperation(Injector *injector) {
	if (!injector->active_dispense_INJECTION_ongoing) {
		sendToPC("PAUSE ignored: No active inject/purge operation.");
		return;
	}

	if (injector->feedState == FEED_INJECT_ACTIVE || injector->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Cmd: Pausing operation...");
		ConnectorM0.MoveStopDecel();
		ConnectorM1.MoveStopDecel();

		// Immediately set paused state, main loop will finalize steps
		if (injector->feedState == FEED_INJECT_ACTIVE) injector->feedState = FEED_INJECT_PAUSED;
		if (injector->feedState == FEED_PURGE_ACTIVE) injector->feedState = FEED_PURGE_PAUSED;
		} else {
		sendToPC("PAUSE ignored: Not in an active inject/purge state.");
	}
}


void handleResumeOperation(Injector *injector) {
	if (!injector->active_dispense_INJECTION_ongoing) {
		sendToPC("RESUME ignored: No operation was ongoing or paused.");
		return;
	}
	if (injector->feedState == FEED_INJECT_PAUSED || injector->feedState == FEED_PURGE_PAUSED) {
		if (injector->active_op_remaining_steps <= 0) {
			sendToPC("RESUME ignored: No remaining volume/steps to dispense.");
			fullyResetActiveDispenseOperation(injector);
			injector->feedState = FEED_STANDBY;
			return;
		}
		sendToPC("Cmd: Resuming operation...");
		injector->active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded(); // Update base for this segment
		injector->feedingDone = false; // New segment starting

		moveInjectorMotors(injector->active_op_remaining_steps, injector->active_op_remaining_steps,
		injector->active_op_torque_percent, injector->active_op_velocity_sps, injector->active_op_accel_sps2);
				
		if(injector->feedState == FEED_INJECT_PAUSED) injector->feedState = FEED_INJECT_RESUMING; // Or directly to _ACTIVE
		if(injector->feedState == FEED_PURGE_PAUSED) injector->feedState = FEED_PURGE_RESUMING;  // Or directly to _ACTIVE
		// main.cpp will transition from RESUMING to ACTIVE once motors are moving, or directly here.
		// For simplicity let's assume it goes active if move command is successful.
		if(injector->feedState == FEED_INJECT_RESUMING) injector->feedState = FEED_INJECT_ACTIVE;
		if(injector->feedState == FEED_PURGE_RESUMING) injector->feedState = FEED_PURGE_ACTIVE;


		} else {
		sendToPC("RESUME ignored: Operation not paused.");
	}
}

void handleCancelOperation(Injector *injector) {
	if (!injector->active_dispense_INJECTION_ongoing) {
		sendToPC("CANCEL ignored: No active inject/purge operation to cancel.");
		return;
	}

	sendToPC("Cmd: Cancelling current feed operation...");
	abortInjectorMove(); // Stop motors abruptly
	Delay_ms(100); // Give motors a moment to fully stop if needed

	// Even though cancelled, calculate what was dispensed before reset for logging/internal use if needed.
	// However, for the purpose of telemetry display when idle, a cancelled op means 0 "completed" dispense.
	if (injector->active_op_steps_per_ml > 0.0001f && injector->active_dispense_INJECTION_ongoing) {
		long steps_moved_on_axis = ConnectorM0.PositionRefCommanded() - injector->active_op_segment_initial_axis_steps;
		float segment_dispensed_ml = (float)fabs(steps_moved_on_axis) / injector->active_op_steps_per_ml;
		injector->active_op_total_dispensed_ml += segment_dispensed_ml;
		// This active_op_total_dispensed_ml will be reset by fullyResetActiveDispenseOperation shortly.
		// If you needed to log the actual amount dispensed before cancel, do it here.
	}
	
	injector->last_completed_dispense_ml = 0.0f; // Set "completed" amount to 0 for a cancelled operation

	fullyResetActiveDispenseOperation(injector); // Clears all active_op_ trackers
	
	injector->feedState = FEED_INJECTION_CANCELLED; // A specific state for cancelled
	// Main loop should transition this to FEED_STANDBY
	injector->feedingDone = true; // Mark as "done" (via cancellation)
	injector->errorState = ERROR_NONE; // Typically, a user cancel is not an "error" condition.
	// If it should be, set injector->errorState = ERROR_MANUAL_ABORT;

	sendToPC("Operation Cancelled. Dispensed value for this attempt will show as 0 ml in idle telemetry.");
}




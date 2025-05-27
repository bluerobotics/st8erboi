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
#define CMD_STR_RETRACT_MOVE         "RETRACT_MOVE "

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

void checkUdpBuffer(SystemStates *states)
{
	memset(packetBuffer, 0, MAX_PACKET_LENGTH);
	uint16_t packetSize = Udp.PacketParse();
	if (packetSize > 0) {
		int32_t bytesRead = Udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
		packetBuffer[bytesRead] = '\0';
		handleMessage((char *)packetBuffer, states);
	}
}

void sendTelem(SystemStates *states) {
	static uint32_t lastTelemTime = 0;
	uint32_t now = Milliseconds();

	if (!terminalDiscovered || (now - lastTelemTime < telemInterval)) {
		return;
	}
	lastTelemTime = now;

	// Get fresh motor data
	float displayTorque1 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue1, &firstTorqueReading1);
	float displayTorque2 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue2, &firstTorqueReading2);
	int hlfb1 = (int)ConnectorM0.HlfbState();
	int hlfb2 = (int)ConnectorM1.HlfbState();
	long pos_cmd1_val = ConnectorM0.PositionRefCommanded(); // Use long for consistency with snprintf %ld
	long pos_cmd2_val = ConnectorM1.PositionRefCommanded();

	char torque1Str[16], torque2Str[16];
	if (displayTorque1 == TORQUE_SENTINEL_INVALID_VALUE) {
		strcpy(torque1Str, "---");
		} else {
		snprintf(torque1Str, sizeof(torque1Str), "%.2f", displayTorque1);
	}
	if (displayTorque2 == TORQUE_SENTINEL_INVALID_VALUE) {
		strcpy(torque2Str, "---");
		} else {
		snprintf(torque2Str, sizeof(torque2Str), "%.2f", displayTorque2);
	}

	// Increased buffer size for additional state strings
	char msg[350];
	snprintf(msg, sizeof(msg),
	"MAIN_STATE: %s, HOMING_STATE: %s, FEED_STATE: %s, ERROR_STATE: %s, "
	"torque1: %s, hlfb1: %d, enabled1: %d, pos_cmd1: %ld, "
	"torque2: %s, hlfb2: %d, enabled2: %d, pos_cmd2: %ld, "
	"machine_steps: %ld, cartridge_steps: %ld",
	states->mainStateStr(),    // Uses method from SystemStates class
	states->homingStateStr(),  // Uses method for HomingState
	states->feedStateStr(),    // Uses method for FeedState
	states->errorStateStr(),   // Uses method for ErrorState
	torque1Str,
	hlfb1,
	motorsAreEnabled ? 1 : 0,  // Assuming motorsAreEnabled is still the global flag
	pos_cmd1_val,
	torque2Str,
	hlfb2,
	motorsAreEnabled ? 1 : 0,  // Both motors share the same enabled status from this flag
	pos_cmd2_val,
	(long)machineStepCounter,  // Cast to long just to be safe with %ld
	(long)cartridgeStepCounter // Cast to long
	);
	sendToPC(msg);
}

MessageCommand parseMessageCommand(const char *msg) {
	if (strncmp(msg, CMD_STR_DISCOVER_TELEM, strlen(CMD_STR_DISCOVER_TELEM)) == 0) return CMD_DISCOVER_TELEM;

	if (strcmp(msg, CMD_STR_ENABLE) == 0) return CMD_ENABLE;
	if (strcmp(msg, CMD_STR_DISABLE) == 0) return CMD_DISABLE;
	if (strcmp(msg, CMD_STR_ABORT) == 0) return CMD_ABORT;

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
	if (strncmp(msg, CMD_STR_RETRACT_MOVE, strlen(CMD_STR_RETRACT_MOVE)) == 0) return CMD_RETRACT_MOVE;

	return CMD_UNKNOWN;
}

void handleMessage(const char *msg, SystemStates *states) {
	MessageCommand command = parseMessageCommand(msg); // Use the corrected parser from above

	switch (command) {
		// --- System & Connection Commands ---
		case CMD_DISCOVER_TELEM:        handleDiscoveryTelemPacket(msg, Udp.RemoteIp(), states); break;
		case CMD_ENABLE:                handleEnable(states); break;
		case CMD_DISABLE:               handleDisable(states); break;
		case CMD_ABORT:                 handleAbort(states); break;

		// --- Mode Setting Commands ---
		case CMD_STANDBY_MODE:          handleStandbyMode(states); break; // Ensure this handler is correctly defined
		case CMD_TEST_MODE:             handleTestMode(states); break;    // Ensure this handler is correctly defined
		case CMD_JOG_MODE:              handleJogMode(states); break;
		case CMD_HOMING_MODE:           handleHomingMode(states); break;
		case CMD_FEED_MODE:             handleFeedMode(states); break;

		// --- Parameter Setting Commands ---
		case CMD_SET_TORQUE_OFFSET:     handleSetTorqueOffset(msg); break; // This handler is in your uploaded messages.cpp

		// --- Jog Operation Commands ---
		case CMD_JOG_MOVE:              handleJogMove(msg, states); break; // You'll need to create/update this handler

		// --- Homing Operation Commands ---
		case CMD_MACHINE_HOME_MOVE:     handleMachineHomeMove(msg, states); break;  // You'll need to create/update this handler
		case CMD_CARTRIDGE_HOME_MOVE:   handleCartridgeHomeMove(msg, states); break; // You'll need to create/update this handler

		// --- Feed Operation Commands ---
		case CMD_INJECT_MOVE:           handleInjectMove(msg, states); break; // You'll need to create/update this handler
		case CMD_PURGE_MOVE:            handlePurgeMove(msg, states); break;  // You'll need to create/update this handler
		case CMD_RETRACT_MOVE:          handleRetractMove(msg, states); break;// You'll need to create/update this handler

		case CMD_UNKNOWN:
		default:
		char unknownCmdMsg[128];
		int msg_len = strlen(msg);
		int max_len_to_copy = (sizeof(unknownCmdMsg) - strlen("Unknown cmd: ''") - 1);
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
	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration, retract_mm, torque_percent;
	// Format: <CMD_STR> <stroke_mm> <rapid_vel_mm_s> <touch_vel_mm_s> <retract_dist_mm> <torque_%>
	
	if (sscanf(msg + strlen(CMD_STR_MACHINE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration, &retract_mm, &torque_percent) == 6) {
		
		if(states->homingMachineDone == false)
		states->homingState = HOMING_MACHINE;
		states->homingMachineDone = false;

		char response[200];
		snprintf(response, sizeof(response), "MACHINE_HOME_MOVE RX: Strk:%.1f RpdV:%.1f TchV:%.1f Ret:%.1f Tq:%.1f%%",
		stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, retract_mm, torque_percent);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("MACHINE_HOME_MOVE blocked: Motors disabled.");
			states->homingState = HOMING_NONE;
			return;
		}
		if (torque_percent <=0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Machine Home. Using default 20%.");
			torque_percent = 20.0f;
		}

		long stroke_steps = (long)(stroke_mm * STEPS_PER_MM_CONST);
		int rapid_sps = (int)(rapid_vel_mm_s * STEPS_PER_MM_CONST);
		int touch_sps = (int)(touch_vel_mm_s * STEPS_PER_MM_CONST);
		// long retract_steps = (long)(retract_mm * STEPS_PER_MM_CONST); // This will be used after touch-off

		// Placeholder for actual machine homing sequence (typically downwards, so negative steps for M0/M1)
		// This is a simplified representation. Real homing is more complex:
		// 1. Rapid move towards endstop (e.g., -stroke_steps) with rapid_sps.
		// 2. On torque spike (using torque_percent as the limit for checkTorqueLimit), stop.
		// 3. Slowly back off a bit.
		// 4. Slowly approach again with touch_sps.
		// 5. On second torque spike/limit switch, stop and set machineStepCounter = 0.
		// 6. Retract by retract_steps.
		// For now, just a single move command as a placeholder.
		sendToPC("Initiating Machine Homing (conceptual move)...");
		// Example: Assume M0 and M1 move together for machine home
		moveMotors(-stroke_steps, -stroke_steps, (int)torque_percent, rapid_sps, acceleration); // Using global accelerationLimit

		// Actual completion and setting machineStepCounter = 0 would be handled
		// in main.cpp based on motor status and states->onHomingMachineDone().
		
		} else {
		sendToPC("Invalid MACHINE_HOME_MOVE format. Expected 5 parameters.");
	}
}

void handleCartridgeHomeMove(const char *msg, SystemStates *states) {
	if (states->mainState != HOMING_MODE) {
		sendToPC("CARTRIDGE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration, retract_mm, torque_percent;
	// Format: <CMD_STR> <stroke_mm> <rapid_vel_mm_s> <touch_vel_mm_s> <retract_dist_mm> <torque_%>
	if (sscanf(msg + strlen(CMD_STR_CARTRIDGE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration, &retract_mm, &torque_percent) == 6) {

		states->homingState = HOMING_CARTRIDGE;
		states->homingCartridgeDone = false;

		char response[200];
		snprintf(response, sizeof(response), "CARTRIDGE_HOME_MOVE RX: Strk:%.1f RpdV:%.1f TchV:%.1f Ret:%.1f Tq:%.1f%%",
		stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, retract_mm, torque_percent);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("CARTRIDGE_HOME_MOVE blocked: Motors disabled.");
			states->homingState = HOMING_NONE;
			return;
		}
		if (torque_percent <=0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Cartridge Home. Using default 20%.");
			torque_percent = 20.0f;
		}

		long stroke_steps = (long)(stroke_mm * STEPS_PER_MM_CONST);
		int rapid_sps = (int)(rapid_vel_mm_s * STEPS_PER_MM_CONST);
		// int touch_sps = (int)(touch_vel_mm_s * STEPS_PER_MM_CONST);
		// long retract_steps = (long)(retract_mm * STEPS_PER_MM_CONST);

		sendToPC("Initiating Cartridge Homing (conceptual move)...");
		// Example: Assume M0 and M1 move together for cartridge home (typically upwards, positive steps)
		moveMotors(stroke_steps, stroke_steps, (int)torque_percent, rapid_sps, acceleration);

		// Actual completion and setting cartridgeStepCounter = 0 in main.cpp
		// based on motor status and states->onHomingCartridgeDone().

		} else {
		sendToPC("Invalid CARTRIDGE_HOME_MOVE format. Expected 5 parameters.");
	}
}

void handleInjectMove(const char *msg, SystemStates *states) {
	if (states->mainState != FEED_MODE) {
		sendToPC("INJECT_MOVE ignored: Not in FEED_MODE.");
		return;
	}
	float volume_ml, speed_ml_s, acceleration, steps_per_ml_val, torque_percent;
	// Format: <CMD_STR> <volume_ml> <speed_ml_s> <steps_per_ml_calc> <torque_%>
	if (sscanf(msg + strlen(CMD_STR_INJECT_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration, &steps_per_ml_val, &torque_percent) == 5) {
		
		states->feedState = FEED_INJECT;
		states->feedingDone = false;

		char response[200];
		snprintf(response, sizeof(response), "INJECT_MOVE RX: Vol:%.3fml, Speed:%.3fml/s, Steps/ml:%.2f, Tq:%.1f%%",
		volume_ml, speed_ml_s, steps_per_ml_val, torque_percent);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("INJECT_MOVE blocked: Motors disabled.");
			states->feedState = FEED_STANDBY; return;
		}
		if (steps_per_ml_val <= 0) {
			sendToPC("Error: Steps/ml must be positive for INJECT_MOVE.");
			states->feedState = FEED_STANDBY; return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Inject. Using default 15%.");
			torque_percent = 15.0f;
		}


		long total_steps = (long)(volume_ml * steps_per_ml_val);
		int feed_velocity_sps = (int)(speed_ml_s * steps_per_ml_val);
		if (feed_velocity_sps <=0) {
			sendToPC("Warning: Calculated feed velocity is zero or negative. Using default 100sps.");
			feed_velocity_sps = 100;
		}
		
		sendToPC("Initiating Inject move...");
		// Assuming inject moves both motors symmetrically
		moveMotors(total_steps, total_steps, (int)torque_percent, feed_velocity_sps, acceleration);
		// Main loop should monitor for completion and call states->onFeedingDone()
		// and then transition states->feedState back to FEED_STANDBY.

		} else {
		sendToPC("Invalid INJECT_MOVE format. Expected 4 parameters.");
	}
}

void handlePurgeMove(const char *msg, SystemStates *states) {
	if (states->mainState != FEED_MODE) {
		sendToPC("PURGE_MOVE ignored: Not in FEED_MODE.");
		return;
	}
	float volume_ml, speed_ml_s, acceleration, steps_per_ml_val, torque_percent;
	if (sscanf(msg + strlen(CMD_STR_PURGE_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration, &steps_per_ml_val, &torque_percent) == 5) {

		states->feedState = FEED_PURGE;
		states->feedingDone = false;

		char response[200];
		snprintf(response, sizeof(response), "PURGE_MOVE RX: Vol:%.3fml, Speed:%.3fml/s, Steps/ml:%.2f, Tq:%.1f%%",
		volume_ml, speed_ml_s, steps_per_ml_val, torque_percent);
		sendToPC(response);
		
		if (!motorsAreEnabled) {
			sendToPC("PURGE_MOVE blocked: Motors disabled.");
			states->feedState = FEED_STANDBY; return;
		}
		if (steps_per_ml_val <= 0) {
			sendToPC("Error: Steps/ml must be positive for PURGE_MOVE.");
			states->feedState = FEED_STANDBY; return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Purge. Using default 15%.");
			torque_percent = 15.0f;
		}

		long total_steps = (long)(volume_ml * steps_per_ml_val);
		int purge_velocity_sps = (int)(speed_ml_s * steps_per_ml_val);
		if (purge_velocity_sps <=0) {
			sendToPC("Warning: Calculated purge velocity is zero or negative. Using default 200sps.");
			purge_velocity_sps = 200;
		}

		sendToPC("Initiating Purge move...");
		moveMotors(total_steps, total_steps, (int)torque_percent, purge_velocity_sps, acceleration);
		} else {
		sendToPC("Invalid PURGE_MOVE format. Expected 4 parameters.");
	}
}

void handleRetractMove(const char *msg, SystemStates *states) {
	if (states->mainState != FEED_MODE) {
		sendToPC("RETRACT_MOVE ignored: Not in FEED_MODE.");
		return;
	}
	float volume_ml, speed_ml_s, steps_per_ml_val, torque_percent, acceleration;
	if (sscanf(msg + strlen(CMD_STR_RETRACT_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &steps_per_ml_val, &torque_percent, &acceleration) == 5) {

		states->feedState = FEED_RETRACT;
		states->feedingDone = false;

		char response[200];
		snprintf(response, sizeof(response), "RETRACT_MOVE RX: Vol:%.3fml, Speed:%.3fml/s, Steps/ml:%.2f, Tq:%.1f%%",
		volume_ml, speed_ml_s, steps_per_ml_val, torque_percent);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("RETRACT_MOVE blocked: Motors disabled.");
			states->feedState = FEED_STANDBY; return;
		}
		if (steps_per_ml_val <= 0) {
			sendToPC("Error: Steps/ml must be positive for RETRACT_MOVE.");
			states->feedState = FEED_STANDBY; return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Retract. Using default 15%.");
			torque_percent = 15.0f;
		}

		long total_steps = - (long)(volume_ml * steps_per_ml_val); // Negative steps for retraction
		int retract_velocity_sps = (int)(speed_ml_s * steps_per_ml_val);
		if (retract_velocity_sps <=0) {
			sendToPC("Warning: Calculated retract velocity is zero or negative. Using default 200sps.");
			retract_velocity_sps = 200;
		}
		
		sendToPC("Initiating Retract move...");
		moveMotors(total_steps, total_steps, (int)torque_percent, retract_velocity_sps, acceleration);
		} else {
		sendToPC("Invalid RETRACT_MOVE format. Expected 4 parameters.");
	}
}




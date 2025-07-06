#include "fillhead.h"

void Fillhead::handleAbort() {
	abortAllMoves();
	state = STATE_STANDBY;
	activeMotor1 = nullptr;
	activeMotor2 = nullptr;
	sendStatus(STATUS_PREFIX_INFO, "ABORT Received. All motion stopped.");
}

void Fillhead::handleSetPeerIp(const char* msg) {
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

void Fillhead::handleClearPeerIp() {
	peerDiscovered = false;
	sendStatus(STATUS_PREFIX_INFO, "Peer IP cleared");
}

// --- REPLACED FUNCTION ---
// This function is now updated to accept all units in mm, mm/s, and mm/s^2.
void Fillhead::handleMove(const char* msg) {
	if (state != STATE_STANDBY) {
		sendStatus(STATUS_PREFIX_ERROR, "Cannot start move: Not in STANDBY");
		return;
	}

	// --- MODIFIED: Arguments now accept float for all motion parameters ---
	float distance_mm, vel_mms, accel_mms2;
	int torque;
	long steps;
	int velocity_sps, accel_sps2;
	float steps_per_mm = 0;

	FillheadCommand cmd = parseCommand(msg);
	const char* args = strchr(msg, ' ');

	// --- MODIFIED: Parsing float for vel and accel, and added accel to the format ---
	if (!args || sscanf(args, "%f %f %f %d", &distance_mm, &vel_mms, &accel_mms2, &torque) != 4) {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid MOVE format. Expected: <dist_mm> <vel_mm_s> <accel_mm_s2> <torque>");
		return;
	}

	// Reset the torque filter before starting a new operation
	firstTorqueReadM0 = true;
	firstTorqueReadM1 = true;
	firstTorqueReadM2 = true;
	firstTorqueReadM3 = true;

	state = STATE_STARTING_MOVE;
	activeMotor1 = nullptr;
	activeMotor2 = nullptr;

	switch(cmd) {
		case CMD_MOVE_X:
		sendStatus(STATUS_PREFIX_INFO, "MOVE_X Received");
		activeMotor1 = &MotorX;
		steps_per_mm = STEPS_PER_MM_X;
		break;
		case CMD_MOVE_Y:
		sendStatus(STATUS_PREFIX_INFO, "MOVE_Y Received");
		activeMotor1 = &MotorY1;
		activeMotor2 = &MotorY2;
		steps_per_mm = STEPS_PER_MM_Y;
		break;
		case CMD_MOVE_Z:
		sendStatus(STATUS_PREFIX_INFO, "MOVE_Z Received");
		activeMotor1 = &MotorZ;
		steps_per_mm = STEPS_PER_MM_Z;
		break;
		default:
		state = STATE_STANDBY;
		return;
	}

	// --- NEW: Convert all mm-based units to step-based units ---
	steps = (long)(distance_mm * steps_per_mm);
	velocity_sps = (int)(vel_mms * steps_per_mm);
	accel_sps2 = (int)(accel_mms2 * steps_per_mm);
	
	if (activeMotor1) moveAxis(activeMotor1, steps, velocity_sps, accel_sps2, torque);
	if (activeMotor2) moveAxis(activeMotor2, steps, velocity_sps, accel_sps2, torque);
}


// --- REPLACED FUNCTION ---
// This function now uses default speeds in mm/s and converts them to steps/s.
void Fillhead::handleHome(const char* msg) {
	if (state != STATE_STANDBY) {
		sendStatus(STATUS_PREFIX_ERROR, "Cannot start home: Not in STANDBY");
		return;
	}

	int torque;
	float max_dist_mm;
	float steps_per_mm = 0;
	FillheadCommand cmd = parseCommand(msg);
	const char* args = strchr(msg, ' ');

	if (!args || sscanf(args, "%d %f", &torque, &max_dist_mm) != 2) {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid HOME format. Expected: <torque> <max_dist_mm>");
		return;
	}

	firstTorqueReadM0 = true;
	firstTorqueReadM1 = true;
	firstTorqueReadM2 = true;
	firstTorqueReadM3 = true;

	state = STATE_HOMING;
	homingPhase = HOMING_START;
	activeMotor1 = nullptr;
	activeMotor2 = nullptr;

	homingTorque = torque;
	
	// Set default speeds in mm/s
	float rapid_vel_mms = 2.5;  // Corresponds to 1000 steps/s with 400 steps/mm
	float touch_vel_mms = 0.25; // Corresponds to 100 steps/s with 400 steps/mm

	switch (cmd) {
		case CMD_HOME_X:
		sendStatus(STATUS_PREFIX_INFO, "HOME_X Received");
		activeMotor1 = &MotorX;
		steps_per_mm = STEPS_PER_MM_X;
		break;
		case CMD_HOME_Y:
		sendStatus(STATUS_PREFIX_INFO, "HOME_Y Received");
		activeMotor1 = &MotorY1;
		activeMotor2 = &MotorY2;
		steps_per_mm = STEPS_PER_MM_Y;
		break;
		case CMD_HOME_Z:
		sendStatus(STATUS_PREFIX_INFO, "HOME_Z Received");
		activeMotor1 = &MotorZ;
		steps_per_mm = STEPS_PER_MM_Z;
		break;
		default:
		sendStatus(STATUS_PREFIX_INFO, "UNKNOWN HOME CMD");
		state = STATE_STANDBY;
		return;
	}
	
	// Convert mm-based values to step-based values for the driver
	homingDistanceSteps = (long)(max_dist_mm * steps_per_mm);
	homingRapidSps = (int)(rapid_vel_mms * steps_per_mm);
	homingTouchSps = (int)(touch_vel_mms * steps_per_mm);
}
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

void Fillhead::handleMove(const char* msg) {
	if (state != STATE_STANDBY) {
		sendStatus(STATUS_PREFIX_ERROR, "Cannot start move: Not in STANDBY");
		return;
	}

	long steps;
	int velocity, torque;
	FillheadCommand cmd = parseCommand(msg);
	const char* args = strchr(msg, ' ');

	if (!args || sscanf(args, "%ld %d %d", &steps, &velocity, &torque) != 3) {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid MOVE format");
		return;
	}

	switch(cmd) {
		case CMD_MOVE_X: sendStatus(STATUS_PREFIX_INFO, "MOVE_X Received"); break;
		case CMD_MOVE_Y: sendStatus(STATUS_PREFIX_INFO, "MOVE_Y Received"); break;
		case CMD_MOVE_Z: sendStatus(STATUS_PREFIX_INFO, "MOVE_Z Received"); break;
		default: break;
	}

    // --- FIX: Reset the torque filter before starting a new operation ---
    firstTorqueReadM0 = true;
    firstTorqueReadM1 = true;
    firstTorqueReadM2 = true;
    firstTorqueReadM3 = true;
    // --------------------------------------------------------------------

	state = STATE_STARTING_MOVE;
	activeMotor1 = nullptr;
	activeMotor2 = nullptr;

	switch(cmd) {
		case CMD_MOVE_X: activeMotor1 = &MotorX; break;
		case CMD_MOVE_Y: activeMotor1 = &MotorY1; activeMotor2 = &MotorY2; break;
		case CMD_MOVE_Z: activeMotor1 = &MotorZ; break;
		default: state = STATE_STANDBY; return;
	}
	
	if (activeMotor1) moveAxis(activeMotor1, steps, velocity, 200000, torque);
	if (activeMotor2) moveAxis(activeMotor2, steps, velocity, 200000, torque);
}

void Fillhead::handleHome(const char* msg) {
	if (state != STATE_STANDBY) {
		sendStatus(STATUS_PREFIX_ERROR, "Cannot start home: Not in STANDBY");
		return;
	}
	
	int torque, rapid_sps, touch_sps;
    long distance_steps;
	FillheadCommand cmd = parseCommand(msg);
	const char* args = strchr(msg, ' ');

	if (!args || sscanf(args, "%d %d %d %ld", &torque, &rapid_sps, &touch_sps, &distance_steps) != 4) {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid HOME format. Expected: <torque> <rapid_sps> <touch_sps> <distance_steps>");
		return;
	}
	
    // Reset the torque filter before starting the new operation.
    firstTorqueReadM0 = true;
    firstTorqueReadM1 = true;
    firstTorqueReadM2 = true;
    firstTorqueReadM3 = true;

	state = STATE_HOMING;
	homingPhase = HOMING_START;
	homingTorque = torque;
	homingRapidSps = rapid_sps;
	homingTouchSps = touch_sps;
    homingDistanceSteps = distance_steps;
	activeMotor1 = nullptr;
	activeMotor2 = nullptr;

    // --- CORRECTED LOGIC ---
    // A single switch statement now handles sending the status AND setting the
    // active motors. This prevents the state from being accidentally reset.
	switch(cmd) {
		case CMD_HOME_X:
            sendStatus(STATUS_PREFIX_INFO, "HOME_X Received");
            activeMotor1 = &MotorX;
            break;
		case CMD_HOME_Y:
            sendStatus(STATUS_PREFIX_INFO, "HOME_Y Received");
            activeMotor1 = &MotorY1;
            activeMotor2 = &MotorY2;
            break;
		case CMD_HOME_Z:
            sendStatus(STATUS_PREFIX_INFO, "HOME_Z Received");
            activeMotor1 = &MotorZ;
            break;
		default:
            // If the command is somehow unknown, revert to standby and exit.
            sendStatus(STATUS_PREFIX_INFO, "UNKNOWN HOME CMD");
			state = STATE_STANDBY;
            return;
	}
}
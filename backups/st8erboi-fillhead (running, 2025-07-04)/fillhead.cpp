#include "fillhead.h"

Fillhead::Fillhead() {
	guiDiscovered = false;
	guiPort = 0;
	peerDiscovered = false;
	
	homedX = false;
	homedY1 = false;
	homedY2 = false;
	homedZ = false;

	state = STATE_STANDBY;
	homingPhase = HOMING_IDLE;
	activeMotor1 = nullptr;
	activeMotor2 = nullptr;

	torqueLimit = 20.0f;
	torqueOffset = 0.0f;
	firstTorqueReadM0 = true;
	firstTorqueReadM1 = true;
	firstTorqueReadM2 = true;
	firstTorqueReadM3 = true;
	smoothedTorqueM0 = 0.0f;
	smoothedTorqueM1 = 0.0f;
	smoothedTorqueM2 = 0.0f;
	smoothedTorqueM3 = 0.0f;
}

void Fillhead::setup() {
	setupUsbSerial();
	setupEthernet();
	setupMotors();
}

const char* Fillhead::stateToString() {
	switch(state) {
		case STATE_STANDBY: return "STANDBY";
		case STATE_STARTING_MOVE: return "STARTING_MOVE";
		case STATE_MOVING:  return "MOVING";
		case STATE_HOMING:  return "HOMING";
		default:            return "UNKNOWN";
	}
}
#include "fillhead.h"

// Generic handler for MOVE commands
void Fillhead::handleMove(MotorDriver* motor, const char* msg) {
	if (isMoving()) return; // Ignore new move commands while busy

	long steps;
	int velocity;
	
	// Find the first space to parse arguments after it
	const char* args = strchr(msg, ' ');
	if (args && sscanf(args, "%ld %d", &steps, &velocity) == 2) {
		motor->VelMax(velocity);
		motor->Move(steps);
	}
}


// --- MODIFIED: Corrected to use software polling for torque, matching original Injector pattern ---
// Generic handler for HOME commands
void Fillhead::handleHome(MotorDriver* motor, bool* homedFlag) {
	if (isMoving()) return; // Ignore new move commands while busy

	// 1. Set a moderate homing velocity
	motor->VelMax(5000);

	// 2. Command a long-distance move in the negative direction.
	// We will monitor and stop this move manually based on torque.
	motor->Move(-200000);
	
	// 3. Poll the motor status until the move is no longer active.
	while(motor->StatusReg().bit.StepsActive) {
		// Check if the torque limit has been exceeded
		if (motor->HlfbPercent() > 15.0f) { // 15% torque limit
			motor->MoveStopAbrupt();
			break; // Exit the while loop
		}
		Delay_ms(5); // Small delay to prevent hogging the processor
	}
	
	// 4. The motor has stopped. Set this position as the new zero.
	motor->PositionRefSet(0);
	
	*homedFlag = true;
}
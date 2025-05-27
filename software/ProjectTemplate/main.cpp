#include "ClearCore.h"
#include "states.h"
#include "motor.h"
#include "messages.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
	SystemStates states;

	// --- Hardware and protocol initialization ---
	//SetupUsbSerial();
	SetupEthernet();
	SetupMotors();
	
	uint32_t now = Milliseconds();
	uint32_t lastMotorTime = now;
	uint32_t motorInterval = 2000;
	int motorFlip = 1;

	// --- Main application loop ---
	while (true)
	{
		checkUdpBuffer(&states);
		sendTelem(&states);
		now = Milliseconds();

		switch (states.mainState)
		{
			case STANDBY_MODE:
			// Idle, do nothing
			break;
			
			case TEST_MODE:
				if (checkTorqueLimit() == true){
					abortMove();
					states.mainState = STANDBY_MODE;
					states.errorState = ERROR_TORQUE_ABORT;
				}
				if (now - lastMotorTime > motorInterval) {
					moveMotors(motorFlip*800, motorFlip*800, 2, 800, 5000);  //void moveMotors(int stepsM0, int stepsM1, int torque_limit, int velocity, int accel)
					motorFlip = motorFlip*-1;
					lastMotorTime = now;
				}
			break;

			case HOMING_MODE:
				// Do nothing until another command is received
				switch (states.homingState){
					case HOMING_CARTRIDGE:
						if(states.homingCartridgeDone == true){
							//zero the cartridge home step counter
							states.homingCartridgeDone == false;
							states.homingState = HOMING_NONE;
						}
					break;
					case HOMING_MACHINE:
						if(states.homingMachineDone == true){
							//zero the cartridge home step counter
							states.homingCartridgeDone == false;
							states.homingState = HOMING_NONE;
						}
					break;
				}
			break;
			
			case FEED_MODE:
			switch (states.feedState){
				case FEED_STANDBY:
				// Idle, do nothing
				break;
				case FEED_PURGE:
				// purge cartridge routine
				case FEED_INJECT:
				// inject cartridge routine
				case FEED_RETRACT:
				// feed cartridge routine
				break;
			}
			break;

			case JOG_MODE:
			if (checkTorqueLimit() == true){
				abortMove();
				states.mainState = STANDBY_MODE;
				states.errorState = ERROR_TORQUE_ABORT;
			}
			break;

			case DISABLED_MODE:
			// Only ENABLE command should change state from here
			break;
		}
	}
}

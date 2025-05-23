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

	// --- Main application loop ---
	while (1)
	{
		checkUdpBuffer(&states);

		switch (states.mainState)
		{
			case STANDBY:
			// Idle, do nothing
			break;

			case MOVING:
			checkTorqueLimit(); // <--- Only checked while moving!
			switch (states.movingState)
			{
				case MOVING_JOG:
				if (!checkMoving()) {
					states.jogDone = true;
				}
				break;
				case MOVING_HOMING:
				switch (states.homingState)
				{
					case MOVING_HOMING_MACHINE:
					if (!checkMoving()) {
						states.homingMachineDone = true;
						states.homingState = MOVING_HOMING_CARTRIDGE;
					}
					break;
					case MOVING_HOMING_CARTRIDGE:
					if (!checkMoving()) {
						states.homingCartridgeDone = true;
						states.mainState = STANDBY;
						states.movingState = MOVING_NONE;
						states.homingState = MOVING_HOMING_NONE;
					}
					break;
					default:
					break;
				}
				break;
				case MOVING_FEEDING:
				if (!checkMoving()) {
					states.feedingDone = true;
					states.mainState = STANDBY;
					states.movingState = MOVING_NONE;
				}
				break;
				default:
				break;
			}
			break;

			case ERROR:
			// Wait for UDP command to reset or standby
			break;

			case DISABLED:
			// Only ENABLE command should change state from here
			break;

			default:
			states.mainState = DISABLED;
			break;
		}
	}
}

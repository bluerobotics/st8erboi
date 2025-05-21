#include "ClearCore.h"

#define motor ConnectorM0
#define SerialPort ConnectorUsb
#define baudRate 9600

// Motion parameters
const float ballscrewPitch = 5.0f;               // mm/rev
const float targetMMperSec = 150.0f;              // mm/s
const float targetRPM = targetMMperSec / ballscrewPitch;  // rev/s
const uint32_t PULSES_PER_REV = 6400;
const uint32_t TOTAL_PULSES = PULSES_PER_REV;
const uint32_t STEP_PERIOD_US = (uint32_t)((60.0f * 1e6f) / (PULSES_PER_REV * targetRPM)); // full step cycle
const uint32_t STEP_HIGH_US = 5;
const uint32_t STEP_LOW_US = STEP_PERIOD_US - STEP_HIGH_US;

// Timing
const uint32_t pauseBeforeCCW = 1000; // ms
const uint32_t pauseAfterCCW = 5000;  // ms
const uint32_t torqueIntervalMs = 10;

// Torque filter
const int torqueFilterSize = 10;
float torqueSamples[torqueFilterSize] = {0};
int torqueSampleIndex = 0;
float torqueSum = 0.0;

// State tracking
enum MotionState {
	MOVE_CW,
	PAUSE_BEFORE_CCW,
	MOVE_CCW,
	FINAL_PAUSE
};
MotionState motionState = MOVE_CW;

int32_t softwarePosition = 0;
uint32_t lastTorqueTime = 0;
uint32_t pauseStartTime = 0;
bool jogDir = false;
uint32_t lastJogTime = 0;
const uint32_t jogIntervalMs = 100;

void MoveSteps(bool dir, int steps);
float ReadFilteredTorque();
void OutputTorque();
void JogToKeepTorqueAlive();

int main() {
	// Setup hardware
	MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_A_DIRECT_B_DIRECT);
	SerialPort.Mode(Connector::USB_CDC);
	SerialPort.Speed(baudRate);
	SerialPort.PortOpen();

	motor.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	motor.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	motor.EnableRequest(true);

	// Wait for motor ready
	while (motor.HlfbState() != MotorDriver::HLFB_ASSERTED) {
		Delay_ms(10);
	}

	SerialPort.SendLine("Motor Ready.");
	SerialPort.Send("Speed: ");
	SerialPort.Send(targetMMperSec);
	SerialPort.Send(" mm/s, Step Cycle: ");
	SerialPort.Send(STEP_PERIOD_US);
	SerialPort.SendLine(" us");

	while (true) {
		uint32_t now = Milliseconds();

		// Live torque/position output
		if (now - lastTorqueTime >= torqueIntervalMs) {
			lastTorqueTime = now;
			OutputTorque();
		}

		switch (motionState) {
			case MOVE_CW:
			SerialPort.SendLine("??  1 rev CW");
			motor.MotorInAState(true);
			MoveSteps(true, TOTAL_PULSES);
			motionState = PAUSE_BEFORE_CCW;
			pauseStartTime = Milliseconds();
			break;

			case PAUSE_BEFORE_CCW:
			if (now - pauseStartTime >= pauseBeforeCCW) {
				motionState = MOVE_CCW;
			}
			break;

			case MOVE_CCW:
			SerialPort.SendLine("??  1 rev CCW");
			motor.MotorInAState(false);
			MoveSteps(false, TOTAL_PULSES);
			motionState = FINAL_PAUSE;
			pauseStartTime = Milliseconds();
			break;

			case FINAL_PAUSE:
			JogToKeepTorqueAlive();
			if (now - pauseStartTime >= pauseAfterCCW) {
				motionState = MOVE_CW;
			}
			break;
		}
	}
}

// Blocking pulse generator with clean HIGH/LOW edges
void MoveSteps(bool dir, int steps) {
	motor.MotorInAState(dir);
	for (int i = 0; i < steps; i++) {
		motor.MotorInBState(true);
		Delay_us(STEP_HIGH_US);
		motor.MotorInBState(false);
		Delay_us(STEP_LOW_US);

		softwarePosition += dir ? 1 : -1;

		if (Milliseconds() - lastTorqueTime >= torqueIntervalMs) {
			lastTorqueTime = Milliseconds();
			OutputTorque();
		}
	}
}

// Output filtered torque and software position
void OutputTorque() {
	float torque = ReadFilteredTorque();
	if (torque >= 0.0f && torque <= 100.0f) {
		SerialPort.Send("Torque: ");
		SerialPort.Send(torque);
		SerialPort.Send(" %, Pos: ");
		SerialPort.Send(softwarePosition);
		SerialPort.SendLine(" counts");
		} else {
		SerialPort.SendLine("Torque: Unavailable");
	}
}

// Apply 10-sample torque smoothing
float ReadFilteredTorque() {
	float raw = motor.HlfbPercent();

	if (raw < 0.0f || raw > 100.0f) {
		return torqueSum / torqueFilterSize;
	}

	torqueSum -= torqueSamples[torqueSampleIndex];
	torqueSamples[torqueSampleIndex] = raw;
	torqueSum += raw;
	torqueSampleIndex = (torqueSampleIndex + 1) % torqueFilterSize;

	return torqueSum / torqueFilterSize;
}

// Jog 1 step back/forth to maintain HLFB torque feedback
void JogToKeepTorqueAlive() {
	if (Milliseconds() - lastJogTime >= jogIntervalMs) {
		jogDir = !jogDir;
		motor.MotorInAState(jogDir);
		motor.MotorInBState(true);
		Delay_us(STEP_HIGH_US);
		motor.MotorInBState(false);
		Delay_us(STEP_LOW_US);
		softwarePosition += jogDir ? 1 : -1;
		lastJogTime = Milliseconds();
	}
}

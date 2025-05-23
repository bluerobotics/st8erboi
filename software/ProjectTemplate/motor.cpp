#include "motor.h"
#include "messages.h"

// --- Module globals ---
float globalTorqueLimit = 2.0f;
float torqueOffset = -2.4f;
float smoothedTorqueValue1 = 0.0f;
float smoothedTorqueValue2 = 0.0f;
bool firstTorqueReading1 = true;
bool firstTorqueReading2 = true;
uint32_t lastTorqueTime = 0;
bool motorsAreEnabled = false;
uint32_t pulsesPerRev = 800;
int32_t machineStepCounter = 0;
int32_t cartridgeStepCounter = 0;

// Extern sendToPC from elsewhere in your project
extern void sendToPC(const char *msg);

void SetupMotors(void)
{
	MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);
	ConnectorM0.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	ConnectorM0.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	ConnectorM0.VelMax(INT32_MAX);
	ConnectorM0.AccelMax(INT32_MAX);
	ConnectorM0.EnableRequest(true);

	ConnectorM1.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	ConnectorM1.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	ConnectorM1.VelMax(INT32_MAX);
	ConnectorM1.AccelMax(INT32_MAX);
	ConnectorM1.EnableRequest(true);

	motorsAreEnabled = true;
	while (ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED ||
	ConnectorM1.HlfbState() != MotorDriver::HLFB_ASSERTED)
	Delay_ms(100);
}

void enableMotors(const char* reason_message)
{
	ConnectorM0.EnableRequest(true);
	ConnectorM1.EnableRequest(true);
	motorsAreEnabled = true;
	while (ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED || ConnectorM1.HlfbState() != MotorDriver::HLFB_ASSERTED)
	Delay_ms(50);
	sendToPC(reason_message);
	smoothedTorqueValue1 = 0.0f;
	smoothedTorqueValue2 = 0.0f;
	firstTorqueReading1 = true;
	firstTorqueReading2 = true;
}

void disableMotors(const char* reason_message)
{
	if (!motorsAreEnabled) return;
	ConnectorM0.EnableRequest(false);
	ConnectorM1.EnableRequest(false);
	motorsAreEnabled = false;
	sendToPC(reason_message);

	smoothedTorqueValue1 = 0.0f;
	smoothedTorqueValue2 = 0.0f;
	firstTorqueReading1 = true;
	firstTorqueReading2 = true;
	Delay_ms(50);
}

void moveMotors(int stepsM0, int stepsM1, int torque_limit, SimpleSpeedState speed, uint8_t motorMask)
{
	if (!motorsAreEnabled) {
		sendToPC("MOVE BLOCKED: Motors are disabled");
		return;
	}

	if (speed == SLOW) {
		if (motorMask & MOTOR_M0) ConnectorM0.EnableTriggerPulse(1, 25, true);
		if (motorMask & MOTOR_M1) ConnectorM1.EnableTriggerPulse(1, 25, true);
		Delay_ms(5);
	}

	globalTorqueLimit = torque_limit;

	if (motorMask & MOTOR_M0) {
		ConnectorM0.Move(stepsM0);
	}
	if (motorMask & MOTOR_M1) {
		ConnectorM1.Move(stepsM1);
	}
	Delay_ms(2);

	char msg[128];
	snprintf(msg, sizeof(msg),
	"MOVE COMMANDED: M0=%ld, M1=%ld, Mask=%02X, Torque=%d, Speed=%s",
	(long)stepsM0, (long)stepsM1, motorMask, torque_limit, speed == FAST ? "FAST" : "SLOW");
	sendToPC(msg);
}

bool checkMoving(void)
{
	if ((ConnectorM0.StepsComplete() && ConnectorM0.HlfbState() == MotorDriver::HLFB_ASSERTED) &&
	(ConnectorM1.StepsComplete() && ConnectorM1.HlfbState() == MotorDriver::HLFB_ASSERTED)) {
		return false; // Both are NOT moving
	}
	return true; // At least one is still moving
}

float getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead)
{
	float currentRawTorque = motor->HlfbPercent();
	if (currentRawTorque == TORQUE_SENTINEL_INVALID_VALUE) {
		return TORQUE_SENTINEL_INVALID_VALUE;
	}
	if (*firstRead) {
		*smoothedValue = currentRawTorque;
		*firstRead = false;
		} else {
		*smoothedValue = EWMA_ALPHA * currentRawTorque + (1.0f - EWMA_ALPHA) * (*smoothedValue);
	}
	float adjusted = *smoothedValue + torqueOffset;
	return adjusted;
}

void checkTorqueLimit(void)
{
	if (motorsAreEnabled && checkMoving()) {
		float currentSmoothedTorque1 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue1, &firstTorqueReading1);
		float currentSmoothedTorque2 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue2, &firstTorqueReading2);
		if ((currentSmoothedTorque1 != TORQUE_SENTINEL_INVALID_VALUE && fabsf(currentSmoothedTorque1) > globalTorqueLimit) ||
		(currentSmoothedTorque2 != TORQUE_SENTINEL_INVALID_VALUE && fabsf(currentSmoothedTorque2) > globalTorqueLimit)) {
			disableMotors("TORQUE LIMIT EXCEEDED");
			enableMotors("MOTORS RE-ENABLED AFTER TORQUE ABORT");
		}
	}
}

void resetMotors(void)
{
	disableMotors("motors reset initiated");
	ConnectorM0.ClearAlerts();
	ConnectorM1.ClearAlerts();
	machineStepCounter = 0;
	cartridgeStepCounter = 0;
	enableMotors("motors reset complete");
}

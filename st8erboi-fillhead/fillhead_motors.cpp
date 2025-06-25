#include "fillhead.h"

void Fillhead::setupMotors() {
	MotorMgr.MotorModeSet(ALL_FILLHEAD_MOTORS, Connector::CPM_MODE_STEP_AND_DIR);

	MotorX.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	MotorX.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	MotorX.VelMax(25000);
	MotorX.AccelMax(100000);
	MotorX.EnableRequest(true);

	MotorY.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	MotorY.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	MotorY.VelMax(25000);
	MotorY.AccelMax(100000);
	MotorY.EnableRequest(true);

	MotorZ.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	MotorZ.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	MotorZ.VelMax(15000);
	MotorZ.AccelMax(80000);
	MotorZ.EnableRequest(true);

	uint32_t timeout = Milliseconds() + 2000;
	while(Milliseconds() < timeout) {
		if (MotorX.StatusReg().bit.Enabled &&
		MotorY.StatusReg().bit.Enabled &&
		MotorZ.StatusReg().bit.Enabled) {
			break;
		}
	}
}

// --- MODIFIED: Replaced MoveActive with StepsActive ---
bool Fillhead::isMoving() {
	return MotorX.StatusReg().bit.StepsActive ||
	MotorY.StatusReg().bit.StepsActive ||
	MotorZ.StatusReg().bit.StepsActive;
}
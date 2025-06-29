#include "fillhead.h"
#include <math.h> // For fabsf()

void Fillhead::setupMotors() {
	MotorMgr.MotorModeSet(ALL_FILLHEAD_MOTORS, Connector::CPM_MODE_STEP_AND_DIR);

	MotorX.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	MotorX.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	MotorX.VelMax(25000);
	MotorX.AccelMax(100000);
	MotorX.EnableRequest(true);

	MotorY1.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	MotorY1.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	MotorY1.VelMax(25000);
	MotorY1.AccelMax(100000);
	MotorY1.EnableRequest(true);

    MotorY2.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	MotorY2.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	MotorY2.VelMax(25000);
	MotorY2.AccelMax(100000);
	MotorY2.EnableRequest(true);

	MotorZ.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	MotorZ.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	MotorZ.VelMax(15000);
	MotorZ.AccelMax(80000);
	MotorZ.EnableRequest(true);

	uint32_t timeout = Milliseconds() + 2000;
	while(Milliseconds() < timeout) {
		if (MotorX.StatusReg().bit.Enabled &&
		    MotorY1.StatusReg().bit.Enabled &&
            MotorY2.StatusReg().bit.Enabled &&
		    MotorZ.StatusReg().bit.Enabled) {
			break;
		}
	}
}

bool Fillhead::isMoving() {
	return MotorX.StatusReg().bit.StepsActive ||
	       MotorY1.StatusReg().bit.StepsActive ||
           MotorY2.StatusReg().bit.StepsActive ||
	       MotorZ.StatusReg().bit.StepsActive;
}

void Fillhead::abortAllMoves() {
    MotorX.MoveStopAbrupt();
    MotorY1.MoveStopAbrupt();
    MotorY2.MoveStopAbrupt();
    MotorZ.MoveStopAbrupt();
}

void Fillhead::moveAxis(MotorDriver* motor, long steps, int vel, int accel, int torque) {
    if (!motor) return;

    torqueLimit = (float)torque;

    motor->VelMax(vel);
    motor->AccelMax(accel);

    motor->Move(steps);
}

float Fillhead::getSmoothedTorque(MotorDriver* motor, float* smoothedValue, bool* firstRead) {
    float currentRawTorque = motor->HlfbPercent();
    
    // CORRECTED LOGIC: Check for the -9999 sentinel value.
    // A reading below -100% indicates an error (motor not enabled, etc.).
    // In this case, we return 0 and do not update the smoothed average.
    if (currentRawTorque < -100) {
        return 0;
    }

    if (*firstRead) {
        *smoothedValue = currentRawTorque;
        *firstRead = false;
    } else {
        *smoothedValue = (EWMA_ALPHA * currentRawTorque) + (1.0f - EWMA_ALPHA) * (*smoothedValue);
    }
    return *smoothedValue + torqueOffset;
}

bool Fillhead::checkTorqueLimit(MotorDriver* motor) {
    if (!motor || !motor->StatusReg().bit.Enabled || !motor->StatusReg().bit.StepsActive) {
        return false;
    }

    float* smoothedValue = nullptr;
    bool* firstRead = nullptr;

    if (motor == &MotorX) {
        smoothedValue = &smoothedTorqueM0;
        firstRead = &firstTorqueReadM0;
    } else if (motor == &MotorY1) {
        smoothedValue = &smoothedTorqueM1;
        firstRead = &firstTorqueReadM1;
    } else if (motor == &MotorY2) {
        smoothedValue = &smoothedTorqueM2;
        firstRead = &firstTorqueReadM2;
    } else if (motor == &MotorZ) {
        smoothedValue = &smoothedTorqueM3;
        firstRead = &firstTorqueReadM3;
    } else {
        return false; 
    }

    float torque = getSmoothedTorque(motor, smoothedValue, firstRead);
    
    if (fabsf(torque) > torqueLimit) {
        return true; 
    }

    return false;
}
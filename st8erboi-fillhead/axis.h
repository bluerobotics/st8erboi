#pragma once

#include "ClearCore.h"
#include "config.h"

class Fillhead;

class Axis {
public:
    Axis(Fillhead* controller, const char* name, MotorDriver* motor1, MotorDriver* motor2, float stepsPerMm, float minPosMm, float maxPosMm);
    void setupMotors();
    void updateState();
    void handleMove(const char* args);
    void handleHome(const char* args);
    void abort();
    void enable();
    void disable();
    bool isMoving();
    bool isHomed() { return m_homed; }
    const char* getStateString();
    long getPositionSteps() { return m_motor1->PositionRefCommanded(); }
    float getPositionMm() const { return (float)m_motor1->PositionRefCommanded() / m_stepsPerMm; }
    float getSmoothedTorque();
    bool isEnabled();

private:
    void moveSteps(long steps, int velSps, int accelSps2, int torque);
    bool checkTorqueLimit(MotorDriver* motor);
    float getRawTorque(MotorDriver* motor, float* smoothedValue, bool* firstRead);
    void sendStatus(const char* statusType, const char* message);

    Fillhead* m_controller;
    const char* m_name;
    MotorDriver* m_motor1;
    MotorDriver* m_motor2;

    typedef enum {
    	STATE_STANDBY,
		STATE_STARTING_MOVE,
    	STATE_MOVING,
    	STATE_HOMING
    } AxisState;
    AxisState m_state;

    uint32_t m_delay_target_ms;

    // RESTORED: The full, detailed homing sequence
	typedef enum {
		// Initial fast move to find the endstop
		RAPID_START,
		RAPID_WAIT_TO_START,
		RAPID_MOVING,

		// Move away from the endstop
		BACKOFF_START,
		BACKOFF_WAIT_TO_START,
		BACKOFF_MOVING,

		// Slower, more accurate move to touch the endstop again
		TOUCH_START,
		TOUCH_WAIT_TO_START,
		TOUCH_MOVING,

		// De-energize motors to release mechanical stress
		DESTRESS_DISABLE,
		AWAIT_DESTRESS_TIMER,
		DESTRESS_ENABLE,
		AWAIT_ENABLE_TIMER, // Waits for motors to be ready after re-enabling

		// Set the zero position
		SET_ZERO,

		// Final move to a known offset from home
		RETRACT_START,
		RETRACT_WAIT_TO_START,
		RETRACT_MOVING,

		// Sentinel for when homing is not active
		HOMING_NONE
	} HomingPhase;
    
    HomingPhase homingPhase;

    float m_stepsPerMm;
    bool m_homed;
    float m_torqueLimit;
    long m_homingDistanceSteps;
    long m_homingBackoffSteps;
    long m_finalRetractSteps;
    int m_homingRapidSps;
    int m_homingTouchSps;
    int m_homingTorque;
    bool m_motor1_hit_rapid;
    bool m_motor2_hit_rapid;
	
    float m_minPosMm;
    float m_maxPosMm;

    float m_smoothedTorqueM1;
    float m_smoothedTorqueM2;
    bool m_firstTorqueReadM1;
    bool m_firstTorqueReadM2;
};
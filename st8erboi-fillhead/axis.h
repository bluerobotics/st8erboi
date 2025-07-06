#pragma once

#include "ClearCore.h"
#include "config.h"

class Fillhead;

class Axis {
public:
    Axis(Fillhead* controller, const char* name, MotorDriver* motor1, MotorDriver* motor2, float stepsPerMm);
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
    void resetHomingState();

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

    typedef enum {
        HOMING_TYPE_NONE,
        HOMING_TYPE_SINGLE,
        HOMING_TYPE_DUAL
    } HomingType;

    // RESTORED: The full, detailed homing sequence
    typedef enum {
        S_IDLE,
        S_START,
        S_RAPID_TO_ENDSTOP,
        S_DESTRESS_DISABLE,
        S_AWAIT_DESTRESS_TIMER,
        S_DESTRESS_ENABLE,
        S_AWAIT_ENABLE_TIMER,
        S_FIRST_BACKOFF,
        S_AWAIT_BACKOFF_MOVE,
        S_TOUCH_TO_ENDSTOP,
        S_AWAIT_TOUCH_MOVE,
        S_FINAL_DESTRESS,
        S_AWAIT_FINAL_DESTRESS_TIMER,
        S_SET_ZERO,
        S_FINAL_ENABLE,
        S_AWAIT_FINAL_ENABLE_TIMER,
        S_FINAL_RETRACT,
        S_AWAIT_FINAL_RETRACT_MOVE,
        S_COMPLETE
    } SingleHomingPhase;

    typedef enum {
        D_IDLE,
        D_START,
        D_GANTRY_SQUARING,
        D_DESTRESS_DISABLE,
        D_AWAIT_DESTRESS_TIMER,
        D_DESTRESS_ENABLE,
        D_AWAIT_ENABLE_TIMER,
        D_FIRST_BACKOFF,
        D_AWAIT_BACKOFF_MOVE,
        D_TOUCH_TO_ENDSTOP,
        D_AWAIT_TOUCH_MOVE,
        D_FINAL_DESTRESS,
        D_AWAIT_FINAL_DESTRESS_TIMER,
        D_SET_ZERO,
        D_FINAL_ENABLE,
        D_AWAIT_FINAL_ENABLE_TIMER,
        D_FINAL_RETRACT,
        D_AWAIT_FINAL_RETRACT_MOVE,
        D_COMPLETE
    } DualHomingPhase;
    
    HomingType m_homingType;
    SingleHomingPhase m_singleHomingPhase;
    DualHomingPhase m_dualHomingPhase;

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

    float m_smoothedTorqueM1;
    float m_smoothedTorqueM2;
    bool m_firstTorqueReadM1;
    bool m_firstTorqueReadM2;
};
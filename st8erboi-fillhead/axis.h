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

    Fillhead* m_controller;
    const char* m_name;
    MotorDriver* m_motor1;
    MotorDriver* m_motor2;

    AxisState m_state;

    // --- UPDATED HomingPhases ---
    typedef enum {
        HOMING_IDLE,
        HOMING_START,
        HOMING_AWAIT_RAPID_START,
        HOMING_RAPID,
        HOMING_Y_GANTRY_SQUARING,
        HOMING_Y_ALIGNING,
        HOMING_BACK_OFF,
        HOMING_AWAIT_BACKOFF_FINISH, // New state to wait for back-off to complete
        HOMING_TOUCH,
        HOMING_DE_STRESS,
        HOMING_SET_ZERO,
        HOMING_FINAL_RETRACT,
        HOMING_AWAIT_RETRACT_FINISH,
        HOMING_ERROR
    } HomingPhase;
    
    HomingPhase m_homingPhase;

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
    long m_gantry_align_max_steps;
    float m_smoothedTorqueM1;
    float m_smoothedTorqueM2;
    bool m_firstTorqueReadM1;
    bool m_firstTorqueReadM2;
};
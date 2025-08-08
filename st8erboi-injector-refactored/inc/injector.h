#pragma once

#include "config.h"
#include "injector_comms.h"

class Injector {
public:
    /**
     * @brief Construct a new Injector motor controller.
     * @param comms A pointer to the shared communications object for sending status messages.
     */
    Injector(InjectorComms* comms);

    /**
     * @brief Initializes the hardware configuration for the injector motors.
     */
    void setup();

    /**
     * @brief Runs the state machines for homing, feeding, and jogging. Called continuously from the main loop.
     */
    void updateState();

    /**
     * @brief Parses a command and its arguments, and calls the appropriate handler function.
     * @param cmd The command to handle.
     * @param args A pointer to the arguments string for the command.
     */
    void handleCommand(UserCommand cmd, const char* args);

    /**
     * @brief Generates and returns a string containing the current telemetry for the injector.
     * @return A const char pointer to the telemetry buffer.
     */
    const char* getTelemetryString();

    /**
     * @brief Enables the injector motors.
     */
    void enable();

    /**
     * @brief Disables the injector motors.
     */
    void disable();

    /**
     * @brief Abruptly stops any motion on the injector motors.
     */
    void abortMove();

    /**
     * @brief Resets all state machines and operation variables to their default idle state.
     */
    void resetState();

private:
    InjectorComms* m_comms;

    // State Machines
    HomingState m_homingState;
    HomingPhase m_homingPhase;
    FeedState m_feedState;

    bool m_homingMachineDone;
    bool m_homingCartridgeDone;
    bool m_feedingDone;
    bool m_jogDone;
    uint32_t m_homingStartTime;
    bool m_isEnabled;

    // Motor parameters
    float m_torqueLimit;
    float m_torqueOffset;
    float m_smoothedTorqueValue0, m_smoothedTorqueValue1;
    bool m_firstTorqueReading0, m_firstTorqueReading1;
    int32_t m_machineHomeReferenceSteps, m_cartridgeHomeReferenceSteps;
    long m_homingDefaultBackoffSteps;
    int m_feedDefaultTorquePercent;
    long m_feedDefaultVelocitySPS;
    long m_feedDefaultAccelSPS2;

    // Operation-specific variables for a single dispense/feed action
    const char* m_activeFeedCommand;
    const char* m_activeJogCommand;
    bool m_active_dispense_INJECTION_ongoing;
    float m_active_op_target_ml;
    float m_active_op_total_dispensed_ml;
    float m_last_completed_dispense_ml;
    float m_active_op_steps_per_ml;
    long m_active_op_total_target_steps;
    long m_active_op_remaining_steps;
    long m_active_op_segment_initial_axis_steps;
    long m_active_op_initial_axis_steps;
    int m_active_op_velocity_sps;
    int m_active_op_accel_sps2;
    int m_active_op_torque_percent;

    // Buffer for telemetry string
    char m_telemetryBuffer[256];

    // --- Private Methods ---
    void move(int stepsM0, int stepsM1, int torque_limit, int velocity, int accel);
    bool isMoving();
    float getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead);
    bool checkTorqueLimit();
    void finalizeAndResetActiveDispenseOperation(bool success);
    void fullyResetActiveDispenseOperation();

    // --- Command Handlers ---
    void handleJogMove(const char* args);
    void handleMachineHome();
    void handleCartridgeHome();
    void handleMoveToCartridgeHome();
    void handleMoveToCartridgeRetract(const char* args);
    void handleInjectMove(const char* args);
    void handlePauseOperation();
    void handleResumeOperation();
    void handleCancelOperation();
    void handleSetTorqueOffset(const char* args);
};
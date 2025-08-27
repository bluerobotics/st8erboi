#pragma once

#include "config.h"
#include "comms_controller.h"

class Injector {
public:
    /**
     * @brief Construct a new Injector motor controller.
     * @param motorA A pointer to the first injector motor connector (e.g., &ConnectorM0).
     * @param motorB A pointer to the second injector motor connector (e.g., &ConnectorM1).
     * @param comms A pointer to the shared communications object for sending status messages.
     */
    Injector(MotorDriver* motorA, MotorDriver* motorB, CommsController* comms);

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
    CommsController* m_comms;
    MotorDriver* m_motorA;
    MotorDriver* m_motorB;

    // Unified State Machine, similar to Axis class
    typedef enum {
        STATE_STANDBY,
        STATE_HOMING,
        STATE_JOGGING,
        STATE_FEEDING
    } State;
    State m_state;

    // Sub-state for Homing
    HomingState m_homingState; // Used to differentiate between Machine and Cartridge homing
    typedef enum {
		HOMING_PHASE_IDLE,
		RAPID_SEARCH_START,
		RAPID_SEARCH_WAIT_TO_START,
		RAPID_SEARCH_MOVING,
		BACKOFF_START,
		BACKOFF_WAIT_TO_START,
		BACKOFF_MOVING,
		SLOW_SEARCH_START,
		SLOW_SEARCH_WAIT_TO_START,
		SLOW_SEARCH_MOVING,
		SET_OFFSET_START,
		SET_OFFSET_WAIT_TO_START,
		SET_OFFSET_MOVING,
		SET_ZERO,
        HOMING_PHASE_ERROR
	} HomingPhase;
    HomingPhase m_homingPhase;

    // Sub-state for Feeding
    FeedState m_feedState;

    bool m_homingMachineDone;
    bool m_homingCartridgeDone;
    uint32_t m_homingStartTime;
    bool m_isEnabled;

    // Motor parameters
    float m_torqueLimit;
    float m_torqueOffset;
    float m_smoothedTorqueValue0, m_smoothedTorqueValue1;
    bool m_firstTorqueReading0, m_firstTorqueReading1;
    int32_t m_machineHomeReferenceSteps, m_cartridgeHomeReferenceSteps;
    int m_feedDefaultTorquePercent;
    long m_feedDefaultVelocitySPS;
    long m_feedDefaultAccelSPS2;

    // Homing parameters, aligned with Axis class
    long m_homingDistanceSteps;
    long m_homingBackoffSteps;
    int m_homingRapidSps;
    int m_homingTouchSps;
    int m_homingBackoffSps;
    int m_homingAccelSps2;


    // Operation-specific variables for a single dispense/feed action
    const char* m_activeFeedCommand;
    const char* m_activeJogCommand;
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
    void moveSteps(long steps, int velSps, int accelSps2);
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

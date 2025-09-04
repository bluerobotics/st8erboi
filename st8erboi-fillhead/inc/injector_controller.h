#pragma once

#include "config.h"
#include "comms_controller.h"
#include "commands.h"

class Fillhead; // Forward declaration

/**
 * @enum HomingState
 * @brief Defines the active homing operation for the injector motors.
 */
enum HomingState : uint8_t {
	HOMING_NONE,        ///< No homing operation is active.
	HOMING_MACHINE,     ///< Homing to the machine's physical zero point (fully retracted).
	HOMING_CARTRIDGE    ///< Homing to the front of the material cartridge.
};

/**
 * @enum HomingPhase
 * @brief Defines the sub-state or "phase" within an active homing operation.
 * @note This is the global definition, which may differ from the private one inside the Injector class.
 */
enum HomingPhase : uint8_t {
	HOMING_PHASE_IDLE_GLOBAL,          ///< Homing is not active.
	HOMING_PHASE_STARTING_MOVE, ///< Initial delay to confirm motor movement has begun.
	HOMING_PHASE_RAPID_MOVE,    ///< High-speed move towards the hard stop.
	HOMING_PHASE_BACK_OFF,      ///< Moving away from the hard stop after initial contact.
	HOMING_PHASE_TOUCH_OFF,     ///< Slow-speed move to precisely locate the hard stop.
	HOMING_PHASE_RETRACT,       ///< Moving to a final, safe position after homing is complete.
	HOMING_PHASE_COMPLETE,      ///< The homing sequence finished successfully.
	HOMING_PHASE_ERROR_GLOBAL          ///< The homing sequence failed.
};

/**
 * @enum FeedState
 * @brief Defines the state of an injection or material feed operation.
 */
enum FeedState : uint8_t {
	FEED_NONE,                  ///< No feed operation defined (should not be used).
	FEED_STANDBY,               ///< Ready to start a feed operation.
	FEED_INJECT_STARTING,       ///< An injection move has been commanded and is starting.
	FEED_INJECT_ACTIVE,         ///< An injection move is currently in progress.
	FEED_INJECT_PAUSED,         ///< An active injection has been paused.
	FEED_INJECT_RESUMING,       ///< A paused injection is resuming.
	FEED_MOVING_TO_HOME,        ///< Moving to the previously established cartridge home position.
	FEED_MOVING_TO_RETRACT,     ///< Moving to a retracted position relative to the cartridge home.
	FEED_INJECTION_CANCELLED,   ///< The injection was cancelled by the user.
	FEED_INJECTION_COMPLETED    ///< The injection finished successfully.
};


/**
 * @class Injector
 * @brief Manages the dual-motor injector system for dispensing material.
 *
 * This class is responsible for homing the injector (both machine and cartridge homing),
 * performing precise injection moves, handling jogging, managing motor states, monitoring

 * torque, and reporting detailed telemetry.
 */
class Injector {
public:
    /**
     * @brief Constructs a new Injector object.
     * @param motorA Pointer to the primary ClearCore motor driver.
     * @param motorB Pointer to the secondary (ganged) ClearCore motor driver.
     * @param controller Pointer to the main Fillhead controller.
     */
    Injector(MotorDriver* motorA, MotorDriver* motorB, Fillhead* controller);

    /**
     * @brief Initializes the injector motors and configuration.
     */
    void setup();

    /**
     * @brief Updates the internal state machine for the injector.
     * This should be called periodically in the main loop to manage ongoing
     * operations like homing and injection moves.
     */
    void updateState();

    /**
     * @brief Handles user commands related to the injector.
     * @param cmd The UserCommand enum representing the command.
     * @param args A string containing arguments for the command.
     */
    void handleCommand(Command cmd, const char* args);

    /**
     * @brief Gets the telemetry string for the injector system.
     * @return A constant character pointer to the telemetry data string.
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
     * @brief Aborts any ongoing injector motion.
     */
    void abortMove();

    /**
     * @brief Resets the injector's state machine and error conditions.
     */
    void reset();

    /**
     * @brief Checks if either of the injector motors is in a fault state.
     * @return True if a motor is in fault, false otherwise.
     */
    bool isInFault() const;

    /**
     * @brief Checks if the injector is busy with an operation.
     * @return True if the injector is not in standby.
     */
    bool isBusy() const;

private:
    void startMove(long steps, int velSps, int accelSps2);
    bool isMoving();
    float getSmoothedTorque(MotorDriver *motor, float *smoothedValue, bool *firstRead);
    bool checkTorqueLimit();
    void finalizeAndResetActiveDispenseOperation(bool success);
    void fullyResetActiveDispenseOperation();
    void reportEvent(const char* statusType, const char* message);

    void jogMove(const char* args);
    void machineHome();
    void cartridgeHome();
    void moveToCartridgeHome();
    void moveToCartridgeRetract(const char* args);
    void initiateInjectMove(const char* args, float piston_a_diam, float piston_b_diam, const char* command_str);
    void pauseOperation();
    void resumeOperation();
    void cancelOperation();
    void setTorqueOffset(const char* args);
    
    Fillhead* m_controller;      ///< Pointer to the main Fillhead controller.
    MotorDriver* m_motorA;      ///< Pointer to the primary motor driver.
    MotorDriver* m_motorB;      ///< Pointer to the secondary motor driver.

    /**
     * @enum State
     * @brief Defines the top-level operational state of the injector.
     */
    typedef enum {
        STATE_STANDBY,      ///< Injector is idle and ready for commands.
        STATE_HOMING,       ///< Injector is performing a homing sequence.
        STATE_JOGGING,      ///< Injector is performing a manual jog move.
        STATE_FEEDING,      ///< Injector is performing an injection or retract move.
        STATE_MOTOR_FAULT   ///< An injector motor has faulted.
    } State;
    State m_state; ///< The current top-level state of the injector.

    HomingState m_homingState; ///< The type of homing being performed (Machine vs. Cartridge).

    /**
     * @enum HomingPhase
     * @brief Defines the detailed sub-states for the injector's homing sequence.
     */
    typedef enum {
		HOMING_PHASE_IDLE,              ///< Homing is not active.
		RAPID_SEARCH_START,             ///< Starting high-speed move towards hard stop.
		RAPID_SEARCH_WAIT_TO_START,     ///< Waiting for rapid move to begin.
		RAPID_SEARCH_MOVING,            ///< Executing high-speed move.
		BACKOFF_START,                  ///< Starting move away from hard stop.
		BACKOFF_WAIT_TO_START,          ///< Waiting for backoff move to begin.
		BACKOFF_MOVING,                 ///< Executing backoff move.
		SLOW_SEARCH_START,              ///< Starting slow-speed move to find precise stop.
		SLOW_SEARCH_WAIT_TO_START,      ///< Waiting for slow move to begin.
		SLOW_SEARCH_MOVING,             ///< Executing slow-speed move.
		SET_OFFSET_START,               ///< Not used in this implementation.
		SET_OFFSET_WAIT_TO_START,       ///< Not used in this implementation.
		SET_OFFSET_MOVING,              ///< Not used in this implementation.
		SET_ZERO,                       ///< Setting the final position as the logical zero.
        HOMING_PHASE_ERROR              ///< Homing sequence failed.
	} HomingPhase;
    HomingPhase m_homingPhase; ///< The current phase of an active homing sequence.

    FeedState m_feedState; ///< The current state of an injection or feed operation.
    bool m_homingMachineDone;       ///< Flag indicating if machine homing is complete.
    bool m_homingCartridgeDone;     ///< Flag indicating if cartridge homing is complete.
    uint32_t m_homingStartTime;     ///< Timestamp when the homing sequence started.
    bool m_isEnabled;               ///< Flag indicating if motors are enabled.
    float m_torqueLimit;            ///< Torque limit for detecting hard stops.
    float m_torqueOffset;           ///< User-configurable offset for torque readings.
    float m_smoothedTorqueValue0, m_smoothedTorqueValue1; ///< Smoothed torque values for each motor.
    bool m_firstTorqueReading0, m_firstTorqueReading1;   ///< Flags for initializing torque smoothing.
    int32_t m_machineHomeReferenceSteps, m_cartridgeHomeReferenceSteps; ///< Stored step counts for home positions.
    int m_feedDefaultTorquePercent; ///< Default torque for feed moves.
    long m_feedDefaultVelocitySPS;  ///< Default velocity for feed moves.
    long m_feedDefaultAccelSPS2;    ///< Default acceleration for feed moves.
    long m_homingDistanceSteps;     ///< Max travel distance for homing.
    long m_homingBackoffSteps;      ///< Backoff distance for homing.
    int m_homingRapidSps;           ///< Rapid speed for homing.
    int m_homingTouchSps;           ///< Slow touch-off speed for homing.
    int m_homingBackoffSps;         ///< Backoff speed for homing.
    int m_homingAccelSps2;          ///< Acceleration for homing moves.
    const char* m_activeFeedCommand; ///< Stores the original feed command string.
    const char* m_activeJogCommand;  ///< Stores the original jog command string.
    
    // Variables for managing the active dispense operation
    float m_active_op_target_ml;            ///< Target dispense volume in mL.
    float m_active_op_total_dispensed_ml;   ///< Total volume dispensed in the current operation.
    float m_last_completed_dispense_ml;     ///< The volume of the last successful dispense.
    float m_active_op_steps_per_ml;         ///< Conversion factor for the current operation.
    long m_active_op_total_target_steps;    ///< Target dispense distance in steps.
    long m_active_op_remaining_steps;       ///< Remaining steps in the current operation.
    long m_active_op_segment_initial_axis_steps; ///< Start position for a move segment.
    long m_active_op_initial_axis_steps;    ///< Start position for the entire operation.
    int m_active_op_velocity_sps;           ///< Velocity for the current operation.
    int m_active_op_accel_sps2;             ///< Acceleration for the current operation.
    int m_active_op_torque_percent;         ///< Torque limit for the current operation.
    
    char m_telemetryBuffer[256]; ///< Buffer for the telemetry string.
};

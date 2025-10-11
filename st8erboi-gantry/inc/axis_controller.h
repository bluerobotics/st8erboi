/**
 * @file axis_controller.h
 * @author Eldin Miller-Stead
 * @date September 10, 2025
 * @brief Defines the controller for a single gantry axis.
 *
 * @details This file defines the `Axis` class, which encapsulates the logic for
 * controlling a single axis of the gantry system (X, Y, or Z). The class manages
 * the motor(s) for the axis, handles motion commands, and implements a state machine
 * for operations like homing.
 */
#pragma once

#include "ClearCore.h"
#include "config.h"

class Gantry; // Forward declaration

/**
 * @class Axis
 * @brief Controls a single gantry axis, which may consist of one or two motors.
 * @details This class provides a high-level interface for axis control. It is
 * responsible for initializing motors, executing motion commands (absolute and
 * incremental), running homing sequences, and reporting status. It uses a state
 * machine to manage its operations in a non-blocking manner.
 */
class Axis {
public:
	/**
	 * @enum AxisState
	 * @brief Defines the main operational states of an axis.
	 */
	typedef enum {
		STATE_STANDBY,        ///< The axis is idle and ready for commands.
		STATE_STARTING_MOVE,  ///< A move has been commanded, and the axis is waiting for motion to start.
		STATE_MOVING,         ///< The axis is currently executing a move.
		STATE_HOMING,         ///< The axis is performing a homing sequence.
		STATE_FAULT           ///< The axis has encountered an error (e.g., motor fault, torque limit) and must be reset.
	} AxisState;
	
	/**
	 * @enum MoveType
	 * @brief Specifies whether a move is absolute or incremental.
	 */
	typedef enum {
		ABSOLUTE,             ///< Move to a specific coordinate.
		INCREMENTAL           ///< Move a specified distance from the current position.
	} MoveType;

    /**
     * @brief Constructs a new Axis object.
     * @param motor A pointer to the primary ClearCore `MotorDriver` for this axis.
     * @param name A string identifier for the axis (e.g., "X-Axis").
     */
    Axis(MotorDriver* motor, const char* name);
	
	/**
	 * @brief Initializes the axis with its specific hardware and configuration.
	 * @param controller A pointer to the main `Gantry` controller.
	 * @param motor2 A pointer to the second motor, if the axis is dual-motor (e.g., Y-axis). Can be `nullptr`.
	 * @param stepsPerMm The conversion factor for this axis.
	 * @param minPosMm The minimum travel limit in millimeters.
	 * @param maxPosMm The maximum travel limit in millimeters.
	 * @param homingSensor1 A pointer to the primary homing sensor.
	 * @param homingSensor2 A pointer to the secondary homing sensor (for dual-motor axes). Can be `nullptr`.
	 * @param limitSensor A pointer to the limit switch for this axis. Can be `nullptr`.
	 * @param zBrake A pointer to the Z-axis brake connector. Can be `nullptr`.
	 */
	void setup(Gantry* controller, MotorDriver* motor2, float stepsPerMm, float minPosMm, float maxPosMm, 
		Connector* homingSensor1, Connector* homingSensor2, Connector* limitSensor, Connector* zBrake);

	/**
	 * @brief Sets up motor-specific configurations.
	 * @details Configures HLFB, sets max velocity and acceleration, and enables the motor(s).
	 * It also configures associated hardware like sensors and brakes.
	 */
	void setupMotors();
	
	/**
	 * @name Public Interface
	 * @{
	 */
	
	/**
	 * @brief The main, non-blocking update loop for the axis.
	 * @details This function is intended to be called continuously from the main `Gantry` loop.
	 * It drives the internal state machine for this axis, handling the progression of
	 * moves and homing sequences.
	 */
	void updateState();

	/**
	 * @brief Initiates a move command parsed from a string.
	 * @details This is the main entry point for `MOVE` commands from the GUI. It parses
	 * the provided arguments to determine the move parameters (e.g., type, target, speed)
	 * and then calls `startMove()` to execute it.
	 * @param args A string containing the move parameters. Expected format:
	 *             "[ABS|INC] <pos_mm> <vel_mms> <accel_mms2> <torque_percent>"
	 */
	void move(const char* args);

	/**
	 * @brief Initiates a homing sequence parsed from a string.
	 * @details This is the entry point for `HOME` commands. It parses an optional
	 * max travel distance and then starts the homing state machine.
	 * @param args A string that can optionally contain the max travel distance in mm.
	 *             If empty or null, the axis's full travel range is used.
	 */
	void home(const char* args);

	/**
	 * @brief Aborts any motion in progress on this axis immediately.
	 * @details Halts the motor(s) using an abrupt stop.
	 */
	void abort();

	/**
	 * @brief Enables the motor(s) for this axis.
	 * @details Also disengages the Z-axis brake if one is configured.
	 */
	void enable();

	/**
	 * @brief Disables the motor(s) for this axis.
	 * @details Also engages the Z-axis brake if one is configured.
	 */
	void disable();

    /**
     * @brief Resets the axis to a safe, idle state.
     * @details Aborts any active motion and resets the state machine to STANDBY.
     * The homed status is preserved.
     */
    void reset();

	/**
	 * @brief Checks if the axis is currently in motion.
	 * @return `true` if the motor(s) are actively executing a move, `false` otherwise.
	 */
	bool isMoving();
	
	/**
	 * @brief Checks if the axis is currently in enabled.
	 * @return `true` if the motor(s) are enabled, `false` otherwise.
	 */
	bool isEnabled();

	/**
	 * @brief Checks if the axis has been successfully homed.
	 * @return `true` if a homing sequence has completed successfully, `false` otherwise.
	 */
	bool isHomed() { return m_homed; }

	/**
	 * @brief Gets the current state of the axis as a human-readable string.
	 * @return A `const char*` representing the current state (e.g., "Homing", "Standby").
	 */
	const char* getState();

	/**
	 * @brief Gets the current state of the axis as an `AxisState` enum.
	 * @return The current value of the `m_state` member.
	 */
	AxisState getStateEnum() const;

	/**
	 * @brief Checks if the axis is currently in a fault condition.
	 * @return `true` if the state is `STATE_FAULT`, `false` otherwise.
	 */
	bool isInFault();

	/**
	 * @brief Checks if the configured limit sensor for this axis is currently triggered.
	 * @return `true` if the sensor exists and is in a triggered state, `false` otherwise.
	 */
	bool isLimitSensorTriggered();

	/**
	 * @brief Forces the axis into a fault state.
	 * @details This is called by an external controller (like Gantry) when a system-level
	 * fault related to this axis is detected (e.g., a limit switch).
	 * @param reason A string describing the reason for the fault, to be used in the error message.
	 */
	void enterFaultState(const char* reason);

	/**
	 * @brief Gets the current commanded position of the axis in millimeters.
	 * @return The current position in mm.
	 */
	float getPositionMm() const;

	/**
	 * @brief Gets the smoothed torque reading for telemetry purposes.
	 * @details This function updates and returns an Exponentially Weighted Moving Average (EWMA)
	 * of the motor torque. For dual-motor axes, it returns the average of both motors'
	 * smoothed torque values.
	 * @return The smoothed torque value as a percentage.
	 */
	float getSmoothedTorque();
	
	/**
	 * @brief Starts a move with precisely defined parameters.
	 * @details This is the core motion execution function. It validates the move against
	 * software limits and then commands the motor(s) to move the specified distance.
	 * @param target_mm For absolute moves, the destination coordinate. For incremental
	 *                  moves, the distance to travel.
	 * @param vel_mms The target velocity in mm/s.
	 * @param accel_mms2 The target acceleration in mm/s^2.
	 * @param torque The torque limit for the move as a percentage.
	 * @param moveType The type of move (`ABSOLUTE` or `INCREMENTAL`).
	 */
	void startMove(float target_mm, float vel_mms, float accel_mms2, int torque, MoveType moveType);
    /** @} */

	private:
	/**
	 * @name Private Helper Methods
	 * @{
	 */
	
	/**
	 * @brief Commands the low-level motor driver(s) to move a set number of steps.
	 * @param steps The number of steps to move. Positive or negative determines direction.
	 * @param velSps The target velocity in steps/s.
	 * @param accelSps2 The target acceleration in steps/s^2.
	 * @param torque The torque limit for the move as a percentage.
	 */
	void moveSteps(long steps, int velSps, int accelSps2, int torque);
	
	/**
	 * @brief Checks if a motor has exceeded its torque limit during a move.
	 * @details This function uses an instantaneous torque reading for the fastest response.
	 * @param motor A pointer to the `MotorDriver` to check.
	 * @return `true` if the absolute torque exceeds the current `m_torqueLimit`, `false` otherwise.
	 */
	bool checkTorqueLimit(MotorDriver* motor);

	/**
	 * @brief Updates and retrieves the smoothed (EWMA) torque value for a single motor.
	 * @param motor A pointer to the `MotorDriver` to measure.
	 * @param smoothedValue A pointer to the float where the smoothed value is stored.
	 * @param firstRead A pointer to the flag used to initialize the EWMA filter.
	 * @return The new smoothed torque value as a percentage.
	 */
	float getSmoothedTorque(MotorDriver* motor, float* smoothedValue, bool* firstRead);

	/**
	 * @brief Gets the instantaneous, unfiltered torque reading from a single motor.
	 * @param motor A pointer to the `MotorDriver` to measure.
	 * @return The raw torque value as a percentage.
	 */
	float getInstantaneousTorque(MotorDriver* motor);
	
	/**
	 * @brief Reports a status event up to the main `Gantry` controller.
	 * @param statusType The prefix for the message (e.g., "GANTRY_INFO: ").
	 * @param message The content of the message to send.
	 */
	void reportEvent(const char* statusType, const char* message);
    /** @} */

	Gantry* m_controller;       ///< Pointer to the main `Gantry` controller for reporting events.
	const char* m_name;         ///< The name of the axis (e.g., "X-Axis").
	MotorDriver* m_motor1;      ///< Pointer to the primary motor driver.
	MotorDriver* m_motor2;      ///< Pointer to the secondary motor driver (if applicable).
	Connector* m_homingSensor1; ///< Pointer to the primary homing sensor.
	Connector* m_homingSensor2; ///< Pointer to the secondary homing sensor (if applicable).
	Connector* m_limitSensor;   ///< Pointer to the limit switch.
	Connector* m_zBrake;        ///< Pointer to the Z-axis brake.
	const char* m_activeCommand;///< The string of the currently active command, for logging.

	AxisState m_state;          ///< The current high-level state of the axis.

	/**
	 * @enum HomingPhase
	 * @brief Defines the detailed sub-states for the axis homing sequence.
	 */
	typedef enum {
		HOMING_NONE,                ///< Homing is not active.
		RAPID_SEARCH_START,         ///< Starting high-speed move towards the homing sensor.
		RAPID_SEARCH_WAIT_TO_START, ///< Waiting for the rapid move to begin.
		RAPID_SEARCH_MOVING,        ///< Executing the high-speed move.
		BACKOFF_START,              ///< Starting the move off the sensor.
		BACKOFF_WAIT_TO_START,      ///< Waiting for the backoff move to begin.
		BACKOFF_MOVING,             ///< Executing the backoff move.
		SLOW_SEARCH_START,          ///< Starting the slow-speed move to precisely locate the sensor edge.
		SLOW_SEARCH_WAIT_TO_START,  ///< Waiting for the slow move to begin.
		SLOW_SEARCH_MOVING,         ///< Executing the slow-speed move.
		SET_OFFSET_START,           ///< Starting the final move to the zero position offset.
		SET_OFFSET_WAIT_TO_START,   ///< Waiting for the final offset move to begin.
		SET_OFFSET_MOVING,          ///< Executing the final offset move.
		SET_ZERO                    ///< Setting the final position as the logical zero.
	} HomingPhase;
	
	HomingPhase homingPhase;    ///< The current phase of an active homing sequence.

	float m_stepsPerMm;         ///< Conversion factor for this axis.
	bool m_homed;               ///< Flag indicating if the axis has been successfully homed.
	float m_torqueLimit;        ///< Current torque limit (%) for moves.
	uint32_t m_homingStartTimeMs; ///< Timestamp (ms) when a homing sequence started, used for timeout.
	uint32_t m_homingTimeoutMs;   ///< Calculated timeout (ms) for the current homing operation.
	
	float m_minPosMm;           ///< The minimum software travel limit in millimeters.
	float m_maxPosMm;           ///< The maximum software travel limit in millimeters.

	long m_homingDistanceSteps; ///< Max travel distance (steps) for a homing move.
	long m_homingBackoffSteps;  ///< Backoff distance (steps) for a homing move.
	int m_homingRapidSps;       ///< Rapid speed (steps/sec) for a homing move.
	int m_homingTouchSps;       ///< Slow touch-off speed (steps/sec) for a homing move.
	int m_homingBackoffSps;     ///< Backoff speed (steps/sec) for a homing move.
	int m_homingAccelSps2;      ///< Acceleration (steps/sec^2) for homing moves.

	bool m_motor1_homed;        ///< Flag for dual-motor homing.
	bool m_motor2_homed;        ///< Flag for dual-motor homing.

	float m_smoothedTorqueM1;   ///< Smoothed torque value for motor 1.
	float m_smoothedTorqueM2;   ///< Smoothed torque value for motor 2.
	bool m_firstTorqueReadM1;   ///< Flag for initializing torque smoothing for motor 1.
	bool m_firstTorqueReadM2;   ///< Flag for initializing torque smoothing for motor 2.
};

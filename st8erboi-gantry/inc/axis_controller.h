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
		STATE_HOMING          ///< The axis is performing a homing sequence.
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
	 * @brief Sets up the motor-specific configurations.
	 */
	void setupMotors();
	
	/**
	 * @name Public Interface
	 * @{
	 */
	void updateState();
	void move(const char* args);
	void home(const char* args);
	void abort();
	void enable();
	void disable();
    void reset();
	bool isMoving();
	bool isHomed() { return m_homed; }
	const char* getState();
	AxisState getStateEnum() const;
	float getPositionMm() const;
	float getSmoothedTorque();
	bool isEnabled();
	
	void startMove(float target_mm, float vel_mms, float accel_mms2, int torque, MoveType moveType);
    /** @} */

	private:
	/**
	 * @name Private Helper Methods
	 * @{
	 */
	void moveSteps(long steps, int velSps, int accelSps2, int torque);
	bool checkTorqueLimit(MotorDriver* motor);
	float getRawTorque(MotorDriver* motor, float* smoothedValue, bool* firstRead);
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

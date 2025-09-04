#pragma once

#include "config.h"
#include "comms_controller.h"
#include "ClearCore.h"
#include "commands.h"

class Fillhead; // Forward declaration

/**
 * @enum PinchValveState
 * @brief Defines the main operational states of a pinch valve.
 */
enum PinchValveState {
	VALVE_IDLE,             ///< The valve is not performing any action and is ready for commands.
	VALVE_HOMING,           ///< The valve is currently executing its homing sequence.
	VALVE_OPENING,          ///< The valve is moving to the fully open position.
	VALVE_CLOSING,          ///< The valve is moving to the fully closed (pinched) position.
	VALVE_JOGGING,          ///< The valve is performing a manual jog move.
	VALVE_OPEN,             ///< The valve is stationary in the fully open position.
	VALVE_CLOSED,           ///< The valve is stationary in the fully closed position.
	VALVE_RESETTING,        ///< The valve is waiting for motion to stop before completing a reset.
	VALVE_OPERATION_ERROR,  ///< An error occurred during an operation (e.g., timeout, torque limit).
	VALVE_MOTOR_FAULT       ///< The motor driver has reported a fault.
};

/**
 * @class PinchValve
 * @brief Controls a single motorized pinch valve.
 *
 * This class encapsulates the logic for homing, opening, closing, and jogging a
 * pinch valve. It includes state management for these operations, motor control,
 * torque monitoring, and communication of status and telemetry.
 */
class PinchValve {
public:
	/**
	 * @brief Constructs a new PinchValve object.
	 * @param name A string identifier for the valve (e.g., "InjectionValve").
	 * @param motor A pointer to the ClearCore MotorDriver object for this valve.
	 * @param controller A pointer to the main Fillhead controller.
	 */
	PinchValve(const char* name, MotorDriver* motor, Fillhead* controller);

	/**
	 * @brief Initializes the pinch valve motor and configuration.
	 */
	void setup();

	/**
	 * @brief Updates the internal state machine for the pinch valve.
	 * This should be called periodically in the main loop to manage ongoing
	 * operations like homing, opening, and closing.
	 */
	void updateState();

	/**
	 * @brief Handles user commands specific to this pinch valve.
	 * @param cmd The UserCommand enum representing the command to execute.
	 * @param args A string containing any arguments for the command.
	 */
	void handleCommand(Command cmd, const char* args);

	/**
	 * @brief Initiates the homing sequence for the valve.
	 */
	void home();

	/**
	 * @brief Commands the valve to move to the fully open position.
	 */
	void open();

	/**
	 * @brief Commands the valve to move to the fully closed (pinched) position.
	 */
	void close();

	/**
	 * @brief Commands the valve to perform a jog move.
	 * @param args A string containing the distance and speed for the jog.
	 */
	void jog(const char* args);

	/**
	 * @brief Enables the motor for the valve.
	 */
	void enable();

	/**
	 * @brief Disables the motor for the valve.
	 */
	void disable();

	/**
	 * @brief Aborts any ongoing motion.
	 */
	void abort();

	/**
	 * @brief Resets the valve's state machine and error conditions.
	 */
	void reset();

	/**
	 * @brief Checks if the valve has been successfully homed.
	 * @return True if homed, false otherwise.
	 */
	bool isHomed() const;

	/**
	 * @brief Gets the telemetry string for this valve.
	 * @return A constant character pointer to the telemetry data string.
	 */
	const char* getTelemetryString();

	/**
	 * @brief Checks if the valve's motor is in a fault state.
	 * @return True if in fault, false otherwise.
	 */
	bool isInFault() const;

	/**
	 * @brief Checks if the valve is busy with an operation.
	 * @return True if the valve is homing, opening, closing, or jogging.
	 */
	bool isBusy() const;

private:
	/**
	 * @brief Commands the motor to move a specific number of steps.
	 * @param steps The target position in motor steps.
	 * @param velocity_sps The maximum velocity in steps per second.
	 * @param accel_sps2 The acceleration in steps per second squared.
	 */
	void moveSteps(long steps, int velocity_sps, int accel_sps2);

	/**
	 * @brief Gets the smoothed motor torque value.
	 * @return The smoothed torque, filtered with an EWMA.
	 */
	float getSmoothedTorque();

	/**
	 * @brief Gets the instantaneous motor torque value.
	 * @return The current, unfiltered torque reading.
	 */
	float getInstantaneousTorque();

	/**
	 * @brief Checks if the motor torque has exceeded the configured limit.
	 * @return True if the torque limit is exceeded, false otherwise.
	 */
	bool checkTorqueLimit();

	/**
	 * @brief Formats and sends a status message via the main Fillhead controller.
	 * @param statusType The prefix for the message (e.g., "INFO: ").
	 * @param message The content of the message to send.
	 */
	void reportEvent(const char* statusType, const char* message);

	/**
	 * @enum OperatingPhase
	 * @brief Defines the sub-states for simple motion operations like open, close, or jog.
	 */
	typedef enum {
		PHASE_IDLE,         ///< No operation is active.
		PHASE_START,        ///< The operation is being initiated.
		PHASE_WAIT_TO_START,///< Waiting for the motor to begin moving.
		PHASE_MOVING        ///< The motor is actively moving.
	} OperatingPhase;

	/**
	 * @enum HomingPhase
	 * @brief Defines the detailed sub-states for the multi-step homing sequence.
	 */
	typedef enum {
		HOMING_PHASE_IDLE,              ///< Homing is not active.
		INITIAL_BACKOFF_START,          ///< Starting the initial move away from a potential start-on-hard-stop condition.
		INITIAL_BACKOFF_WAIT_TO_START,  ///< Waiting for the initial backoff move to start.
		INITIAL_BACKOFF_MOVING,         ///< Executing the initial backoff move.
		RAPID_SEARCH_START,             ///< Starting the high-speed search for the hard stop.
		RAPID_SEARCH_WAIT_TO_START,     ///< Waiting for the rapid search to start.
		RAPID_SEARCH_MOVING,            ///< Executing the rapid search.
		BACKOFF_START,                  ///< Starting the move away from the hard stop after the first touch.
		BACKOFF_WAIT_TO_START,          ///< Waiting for the backoff move to start.
		BACKOFF_MOVING,                 ///< Executing the backoff move.
		SLOW_SEARCH_START,              ///< Starting the slow-speed search for a precise hard stop location.
		SLOW_SEARCH_WAIT_TO_START,      ///< Waiting for the slow search to start.
		SLOW_SEARCH_MOVING,             ///< Executing the slow search.
		SET_OFFSET_START,               ///< Starting the final move to the operational 'home' position.
		SET_OFFSET_WAIT_TO_START,       ///< Waiting for the final offset move to start.
		SET_OFFSET_MOVING,              ///< Executing the final offset move.
		SET_ZERO                        ///< Setting the final position as the logical zero.
	} HomingPhase;

	/// @brief The name of the valve instance.
	const char* m_name;
	/// @brief Pointer to the motor driver.
	MotorDriver* m_motor;
	/// @brief Pointer to the main Fillhead controller.
	Fillhead* m_controller;
	/// @brief The main state of the pinch valve.
	PinchValveState m_state;
	/// @brief The current phase of the homing sequence.
	HomingPhase m_homingPhase;
	/// @brief The current phase of a standard open/close/jog operation.
	OperatingPhase m_opPhase;
	/// @brief The timestamp when a move operation was started.
	uint32_t m_moveStartTime;
	/// @brief Flag indicating if the valve has been successfully homed.
	bool m_isHomed;
	/// @brief The timestamp when a homing sequence was started.
	uint32_t m_homingStartTime;
	/// @brief The torque limit for detecting hard stops.
	float m_torqueLimit;
	/// @brief The smoothed torque value.
	float m_smoothedTorque;
	/// @brief Flag to handle the first reading for torque smoothing.
	bool m_firstTorqueReading;
	/// @brief Buffer for the telemetry string.
	char m_telemetryBuffer[128];
	/// @brief The total travel distance for homing searches in steps.
	long m_homingDistanceSteps;
	/// @brief The backoff distance used in homing, in steps.
	long m_homingBackoffSteps;
	/// @brief The final offset from the hard stop to the 'home' position, in steps.
	long m_homingFinalOffsetSteps;
	/// @brief The unified speed for all homing moves, in steps per second.
	int m_homingUnifiedSps;
	/// @brief The acceleration for homing moves, in steps per second squared.
	int m_homingAccelSps2;
};

#pragma once

#include "config.h"
#include "comms_controller.h"
#include "commands.h"

class Fillhead; // Forward declaration

/**
 * @enum HeaterState
 * @brief Defines the operational state of the heater.
 */
enum HeaterState : uint8_t {
	HEATER_OFF,         ///< Heater is off.
	HEATER_PID_ACTIVE   ///< Heater is being controlled by the PID loop.
};

/**
 * @class HeaterController
 * @brief Manages the heating element of the fillhead using a PID controller.
 *
 * This class handles initializing the heater hardware, reading temperature from
 * a thermocouple, implementing a PID control loop to maintain a target
 * temperature, handling user commands, and reporting telemetry.
 */
class HeaterController {
	public:
	/**
	 * @brief Constructs a new HeaterController object.
	 * @param controller A pointer to the main Fillhead controller for communication tasks.
	 */
	HeaterController(Fillhead* controller);

	/**
	 * @brief Initializes the hardware and software for the heater controller.
	 * This includes setting up the GPIO for the heater relay and the analog input
	 * for the thermocouple.
	 */
	void setup();

	/**
	 * @brief Reads and updates the current temperature reading.
	 * This method should be called periodically. It reads the raw thermocouple
	 * value, converts it to Celsius, and applies a smoothing filter.
	 */
	void updateTemperature();

	/**
	 * @brief Manages the state machine and PID control for the heater.
	 * This method should be called periodically in the main loop. It runs the
	 * PID calculation when the heater is active and controls the heater relay.
	 */
	void updateState();

	/**
	 * @brief Handles user commands related to the heater.
	 * @param cmd The UserCommand enum representing the command to be executed.
	 * @param args A string containing any arguments required for the command.
	 */
	void handleCommand(Command cmd, const char* args);

	/**
	 * @brief Gets the telemetry string for the heater system.
	 * @return A constant character pointer to a string containing key-value pairs
	 *         of heater system telemetry data (e.g., temperature, setpoint, state).
	 */
	const char* getTelemetryString();

	private:
	/// @brief Pointer to the main Fillhead controller for sending messages.
	Fillhead* m_controller;
	/// @brief The current operational state of the heater (ON/OFF).
	HeaterState m_heaterState;
	/// @brief The raw temperature reading from the thermocouple in Celsius.
	float m_temperatureCelsius;
	/// @brief The smoothed (Exponentially Weighted Moving Average) temperature in Celsius.
	float m_smoothedTemperatureCelsius;
	/// @brief Flag to handle the first reading for the smoothing filter.
	bool m_firstTempReading;
	/// @brief The target temperature for the PID controller in Celsius.
	float m_pid_setpoint;
	/// @brief The Proportional gain for the PID controller.
	float m_pid_kp;
	/// @brief The Integral gain for the PID controller.
	float m_pid_ki;
	/// @brief The Derivative gain for the PID controller.
	float m_pid_kd;
	/// @brief The integral term accumulator for the PID controller.
	float m_pid_integral;
	/// @brief The previous error value used for the derivative term calculation.
	float m_pid_last_error;
	/// @brief The timestamp of the last PID update, used to calculate delta time.
	uint32_t m_pid_last_time;
	/// @brief The output of the PID calculation, typically representing a duty cycle.
	float m_pid_output;
	/// @brief A buffer to store the formatted telemetry string.
	char m_telemetryBuffer[256];

	/**
	 * @brief Resets the PID controller's state variables.
	 * This is typically called when the heater is turned on or the gains are changed.
	 */
	void resetPID();

	/**
	 * @brief Formats and sends a status message via the main Fillhead controller.
	 * @param statusType The prefix for the message (e.g., "INFO: ").
	 * @param message The content of the message to send.
	 */
	void reportEvent(const char* statusType, const char* message);

	/**
	 * @brief Turns the heater on and enables the PID control loop.
	 */
	void heaterOn();

	/**
	 * @brief Turns the heater off and disables the PID control loop.
	 */
	void heaterOff();

	/**
	 * @brief Sets the PID gains from a string argument.
	 * @param args A string containing the new Kp, Ki, and Kd values, typically comma-separated.
	 */
	void setGains(const char* args);

	/**
	 * @brief Sets the PID setpoint (target temperature) from a string argument.
	 * @param args A string containing the new setpoint in Celsius.
	 */
	void setSetpoint(const char* args);
};

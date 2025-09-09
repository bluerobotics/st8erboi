#pragma once

#include "config.h"
#include "comms_controller.h"
#include "commands.h"

class Fillhead; // Forward declaration

/**
 * @enum VacuumState
 * @brief Defines the operational state of the vacuum system.
 */
enum VacuumState : uint8_t {
	VACUUM_OFF,             ///< The vacuum pump and valve are off.
	VACUUM_PULLDOWN,        ///< The pump is on, pulling down to the target pressure for a leak test.
	VACUUM_SETTLING,        ///< The pump is off, allowing pressure to stabilize before a leak test.
	VACUUM_LEAK_TESTING,    ///< The system is holding vacuum and monitoring for pressure changes.
	VACUUM_ACTIVE_HOLD,     ///< The pump is actively maintaining the target pressure.
	VACUUM_ERROR            ///< The system failed to reach target or failed a leak test.
};

/**
 * @class VacuumController
 * @brief Manages and controls the vacuum system of the fillhead.
 *
 * This class is responsible for initializing the vacuum hardware, reading pressure
 * from the sensor, managing the vacuum state machine (e.g., pulldown, leak testing,
 * active hold), handling user commands, and reporting telemetry.
 */
class VacuumController {
	public:
	/**
	 * @brief Constructs a new VacuumController object.
	 * @param controller A pointer to the main Fillhead controller for communication tasks.
	 */
	VacuumController(Fillhead* controller);

	/**
	 * @brief Initializes the hardware and software components for the vacuum controller.
	 * This includes setting up GPIO pins for the vacuum pump relay and configuring
	 * the analog input for the pressure transducer.
	 */
	void setup();

	/**
	 * @brief Reads and updates the current vacuum pressure reading.
	 * This method should be called periodically. It reads the raw value from the
	 * pressure transducer, converts it to PSIG, and applies a smoothing filter.
	 */
	void updateVacuum();

	/**
	 * @brief Manages the state machine for vacuum control.
	 * This method should be called periodically in the main loop. It transitions
	 * between states like VACUUM_OFF, VACUUM_PULLDOWN, VACUUM_LEAK_TESTING, etc.,
	 * based on sensor readings, timers, and user commands.
	 */
	void updateState();

	/**
	 * @brief Handles user commands related to the vacuum system.
	 * @param cmd The UserCommand enum representing the command to be executed.
	 * @param args A string containing any arguments required for the command.
	 */
	void handleCommand(Command cmd, const char* args);

	/**
	 * @brief Gets the telemetry string for the vacuum system.
	 * @return A constant character pointer to a string containing key-value pairs
	 *         of vacuum system telemetry data (e.g., pressure, state).
	 */
	const char* getTelemetryString();

	/**
	 * @brief Resets the state of the vacuum controller to its initial, idle state.
	 * This will turn off the vacuum and reset any ongoing operations.
	 */
	void resetState();

	/**
	 * @brief Checks if the vacuum controller is busy with an operation.
	 * @return True if the controller is in a state other than VACUUM_OFF or VACUUM_ERROR,
	 *         false otherwise.
	 */
	bool isBusy() const;

	/**
	 * @brief Gets the current state of the vacuum system as a string.
	 * @return A const char* representing the current state.
	 */
	const char* getStateString() const;

	private:
	/**
	 * @brief Formats and sends a status message via the main Fillhead controller.
	 * @param statusType The prefix for the message (e.g., "INFO: ").
	 * @param message The content of the message to send.
	 */
	void reportEvent(const char* statusType, const char* message);
	
	/// @brief Pointer to the main Fillhead controller for sending messages.
	Fillhead* m_controller;
	/// @brief The current operational state of the vacuum system.
	VacuumState m_state;
	/// @brief The raw vacuum pressure reading from the sensor in PSIG.
	float m_vacuumPressurePsig;
	/// @brief The smoothed (Exponentially Weighted Moving Average) vacuum pressure in PSIG.
	float m_smoothedVacuumPsig;
	/// @brief Flag to handle the first reading for the smoothing filter.
	bool m_firstVacuumReading;
	/// @brief The target vacuum pressure for pulldown and hold states in PSIG.
	float m_targetPsig;
	/// @brief The timeout in seconds for reaching the target pressure during pulldown.
	float m_rampTimeoutSec;
	/// @brief The maximum allowed pressure increase (less negative) during a leak test in PSIG.
	float m_leakTestDeltaPsig;
	/// @brief The duration of the leak test in seconds.
	float m_leakTestDurationSec;
	/// @brief The timestamp (in milliseconds) when the current state was entered.
	uint32_t m_stateStartTimeMs;
	/// @brief The pressure recorded at the beginning of a leak test.
	float m_leakTestStartPressure;
	/// @brief A buffer to store the formatted telemetry string.
	char m_telemetryBuffer[256];

	/**
	 * @brief Activates the vacuum pump.
	 */
	void vacuumOn();

	/**
	 * @brief Deactivates the vacuum pump.
	 */
	void vacuumOff();

	/**
	 * @brief Initiates a leak test sequence.
	 */
	void leakTest();

	/**
	 * @brief Sets the target vacuum pressure from a string argument.
	 * @param args A string containing the new target pressure in PSIG.
	 */
	void setTarget(const char* args);

	/**
	 * @brief Sets the pulldown timeout from a string argument.
	 * @param args A string containing the new timeout in seconds.
	 */
	void setTimeout(const char* args);

	/**
	 * @brief Sets the leak test delta pressure from a string argument.
	 * @param args A string containing the new delta pressure in PSIG.
	 */
	void setLeakDelta(const char* args);

	/**
	 * @brief Sets the leak test duration from a string argument.
	 * @param args A string containing the new duration in seconds.
	 */
	void setLeakDuration(const char* args);
};

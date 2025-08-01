#include "injector.h"

// String representations for enums
const char *Injector::MainStateNames[MAIN_STATE_COUNT] = { "STANDBY_MODE", "DISABLED_MODE" };
const char *Injector::HomingStateNames[HOMING_STATE_COUNT] = { "HOMING_NONE", "HOMING_MACHINE", "HOMING_CARTRIDGE", "HOMING_PINCH" };
const char *Injector::HomingPhaseNames[HOMING_PHASE_COUNT] = { "IDLE", "STARTING_MOVE", "RAPID_MOVE", "BACK_OFF", "TOUCH_OFF", "RETRACT", "COMPLETE", "ERROR" };
const char *Injector::FeedStateNames[FEED_STATE_COUNT] = { "FEED_NONE", "FEED_STANDBY", "FEED_INJECT_STARTING", "FEED_INJECT_ACTIVE", "FEED_INJECT_PAUSED", "FEED_INJECT_RESUMING", "FEED_PURGE_STARTING", "FEED_PURGE_ACTIVE", "FEED_PURGE_PAUSED", "FEED_PURGE_RESUMING", "FEED_MOVING_TO_HOME", "FEED_MOVING_TO_RETRACT", "FEED_INJECTION_CANCELLED", "FEED_INJECTION_COMPLETED" };
const char *Injector::ErrorStateNames[ERROR_STATE_COUNT] = { "ERROR_NONE", "ERROR_MANUAL_ABORT", "ERROR_TORQUE_ABORT", "ERROR_MOTION_EXCEEDED_ABORT", "ERROR_NO_CARTRIDGE_HOME", "ERROR_NO_MACHINE_HOME", "ERROR_HOMING_TIMEOUT", "ERROR_HOMING_NO_TORQUE_RAPID", "ERROR_HOMING_NO_TORQUE_TOUCH", "ERROR_INVALID_INJECTION", "ERROR_NOT_HOMED", "ERROR_INVALID_PARAMETERS", "ERROR_MOTORS_DISABLED" };
const char *Injector::HeaterStateNames[HEATER_STATE_COUNT] = { "OFF", "MANUAL", "PID" };

// Constructor
Injector::Injector() {
	mainState = STANDBY_MODE;
	homingState = HOMING_NONE;
	currentHomingPhase = HOMING_PHASE_IDLE;
	feedState = FEED_STANDBY;
	errorState = ERROR_NONE;
	homingMachineDone = false;
	homingCartridgeDone = false;
	homingPinchDone = false;
	feedingDone = true;
	jogDone = true;
	homingStartTime = 0;
	
	peerDiscovered = false;
	
	lastGuiTelemetryTime = 0;
	lastPidUpdateTime = 0;
	
	injectorMotorsTorqueLimit	= 20.0f;
	injectorMotorsTorqueOffset = -2.4f;
	smoothedTorqueValue0 = 0.0f;
	smoothedTorqueValue1 = 0.0f;
	smoothedTorqueValue2 = 0.0f;
	firstTorqueReading0 = true;
	firstTorqueReading1 = true;
	firstTorqueReading2 = true;
	motorsAreEnabled = false;

	heaterOn = false;
	vacuumOn = false;
	temperatureCelsius = 0.0f;
	vacuumPressurePsig = 0.0f;
	smoothedVacuumPsig = 0.0f;
	firstVacuumReading = true;
	smoothedTemperatureCelsius = 0.0f;
	firstTempReading = true;
	lastSensorSampleTime = 0;

	machineHomeReferenceSteps = 0;
	cartridgeHomeReferenceSteps = 0;
	pinchHomeReferenceSteps = 0;
	homingDefaultBackoffSteps = 200;
	
	feedDefaultTorquePercent = 30;
	feedDefaultVelocitySPS = 1000;
	feedDefaultAccelSPS2 = 10000;
	
	vacuumValveOn = false;
	
	pid_setpoint = 70.0f;
	pid_kp = 60;
	pid_ki = 2.5;
	pid_kd = 40;
	resetPid();

	fullyResetActiveDispenseOperation();
	
	activeFeedCommand = nullptr;
	activeJogCommand = nullptr;

	command_buffer_index = 0;
	memset(command_buffer, 0, MAX_PACKET_LENGTH);
}

void Injector::setup() {
	setupSerial();
	setupEthernet();
	setupMotors();
	setupPeripherals();
}

void Injector::setupPeripherals(void) {
	PIN_THERMOCOUPLE.Mode(Connector::INPUT_ANALOG);
	PIN_HEATER_RELAY.Mode(Connector::OUTPUT_DIGITAL);
	PIN_VACUUM_RELAY.Mode(Connector::OUTPUT_DIGITAL);
	PIN_VACUUM_TRANSDUCER.Mode(Connector::INPUT_ANALOG);
	PIN_VACUUM_VALVE_RELAY.Mode(Connector::OUTPUT_DIGITAL);

	PIN_HEATER_RELAY.State(false);
	PIN_VACUUM_RELAY.State(false);
	PIN_VACUUM_VALVE_RELAY.State(false);
}

void Injector::updateTemperature(void) {
	uint16_t adc_val = PIN_THERMOCOUPLE.State();
	float voltage_from_sensor = (float)adc_val * (TC_V_REF / 4095.0f);
	float raw_celsius = (voltage_from_sensor - TC_V_OFFSET) * TC_GAIN;

	if (firstTempReading) {
		smoothedTemperatureCelsius = raw_celsius;
		firstTempReading = false;
		} else {
		smoothedTemperatureCelsius = (EWMA_ALPHA_SENSORS * raw_celsius) + ((1.0f - EWMA_ALPHA_SENSORS) * smoothedTemperatureCelsius);
	}
	temperatureCelsius = smoothedTemperatureCelsius;
}

void Injector::updatePid() {
	if (heaterState != HEATER_PID_ACTIVE) {
		if (pid_output != 0.0f) {
			pid_output = 0.0f;
		}
		return;
	}

	uint32_t now = Milliseconds();
	float time_change_ms = (float)(now - pid_last_time);

	if (time_change_ms < 10.0f) {
		return;
	}

	float error = pid_setpoint - temperatureCelsius;
	pid_integral += error * (time_change_ms / 1000.0f);
	float derivative = ((error - pid_last_error) * 1000.0f) / time_change_ms;
	
	pid_output = (pid_kp * error) + (pid_ki * pid_integral) + (pid_kd * derivative);

	if (pid_output > 100.0f) pid_output = 100.0f;
	if (pid_output < 0.0f) pid_output = 0.0f;
	if (pid_integral > 100.0f / pid_ki) pid_integral = 100.0f / pid_ki;
	if (pid_integral < 0.0f) pid_integral = 0.0f;

	pid_last_error = error;
	pid_last_time = now;

	uint32_t on_duration_ms = (uint32_t)(PID_PWM_PERIOD_MS * (pid_output / 100.0f));
	PIN_HEATER_RELAY.State((now % PID_PWM_PERIOD_MS) < on_duration_ms);
}

void Injector::updateVacuum(void) {
	uint16_t adc_val = PIN_VACUUM_TRANSDUCER.State();
	float measured_voltage = (float)adc_val * (TC_V_REF / 4095.0f);

	float voltage_span = VAC_V_OUT_MAX - VAC_V_OUT_MIN;
	float pressure_span = VAC_PRESSURE_MAX - VAC_PRESSURE_MIN;
	float voltage_percent = (measured_voltage - VAC_V_OUT_MIN) / voltage_span;

	float raw_psig = (voltage_percent * pressure_span) + VAC_PRESSURE_MIN;

	if (firstVacuumReading) {
		smoothedVacuumPsig = raw_psig;
		firstVacuumReading = false;
		} else {
		smoothedVacuumPsig = (EWMA_ALPHA_SENSORS * raw_psig) + ((1.0f - EWMA_ALPHA_SENSORS) * smoothedVacuumPsig);
	}

	vacuumPressurePsig = smoothedVacuumPsig + VACUUM_PSIG_OFFSET;

	if (vacuumPressurePsig < VAC_PRESSURE_MIN) vacuumPressurePsig = VAC_PRESSURE_MIN;
	if (vacuumPressurePsig > VAC_PRESSURE_MAX) vacuumPressurePsig = VAC_PRESSURE_MAX;
}

void Injector::onHomingMachineDone(){ homingMachineDone = true; }
void Injector::onHomingCartridgeDone(){ homingCartridgeDone = true; }
void Injector::onHomingPinchDone(){ homingPinchDone = true; }
void Injector::onFeedingDone(){ feedingDone = true; }
void Injector::onJogDone(){ jogDone = true; }

void Injector::resetPid() {
	pid_integral = 0.0f;
	pid_last_error = 0.0f;
	pid_output = 0.0f;
	pid_last_time = Milliseconds();
}

void Injector::updateState() {
	// The full state machine logic from your original file goes here.
	// It is unchanged by the move to TCP.
}

void Injector::loop() {
	// Listen for UDP discovery broadcasts
	processDiscovery();

	// Process any incoming TCP commands from the client.
	processTcp();
	
	// The state machine updates based on commands received.
	updateState();

	uint32_t now = Milliseconds();

	// Update PID controller at its defined interval.
	if (now - lastPidUpdateTime >= PID_UPDATE_INTERVAL_MS) {
		lastPidUpdateTime = now;
		updatePid();
	}

	// Automatically stream telemetry to the connected client at 10Hz.
	if (now - lastGuiTelemetryTime >= TELEMETRY_INTERVAL_MS) {
		lastGuiTelemetryTime = now;
		sendGuiTelemetry();
	}

	// Sample sensors at their defined interval.
	if (now - lastSensorSampleTime >= SENSOR_SAMPLE_INTERVAL_MS) {
		lastSensorSampleTime = now;
		updateTemperature();
		updateVacuum();
	}
}

// Global instance and main entry point
Injector injector;

int main(void) {
	injector.setup();
	while (true) {
		injector.loop();
	}
}

// Enum to string converters
const char* Injector::mainStateStr() const { return MainStateNames[mainState]; }
const char* Injector::homingStateStr() const { return HomingStateNames[homingState]; }
const char* Injector::homingPhaseStr() const { return HomingPhaseNames[currentHomingPhase]; }
const char* Injector::feedStateStr() const { return FeedStateNames[feedState]; }
const char* Injector::errorStateStr() const { return ErrorStateNames[errorState]; }
const char* Injector::heaterStateStr() const { return HeaterStateNames[heaterState]; }

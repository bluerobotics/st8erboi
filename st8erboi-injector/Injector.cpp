#include "injector.h"

// String representations for enums
const char *Injector::MainStateNames[MAIN_STATE_COUNT] = { "STANDBY_MODE", "JOG_MODE", "HOMING_MODE", "FEED_MODE", "DISABLED_MODE" };
const char *Injector::HomingStateNames[HOMING_STATE_COUNT] = { "HOMING_NONE", "HOMING_MACHINE", "HOMING_CARTRIDGE", "HOMING_PINCH" };
const char *Injector::HomingPhaseNames[HOMING_PHASE_COUNT] = { "IDLE", "STARTING_MOVE", "RAPID_MOVE", "BACK_OFF", "TOUCH_OFF", "RETRACT", "COMPLETE", "ERROR" };
const char *Injector::FeedStateNames[FEED_STATE_COUNT] = { "FEED_NONE", "FEED_STANDBY", "FEED_INJECT_STARTING", "FEED_INJECT_ACTIVE", "FEED_INJECT_PAUSED", "FEED_INJECT_RESUMING", "FEED_PURGE_STARTING", "FEED_PURGE_ACTIVE", "FEED_PURGE_PAUSED", "FEED_PURGE_RESUMING", "FEED_MOVING_TO_HOME", "FEED_MOVING_TO_RETRACT", "FEED_INJECTION_CANCELLED", "FEED_INJECTION_COMPLETED" };
const char *Injector::ErrorStateNames[ERROR_STATE_COUNT] = { "ERROR_NONE", "ERROR_MANUAL_ABORT", "ERROR_TORQUE_ABORT", "ERROR_MOTION_EXCEEDED_ABORT", "ERROR_NO_CARTRIDGE_HOME", "ERROR_NO_MACHINE_HOME", "ERROR_HOMING_TIMEOUT", "ERROR_HOMING_NO_TORQUE_RAPID", "ERROR_HOMING_NO_TORQUE_TOUCH", "ERROR_INVALID_INJECTION", "ERROR_NOT_HOMED", "ERROR_INVALID_PARAMETERS", "ERROR_MOTORS_DISABLED" };

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
	
	peerIp = IpAddress();
	peerDiscovered = false;
	guiDiscovered = false;
	guiPort = 0;
	
	lastGuiTelemetryTime = 0;
	
	injectorMotorsTorqueLimit	= 20.0f;
	injectorMotorsTorqueOffset = -2.4f;
	smoothedTorqueValue0 = 0.0f;
	smoothedTorqueValue1 = 0.0f;
	smoothedTorqueValue2 = 0.0f;
	firstTorqueReading0 = true;
	firstTorqueReading1 = true;
	firstTorqueReading2 = true;
	motorsAreEnabled = false;

	// --- NEW: Initialize new state variables ---
	heaterOn = false;
	vacuumOn = false;
	temperatureCelsius = 0.0f;

	machineHomeReferenceSteps = 0;
	cartridgeHomeReferenceSteps = 0;
	homingDefaultBackoffSteps = 200;
	
	feedDefaultTorquePercent = 30;
	feedDefaultVelocitySPS = 1000;
	feedDefaultAccelSPS2 = 10000;

	fullyResetActiveDispenseOperation();
}

void Injector::setup() {
	setupSerial();
	setupEthernet();
	setupMotors();
	setupPeripherals();
}

// --- Function to set up non-motor peripherals ---
void Injector::setupPeripherals(void) {
	// Set pin modes based on hardware connections
	PIN_THERMOCOUPLE.Mode(Connector::INPUT_ANALOG);
	PIN_HEATER_RELAY.Mode(Connector::OUTPUT_DIGITAL);
	PIN_VACUUM_RELAY.Mode(Connector::OUTPUT_DIGITAL);

	// Ensure relays are off at startup
	PIN_HEATER_RELAY.State(false); // CORRECTED from StateSet
	PIN_VACUUM_RELAY.State(false); // CORRECTED from StateSet
}

// --- Function to read and convert thermocouple value ---
void Injector::updateTemperature(void) {
	// Read the raw 12-bit ADC value (0-4095)
	uint16_t adc_val = PIN_THERMOCOUPLE.State();

	// Convert ADC value directly to the 0-10V sensor voltage
	float voltage_from_sensor = (float)adc_val * (TC_V_REF / 4095.0f);

	// Convert the sensor voltage to temperature
	temperatureCelsius = (voltage_from_sensor - TC_V_OFFSET) * TC_GAIN;
}


void Injector::onHomingMachineDone(){
	homingMachineDone = true;
}

void Injector::onHomingCartridgeDone(){
	homingCartridgeDone = true;
}

void Injector::onHomingPinchDone(){
	homingPinchDone = true;
}

void Injector::onFeedingDone(){
	feedingDone = true;
}

void Injector::onJogDone(){
	jogDone = true;
}

void Injector::updateState() {
	switch (mainState)
	{
		case STANDBY_MODE:
		// No active operations expected here.
		break;

		case HOMING_MODE: {
			// This top-level check handles the end-of-sequence for success or error
			if (currentHomingPhase == HOMING_PHASE_COMPLETE || currentHomingPhase == HOMING_PHASE_ERROR) {
				if (currentHomingPhase == HOMING_PHASE_COMPLETE) {
					sendStatus(STATUS_PREFIX_DONE, "Homing sequence complete. Returning to STANDBY_MODE.");
					} else {
					sendStatus(STATUS_PREFIX_ERROR,"Homing sequence ended with error. Returning to STANDBY_MODE.");
				}
				mainState = STANDBY_MODE;
				homingState = HOMING_NONE;
				currentHomingPhase = HOMING_PHASE_IDLE;
				break;
			}

			if (homingState != HOMING_NONE) {
				uint32_t current_time = Milliseconds();
				if (current_time - homingStartTime > MAX_HOMING_DURATION_MS) {
					sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Timeout. Aborting.");
					abortInjectorMove();
					errorState = ERROR_HOMING_TIMEOUT;
					currentHomingPhase = HOMING_PHASE_ERROR;
					break;
				}
			}

			// --- Logic for Machine and Cartridge Homing ---
			if (homingState == HOMING_MACHINE || homingState == HOMING_CARTRIDGE) {
				bool is_machine_homing = (homingState == HOMING_MACHINE);
				int direction = is_machine_homing ? -1 : 1;

				// Use a switch to handle each specific phase of the homing operation
				switch (currentHomingPhase) {
					case HOMING_PHASE_IDLE:
					// Do nothing, waiting for a homing command
					break;
					
					case HOMING_PHASE_STARTING_MOVE: {
						uint32_t current_time = Milliseconds();
						if (current_time - homingStartTime > 50) {
							if (checkInjectorMoving()) {
								sendStatus(STATUS_PREFIX_START, "Homing: Move started. Entering RAPID_MOVE phase.");
								currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
								} else {
								sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Motor failed to start moving.");
								errorState = ERROR_HOMING_NO_TORQUE_RAPID;
								currentHomingPhase = HOMING_PHASE_ERROR;
							}
						}
						break;
					}

					case HOMING_PHASE_RAPID_MOVE:
					case HOMING_PHASE_TOUCH_OFF: {
						if (checkInjectorTorqueLimit()) {
							if (currentHomingPhase == HOMING_PHASE_RAPID_MOVE) {
								const char* msg = is_machine_homing ? "Machine Homing: Torque (RAPID). Backing off." : "Cartridge Homing: Torque (RAPID). Backing off.";
								sendStatus(STATUS_PREFIX_INFO, msg);
								currentHomingPhase = HOMING_PHASE_BACK_OFF;
								long back_off_s = -direction * homingDefaultBackoffSteps;
								moveInjectorMotors(back_off_s, back_off_s, (int)homing_torque_percent_param, homing_actual_touch_sps, homing_actual_accel_sps2);
								} else {
								const char* msg = is_machine_homing ? "Machine Homing: Torque (TOUCH_OFF). Zeroing & Retracting." : "Cartridge Homing: Torque (TOUCH_OFF). Zeroing & Retracting.";
								sendStatus(STATUS_PREFIX_INFO, msg);
								if (is_machine_homing) {
									machineHomeReferenceSteps = ConnectorM0.PositionRefCommanded();
									sendStatus(STATUS_PREFIX_INFO, "Machine home reference point set.");
									} else {
									cartridgeHomeReferenceSteps = ConnectorM0.PositionRefCommanded();
									sendStatus(STATUS_PREFIX_INFO, "Cartridge home reference point set.");
								}
								currentHomingPhase = HOMING_PHASE_RETRACT;
								long retract_s = -direction * homing_actual_retract_steps;
								moveInjectorMotors(retract_s, retract_s, (int)homing_torque_percent_param, homing_actual_rapid_sps, homing_actual_accel_sps2);
							}
							} else if (!checkInjectorMoving()) {
							// If motors stopped but torque limit wasn't hit, it's an error.
							const char* msg = currentHomingPhase == HOMING_PHASE_RAPID_MOVE ? "Machine Homing Err: No torque in RAPID." : "Machine Homing Err: No torque in TOUCH_OFF.";
							sendStatus(STATUS_PREFIX_ERROR, msg);
							errorState = currentHomingPhase == HOMING_PHASE_RAPID_MOVE ? ERROR_HOMING_NO_TORQUE_RAPID : ERROR_HOMING_NO_TORQUE_TOUCH;
							currentHomingPhase = HOMING_PHASE_ERROR;
						}
						break;
					}

					case HOMING_PHASE_BACK_OFF: {
						if (!checkInjectorMoving()) {
							const char* msg = is_machine_homing ? "Machine Homing: Back-off done. Touching off." : "Cartridge Homing: Back-off done. Touching off.";
							sendStatus(STATUS_PREFIX_INFO, msg);
							currentHomingPhase = HOMING_PHASE_TOUCH_OFF;
							long touch_off_move_length = homingDefaultBackoffSteps * 2;
							long final_touch_off_move_steps = direction * touch_off_move_length;
							moveInjectorMotors(final_touch_off_move_steps, final_touch_off_move_steps, (int)homing_torque_percent_param, homing_actual_touch_sps, homing_actual_accel_sps2);
						}
						break;
					}
					
					case HOMING_PHASE_RETRACT: {
						if (!checkInjectorMoving()) {
							const char* msg = is_machine_homing ? "Machine Homing: RETRACT complete. Success." : "Cartridge Homing: RETRACT complete. Success.";
							sendStatus(STATUS_PREFIX_DONE, msg);
							if (is_machine_homing) {
								onHomingMachineDone();
								} else {
								onHomingCartridgeDone();
							}
							errorState = ERROR_NONE;
							currentHomingPhase = HOMING_PHASE_COMPLETE;
						}
						break;
					}

					default:
					break;
				}
			}
			// --- Logic for Pinch Homing ---
			else if (homingState == HOMING_PINCH) {
				switch (currentHomingPhase) {
					case HOMING_PHASE_STARTING_MOVE: {
						uint32_t current_time = Milliseconds();
						if (current_time - homingStartTime > 50) {
							if (checkInjectorMoving()) {
								sendStatus(STATUS_PREFIX_START, "Pinch Homing: Move started. Entering TOUCH_OFF phase.");
								currentHomingPhase = HOMING_PHASE_TOUCH_OFF;
								} else {
								sendStatus(STATUS_PREFIX_ERROR, "Pinch Homing Error: Motor failed to start moving.");
								currentHomingPhase = HOMING_PHASE_ERROR;
							}
						}
						break;
					}
					case HOMING_PHASE_TOUCH_OFF: {
						if (checkInjectorTorqueLimit()) {
							// Torque limit was hit, this is success for pinch homing
							sendStatus(STATUS_PREFIX_INFO, "Pinch Homing: Torque limit reached. Success.");
							ConnectorM2.PositionRefSet(0); // Zero the position
							onHomingPinchDone();
							errorState = ERROR_NONE;
							currentHomingPhase = HOMING_PHASE_COMPLETE;
							} else if (!checkInjectorMoving()) {
							// If motor stopped but torque limit wasn't hit, it's an error.
							sendStatus(STATUS_PREFIX_ERROR, "Pinch Homing Error: Motor stopped without reaching torque limit.");
							currentHomingPhase = HOMING_PHASE_ERROR;
						}
						break;
					}
					default:
					break;
				}
			}
		}
		break;
		
		case FEED_MODE: {
			if (feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE ||
			feedState == FEED_MOVING_TO_HOME || feedState == FEED_MOVING_TO_RETRACT ||
			feedState == FEED_INJECT_RESUMING || feedState == FEED_PURGE_RESUMING ) {
				if (checkInjectorTorqueLimit()) {
					errorState = ERROR_TORQUE_ABORT;
					sendStatus(STATUS_PREFIX_ERROR,"FEED_MODE: Torque limit! Operation stopped.");
					finalizeAndResetActiveDispenseOperation(false);
					feedState = FEED_STANDBY;
					feedingDone = true;
				}
			}

			if (!checkInjectorMoving()) {
				if (!feedingDone) {
					
					if (feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE) {
						if (active_dispense_INJECTION_ongoing) {
							if (active_op_steps_per_ml > 0.0001f) {
								long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - active_op_segment_initial_axis_steps;
								float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / active_op_steps_per_ml;
								active_op_total_dispensed_ml += segment_dispensed_ml;
							}
							last_completed_dispense_ml = active_op_total_dispensed_ml;
						}
						sendStatus(STATUS_PREFIX_DONE, "Feed Op: Inject/Purge segment/operation completed.");
						feedState = FEED_INJECTION_COMPLETED;
					}
					else if (feedState == FEED_MOVING_TO_HOME || feedState == FEED_MOVING_TO_RETRACT) {
						sendStatus(STATUS_PREFIX_DONE, "Feed Op: Positioning move completed.");
						feedState = FEED_INJECTION_COMPLETED;
					}
					
					if (feedState == FEED_INJECTION_COMPLETED) {
						feedingDone = true;
						onFeedingDone();
						finalizeAndResetActiveDispenseOperation(true);
						feedState = FEED_STANDBY;
						sendStatus(STATUS_PREFIX_DONE, "Feed Op: Finalized. Returning to Feed Standby.");
					}
				}
				} else { // Motors are moving
				if (feedState == FEED_INJECT_STARTING) {
					feedState = FEED_INJECT_ACTIVE;
					active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
					sendStatus(STATUS_PREFIX_INFO, "Feed Op: Inject Active.");
					} else if (feedState == FEED_PURGE_STARTING) {
					feedState = FEED_PURGE_ACTIVE;
					active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
					sendStatus(STATUS_PREFIX_INFO, "Feed Op: Purge Active.");
					} else if (feedState == FEED_INJECT_RESUMING) {
					feedState = FEED_INJECT_ACTIVE;
					active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
					sendStatus(STATUS_PREFIX_INFO, "Feed Op: Inject Resumed, Active.");
					} else if (feedState == FEED_PURGE_RESUMING) {
					feedState = FEED_PURGE_ACTIVE;
					active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
					sendStatus(STATUS_PREFIX_INFO, "Feed Op: Purge Resumed, Active.");
				}
			}

			if ((feedState == FEED_INJECT_PAUSED || feedState == FEED_PURGE_PAUSED) &&
			!feedingDone && !checkInjectorMoving()) {

				long current_pos = ConnectorM0.PositionRefCommanded();
				long steps_moved_this_segment = current_pos - active_op_segment_initial_axis_steps;

				if (active_op_steps_per_ml > 0.0001f) {
					float segment_dispensed_ml = fabs(steps_moved_this_segment) / active_op_steps_per_ml;
					active_op_total_dispensed_ml += segment_dispensed_ml;

					long total_steps_dispensed = (long)(active_op_total_dispensed_ml * active_op_steps_per_ml);
					active_op_remaining_steps = active_op_total_target_steps - total_steps_dispensed;

					if (active_op_remaining_steps < 0) active_op_remaining_steps = 0;
				}

				feedingDone = true;
				sendStatus(STATUS_PREFIX_INFO, "Feed Op: Operation Paused. Waiting for Resume/Cancel.");
			}
		} break;

		case JOG_MODE:
		if (checkInjectorTorqueLimit()) {
			mainState = STANDBY_MODE;
			errorState = ERROR_TORQUE_ABORT;
			sendStatus(STATUS_PREFIX_INFO, "JOG_MODE: Torque limit, returning to STANDBY.");
			} else if (!checkInjectorMoving() && !jogDone) {
			jogDone = true;
		}
		break;

		case DISABLED_MODE:
		// Only ENABLE command should change state from here
		break;
		
		default:
		// This case handles any unhandled enum values (like ..._COUNT) and prevents a compiler warning.
		break;
	}
}

void Injector::loop() {
	processUdp();
	updateState();
	updateTemperature();
}

Injector injector;

int main(void) {
	// Perform one-time setup
	injector.setup();

	// Main non-blocking application loop
	while (true) {
		// Just call the main loop handler for our injector object.
		injector.loop();
	}
}


// Implementations for string converters
const char* Injector::mainStateStr() const { return MainStateNames[mainState]; }
const char* Injector::homingStateStr() const { return HomingStateNames[homingState]; }
const char* Injector::homingPhaseStr() const { return HomingPhaseNames[currentHomingPhase]; }
const char* Injector::feedStateStr() const { return FeedStateNames[feedState]; }
const char* Injector::errorStateStr() const { return ErrorStateNames[errorState]; }

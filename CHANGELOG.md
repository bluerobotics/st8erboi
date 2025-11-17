# Changelog

All notable changes to the Fillhead firmware will be documented in this file.

## [1.0.0] - 2025-11-17

### Added
- **Initial release**: Fillhead injection system firmware
- **Dual injector motors**: Synchronized control of two ganged injector motors for material injection
- **Pinch valve control**: Two independent pinch valves (vacuum and injection sides) with motorized control
- **Heater control**: PID temperature control with thermocouple feedback and relay output
- **Vacuum system**: Pressure monitoring and control with transducer feedback
- **UDP/Ethernet communication**: Network-based control with device discovery
- **USB Serial support**: Direct USB communication fallback
- **Command-based control**: Simple text-based command protocol for all operations
- **Real-time telemetry**: Position, temperature, pressure, torque, and status reporting
- **Homing routines**: Automatic homing for injector and pinch valves (tubed/untubed modes)
- **Injection control**: Volume-based injection with configurable speeds and piston diameter support
- **Leak testing**: Automated vacuum leak detection with configurable thresholds
- **Error handling**: Comprehensive error reporting and recovery mechanisms

### Features
- **Injector system**: Dual motor synchronized control with configurable pitch and steps/mm
- **Pinch valves**: Motorized pinch valves with separate homing routines for tubed/untubed operation
- **Temperature control**: PID-based heater control with configurable setpoints and gains
- **Vacuum control**: Pressure monitoring with configurable targets and ramp timeouts
- **Material handling**: Support for stator and rotor injection modes with different piston diameters
- **Feed operations**: Configurable feed speeds and accelerations for material handling

### Hardware Support
- Built on Teknic ClearCore platform
- ClearPath motor drivers (M0-M3 connectors)
- Analog inputs for thermocouple and vacuum transducer
- Digital I/O for relay control (heater, vacuum pump, solenoid valve)
- Ethernet connectivity for network communication
- USB serial for direct communication


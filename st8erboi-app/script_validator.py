# script_validator.py

"""
Contains the command reference with validation rules (min/max)
and the script validation logic.
"""

COMMANDS = {
    # --- Injector Commands ---
    "INJECT_MOVE": {
        "device": "injector",
        "params": [
            {"name": "Volume(ml)", "type": float, "min": 0, "max": 1000},
            {"name": "Speed(ml/s)", "type": float, "min": 0.01, "max": 10},
            {"name": "Accel(sps^2)", "type": float, "min": 100, "max": 100000},
            {"name": "Steps/ml", "type": float, "min": 0, "max": 1000000},
            {"name": "Torque(%)", "type": float, "min": 0, "max": 100}
        ],
        "help": "Executes a precision injection move based on volume."
    },
    "PURGE_MOVE": {
        "device": "injector",
        "params": [
            {"name": "Volume(ml)", "type": float, "min": 0, "max": 1000},
            {"name": "Speed(ml/s)", "type": float, "min": 0.01, "max": 10},
            {"name": "Accel(sps^2)", "type": float, "min": 100, "max": 100000},
            {"name": "Steps/ml", "type": float, "min": 0, "max": 1000000},
            {"name": "Torque(%)", "type": float, "min": 0, "max": 100}
        ],
        "help": "Executes a purge move to expel a certain volume of material."
    },
    "VACUUM_CHECK": {
        "device": "injector",
        "params": [
            {"name": "Max-PSI-Increase", "type": float, "min": 0, "max": 10},
            {"name": "Duration(s)", "type": float, "min": 1, "max": 300}
        ],
        "help": "Monitors vacuum for a leak. Fails if pressure increases more than the max allowed over the duration."
    },
    "SET_DEFAULT_VACUUM_CHECK": {
        "device": "injector",
        "params": [
            {"name": "Max-PSI-Increase", "type": float, "min": 0, "max": 10},
            {"name": "Duration(s)", "type": float, "min": 1, "max": 300}
        ],
        "help": "Sets the default parameters for future VACUUM_CHECK commands."
    },
    "MACHINE_HOME_MOVE": {
        "device": "injector",
        "params": [
            {"name": "Stroke(mm)", "type": float, "min": 1, "max": 1000},
            {"name": "Rapid-Vel(mm/s)", "type": float, "min": 1, "max": 100},
            {"name": "Touch-Vel(mm/s)", "type": float, "min": 0.1, "max": 10},
            {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000},
            {"name": "Retract(mm)", "type": float, "min": 0, "max": 100},
            {"name": "Torque(%)", "type": float, "min": 0, "max": 100}
        ],
        "help": "Homes the main machine axis."
    },
    "CARTRIDGE_HOME_MOVE": {
        "device": "injector",
        "params": [
            {"name": "Stroke(mm)", "type": float, "min": 1, "max": 1000},
            {"name": "Rapid-Vel(mm/s)", "type": float, "min": 1, "max": 100},
            {"name": "Touch-Vel(mm/s)", "type": float, "min": 0.1, "max": 10},
            {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000},
            {"name": "Retract(mm)", "type": float, "min": 0, "max": 100},
            {"name": "Torque(%)", "type": float, "min": 0, "max": 100}
        ],
        "help": "Homes the injector relative to the cartridge."
    },
    "JOG_MOVE": {
        "device": "injector",
        "params": [
            {"name": "M0-Dist(mm)", "type": float, "min": -1000, "max": 1000},
            {"name": "M1-Dist(mm)", "type": float, "min": -1000, "max": 1000},
            {"name": "Vel(mm/s)", "type": float, "min": 1, "max": 100},
            {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000},
            {"name": "Torque(%)", "type": float, "min": 0, "max": 100}
        ],
        "help": "Jogs injector motors by a relative distance."
    },
    "MOVE_TO_CARTRIDGE_RETRACT": {
        "device": "injector",
        "params": [{"name": "Retract-Offset(mm)", "type": float, "min": 0, "max": 200}],
        "help": "Moves the injector to the retract position, away from the cartridge."
    },
    "SET_HEATER_SETPOINT": {
        "device": "injector",
        "params": [{"name": "Temp(°C)", "type": float, "min": 20, "max": 150}],
        "help": "Sets the target temperature for the heater PID."
    },
    # --- Fillhead Commands ---
    "MOVE_X": {"device": "fillhead", "params": [{"name": "Dist(mm)", "type": float, "min": -1000, "max": 1000},
                                                {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 200},
                                                {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000},
                                                {"name": "Torque(%)", "type": float, "min": 0, "max": 100}],
               "help": "Moves the Fillhead X-axis by a relative distance."},
    "MOVE_Y": {"device": "fillhead", "params": [{"name": "Dist(mm)", "type": float, "min": -1000, "max": 1000},
                                                {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 200},
                                                {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000},
                                                {"name": "Torque(%)", "type": float, "min": 0, "max": 100}],
               "help": "Moves the Fillhead Y-axis by a relative distance."},
    "MOVE_Z": {"device": "fillhead", "params": [{"name": "Dist(mm)", "type": float, "min": -1000, "max": 1000},
                                                {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 200},
                                                {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000},
                                                {"name": "Torque(%)", "type": float, "min": 0, "max": 100}],
               "help": "Moves the Fillhead Z-axis by a relative distance."},
    "HOME_X": {"device": "fillhead", "params": [{"name": "Torque(%)", "type": float, "min": 1, "max": 100},
                                                {"name": "Max-Dist(mm)", "type": float, "min": 1, "max": 1000}],
               "help": "Homes the Fillhead X-axis."},
    "HOME_Y": {"device": "fillhead", "params": [{"name": "Torque(%)", "type": float, "min": 1, "max": 100},
                                                {"name": "Max-Dist(mm)", "type": float, "min": 1, "max": 1000}],
               "help": "Homes the Fillhead Y-axis."},
    "HOME_Z": {"device": "fillhead", "params": [{"name": "Torque(%)", "type": float, "min": 1, "max": 100},
                                                {"name": "Max-Dist(mm)", "type": float, "min": 1, "max": 1000}],
               "help": "Homes the Fillhead Z-axis."},

    # --- Simple Commands (No Params) ---
    "PINCH_HOME_MOVE": {"device": "injector", "params": [], "help": "Homes the pinch valve motor."},
    "MOVE_TO_CARTRIDGE_HOME": {"device": "injector", "params": [],
                               "help": "Moves the injector to the zero position of the cartridge."},
    "PAUSE_INJECTION": {"device": "injector", "params": [], "help": "Pauses an ongoing injection or purge."},
    "RESUME_INJECTION": {"device": "injector", "params": [], "help": "Resumes a paused injection or purge."},
    "CANCEL_INJECTION": {"device": "injector", "params": [], "help": "Cancels an injection or purge."},
    "ENABLE": {"device": "injector", "params": [], "help": "Enables injector motors 0 & 1."},
    "DISABLE": {"device": "injector", "params": [], "help": "Disables injector motors 0 & 1."},
    "ENABLE_PINCH": {"device": "injector", "params": [], "help": "Enables the pinch motor."},
    "DISABLE_PINCH": {"device": "injector", "params": [], "help": "Disables the pinch motor."},
    "HEATER_PID_ON": {"device": "injector", "params": [], "help": "Turns the heater PID controller on."},
    "HEATER_PID_OFF": {"device": "injector", "params": [], "help": "Turns the heater PID controller off."},
    "VACUUM_ON": {"device": "injector", "params": [], "help": "Turns the vacuum pump on."},
    "VACUUM_OFF": {"device": "injector", "params": [], "help": "Turns the vacuum pump off."},
    "VACUUM_VALVE_ON": {"device": "injector", "params": [], "help": "Opens the vacuum valve."},
    "VACUUM_VALVE_OFF": {"device": "injector", "params": [], "help": "Closes the vacuum valve."},
    "START_DEMO": {"device": "fillhead", "params": [], "help": "Starts the circle demo on the fillhead."},
    "ABORT": {"device": "both", "params": [], "help": "Stops all motion on the target device."},
}


def validate_script(script_content):
    """
    Validates a script against the command reference.
    Checks for command existence, parameter count, types, and bounds.

    Args:
        script_content (str): The full text of the script to validate.

    Returns:
        list: A list of dictionaries, where each dictionary represents an error.
              Returns an empty list if the script is valid.
    """
    errors = []
    lines = script_content.splitlines()

    for i, line in enumerate(lines):
        line_num = i + 1
        line = line.strip()

        if not line or line.startswith('#'):
            continue

        parts = line.split()
        command_word = parts[0].upper()
        args = parts[1:]

        # 1. Check if command exists
        if command_word not in COMMANDS:
            # Skip script-control commands, they are handled by the runner
            if command_word.startswith("SET_DEFAULT_") or command_word in ["WAIT", "WAIT_UNTIL_VACUUM",
                                                                           "WAIT_UNTIL_HEATER_AT_TEMP"]:
                continue
            errors.append({
                "line": line_num,
                "error": f"Unknown command: '{command_word}'. Check for spelling errors."
            })
            continue

        command_info = COMMANDS[command_word]
        params_def = command_info['params']

        # 2. Check parameter count
        if len(args) != len(params_def):
            errors.append({
                "line": line_num,
                "error": f"Incorrect parameter count for '{command_word}'. Expected {len(params_def)}, but got {len(args)}."
            })
            continue

            # 3. Check parameter types and bounds
        for j, arg in enumerate(args):
            param_def = params_def[j]
            param_name = param_def['name']

            # Try to convert to a float for validation
            try:
                value = float(arg)
            except ValueError:
                errors.append({
                    "line": line_num,
                    "error": f"Parameter '{param_name}' for '{command_word}' must be a number, but got '{arg}'."
                })
                continue

                # Check min/max bounds if they exist
            if 'min' in param_def and value < param_def['min']:
                errors.append({
                    "line": line_num,
                    "error": f"Parameter '{param_name}' for '{command_word}' is below the minimum of {param_def['min']}. Got {value}."
                })

            if 'max' in param_def and value > param_def['max']:
                errors.append({
                    "line": line_num,
                    "error": f"Parameter '{param_name}' for '{command_word}' is above the maximum of {param_def['max']}. Got {value}."
                })

    return errors

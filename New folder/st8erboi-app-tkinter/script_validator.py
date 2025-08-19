# script_validator.py

"""
Contains the command reference with validation rules (min/max) and default values.
Also contains the script validation logic.
"""

COMMANDS = {
    # --- Fillhead Commands (Formerly Injector) ---
    "INJECT_MOVE": {
        "device": "fillhead",
        "params": [
            {"name": "Volume(ml)", "type": float, "min": 0, "max": 1000},
            {"name": "Speed(ml/s)", "type": float, "min": 0.01, "max": 10, "optional": True, "default": 0.1},
            {"name": "Accel(sps^2)", "type": float, "min": 100, "max": 100000, "optional": True, "default": 5000},
            {"name": "Steps/ml", "type": float, "min": 0, "max": 1000000},
            {"name": "Torque(%)", "type": float, "min": 0, "max": 100, "optional": True, "default": 50}
        ],
        "help": "Executes a precision injection move based on volume."
    },
    "PURGE_MOVE": {
        "device": "fillhead",
        "params": [
            {"name": "Volume(ml)", "type": float, "min": 0, "max": 1000},
            {"name": "Speed(ml/s)", "type": float, "min": 0.01, "max": 10, "optional": True, "default": 0.5},
            {"name": "Accel(sps^2)", "type": float, "min": 100, "max": 100000, "optional": True, "default": 5000},
            {"name": "Steps/ml", "type": float, "min": 0, "max": 1000000},
            {"name": "Torque(%)", "type": float, "min": 0, "max": 100, "optional": True, "default": 50}
        ],
        "help": "Executes a purge move to expel a certain volume of material."
    },
    "SET_HEATER_SETPOINT": {
        "device": "fillhead",
        "params": [{"name": "Temp(°C)", "type": float, "min": 20, "max": 150}],
        "help": "Sets the target temperature for the heater PID."
    },
    "SET_HEATER_GAINS": {
        "device": "fillhead",
        "params": [
            {"name": "Kp", "type": float, "min": 0, "max": 1000},
            {"name": "Ki", "type": float, "min": 0, "max": 1000},
            {"name": "Kd", "type": float, "min": 0, "max": 1000}
        ],
        "help": "Sets the PID gains for the heater."
    },
    "MACHINE_HOME_MOVE": {
        "device": "fillhead",
        "params": [],
        "help": "Homes the main machine axis using parameters from the manual controls tab."
    },
    "CARTRIDGE_HOME_MOVE": {
        "device": "fillhead",
        "params": [],
        "help": "Homes the injector against the cartridge using parameters from the manual controls tab."
    },
    "INJECTION_VALVE_HOME": {"device": "fillhead", "params": [], "help": "Homes the injection pinch valve."},
    "INJECTION_VALVE_OPEN": {"device": "fillhead", "params": [], "help": "Opens the injection pinch valve."},
    "INJECTION_VALVE_CLOSE": {"device": "fillhead", "params": [], "help": "Closes the injection pinch valve."},
    "INJECTION_VALVE_JOG": {
        "device": "fillhead",
        "params": [{"name": "Dist(mm)", "type": float, "min": -50, "max": 50}],
        "help": "Jogs the injection pinch valve by a relative distance."
    },
    "VACUUM_VALVE_HOME": {"device": "fillhead", "params": [], "help": "Homes the vacuum pinch valve."},
    "VACUUM_VALVE_OPEN": {"device": "fillhead", "params": [], "help": "Opens the vacuum pinch valve."},
    "VACUUM_VALVE_CLOSE": {"device": "fillhead", "params": [], "help": "Closes the vacuum pinch valve."},
    "VACUUM_VALVE_JOG": {
        "device": "fillhead",
        "params": [{"name": "Dist(mm)", "type": float, "min": -50, "max": 50}],
        "help": "Jogs the vacuum pinch valve by a relative distance."
    },
    "MOVE_TO_CARTRIDGE_HOME": {"device": "fillhead", "params": [],
                               "help": "Moves the injector to the zero position of the cartridge."},
    "PAUSE_INJECTION": {"device": "fillhead", "params": [], "help": "Pauses an ongoing injection or purge."},
    "RESUME_INJECTION": {"device": "fillhead", "params": [], "help": "Resumes a paused injection or purge."},
    "CANCEL_INJECTION": {"device": "fillhead", "params": [], "help": "Cancels an injection or purge."},
    "ENABLE": {"device": "fillhead", "params": [], "help": "Enables all injector motors."},
    "DISABLE": {"device": "fillhead", "params": [], "help": "Disables all injector motors."},
    "HEATER_PID_ON": {"device": "fillhead", "params": [], "help": "Turns the heater PID controller on."},
    "HEATER_PID_OFF": {"device": "fillhead", "params": [], "help": "Turns the heater PID controller off."},
    "VACUUM_ON": {"device": "fillhead", "params": [], "help": "Turns the vacuum pump ON and opens the valve."},
    "VACUUM_OFF": {"device": "fillhead", "params": [], "help": "Turns the vacuum pump OFF and closes the valve."},

    # --- Gantry Commands ---
    "MOVE_X": {"device": "gantry", "params": [{"name": "Dist(mm)", "type": float, "min": -2000, "max": 2000},
                                                {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 500,
                                                 "optional": True, "default": 50},
                                                {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000,
                                                 "optional": True, "default": 200},
                                                {"name": "Torque(%)", "type": float, "min": 0, "max": 100,
                                                 "optional": True, "default": 25}],
               "help": "Moves the gantry X-axis by a relative distance."},
    "MOVE_Y": {"device": "gantry", "params": [{"name": "Dist(mm)", "type": float, "min": -1000, "max": 1000},
                                                {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 500,
                                                 "optional": True, "default": 50},
                                                {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000,
                                                 "optional": True, "default": 200},
                                                {"name": "Torque(%)", "type": float, "min": 0, "max": 100,
                                                 "optional": True, "default": 25}],
               "help": "Moves the gantry Y-axis by a relative distance."},
    "MOVE_Z": {"device": "gantry", "params": [{"name": "Dist(mm)", "type": float, "min": -1000, "max": 1000},
                                                {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 200,
                                                 "optional": True, "default": 50},
                                                {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000,
                                                 "optional": True, "default": 200},
                                                {"name": "Torque(%)", "type": float, "min": 0, "max": 100,
                                                 "optional": True, "default": 25}],
               "help": "Moves the gantry Z-axis by a relative distance."},
    "HOME_X": {
        "device": "gantry",
        "params": [{"name": "Max-Dist(mm)", "type": float, "min": 1, "max": 2000, "optional": True}],
        "help": "Homes the gantry X-axis. Searches up to Max-Dist(mm). If no distance is given, it uses the axis travel limit."
    },
    "HOME_Y": {
        "device": "gantry",
        "params": [{"name": "Max-Dist(mm)", "type": float, "min": 1, "max": 1000, "optional": True}],
        "help": "Homes the gantry Y-axis. Searches up to Max-Dist(mm). If no distance is given, it uses the axis travel limit."
    },
    "HOME_Z": {
        "device": "gantry",
        "params": [{"name": "Max-Dist(mm)", "type": float, "min": 1, "max": 1000, "optional": True}],
        "help": "Homes the gantry Z-axis. Searches up to Max-Dist(mm). If no distance is given, it uses the axis travel limit."
    },
    "START_DEMO": {"device": "gantry", "params": [], "help": "Starts the circle demo on the gantry."},

    # --- Global Commands ---
    "ABORT": {"device": "both", "params": [], "help": "Stops all motion on the target device."},

    # --- Script-Control Commands ---
    "VACUUM_CHECK": { "device": "script", "params": [], "help": "Checks for vacuum leak based on set parameters."},
    "SET_VACUUM_CHECK_PASS_PRESS_DROP": { "device": "script", "params": [{"name": "Max-PSI-Increase", "type": float, "min": 0, "max": 10}], "help": "Sets the max allowed pressure increase for VACUUM_CHECK."},
    "SET_VACUUM_CHECK_DURATION": { "device": "script", "params": [{"name": "Duration(s)", "type": float, "min": 1, "max": 300}], "help": "Sets the duration for VACUUM_CHECK."},
    "WAIT_MS": {"device": "script", "params": [{"name": "Milliseconds", "type": float, "min": 0, "max": 600000}], "help": "Pauses script execution for a given time in milliseconds."},
    "WAIT_S": {"device": "script", "params": [{"name": "Seconds", "type": float, "min": 0, "max": 600}], "help": "Pauses script execution for a given time in seconds."},
    "WAIT_UNTIL_VACUUM": {"device": "script",
                          "params": [{"name": "Target-PSI", "type": float, "min": -14.5, "max": 0, "optional": True},
                                     {"name": "Timeout(s)", "type": float, "min": 1, "max": 600, "optional": True}],
                          "help": "Pauses until vacuum reaches the target pressure."},
    "WAIT_UNTIL_HEATER_AT_TEMP": {"device": "script", "params": [
        {"name": "Target-Temp(C)", "type": float, "min": 20, "max": 150, "optional": True},
        {"name": "Timeout(s)", "type": float, "min": 1, "max": 600, "optional": True}],
                                  "help": "Pauses until the heater reaches the target temperature."},
    "SET_DEFAULT_MOVE_VEL": {"device": "script",
                             "params": [{"name": "Speed(mm/s)", "type": float, "min": 1, "max": 200}],
                             "help": "Sets the default velocity for subsequent MOVE commands."},
    "SET_DEFAULT_MOVE_ACC": {"device": "script",
                             "params": [{"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000}],
                             "help": "Sets the default acceleration for subsequent MOVE commands."},
    "SET_DEFAULT_MOVE_TORQUE": {"device": "script",
                                "params": [{"name": "Torque(%)", "type": float, "min": 0, "max": 100}],
                                "help": "Sets the default torque for subsequent MOVE commands."},
    "SET_DEFAULT_VACUUM_TARGET": {"device": "script",
                                  "params": [{"name": "Target-PSI", "type": float, "min": -14.5, "max": 0}],
                                  "help": "Sets the default vacuum target for WAIT_UNTIL_VACUUM."},
    "SET_DEFAULT_VACUUM_TIMEOUT": {"device": "script",
                                   "params": [{"name": "Timeout(s)", "type": float, "min": 1, "max": 600}],
                                   "help": "Sets the default timeout for WAIT_UNTIL_VACUUM."},
    "SET_DEFAULT_HEATER_TIMEOUT": {"device": "script",
                                   "params": [{"name": "Timeout(s)", "type": float, "min": 1, "max": 600}],
                                   "help": "Sets the default timeout for WAIT_UNTIL_HEATER_AT_TEMP."}
}


def _validate_line(line_content, line_num):
    """Helper function to validate a single, non-empty, non-comment line."""
    errors = []
    sub_commands = line_content.strip().split(',')

    for sub_cmd_str in sub_commands:
        sub_cmd_str = sub_cmd_str.strip()
        if not sub_cmd_str:
            continue

        parts = sub_cmd_str.split()
        command_word = parts[0].upper()
        args = parts[1:]

        if command_word not in COMMANDS:
            errors.append({"line": line_num, "error": f"In '{sub_cmd_str}': Unknown command '{command_word}'."})
            continue

        command_info = COMMANDS[command_word]
        params_def = command_info['params']
        num_required_params = sum(1 for p in params_def if not p.get("optional"))

        if len(args) < num_required_params or len(args) > len(params_def):
            expected = f"{num_required_params}" if num_required_params == len(params_def) else f"{num_required_params}-{len(params_def)}"
            errors.append({"line": line_num, "error": f"In '{sub_cmd_str}': Incorrect parameter count for '{command_word}'. Expected {expected}, but got {len(args)}."})
            continue

        for j, arg in enumerate(args):
            param_def = params_def[j]
            param_name = param_def['name']
            try:
                value = float(arg)
            except ValueError:
                errors.append({"line": line_num, "error": f"In '{sub_cmd_str}': Parameter '{param_name}' must be a number, but got '{arg}'."})
                continue

            if 'min' in param_def and value < param_def['min']:
                errors.append({"line": line_num, "error": f"In '{sub_cmd_str}': Parameter '{param_name}' is below minimum of {param_def['min']}. Got {value}."})

            if 'max' in param_def and value > param_def['max']:
                errors.append({"line": line_num, "error": f"In '{sub_cmd_str}': Parameter '{param_name}' is above maximum of {param_def['max']}. Got {value}."})

    return errors

def validate_script(script_content):
    """Validates an entire script against the command reference."""
    errors = []
    lines = script_content.splitlines()
    for i, line in enumerate(lines):
        line_num = i + 1
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        errors.extend(_validate_line(line, line_num))
    return errors

def validate_single_line(line_content, line_num):
    """Validates a single line of a script."""
    line = line_content.strip()
    if not line or line.startswith('#'):
        return []
    return _validate_line(line, line_num)

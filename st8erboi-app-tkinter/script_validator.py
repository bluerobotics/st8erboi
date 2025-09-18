# script_validator.py

"""
Contains the command reference with validation rules (min/max) and default values.
Also contains the script validation logic.
"""

import re


COMMANDS = {
    # --- Fillhead Commands (Formerly Injector) ---
    "INJECT_STATOR": {
        "device": "fillhead",
        "params": [
            {"name": "Volume(ml)", "type": float, "min": 0, "max": 1000},
            {"name": "Speed(ml/s)", "type": float, "min": 0.01, "max": 5.0, "optional": True, "default": 0.25}
        ],
        "help": "Injects a specific volume using the Stator (5:1) cartridge settings."
    },
    "INJECT_ROTOR": {
        "device": "fillhead",
        "params": [
            {"name": "Volume(ml)", "type": float, "min": 0, "max": 1000},
            {"name": "Speed(ml/s)", "type": float, "min": 0.01, "max": 5.0, "optional": True, "default": 0.25}
        ],
        "help": "Injects a specific volume using the Rotor (1:1) cartridge settings."
    },
    "JOG_MOVE": {
        "device": "fillhead",
        "params": [
            {"name": "Dist-M0(mm)", "type": float, "min": -100, "max": 100},
            {"name": "Dist-M1(mm)", "type": float, "min": -100, "max": 100},
            {"name": "Speed(mm/s)", "type": float, "min": 0.01, "max": 5.0, "optional": True, "default": 1.0},
            {"name": "Accel(mm/s^2)", "type": float, "min": 1, "max": 50.0, "optional": True, "default": 10.0},
            {"name": "Torque(%)", "type": float, "min": 0, "max": 100, "optional": True, "default": 20}
        ],
        "help": "Jogs the injector motors by a relative distance. M0 is Machine, M1 is Cartridge."
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
        "help": "Homes the main machine axis using hardcoded parameters from firmware."
    },
    "CARTRIDGE_HOME_MOVE": {
        "device": "fillhead",
        "params": [],
        "help": "Homes the injector against the cartridge using hardcoded parameters from firmware."
    },
    "INJECTION_VALVE_CLOSE": {"device": "fillhead", "params": [], "help": "Closes the injection pinch valve."},
    "INJECTION_VALVE_HOME_TUBED": {"device": "fillhead", "params": [], "help": "Homes the injection pinch valve with a tube installed."},
    "INJECTION_VALVE_HOME_UNTUBED": {"device": "fillhead", "params": [], "help": "Homes the injection pinch valve without a tube installed."},
    "INJECTION_VALVE_JOG": {
        "device": "fillhead",
        "params": [{"name": "Dist(mm)", "type": float, "min": -50, "max": 50}],
        "help": "Jogs the injection pinch valve by a relative distance."
    },
    "INJECTION_VALVE_OPEN": {"device": "fillhead", "params": [], "help": "Opens the injection pinch valve."},
    "VACUUM_VALVE_CLOSE": {"device": "fillhead", "params": [], "help": "Closes the vacuum pinch valve."},
    "VACUUM_VALVE_HOME_TUBED": {"device": "fillhead", "params": [], "help": "Homes the vacuum pinch valve with a tube installed."},
    "VACUUM_VALVE_HOME_UNTUBED": {"device": "fillhead", "params": [], "help": "Homes the vacuum pinch valve without a tube installed."},
    "VACUUM_VALVE_JOG": {
        "device": "fillhead",
        "params": [{"name": "Dist(mm)", "type": float, "min": -50, "max": 50}],
        "help": "Jogs the vacuum pinch valve by a relative distance."
    },
    "VACUUM_VALVE_OPEN": {"device": "fillhead", "params": [], "help": "Opens the vacuum pinch valve."},
    "MOVE_TO_CARTRIDGE_HOME": {"device": "fillhead", "params": [],
                               "help": "Moves the injector to the zero position of the cartridge."},
    "PAUSE_INJECTION": {"device": "fillhead", "params": [], "help": "Pauses an ongoing injection or purge."},
    "RESUME_INJECTION": {"device": "fillhead", "params": [], "help": "Resumes a paused injection or purge."},
    "CANCEL_INJECTION": {"device": "fillhead", "params": [], "help": "Cancels an injection or purge."},
    "ENABLE": {"device": "fillhead", "params": [], "help": "Enables all injector motors."},
    "DISABLE": {"device": "fillhead", "params": [], "help": "Disables all injector motors."},
    "HEATER_ON": {"device": "fillhead", "params": [], "help": "Turns the heater PID controller on."},
    "HEATER_OFF": {"device": "fillhead", "params": [], "help": "Turns the heater PID controller off."},
    "VACUUM_ON": {"device": "fillhead", "params": [], "help": "Turns the vacuum pump ON and opens the valve."},
    "VACUUM_OFF": {"device": "fillhead", "params": [], "help": "Turns the vacuum pump OFF and closes the valve."},

    # --- NEW: Firmware-based Vacuum Test Commands ---
    "VACUUM_LEAK_TEST": {
        "device": "fillhead",
        "params": [],
        "help": "Starts the automated vacuum leak test sequence in the firmware."
    },
    "SET_VACUUM_TARGET": {
        "device": "fillhead",
        "params": [{"name": "Target(PSIG)", "type": float, "min": -14.5, "max": 0}],
        "help": "Sets the target pressure for the vacuum system."
    },
    "SET_VACUUM_TIMEOUT_S": {
        "device": "fillhead",
        "params": [{"name": "Timeout(s)", "type": float, "min": 1, "max": 300}],
        "help": "Sets the timeout for reaching the vacuum target."
    },
    "SET_LEAK_TEST_DELTA": {
        "device": "fillhead",
        "params": [{"name": "Delta(PSI)", "type": float, "min": 0.01, "max": 5.0}],
        "help": "Sets the maximum allowed pressure drop for the leak test."
    },
    "SET_LEAK_TEST_DURATION_S": {
        "device": "fillhead",
        "params": [{"name": "Duration(s)", "type": float, "min": 1, "max": 300}],
        "help": "Sets the duration of the leak test measurement period."
    },

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
    "ENABLE_X": {"device": "gantry", "params": [], "help": "Enables the gantry X-axis motor."},
    "ENABLE_Y": {"device": "gantry", "params": [], "help": "Enables the gantry Y-axis motor."},
    "ENABLE_Z": {"device": "gantry", "params": [], "help": "Enables the gantry Z-axis motor."},
    "DISABLE_X": {"device": "gantry", "params": [], "help": "Disables the gantry X-axis motor."},
    "DISABLE_Y": {"device": "gantry", "params": [], "help": "Disables the gantry Y-axis motor."},
    "DISABLE_Z": {"device": "gantry", "params": [], "help": "Disables the gantry Z-axis motor."},
    "START_DEMO": {"device": "gantry", "params": [], "help": "Starts the circle demo on the gantry."},

    # --- Global Commands ---
    "ABORT": {"device": "both", "params": [], "help": "Stops all motion on the target device."},

    # --- Script-Control Commands ---
    "CYCLE": {
        "device": "script",
        "params": [{"name": "Count", "type": int, "min": 1, "max": 10000}],
        "help": "Cycles through the following indented block of commands 'Count' times."
    },
    "WAIT_MS": {"device": "script", "params": [{"name": "Milliseconds", "type": float, "min": 0, "max": 600000}],
                "help": "Pauses script execution for a given time in milliseconds."},
    "WAIT": {"device": "script", "params": [{"name": "Seconds", "type": float, "min": 0, "max": 600}],
               "help": "Pauses script execution for a given time in seconds."},
    "WAIT_UNTIL_VACUUM": {"device": "script",
                          "params": [{"name": "Target-PSI", "type": float, "min": -14.5, "max": 0, "optional": True},
                                     {"name": "Timeout(s)", "type": float, "min": 1, "max": 600, "optional": True}],
                          "help": "Pauses until vacuum reaches the target pressure."},
    "WAIT_UNTIL_HEATER_AT_TEMP": {"device": "script", "params": [
        {"name": "Target-Temp(C)", "type": float, "min": 20, "max": 150},
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
    
    command_word_for_line = sub_commands[0].strip().split()[0].upper()
    if (command_word_for_line == "CYCLE" or command_word_for_line == "END_REPEAT") and len(sub_commands) > 1:
        errors.append({"line": line_num, "error": "CYCLE and END_REPEAT commands must be on their own line."})
        return errors # Stop processing this line as it's fundamentally malformed

    for sub_cmd_str in sub_commands:
        sub_cmd_str = sub_cmd_str.strip()
        if not sub_cmd_str:
            continue

        parts = sub_cmd_str.split()
        command_word = parts[0].upper()

        # --- NEW: Extract only numeric parts for arguments ---
        args = []
        for part in parts[1:]:
            # Match a floating point or integer number at the start of the string.
            # This allows for comments/units after the number (e.g., "5 ml", "100_ms")
            match = re.match(r'^-?\d+(\.\d+)?', part)
            if match:
                args.append(match.group(0))

        if command_word not in COMMANDS:
            errors.append({"line": line_num, "error": f"In '{sub_cmd_str}': Unknown command '{command_word}'."})
            continue

        command_info = COMMANDS[command_word]
        params_def = command_info['params']
        num_required_params = sum(1 for p in params_def if not p.get("optional"))

        if len(args) < num_required_params:
            errors.append({"line": line_num,
                           "error": f"In '{sub_cmd_str}': Not enough numeric parameters for '{command_word}'. Expected at least {num_required_params}, but found {len(args)}."})
            continue

        # Allow more "arguments" than defined, since they are comments, but only validate the ones that map to params.
        args_to_validate = args[:len(params_def)]

        for j, arg in enumerate(args_to_validate):
            param_def = params_def[j]
            param_name = param_def['name']
            try:
                value = float(arg)
            except ValueError:
                errors.append({"line": line_num,
                               "error": f"In '{sub_cmd_str}': Parameter '{param_name}' must be a number, but got '{arg}'."})
                continue

            if 'min' in param_def and value < param_def['min']:
                errors.append({"line": line_num,
                               "error": f"In '{sub_cmd_str}': Parameter '{param_name}' is below minimum of {param_def['min']}. Got {value}."})

            if 'max' in param_def and value > param_def['max']:
                errors.append({"line": line_num,
                               "error": f"In '{sub_cmd_str}': Parameter '{param_name}' is above maximum of {param_def['max']}. Got {value}."})

    return errors


def validate_script(script_content):
    """Validates an entire script against the command reference."""
    errors = []
    lines = script_content.splitlines()
    indent_stack = [0]  # Stack of indentation levels (in spaces)

    for i, line in enumerate(lines):
        line_num = i + 1
        line_content = line.strip()
        if not line_content or line_content.startswith('#'):
            continue

        leading_spaces = len(line) - len(line.lstrip(' '))
        
        # Check indentation rules
        if leading_spaces > indent_stack[-1]:
            # Indent should only happen after a REPEAT command
            prev_line_idx = i - 1
            prev_line_content = ""
            # Find the previous non-empty line
            while prev_line_idx >= 0:
                prev_line_content = lines[prev_line_idx].strip()
                if prev_line_content:
                    break
                prev_line_idx -= 1

            if not prev_line_content.upper().startswith("CYCLE"):
                errors.append({"line": line_num, "error": "Unexpected indent."})
            indent_stack.append(leading_spaces)
        elif leading_spaces < indent_stack[-1]:
            # Dedent must match a previous indentation level
            while indent_stack and leading_spaces < indent_stack[-1]:
                indent_stack.pop()
            if not indent_stack or leading_spaces != indent_stack[-1]:
                errors.append({"line": line_num, "error": "Dedent does not match any outer indentation level."})

        # Validate the command itself
        first_command_word = line_content.split(',')[0].strip().split()[0].upper()
        if first_command_word == "END_REPEAT":
            errors.append({"line": line_num, "error": "END_REPEAT is no longer used. Use indentation to define blocks."})
            continue

        errors.extend(_validate_line(line, line_num))

    if len(indent_stack) > 1:
        errors.append({"line": len(lines), "error": "Unexpected end of file: missing dedent for a CYCLE block."})

    return errors


def validate_single_line(line_content, line_num):
    """Validates a single line of a script."""
    line = line_content.strip()
    if not line or line.startswith('#'):
        return []
    return _validate_line(line, line_num)

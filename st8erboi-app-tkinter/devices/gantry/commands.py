def get_command_functions(send_cmd_func, shared_gui_refs):
    """
    Returns a dictionary of functions that can be called directly
    by the GUI or script processor.
    """
    return {
        "home_x": lambda: send_cmd_func("HOME_X"),
        "home_y": lambda: send_cmd_func("HOME_Y"),
        "home_z": lambda: send_cmd_func("HOME_Z"),
        "enable_x": lambda: send_cmd_func("ENABLE_X"),
        "enable_y": lambda: send_cmd_func("ENABLE_Y"),
        "enable_z": lambda: send_cmd_func("ENABLE_Z"),
        "disable_x": lambda: send_cmd_func("DISABLE_X"),
        "disable_y": lambda: send_cmd_func("DISABLE_Y"),
        "disable_z": lambda: send_cmd_func("DISABLE_Z"),
        "start_demo": lambda: send_cmd_func("START_DEMO"),
        
        # For script processor compatibility
        "send_gantry": send_cmd_func
    }

def get_scripting_commands():
    """
    Returns a dictionary of commands for the scripting engine and command reference.
    """
    return {
        "MOVE_X": {"device": "gantry", "params": [{"name": "Dist(mm)", "type": float, "min": -2000, "max": 2000},
                                                 {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 500, "optional": True, "default": 50},
                                                 {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000, "optional": True, "default": 200},
                                                 {"name": "Torque(%)", "type": float, "min": 0, "max": 100, "optional": True, "default": 25}],
                   "help": "Moves the gantry X-axis by a relative distance."},
        "MOVE_Y": {"device": "gantry", "params": [{"name": "Dist(mm)", "type": float, "min": -1000, "max": 1000},
                                                 {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 500, "optional": True, "default": 50},
                                                 {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000, "optional": True, "default": 200},
                                                 {"name": "Torque(%)", "type": float, "min": 0, "max": 100, "optional": True, "default": 25}],
                   "help": "Moves the gantry Y-axis by a relative distance."},
        "MOVE_Z": {"device": "gantry", "params": [{"name": "Dist(mm)", "type": float, "min": -1000, "max": 1000},
                                                 {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 200, "optional": True, "default": 50},
                                                 {"name": "Accel(mm/s^2)", "type": float, "min": 10, "max": 10000, "optional": True, "default": 200},
                                                 {"name": "Torque(%)", "type": float, "min": 0, "max": 100, "optional": True, "default": 25}],
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
        "START_DEMO": {"device": "gantry", "params": [], "help": "Starts the circle demo on the gantry."}
    }

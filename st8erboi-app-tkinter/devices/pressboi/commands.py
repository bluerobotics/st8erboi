COMMANDS = {
    "PRESS_HOME": {
        "device": "pressboi",
        "params": [],
        "help": "Homes both axes of the press."
    },
    "PRESS_MOVE_ABS": {
        "device": "pressboi",
        "params": [
            {"name": "M0-Pos(mm)", "type": float, "min": 0, "max": 100},
            {"name": "M1-Pos(mm)", "type": float, "min": 0, "max": 100},
            {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 50, "optional": True, "default": 10}
        ],
        "help": "Moves both press axes to absolute positions."
    },
    "PRESS_MOVE_REL": {
        "device": "pressboi",
        "params": [
            {"name": "M0-Dist(mm)", "type": float, "min": -100, "max": 100},
            {"name": "M1-Dist(mm)", "type": float, "min": -100, "max": 100},
            {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 50, "optional": True, "default": 10}
        ],
        "help": "Moves both press axes by a relative distance."
    },
    "PRESS_JOG": {
        "device": "pressboi",
        "params": [
            {"name": "Motor(0/1)", "type": int, "min": 0, "max": 1},
            {"name": "Dist(mm)", "type": float, "min": -20, "max": 20},
            {"name": "Speed(mm/s)", "type": float, "min": 1, "max": 50, "optional": True, "default": 5}
        ],
        "help": "Jogs a single motor on the press by a relative distance."
    },
    "PRESS_ENABLE": {
        "device": "pressboi",
        "params": [],
        "help": "Enables the press motors."
    },
    "PRESS_DISABLE": {
        "device": "pressboi",
        "params": [],
        "help": "Disables the press motors."
    }
}

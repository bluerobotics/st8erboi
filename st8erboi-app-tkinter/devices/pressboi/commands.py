def get_commands():
    """
    Returns a dictionary of commands for the Pressboi device.
    """
    # Helper for repetitive move commands
    def move_params(is_optional=False):
        return [
            {"name": "Pos(mm)", "type": float, "min": -500, "max": 500, "optional": is_optional},
            {"name": "Speed(mm/s)", "type": float, "min": 0.1, "max": 100, "optional": True},
            {"name": "Force(N)", "type": float, "min": 0, "max": 50000, "optional": True}
        ]

    return {
        "PRESSBOI_HOME": {"device": "pressboi", "params": [], "help": "Homes the press."},
        "PRESSBOI_MOVE": {"device": "pressboi", "params": move_params(), "help": "Moves the press to an absolute position."},
        "PRESSBOI_SET_START_POS": {"device": "pressboi", "params": move_params(is_optional=True), "help": "Sets the starting position for relative moves."},
        "PRESSBOI_MOVE_TO_START": {"device": "pressboi", "params": move_params(is_optional=True), "help": "Moves to the defined start position."},
        "PRESSBOI_APPROACH": {"device": "pressboi", "params": move_params(), "help": "Moves to a position to begin a search or press."},
        "PRESSBOI_SEARCH": {"device": "pressboi", "params": move_params(), "help": "Searches for contact."},
        "PRESSBOI_PRESS": {"device": "pressboi", "params": move_params(), "help": "Presses to a specified position or force."},
        "PRESSBOI_STOP": {"device": "pressboi", "params": [{"name": "Duration(s)", "type": float, "min": 0, "max": 3600, "optional": True}], "help": "Stops motion for a duration."},
        "PRESSBOI_DEPRESS": {"device": "pressboi", "params": move_params(), "help": "Moves to release pressure."},
        "PRESSBOI_RETURN": {"device": "pressboi", "params": move_params(), "help": "Returns to a safe or start position."},
        "PRESSBOI_CLEAR_ERRORS": {"device": "pressboi", "params": [], "help": "Clears any active errors on the device."},
        "PRESSBOI_ENABLE": {"device": "pressboi", "params": [], "help": "Enables the motors."},
        "PRESSBOI_DISABLE": {"device": "pressboi", "params": [], "help": "Disables the motors."}
    }

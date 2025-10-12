
def get_schema():
    """
    Returns the telemetry schema for this device.
    This is used by the simulator to generate mock telemetry data.
    """
    return {
        'gantry_state': 'STANDBY', 'x_st': 'Standby', 'y_st': 'Standby', 'z_st': 'Standby',
        'x_p': 0.0, 'x_t': 0.0, 'x_e': 1, 'x_h': 0,
        'y_p': 0.0, 'y_t': 0.0, 'y_e': 1, 'y_h': 0,
        'z_p': 0.0, 'z_t': 0.0, 'z_e': 1, 'z_h': 0,
    }

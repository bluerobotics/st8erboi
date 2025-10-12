
def get_schema():
    """
    Returns the telemetry schema for this device.
    This is used by the simulator to generate mock telemetry data.
    """
    return {
        'MAIN_STATE': 'STANDBY',
        'pressure_psi': 0.0,
        'enabled': 1,
        'error_code': 0,
        'cycles_programmed': 10,
        'cycles_complete': 0,
        'tank1_temp_c': 25.0,
        'tank2_temp_c': 25.1
    }

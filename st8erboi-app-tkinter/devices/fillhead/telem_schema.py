
def get_schema():
    """
    Returns the telemetry schema for this device.
    This is used by the simulator to generate mock telemetry data.
    """
    return {
        'MAIN_STATE': 'STANDBY',
        'inj_t0': 0.0, 'inj_t1': 0.0, 'inj_h_mach': 0, 'inj_h_cart': 0,
        'inj_mach_mm': 0.0, 'inj_cart_mm': 0.0,
        'inj_cumulative_ml': 0.0, 'inj_active_ml': 0.0, 'inj_tgt_ml': 0.0,
        'enabled0': 1, 'enabled1': 1,
        'inj_valve_pos': 0.0, 'inj_valve_torque': 0.0, 'inj_valve_homed': 0, 'inj_valve_state': 0,
        'vac_valve_pos': 0.0, 'vac_valve_torque': 0.0, 'vac_valve_homed': 0, 'vac_valve_state': 0,
        'h_st': 0, 'h_sp': 70.0, 'h_pv': 25.0, 'h_op': 0.0,
        'vac_st': 0, 'vac_pv': 0.5, 'vac_sp': -14.0,
        'inj_st': 'Standby', 'inj_v_st': 'Not Homed', 'vac_v_st': 'Not Homed',
        'h_st_str': 'Off', 'vac_st_str': 'Off',
    }

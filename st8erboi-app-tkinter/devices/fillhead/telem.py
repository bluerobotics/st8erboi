

def parse_telemetry(msg, gui_refs, queue_ui_update, safe_float):
    """Parses the telemetry string from the Fillhead controller."""
    if "FILLHEAD_TELEM: " not in msg:
        return
            
    try:
        payload = msg.split("FILLHEAD_TELEM: ")[1]
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)

        # Main Controller States
        queue_ui_update(gui_refs, 'fillhead_main_state_var', parts.get("MAIN_STATE", "---"))

        # Injector Motors
        queue_ui_update(gui_refs, 'fillhead_torque0_var', parts.get('inj_t0', '0.0'))
        queue_ui_update(gui_refs, 'fillhead_torque1_var', parts.get('inj_t1', '0.0'))
        queue_ui_update(gui_refs, 'fillhead_enabled_state1_var', "Enabled" if parts.get("enabled0", "0") == "1" else "Disabled")
        queue_ui_update(gui_refs, 'fillhead_enabled_state2_var', "Enabled" if parts.get("enabled1", "0") == "1" else "Disabled")
        queue_ui_update(gui_refs, 'fillhead_homed0_var', "Homed" if parts.get("inj_h_mach", "0") == "1" else "Not Homed")
        queue_ui_update(gui_refs, 'fillhead_homed1_var', "Homed" if parts.get("inj_h_cart", "0") == "1" else "Not Homed")

        # Component States
        queue_ui_update(gui_refs, 'fillhead_injector_state_var', parts.get("inj_st", "---"))
        queue_ui_update(gui_refs, 'fillhead_inj_valve_state_var', parts.get("inj_v_st", "---"))
        queue_ui_update(gui_refs, 'fillhead_vac_valve_state_var', parts.get("vac_v_st", "---"))
        queue_ui_update(gui_refs, 'fillhead_heater_state_var', parts.get("h_st_str", "---"))
        queue_ui_update(gui_refs, 'fillhead_vacuum_state_var', parts.get("vac_st_str", "---"))
        
        # Pinch Valves
        queue_ui_update(gui_refs, 'fillhead_inj_valve_pos_var', parts.get("inj_valve_pos", "---"))
        queue_ui_update(gui_refs, 'fillhead_inj_valve_homed_var', "Homed" if parts.get("inj_valve_homed", "0") == "1" else "Not Homed")
        queue_ui_update(gui_refs, 'fillhead_torque2_var', parts.get('inj_valve_torque', '0.0'))
        queue_ui_update(gui_refs, 'fillhead_vac_valve_pos_var', parts.get("vac_valve_pos", "---"))
        queue_ui_update(gui_refs, 'fillhead_vac_valve_homed_var', "Homed" if parts.get("vac_valve_homed", "0") == "1" else "Not Homed")
        queue_ui_update(gui_refs, 'fillhead_torque3_var', parts.get('vac_valve_torque', '0.0'))

        # Other System Status
        queue_ui_update(gui_refs, 'fillhead_machine_steps_var', parts.get("inj_mach_mm", "N/A"))
        queue_ui_update(gui_refs, 'fillhead_cartridge_steps_var', parts.get("inj_cart_mm", "N/A"))
        queue_ui_update(gui_refs, 'fillhead_inject_cumulative_ml_var', f'{safe_float(parts.get("inj_cumulative_ml")):.2f} ml')
        queue_ui_update(gui_refs, 'fillhead_inject_active_ml_var', f'{safe_float(parts.get("inj_active_ml")):.2f} ml')
        queue_ui_update(gui_refs, 'fillhead_injection_target_ml_var', f'{safe_float(parts.get("inj_tgt_ml")):.2f}')
        queue_ui_update(gui_refs, 'fillhead_temp_c_var', f'{safe_float(parts.get("h_pv")):.1f} °C')
        queue_ui_update(gui_refs, 'fillhead_pid_setpoint_var', f'{safe_float(parts.get("h_sp")):.1f}')
        queue_ui_update(gui_refs, 'fillhead_vacuum_psig_var', f"{safe_float(parts.get('vac_pv')):.2f} PSIG")

    except Exception as e:
        log_func = gui_refs.get('log_func')
        if log_func:
            log_func(f"Fillhead telem parse error: {e}")

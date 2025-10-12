

def parse_telemetry(msg, gui_refs, queue_ui_update, safe_float):
    """Parses the telemetry string from the Gantry controller."""
    if "GANTRY_TELEM:" not in msg:
        return
        
    try:
        payload = msg.split("GANTRY_TELEM:")[1]
        parts = {k.strip().lower(): v for k, v in (item.split(':', 1) for item in payload.split(',') if ':' in item)}

        # Update Gantry State
        queue_ui_update(gui_refs, 'gantry_main_state_var', parts.get("gantry_state", "---"))

        # Update Axis States
        axes = ['x', 'y', 'z']
        
        for axis in axes:
            queue_ui_update(gui_refs, f'gantry_{axis}_pos_var', parts.get(f"{axis}_p", "---"))
            queue_ui_update(gui_refs, f'gantry_{axis}_torque_var', parts.get(f"{axis}_t", '0.0'))
            queue_ui_update(gui_refs, f'gantry_{axis}_enabled_var', "Enabled" if parts.get(f"{axis}_e", "0") == "1" else "Disabled")
            queue_ui_update(gui_refs, f'gantry_{axis}_homed_var', "Homed" if parts.get(f"{axis}_h", "0") == "1" else "Not Homed")
            queue_ui_update(gui_refs, f'gantry_{axis}_state_var', parts.get(f"{axis}_st", "---"))

    except Exception as e:
        log_func = gui_refs.get('log_func')
        if log_func:
            log_func(f"Gantry telem parse error: {e}")



def parse_telemetry(msg, gui_refs, queue_ui_update, safe_float):
    """Parses the telemetry string from the Gantry controller."""
    try:
        payload = msg.split("GANTRY_TELEM:")[1]
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)

        # Update Gantry State
        queue_ui_update(gui_refs, 'fh_state_var', parts.get("gantry_state", "---"))

        # Update Axis States
        axes = ['x', 'y', 'z']
        fh_map = {'x': 'm0', 'y': 'm1', 'z': 'm3'}

        for axis in axes:
            key_suffix = fh_map[axis]
            queue_ui_update(gui_refs, f'fh_pos_{key_suffix}_var', parts.get(f"{axis}_p", "---"))
            queue_ui_update(gui_refs, f'fh_torque_{key_suffix}_var', parts.get(f"{axis}_t", '0.0'))
            queue_ui_update(gui_refs, f'fh_enabled_{key_suffix}_var', "Enabled" if parts.get(f"{axis}_e", "0") == "1" else "Disabled")
            queue_ui_update(gui_refs, f'fh_homed_{key_suffix}_var', "Homed" if parts.get(f"{axis}_h", "0") == "1" else "Not Homed")
            queue_ui_update(gui_refs, f'fh_state_{axis}_var', parts.get(f"{axis}_st", "---"))

    except Exception as e:
        # It's better to have a generic log call here that doesn't depend on GUI elements
        print(f"Gantry telemetry parse error: {e} -> on msg: {msg}\n")

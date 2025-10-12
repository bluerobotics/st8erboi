
def parse_telemetry(msg, gui_refs, queue_ui_update, safe_float):
    """Parses telemetry from the Pressurizer device."""
    try:
        if "PRESSURIZER_TElem:" not in msg.upper(): # Make case-insensitive
            return

        payload = msg.split(":", 1)[1]
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)
        
        # Update the main state for the device panel header
        queue_ui_update(gui_refs, 'pressurizer_main_state_var', parts.get("MAIN_STATE", "---"))
        
        # Update pressure (and convert to msw)
        pressure_psi = safe_float(parts.get("pressure_psi", 0.0))
        pressure_msw = pressure_psi * 0.7032496149
        queue_ui_update(gui_refs, 'pressurizer_pressure_var', f"{pressure_msw:.2f}")
        
        # Update new fields
        queue_ui_update(gui_refs, 'pressurizer_cycles_prog_var', parts.get("cycles_programmed", "---"))
        queue_ui_update(gui_refs, 'pressurizer_cycles_comp_var', parts.get("cycles_complete", "---"))
        queue_ui_update(gui_refs, 'pressurizer_tank1_temp_var', f'{safe_float(parts.get("tank1_temp_c", 0.0)):.1f}')
        queue_ui_update(gui_refs, 'pressurizer_tank2_temp_var', f'{safe_float(parts.get("tank2_temp_c", 0.0)):.1f}')

    except Exception as e:
        print(f"Pressurizer telemetry parse error: {e} -> on msg: {msg}\n")

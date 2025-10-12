
def parse_telemetry(msg, gui_refs, queue_ui_update, safe_float):
    """Parses telemetry messages for the Pressurizer device."""
    # Make the check case-insensitive to handle potential firmware differences
    if "PRESSURIZER_TELEM:" not in msg.upper():
        return

    try:
        # Find the start of the payload, case-insensitively
        payload_start = msg.upper().find("PRESSURIZER_TELEM:") + len("PRESSURIZER_TELEM:")
        payload = msg[payload_start:].strip()
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)

        # Update main state
        queue_ui_update(gui_refs, 'pressurizer_main_state_var', parts.get("MAIN_STATE", "---"))

        # Update pressure
        pressure_psi = safe_float(parts.get("pressure_psi", 0.0))
        pressure_msw = pressure_psi * 0.7032496149
        queue_ui_update(gui_refs, 'pressurizer_pressure_var', f"{pressure_msw:.2f}")

        # Update cycle counts
        queue_ui_update(gui_refs, 'pressurizer_cycles_programmed_var', parts.get("cycles_programmed", "---"))
        queue_ui_update(gui_refs, 'pressurizer_cycles_complete_var', parts.get("cycles_complete", "---"))
        
        # Update tank temperatures
        queue_ui_update(gui_refs, 'pressurizer_tank1_temp_var', f'{safe_float(parts.get("tank1_temp_c", 0.0)):.1f}')
        queue_ui_update(gui_refs, 'pressurizer_tank2_temp_var', f'{safe_float(parts.get("tank2_temp_c", 0.0)):.1f}')

        # Update enabled status
        is_enabled = parts.get("enabled", "0") == "1"
        queue_ui_update(gui_refs, 'pressurizer_enabled_var', "Enabled" if is_enabled else "Disabled")

    except Exception as e:
        log_func = gui_refs.get('log_func')
        if log_func:
            log_func(f"Pressurizer telem parse error: {e}")

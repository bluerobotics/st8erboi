import tkinter as tk

def parse_telemetry(message, gui_refs, queue_ui_update, safe_float):
    """Parses telemetry messages for the Pressboi device."""
    if "PRESSBOI_TELEM:" not in message:
        return

    try:
        payload = message.split("PRESSBOI_TELEM: ")[1]
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)

        # Update main state
        queue_ui_update(gui_refs, 'pressboi_main_state_var', parts.get("MAIN_STATE", "---"))

        # Update pressure and temperature
        pressure_psi = safe_float(parts.get("pressure_psi", 0.0))
        queue_ui_update(gui_refs, 'pressboi_pressure_var', f"{pressure_psi:.2f} PSI")
        
        temp_c = safe_float(parts.get("temperature_c", 0.0))
        queue_ui_update(gui_refs, 'pressboi_temperature_var', f"{temp_c:.1f} °C")

        # Update enabled status
        is_enabled = parts.get("enabled", "0") == "1"
        queue_ui_update(gui_refs, 'pressboi_enabled_var', "Enabled" if is_enabled else "Disabled")

    except Exception as e:
        # It's helpful to log parsing errors to the in-app terminal
        log_func = gui_refs.get('log_func')
        if log_func:
            log_func(f"Pressboi telem parse error: {e}")

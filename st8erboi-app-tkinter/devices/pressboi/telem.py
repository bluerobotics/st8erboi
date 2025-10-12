import comms

def parse_telemetry(message, gui_refs, queue_ui_update, safe_float):
    """
    Parses telemetry data for the PressBoi and updates the GUI.
    Expected format: PRESSBOI_TELEM: state=[STATE] m0_pos=[POS] m1_pos=[POS] ...
    """
    try:
        # Check if the message is valid and contains the expected marker
        if "PRESSBOI_TELEM:" not in message:
            return

        payload = message.split("PRESSBOI_TELEM:")[1].strip()
        parts = dict(item.split('=', 1) for item in payload.split() if '=' in item)

        # Update State
        state = parts.get("state", "---")
        queue_ui_update(gui_refs, 'pressboi_state_var', state)

        # Update Motor 0
        queue_ui_update(gui_refs, 'pressboi_pos_m0_var', parts.get("m0_pos", "---"))
        queue_ui_update(gui_refs, 'pressboi_torque_m0_var', parts.get("m0_torque", 0.0))
        queue_ui_update(gui_refs, 'pressboi_homed_m0_var', "Homed" if parts.get("m0_homed") == "1" else "Not Homed")
        queue_ui_update(gui_refs, 'pressboi_enabled_m0_var', "Enabled" if parts.get("m0_en") == "1" else "Disabled")

        # Update Motor 1
        queue_ui_update(gui_refs, 'pressboi_pos_m1_var', parts.get("m1_pos", "---"))
        queue_ui_update(gui_refs, 'pressboi_torque_m1_var', parts.get("m1_torque", 0.0))
        queue_ui_update(gui_refs, 'pressboi_homed_m1_var', "Homed" if parts.get("m1_homed") == "1" else "Not Homed")
        queue_ui_update(gui_refs, 'pressboi_enabled_m1_var', "Enabled" if parts.get("m1_en") == "1" else "Disabled")

    except IndexError:
        print(f"Error parsing PressBoi telemetry: Unexpected format '{message}'")
    except Exception as e:
        print(f"An error occurred during PressBoi telemetry parsing: {e}")

import socket
import threading
import time
import datetime
from gui import build_gui  # Assumes updated gui.py is in the same directory

CLEARCORE_PORT = 8888
CLIENT_PORT = 6272  # Make sure this port is not in use
HEARTBEAT_INTERVAL = 0.3  # How often to check connection status
DISCOVERY_RETRY_INTERVAL = 2.0  # How often to send discovery if not connected
TIMEOUT_THRESHOLD = 1.5  # Seconds of no telemetry before considering disconnected
TORQUE_HISTORY_LENGTH = 200

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    sock.bind(('', CLIENT_PORT))
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(0.1)  # Non-blocking recv
except OSError as e:
    print(f"ERROR binding socket: {e}. Port {CLIENT_PORT} may be in use by another application.")
    exit()

clearcore_ip = None
last_rx_time = 0
connected = False
last_fw_main_state_for_gui_update = None  # Tracks MAIN_STATE to update GUI panels only on change


def log_to_terminal(msg, terminal_cb_func):
    timestr = datetime.datetime.now().strftime("[%H:%M:%S.%f]")[:-3]
    if terminal_cb_func:
        # Ensure GUI updates are done in the main thread if tkinter is not thread-safe
        # For simple text append, often direct call is fine, but complex changes might need root.after
        terminal_cb_func(f"{timestr} {msg}\n")
    else:
        print(f"{timestr} {msg}")


def discover(terminal_cb_func):
    msg = f"DISCOVER_TELEM PORT={CLIENT_PORT}"
    try:
        sock.sendto(msg.encode(),
                    ('192.168.1.255', CLEARCORE_PORT))  # Ensure this broadcast IP is correct for your network
        log_to_terminal(f"Discovery: '{msg}' sent to broadcast", terminal_cb_func)
    except Exception as e:
        log_to_terminal(f"Discovery error: {e}", terminal_cb_func)


def send_udp(msg, terminal_cb_func):
    if clearcore_ip:
        try:
            sock.sendto(msg.encode(), (clearcore_ip, CLEARCORE_PORT))
            # log_to_terminal(f"Sent to {clearcore_ip}: '{msg}'", terminal_cb_func) # Optional
        except Exception as e:
            log_to_terminal(f"UDP send error to {clearcore_ip}: {e}", terminal_cb_func)
    else:
        if not msg.startswith("DISCOVER_TELEM"):
            log_to_terminal(f"Cannot send '{msg}': No ClearCore IP. Discovery active.", terminal_cb_func)


def monitor_connection(gui_refs):
    global connected, last_rx_time, clearcore_ip, last_fw_main_state_for_gui_update
    prev_conn_status = False  # Initialize prev_conn_status

    default_prominent_bg = "#444"  # As defined in gui.py for prominent_state_display_frame
    disconnected_prominent_bg = "#500000"  # Dark Red

    while True:
        now = time.time()
        # Read global 'connected' flag, which is set by recv_loop
        current_conn_status = connected

        if current_conn_status and (now - last_rx_time) > TIMEOUT_THRESHOLD:
            log_to_terminal("Connection timeout.", gui_refs.get('terminal_cb'))
            connected = False  # Update global flag
            current_conn_status = False  # Reflect change for this iteration's logic

        if current_conn_status != prev_conn_status:
            if current_conn_status:  # Transitioned from False to True (connected)
                log_to_terminal(f"Connected to ClearCore at {clearcore_ip}", gui_refs.get('terminal_cb'))
                if 'update_status' in gui_refs and callable(gui_refs['update_status']):
                    gui_refs['update_status'](f"✅ Connected to {clearcore_ip}")

                # Prominent display text will be updated by parse_telemetry with actual MAIN_STATE.
                # Here, we just ensure the visual "disconnected" alarm (red background) is cleared
                # and set a temporary text.
                if 'prominent_firmware_state_var' in gui_refs:
                    gui_refs['prominent_firmware_state_var'].set("Receiving...")

                if 'prominent_state_display_frame' in gui_refs and gui_refs[
                    'prominent_state_display_frame'].winfo_exists():
                    gui_refs['prominent_state_display_frame'].config(bg=default_prominent_bg)
                if 'prominent_state_label' in gui_refs and gui_refs['prominent_state_label'].winfo_exists():
                    gui_refs['prominent_state_label'].config(bg=default_prominent_bg, fg="white")

            else:  # Transitioned from True to False (disconnected)
                log_to_terminal("Disconnected from ClearCore.", gui_refs.get('terminal_cb'))
                if 'update_status' in gui_refs and callable(gui_refs['update_status']):
                    gui_refs['update_status']("🔌 Disconnected (Timeout)")

                # Set prominent display to full "DISCONNECTED" state
                if 'prominent_firmware_state_var' in gui_refs:
                    gui_refs['prominent_firmware_state_var'].set("DISCONNECTED")

                if 'prominent_state_display_frame' in gui_refs and gui_refs[
                    'prominent_state_display_frame'].winfo_exists():
                    gui_refs['prominent_state_display_frame'].config(bg=disconnected_prominent_bg)
                if 'prominent_state_label' in gui_refs and gui_refs['prominent_state_label'].winfo_exists():
                    gui_refs['prominent_state_label'].config(bg=disconnected_prominent_bg, fg="white")

                # Reset other relevant GUI fields as per your existing logic
                if 'main_state_var' in gui_refs: gui_refs['main_state_var'].set("UNKNOWN")
                if 'homing_state_var' in gui_refs: gui_refs['homing_state_var'].set("---")
                if 'homing_phase_var' in gui_refs: gui_refs['homing_phase_var'].set("---")
                if 'feed_state_var' in gui_refs: gui_refs['feed_state_var'].set("IDLE")  # Reset to IDLE
                if 'error_state_var' in gui_refs: gui_refs['error_state_var'].set("---")
                if 'machine_steps_var' in gui_refs: gui_refs['machine_steps_var'].set("0")
                if 'cartridge_steps_var' in gui_refs: gui_refs['cartridge_steps_var'].set("0")
                if 'enable_disable_var' in gui_refs: gui_refs['enable_disable_var'].set("Disabled")

                for i_motor in [1, 2]:  # Using i_motor to avoid conflict if 'i' is used elsewhere
                    if f'motor_state{i_motor}_var' in gui_refs: gui_refs[f'motor_state{i_motor}_var'].set("N/A")
                    if f'enabled_state{i_motor}_var' in gui_refs: gui_refs[f'enabled_state{i_motor}_var'].set("N/A")
                    if f'torque_value{i_motor}_var' in gui_refs: gui_refs[f'torque_value{i_motor}_var'].set("--- %")
                    if f'position_cmd{i_motor}_var' in gui_refs: gui_refs[f'position_cmd{i_motor}_var'].set("0")

                if 'inject_dispensed_ml_var' in gui_refs: gui_refs['inject_dispensed_ml_var'].set("0.00 ml")
                if 'purge_dispensed_ml_var' in gui_refs: gui_refs['purge_dispensed_ml_var'].set("0.00 ml")

                if 'update_state' in gui_refs and callable(gui_refs['update_state']):
                    gui_refs['update_state']("UNKNOWN")
                last_fw_main_state_for_gui_update = "UNKNOWN"

            prev_conn_status = current_conn_status
        time.sleep(HEARTBEAT_INTERVAL)


def auto_discover_loop(terminal_cb_func):
    while True:
        if not connected:  # Only discover if not connected
            discover(terminal_cb_func)
        time.sleep(DISCOVERY_RETRY_INTERVAL)


def parse_telemetry(msg, gui_refs):
    global last_fw_main_state_for_gui_update
    try:
        parts = {}
        # Standardize parsing for robustness: split by comma, then by colon
        for item_group in msg.split(','):
            if ':' in item_group:  # Ensure there's a colon before trying to split
                key, value = item_group.split(':', 1)
                parts[key.strip()] = value.strip()
            # else: # Optional: Log malformed part if necessary
            # log_to_terminal(f"Malformed telemetry part (no colon): {item_group}", gui_refs.get('terminal_cb'))

        # Update GUI vars directly
        # Set MAIN_STATE first as other logic might depend on it
        fw_main_state = parts.get("MAIN_STATE", "---")
        if 'main_state_var' in gui_refs: gui_refs['main_state_var'].set(fw_main_state)

        # Update prominent display with MAIN_STATE from firmware
        if 'prominent_firmware_state_var' in gui_refs:
            gui_refs['prominent_firmware_state_var'].set(fw_main_state)

        # Ensure prominent display background is normal color now that telemetry is received
        default_prominent_bg = "#444"  # As defined in gui.py
        if 'prominent_state_display_frame' in gui_refs and gui_refs['prominent_state_display_frame'].winfo_exists():
            if gui_refs['prominent_state_display_frame'].cget('bg') != default_prominent_bg:
                gui_refs['prominent_state_display_frame'].config(bg=default_prominent_bg)
                if 'prominent_state_label' in gui_refs and gui_refs['prominent_state_label'].winfo_exists():
                    gui_refs['prominent_state_label'].config(bg=default_prominent_bg, fg="white")

        if 'homing_state_var' in gui_refs: gui_refs['homing_state_var'].set(parts.get("HOMING_STATE", "---"))
        if 'homing_phase_var' in gui_refs: gui_refs['homing_phase_var'].set(parts.get("HOMING_PHASE", "---"))

        current_feed_state_str = parts.get("FEED_STATE", "IDLE")  # Default to IDLE if not present
        if 'feed_state_var' in gui_refs: gui_refs['feed_state_var'].set(current_feed_state_str)

        if 'error_state_var' in gui_refs: gui_refs['error_state_var'].set(parts.get("ERROR_STATE", "No Error"))

        # Motor values
        if 'torque_value1_var' in gui_refs: gui_refs['torque_value1_var'].set(parts.get("torque1", "--- %"))
        if 'position_cmd1_var' in gui_refs: gui_refs['position_cmd1_var'].set(parts.get("pos_cmd1", "0"))
        if 'torque_value2_var' in gui_refs: gui_refs['torque_value2_var'].set(parts.get("torque2", "--- %"))
        if 'position_cmd2_var' in gui_refs: gui_refs['position_cmd2_var'].set(parts.get("pos_cmd2", "0"))

        if 'machine_steps_var' in gui_refs: gui_refs['machine_steps_var'].set(parts.get("machine_steps", "N/A"))
        if 'cartridge_steps_var' in gui_refs: gui_refs['cartridge_steps_var'].set(parts.get("cartridge_steps", "N/A"))

        # Assuming 'enabled1' reflects the overall motor enable state for the 'enable_disable_var'
        motors_enabled_str = parts.get("enabled1", "0")
        if 'enable_disable_var' in gui_refs: gui_refs['enable_disable_var'].set(
            "Enabled" if motors_enabled_str == "1" else "Disabled")

        # Individual motor status based on their respective HLFB and enable flags
        if 'motor_state1_var' in gui_refs: gui_refs['motor_state1_var'].set(
            "Ready" if parts.get("hlfb1", "0") == "1" else "Busy/Fault")
        if 'enabled_state1_var' in gui_refs: gui_refs['enabled_state1_var'].set(
            "Enabled" if parts.get("enabled1", "0") == "1" else "Disabled")

        if 'motor_state2_var' in gui_refs: gui_refs['motor_state2_var'].set(
            "Ready" if parts.get("hlfb2", "0") == "1" else "Busy/Fault")
        if 'enabled_state2_var' in gui_refs: gui_refs['enabled_state2_var'].set(
            "Enabled" if parts.get("enabled2", "0") == "1" else "Disabled")

        # Update torque plot
        current_time = time.time()
        try:
            torque1_float = float(parts.get("torque1", "0.0"))
        except ValueError:
            torque1_float = 0.0
        try:
            torque2_float = float(parts.get("torque2", "0.0"))
        except ValueError:
            torque2_float = 0.0

        if 'torque_history1' in gui_refs: gui_refs['torque_history1'].append(torque1_float)
        if 'torque_history2' in gui_refs: gui_refs['torque_history2'].append(torque2_float)
        if 'torque_times' in gui_refs: gui_refs['torque_times'].append(current_time)

        # Trim lists (ensure TORQUE_HISTORY_LENGTH is defined appropriately)
        if 'torque_times' in gui_refs:
            while len(gui_refs['torque_times']) > TORQUE_HISTORY_LENGTH:
                gui_refs['torque_times'].pop(0)
                if 'torque_history1' in gui_refs and len(gui_refs['torque_history1']) > 0: gui_refs[
                    'torque_history1'].pop(0)
                if 'torque_history2' in gui_refs and len(gui_refs['torque_history2']) > 0: gui_refs[
                    'torque_history2'].pop(0)

        if 'update_torque_plot' in gui_refs and callable(gui_refs['update_torque_plot']):
            gui_refs['update_torque_plot'](
                parts.get("torque1", "--- %"),
                parts.get("torque2", "--- %"),
                gui_refs.get('torque_times', []),
                gui_refs.get('torque_history1', []),
                gui_refs.get('torque_history2', [])
            )

        # Update dispensed ML variables
        dispensed_ml_str = parts.get("dispensed_ml", "0.00")
        try:
            dispensed_value = float(dispensed_ml_str)
        except ValueError:
            dispensed_value = 0.00

        # Logic to direct the dispensed_ml to the correct GUI variable
        # Uses current_feed_state_str which was set earlier from parts.get("FEED_STATE", "IDLE")
        if "INJECT" in current_feed_state_str:
            if 'inject_dispensed_ml_var' in gui_refs: gui_refs['inject_dispensed_ml_var'].set(
                f"{dispensed_value:.2f} ml")
            if gui_refs.get("last_feed_op_type") != "INJECT":  # If switching to inject, clear purge display
                if 'purge_dispensed_ml_var' in gui_refs: gui_refs['purge_dispensed_ml_var'].set("0.00 ml")
            gui_refs["last_feed_op_type"] = "INJECT"
        elif "PURGE" in current_feed_state_str:
            if 'purge_dispensed_ml_var' in gui_refs: gui_refs['purge_dispensed_ml_var'].set(f"{dispensed_value:.2f} ml")
            if gui_refs.get("last_feed_op_type") != "PURGE":  # If switching to purge, clear inject display
                if 'inject_dispensed_ml_var' in gui_refs: gui_refs['inject_dispensed_ml_var'].set("0.00 ml")
            gui_refs["last_feed_op_type"] = "PURGE"
        else:  # Idle, completed, cancelled, or other feed states
            # If the operation was cancelled or it's a fresh start (where firmware sends 0 for dispensed_ml for this field)
            if dispensed_value == 0.0:
                if 'inject_dispensed_ml_var' in gui_refs: gui_refs['inject_dispensed_ml_var'].set("0.00 ml")
                if 'purge_dispensed_ml_var' in gui_refs: gui_refs['purge_dispensed_ml_var'].set("0.00 ml")
            # If it's completed with a value, main.py needs to know which field to update.
            # The current logic implies 'dispensed_ml' is generic; GUI relies on FEED_STATE to route it.
            # If FEED_STATE is IDLE/STANDBY, this 'dispensed_ml' is the 'last_completed_dispense_ml'.
            # We need to know if that last op was INJECT or PURGE to update the correct field.
            # This might require an additional telemetry field or smarter logic in main.py.
            # For now, if 'dispensed_ml' is non-zero and state is idle, we don't know where to put it without more info.
            # However, the firmware changes ensure that on start of a new op, last_completed is cleared
            # and on cancel, last_completed is set to 0.
            # Let's assume if state is idle, and dispensed_value is non-zero, it was the last one running
            if current_feed_state_str in ["FEED_STANDBY", "FEED_NONE", "FEED_OPERATION_COMPLETED",
                                          "FEED_OPERATION_CANCELLED", "IDLE", "---"]:
                if gui_refs.get("last_feed_op_type") == "INJECT":
                    if 'inject_dispensed_ml_var' in gui_refs: gui_refs['inject_dispensed_ml_var'].set(
                        f"{dispensed_value:.2f} ml")
                elif gui_refs.get("last_feed_op_type") == "PURGE":
                    if 'purge_dispensed_ml_var' in gui_refs: gui_refs['purge_dispensed_ml_var'].set(
                        f"{dispensed_value:.2f} ml")
                # If last_feed_op_type is not set, and it's idle, both will show 0.00 if dispensed_value is 0

        # Update contextual panels if MAIN_STATE changed
        if fw_main_state != last_fw_main_state_for_gui_update:
            if 'update_state' in gui_refs and callable(gui_refs['update_state']):
                gui_refs['update_state'](fw_main_state)
            last_fw_main_state_for_gui_update = fw_main_state

        # Button state logic (ensure all relevant ui_elements are checked for existence)
        # Firmware states from states.h: FEED_INJECT_ACTIVE, FEED_PURGE_ACTIVE, FEED_INJECT_PAUSED, FEED_PURGE_PAUSED
        injecting = current_feed_state_str == "FEED_INJECT_ACTIVE"  # More precise
        purging = current_feed_state_str == "FEED_PURGE_ACTIVE"  # More precise
        inject_paused = current_feed_state_str == "FEED_INJECT_PAUSED"
        purge_paused = current_feed_state_str == "FEED_PURGE_PAUSED"

        any_feed_op_active_or_paused = injecting or purging or inject_paused or purge_paused

        if 'start_inject_btn' in gui_refs and gui_refs['start_inject_btn'].winfo_exists():
            gui_refs['start_inject_btn'].config(state='disabled' if any_feed_op_active_or_paused else 'normal')
        if 'pause_inject_btn' in gui_refs and gui_refs['pause_inject_btn'].winfo_exists():
            gui_refs['pause_inject_btn'].config(state='normal' if injecting else 'disabled')
        if 'resume_inject_btn' in gui_refs and gui_refs['resume_inject_btn'].winfo_exists():
            gui_refs['resume_inject_btn'].config(state='normal' if inject_paused else 'disabled')
        if 'cancel_inject_btn' in gui_refs and gui_refs['cancel_inject_btn'].winfo_exists():
            gui_refs['cancel_inject_btn'].config(state='normal' if injecting or inject_paused else 'disabled')

        if 'start_purge_btn' in gui_refs and gui_refs['start_purge_btn'].winfo_exists():
            gui_refs['start_purge_btn'].config(state='disabled' if any_feed_op_active_or_paused else 'normal')
        if 'pause_purge_btn' in gui_refs and gui_refs['pause_purge_btn'].winfo_exists():
            gui_refs['pause_purge_btn'].config(state='normal' if purging else 'disabled')
        if 'resume_purge_btn' in gui_refs and gui_refs['resume_purge_btn'].winfo_exists():
            gui_refs['resume_purge_btn'].config(state='normal' if purge_paused else 'disabled')
        if 'cancel_purge_btn' in gui_refs and gui_refs['cancel_purge_btn'].winfo_exists():
            gui_refs['cancel_purge_btn'].config(state='normal' if purging or purge_paused else 'disabled')

    except Exception as e:
        term_cb = gui_refs.get('terminal_cb')
        log_msg = f"Telemetry parse error: {e} on data: '{msg}'"
        if term_cb and callable(term_cb):
            term_cb(log_msg + "\n")
        else:
            print(log_msg)


def recv_loop(gui_refs):
    global clearcore_ip, connected, last_rx_time
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode('utf-8', errors='replace').strip()

            # More robust check for a valid telemetry packet (must contain key fields)
            if "MAIN_STATE:" in msg and "torque1:" in msg and "FEED_STATE:" in msg:
                if not connected or clearcore_ip != addr[0]:
                    clearcore_ip = addr[0]
                    # log_to_terminal(f"New ClearCore IP: {clearcore_ip}", gui_refs.get('terminal_cb')) # monitor_connection logs "Connected"

                if not connected:  # If it was previously not connected
                    connected = True  # Set before parsing, so monitor_connection sees it correctly
                    # The monitor_connection will handle the initial "Connected" prominent bar update

                last_rx_time = time.time()
                connected = True  # Ensure it's set if we received valid telemetry
                parse_telemetry(msg, gui_refs)
            else:
                # Log other messages if from a known ClearCore IP or for debugging
                if clearcore_ip and addr[0] == clearcore_ip:
                    log_to_terminal(f"[CC@{addr[0]} Other]: {msg}", gui_refs.get('terminal_cb'))
                # else: # Optional: Log messages from unexpected IPs
                # log_to_terminal(f"[Unknown@{addr[0]}]: {msg}", gui_refs.get('terminal_cb'))
        except socket.timeout:
            continue
        except Exception as e:
            log_to_terminal(f"Recv_loop error: {e}", gui_refs.get('terminal_cb'))
            time.sleep(0.1)


def main():
    # Initialize shared_gui_refs before build_gui in case terminal_cb is needed early
    shared_gui_refs = {"last_feed_op_type": None}

    # Pass lambda functions that will use shared_gui_refs for terminal_cb
    # The terminal_cb in gui_refs will be populated by build_gui
    gui = build_gui(
        lambda msg_to_send: send_udp(msg_to_send, shared_gui_refs.get('terminal_cb')),
        lambda: discover(shared_gui_refs.get('terminal_cb'))
    )
    shared_gui_refs.update(gui)  # gui now contains all ui_elements including 'terminal_cb'

    # Ensure all threads get the fully populated shared_gui_refs
    threading.Thread(target=recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=monitor_connection, args=(shared_gui_refs,), daemon=True).start()
    # auto_discover_loop should also use the shared_gui_refs for its terminal_cb if it logs
    threading.Thread(target=auto_discover_loop, args=(shared_gui_refs.get('terminal_cb'),), daemon=True).start()

    # Initial call to update panels based on default state
    if 'update_state' in shared_gui_refs and callable(shared_gui_refs['update_state']):
        shared_gui_refs['root'].after(50,
                                      lambda: shared_gui_refs['update_state'](shared_gui_refs['main_state_var'].get()))

    shared_gui_refs['root'].mainloop()


if __name__ == "__main__":
    main()
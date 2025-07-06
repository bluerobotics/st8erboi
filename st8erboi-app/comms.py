import socket
import threading
import time
import datetime
import tkinter as tk

# --- Constants ---
CLEARCORE_PORT = 8888
CLIENT_PORT = 6272
HEARTBEAT_INTERVAL = 0.5
DISCOVERY_INTERVAL = 2.0
TIMEOUT_THRESHOLD = 3.0
TORQUE_HISTORY_LENGTH = 200

# --- Global State ---
last_fw_main_state_for_gui_update = None
last_fw_feed_state_for_gui_update = None

# --- Communication State ---
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    sock.bind(('', CLIENT_PORT))
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(0.1)
except OSError as e:
    print(f"ERROR binding socket: {e}. Port {CLIENT_PORT} may be in use by another application.")
    exit()

devices = {
    "injector": {"ip": None, "last_rx": 0, "connected": False, "last_discovery_attempt": 0},
    "fillhead": {"ip": None, "last_rx": 0, "connected": False, "last_discovery_attempt": 0}
}


# --- Communication Functions ---

def log_to_terminal(msg, terminal_cb_func):
    """Safely logs a message to the GUI terminal."""
    timestr = datetime.datetime.now().strftime("[%H:%M:%S.%f]")[:-3]
    if terminal_cb_func:
        terminal_cb_func(f"{timestr} {msg}\n")
    else:
        print(f"{timestr} {msg}")


def discover(device_key, gui_refs):
    """Sends a targeted discovery message, logging only if the checkbox is ticked."""
    terminal_cb_func = gui_refs.get('terminal_cb')
    msg = f"DISCOVER_{device_key.upper()} PORT={CLIENT_PORT}"

    log_discovery = gui_refs.get('show_discovery_var', tk.BooleanVar(value=False)).get()
    if log_discovery:
        log_to_terminal(f"[CMD SENT to BROADCAST]: {msg}", terminal_cb_func)

    try:
        sock.sendto(msg.encode(), ('192.168.1.255', CLEARCORE_PORT))
    except Exception as e:
        log_to_terminal(f"Discovery error for {device_key}: {e}", terminal_cb_func)


def send_to_device(device_key, msg, gui_refs):
    """Sends a message to a specific device if its IP is known."""
    terminal_cb_func = gui_refs.get('terminal_cb')
    device_ip = devices[device_key].get("ip")
    if device_ip:
        try:
            if "REQUEST_TELEM" not in msg:
                log_to_terminal(f"[CMD SENT to {device_key.upper()}]: {msg}", terminal_cb_func)
            sock.sendto(msg.encode(), (device_ip, CLEARCORE_PORT))
        except Exception as e:
            log_to_terminal(f"UDP send error to {device_key} ({device_ip}): {e}", terminal_cb_func)
    elif "DISCOVER" not in msg:
        log_to_terminal(f"Cannot send to {device_key}: IP unknown.", terminal_cb_func)


def monitor_connections(gui_refs):
    """
    Runs in a thread to monitor device connection status and handle auto-discovery
    for any disconnected devices.
    """
    terminal_cb = gui_refs.get('terminal_cb')
    while True:
        now = time.time()
        for key, device in devices.items():
            prev_conn_status = device["connected"]

            if prev_conn_status and (now - device["last_rx"]) > TIMEOUT_THRESHOLD:
                device["connected"] = False
                device["ip"] = None  # Clear IP on disconnect

            if prev_conn_status and not device["connected"]:
                status_text = f"🔌 {key.capitalize()} Disconnected"
                log_to_terminal(status_text, terminal_cb)
                gui_refs[f'status_var_{key}'].set(status_text)

                other_key = "fillhead" if key == "injector" else "injector"
                if devices[other_key]["connected"]:
                    log_to_terminal(f"INFO: {key.capitalize()} disconnected. Informing peer.", terminal_cb)
                    send_to_device(other_key, "CLEAR_PEER_IP", gui_refs)

            if not device["connected"]:
                if now - device["last_discovery_attempt"] > DISCOVERY_INTERVAL:
                    discover(key, gui_refs)
                    device["last_discovery_attempt"] = now

        time.sleep(HEARTBEAT_INTERVAL)


def handle_connection(device_key, source_ip, gui_refs):
    """Handles the logic for a new or existing connection."""
    device = devices[device_key]

    if not device["connected"] or device["ip"] != source_ip:
        device["ip"] = source_ip
        device["connected"] = True

        status_text = f"✅ {device_key.capitalize()} Connected ({source_ip})"
        log_to_terminal(status_text, gui_refs.get('terminal_cb'))
        gui_refs[f'status_var_{device_key}'].set(status_text)

        # Send one request on initial connection, then let the timed loop take over.
        send_to_device(device_key, "REQUEST_TELEM", gui_refs)

        other_key = "fillhead" if device_key == "injector" else "injector"
        if devices[other_key]["connected"]:
            log_to_terminal("INFO: Both devices now connected. Brokering IPs.", gui_refs.get('terminal_cb'))
            send_to_device(device_key, f"SET_PEER_IP {devices[other_key]['ip']}", gui_refs)
            send_to_device(other_key, f"SET_PEER_IP {device['ip']}", gui_refs)

    device["last_rx"] = time.time()


# --- Parser Functions ---

def parse_injector_telemetry(msg, gui_refs):
    global last_fw_main_state_for_gui_update, last_fw_feed_state_for_gui_update
    try:
        payload = msg.split("INJ_TELEM_GUI:")[1]
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)

        fw_main_state = parts.get("MAIN_STATE", "---")
        fw_feed_state = parts.get("FEED_STATE", "IDLE")

        if 'main_state_var' in gui_refs: gui_refs['main_state_var'].set(fw_main_state)
        if 'feed_state_var' in gui_refs: gui_refs['feed_state_var'].set(fw_feed_state)

        if 'homing_state_var' in gui_refs: gui_refs['homing_state_var'].set(parts.get("HOMING_STATE", "---"))
        if 'homing_phase_var' in gui_refs: gui_refs['homing_phase_var'].set(parts.get("HOMING_PHASE", "---"))
        if 'error_state_var' in gui_refs: gui_refs['error_state_var'].set(parts.get("ERROR_STATE", "No Error"))

        torque_floats = []
        for i in range(3):
            try:
                torque_val = float(parts.get(f'torque{i}', '0.0'))
            except (ValueError, TypeError):
                torque_val = 0.0
            torque_floats.append(torque_val)

        current_time = time.time()
        gui_refs.get('injector_torque_times', []).append(current_time)
        gui_refs.get('injector_torque_history1', []).append(torque_floats[0])
        gui_refs.get('injector_torque_history2', []).append(torque_floats[1])
        gui_refs.get('injector_torque_history3', []).append(torque_floats[2])

        cutoff_time = current_time - 15
        timestamps = gui_refs.get('injector_torque_times', [])
        start_index = 0
        for i, ts in enumerate(timestamps):
            if ts >= cutoff_time:
                start_index = i
                break

        if start_index > 0:
            gui_refs['injector_torque_times'] = timestamps[start_index:]
            gui_refs['injector_torque_history1'] = gui_refs.get('injector_torque_history1', [])[start_index:]
            gui_refs['injector_torque_history2'] = gui_refs.get('injector_torque_history2', [])[start_index:]
            gui_refs['injector_torque_history3'] = gui_refs.get('injector_torque_history3', [])[start_index:]

        for i in range(3):
            gui_var_index = i + 1
            torque_str = parts.get(f'torque{i}', '0.0')
            if f'torque_value{gui_var_index}_var' in gui_refs:
                gui_refs[f'torque_value{gui_var_index}_var'].set(torque_str)

            if f'position_cmd{gui_var_index}_var' in gui_refs: gui_refs[f'position_cmd{gui_var_index}_var'].set(
                parts.get(f'pos_mm{i}', '0.0'))
            if f'motor_state{gui_var_index}_var' in gui_refs: gui_refs[f'motor_state{gui_var_index}_var'].set(
                "Ready" if parts.get(f"hlfb{i}", "0") == "1" else "Busy/Fault")
            if f'enabled_state{gui_var_index}_var' in gui_refs: gui_refs[f'enabled_state{gui_var_index}_var'].set(
                "Enabled" if parts.get(f"enabled{i}", "0") == "1" else "Disabled")

        if 'pinch_homed_var' in gui_refs: gui_refs['pinch_homed_var'].set(
            "Homed" if parts.get("homed2", "0") == "1" else "Not Homed")

        if 'machine_steps_var' in gui_refs: gui_refs['machine_steps_var'].set(parts.get("machine_mm", "N/A"))
        if 'cartridge_steps_var' in gui_refs: gui_refs['cartridge_steps_var'].set(parts.get("cartridge_mm", "N/A"))
        if 'inject_dispensed_ml_var' in gui_refs: gui_refs['inject_dispensed_ml_var'].set(
            f'{float(parts.get("dispensed_ml", 0.0)):.2f} ml')

        if 'peer_status_injector_var' in gui_refs:
            is_peer_discovered = parts.get("peer_disc", "0") == "1"
            peer_ip = parts.get("peer_ip", "0.0.0.0")
            if is_peer_discovered:
                gui_refs['peer_status_injector_var'].set(f"Peer: {peer_ip}")
            else:
                gui_refs['peer_status_injector_var'].set("Peer: Not Connected")

    except Exception as e:
        log_to_terminal(f"Injector telemetry parse error: {e}", gui_refs.get('terminal_cb'))


# --- REWRITTEN FUNCTION ---
def parse_fillhead_telemetry(msg, gui_refs):
    try:
        # The telemetry message starts with "FH_TELEM_GUI: " - remove it to get to the payload
        payload = msg.split("FH_TELEM_GUI: ")[1]
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)

        # Update the individual axis states
        if 'fh_state_x_var' in gui_refs:
            gui_refs['fh_state_x_var'].set(parts.get("x_s", "UNKNOWN"))
        if 'fh_state_y_var' in gui_refs:
            gui_refs['fh_state_y_var'].set(parts.get("y_s", "UNKNOWN"))
        if 'fh_state_z_var' in gui_refs:
            gui_refs['fh_state_z_var'].set(parts.get("z_s", "UNKNOWN"))

        # --- Define the mapping from axis prefix to motor index ---
        # This allows us to loop through the motors and get the right data from the parsed dictionary
        # Y-axis data (y_*) will be mapped to both motor 1 and 2
        axis_to_motor_map = {
            'x': [0],
            'y': [1, 2],
            'z': [3]
        }

        torque_floats = [0.0] * 4  # A list to hold torque values for plotting

        for axis_prefix, motor_indices in axis_to_motor_map.items():
            # Get the values for the current axis from the parsed data
            pos_val = parts.get(f'{axis_prefix}_p', '0.00')
            torque_str = parts.get(f'{axis_prefix}_t', '0.0')
            enabled_val = "Enabled" if parts.get(f'{axis_prefix}_e', '0') == "1" else "Disabled"
            homed_val = "Homed" if parts.get(f'{axis_prefix}_h', '0') == "1" else "Not Homed"

            # Apply these values to all motors controlled by this axis
            for motor_index in motor_indices:
                # Update GUI variables for this motor
                if f'fh_pos_m{motor_index}_var' in gui_refs:
                    gui_refs[f'fh_pos_m{motor_index}_var'].set(f"{float(pos_val):.2f}")
                if f'fh_torque_m{motor_index}_var' in gui_refs:
                    gui_refs[f'fh_torque_m{motor_index}_var'].set(f"{float(torque_str):.1f} %")
                if f'fh_enabled_m{motor_index}_var' in gui_refs:
                    gui_refs[f'fh_enabled_m{motor_index}_var'].set(enabled_val)
                if f'fh_homed_m{motor_index}_var' in gui_refs:
                    gui_refs[f'fh_homed_m{motor_index}_var'].set(homed_val)

                # Store torque for the plot
                try:
                    torque_floats[motor_index] = float(torque_str)
                except ValueError:
                    torque_floats[motor_index] = 0.0

        # Update torque history for plotting
        current_time = time.time()
        gui_refs.get('fillhead_torque_times', []).append(current_time)
        for i in range(4):
            gui_refs.get(f'fillhead_torque_history{i}', []).append(torque_floats[i])

        # Trim old history data to keep the plot responsive
        cutoff_time = current_time - 15
        timestamps = gui_refs['fillhead_torque_times']
        start_index = 0
        for i, ts in enumerate(timestamps):
            if ts >= cutoff_time:
                start_index = i
                break

        if start_index > 0:
            gui_refs['fillhead_torque_times'] = timestamps[start_index:]
            for i in range(4):
                history_key = f'fillhead_torque_history{i}'
                if history_key in gui_refs:
                    gui_refs[history_key] = gui_refs[history_key][start_index:]

        # Update Peer Status
        if 'peer_status_fillhead_var' in gui_refs:
            is_peer_discovered = parts.get("pd", "0") == "1"
            peer_ip = parts.get("pip", "0.0.0.0")
            if is_peer_discovered:
                gui_refs['peer_status_fillhead_var'].set(f"Peer: {peer_ip}")
            else:
                gui_refs['peer_status_fillhead_var'].set("Peer: Not Connected")

    except Exception as e:
        log_to_terminal(f"Fillhead telemetry parse error: {e} -> on msg: {msg}", gui_refs.get('terminal_cb'))


# --- END REWRITTEN FUNCTION ---


def recv_loop(gui_refs):
    """Main network receive loop. Routes packets to the correct local parser."""
    terminal_cb = gui_refs.get('terminal_cb')
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode('utf-8', errors='replace').strip()
            source_ip = addr[0]
            log_telemetry = gui_refs.get('show_telemetry_var', tk.BooleanVar(value=False)).get()

            # Discovery messages have a unique prefix and are not telemetry
            if msg == "DISCOVERY: INJECTOR DISCOVERED":
                handle_connection("injector", source_ip, gui_refs)
            elif msg == "DISCOVERY: FILLHEAD DISCOVERED":
                handle_connection("fillhead", source_ip, gui_refs)

            # Check for telemetry prefixes
            elif msg.startswith("INJ_TELEM_GUI:"):
                handle_connection("injector", source_ip, gui_refs)
                if log_telemetry:
                    log_to_terminal(f"[TELEM @{source_ip}]: {msg}", terminal_cb)
                parse_injector_telemetry(msg, gui_refs)

            elif msg.startswith("FH_TELEM_GUI:"):
                handle_connection("fillhead", source_ip, gui_refs)
                if log_telemetry:
                    log_to_terminal(f"[TELEM @{source_ip}]: {msg}", terminal_cb)
                parse_fillhead_telemetry(msg, gui_refs)

            # Handle generic status messages from either device
            elif msg.startswith(("INFO:", "DONE:", "ERROR:", "DISCOVERY:", "Axis X:", "Axis Y:", "Axis Z:")):
                log_to_terminal(f"[STATUS @{source_ip}]: {msg}", terminal_cb)
                # Update last receive time for whichever device sent the message
                for key, device in devices.items():
                    if device["ip"] == source_ip:
                        device["last_rx"] = time.time()
                        break
            else:
                # This can be used for debugging unknown packet formats
                # log_to_terminal(f"[UNKNOWN @{source_ip}]: {msg}", terminal_cb)
                pass

        except socket.timeout:
            continue
        except Exception as e:
            log_to_terminal(f"Recv_loop error: {e}", terminal_cb)
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

# MODIFIED: Added a threading lock to prevent race conditions on the socket.
socket_lock = threading.Lock()

devices = {
    "fillhead": {"ip": None, "last_rx": 0, "connected": False, "peer_connected": False, "last_discovery_attempt": 0},
    "gantry": {"ip": None, "last_rx": 0, "connected": False, "peer_connected": False, "last_discovery_attempt": 0}
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
        # No lock needed for broadcast as it's less critical and frequent
        sock.sendto(msg.encode(), ('192.168.1.255', CLEARCORE_PORT))
    except Exception as e:
        log_to_terminal(f"Discovery error for {device_key}: {e}", terminal_cb_func)


def send_to_device(device_key, msg, gui_refs):
    """Sends a message to a specific device if its IP is known."""
    terminal_cb_func = gui_refs.get('terminal_cb')
    device_ip = devices[device_key].get("ip")
    if device_ip:
        # MODIFIED: Added a lock to ensure thread-safe socket access.
        with socket_lock:
            try:
                if "REQUEST_TELEM" not in msg:
                    log_to_terminal(f"[CMD SENT to {device_key.upper()}]: {msg}", terminal_cb_func)
                sock.sendto(msg.encode(), (device_ip, CLEARCORE_PORT))
            except Exception as e:
                log_to_terminal(f"UDP send error to {device_key} ({device_ip}): {e}", terminal_cb_func)
    elif "DISCOVER" not in msg:
        log_to_terminal(f"Cannot send to {device_key}: IP unknown.", terminal_cb_func)


def monitor_connections(gui_refs):
    """Monitors device connection status and handles auto-discovery."""
    terminal_cb = gui_refs.get('terminal_cb')
    while True:
        now = time.time()
        for key, device in devices.items():
            prev_conn_status = device["connected"]

            if prev_conn_status and (now - device["last_rx"]) > TIMEOUT_THRESHOLD:
                device["connected"] = False
                device["peer_connected"] = False
                device["ip"] = None

                if prev_conn_status and not device["connected"]:
                    status_text = f"🔌 {key.capitalize()} Disconnected"
                    log_to_terminal(status_text, terminal_cb)
                    gui_refs[f'status_var_{key}'].set(status_text)

                    other_key = "gantry" if key == "fillhead" else "fillhead"
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

        other_key = "gantry" if device_key == "fillhead" else "fillhead"
        if devices[other_key]["connected"]:
            log_to_terminal("INFO: Both devices now connected. Brokering IPs.", gui_refs.get('terminal_cb'))
            send_to_device(device_key, f"SET_PEER_IP {devices[other_key]['ip']}", gui_refs)
            send_to_device(other_key, f"SET_PEER_IP {device['ip']}", gui_refs)

    device["last_rx"] = time.time()


# --- Parser Functions ---

def safe_float(s, default_val=0.0):
    """Safely converts a string to a float."""
    if not s: return default_val
    try:
        return float(s)
    except (ValueError, TypeError):
        return default_val


def parse_fillhead_telemetry(msg, gui_refs):
    """Parses the telemetry string from the Fillhead controller."""
    try:
        payload = msg.split("FILLHEAD_TELEM: ")[1]
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)

        # Main Controller States
        if 'main_state_var' in gui_refs: gui_refs['main_state_var'].set(parts.get("MAIN_STATE", "---"))
        if 'feed_state_var' in gui_refs: gui_refs['feed_state_var'].set(parts.get("FEED_STATE", "IDLE"))
        if 'homing_state_var' in gui_refs: gui_refs['homing_state_var'].set(parts.get("HOMING_STATE", "---"))
        if 'homing_phase_var' in gui_refs: gui_refs['homing_phase_var'].set(parts.get("HOMING_PHASE", "---"))
        if 'error_state_var' in gui_refs: gui_refs['error_state_var'].set(parts.get("ERROR_STATE", "No Error"))

        # Injector Motors (M0, M1)
        for i in range(2):
            gui_var_index = i + 1
            if f'torque{i}_var' in gui_refs: gui_refs[f'torque{i}_var'].set(safe_float(parts.get(f'inj_t{i}', '0.0')))
            if f'pos_mm{i}_var' in gui_refs: gui_refs[f'pos_mm{i}_var'].set(parts.get(f'pos_mm{i}', '0.0'))
            if f'motor_state{gui_var_index}_var' in gui_refs: gui_refs[f'motor_state{gui_var_index}_var'].set(
                "Ready" if parts.get(f"hlfb{i}", "0") == "1" else "Busy/Fault")
            if f'enabled_state{gui_var_index}_var' in gui_refs: gui_refs[f'enabled_state{gui_var_index}_var'].set(
                "Enabled" if parts.get(f"enabled{i}", "0") == "1" else "Disabled")

        # Pinch Valves
        if 'inj_valve_pos_var' in gui_refs: gui_refs['inj_valve_pos_var'].set(parts.get("inj_valve_pos", "---"))
        if 'inj_valve_homed_var' in gui_refs: gui_refs['inj_valve_homed_var'].set(
            "Homed" if parts.get("inj_valve_homed", "0") == "1" else "Not Homed")
        if 'torque2_var' in gui_refs: gui_refs['torque2_var'].set(safe_float(parts.get('torque2', '0.0')))
        if 'vac_valve_pos_var' in gui_refs: gui_refs['vac_valve_pos_var'].set(parts.get("vac_valve_pos", "---"))
        if 'vac_valve_homed_var' in gui_refs: gui_refs['vac_valve_homed_var'].set(
            "Homed" if parts.get("vac_valve_homed", "0") == "1" else "Not Homed")
        if 'torque3_var' in gui_refs: gui_refs['torque3_var'].set(safe_float(parts.get('torque3', '0.0')))

        # Other System Status
        if 'machine_steps_var' in gui_refs: gui_refs['machine_steps_var'].set(parts.get("inj_mach_mm", "N/A"))
        if 'cartridge_steps_var' in gui_refs: gui_refs['cartridge_steps_var'].set(parts.get("inj_cart_mm", "N/A"))
        if 'inject_dispensed_ml_var' in gui_refs: gui_refs['inject_dispensed_ml_var'].set(
            f'{safe_float(parts.get("inj_disp_ml")):.2f} ml')

        if 'temp_c_var' in gui_refs:
            gui_refs['temp_c_var'].set(f'{safe_float(parts.get("h_pv")):.1f} °C')

        if 'vacuum_psig_var' in gui_refs: gui_refs['vacuum_psig_var'].set(f"{safe_float(parts.get('vac_pv')):.2f} PSIG")
        if 'pid_output_var' in gui_refs: gui_refs['pid_output_var'].set(f'{safe_float(parts.get("h_op")):.1f}%')
        if 'vacuum_state_var' in gui_refs: gui_refs['vacuum_state_var'].set(
            "On" if parts.get("vac_st", "0") == "1" else "Off")
        if 'heater_mode_var' in gui_refs: gui_refs['heater_mode_var'].set(
            "PID Active" if parts.get("h_st", "0") == "1" else "OFF")

        # Peer Connection Status
        if 'peer_status_fillhead_var' in gui_refs:
            is_peer_discovered = parts.get("peer_disc", "0") == "1"
            devices["fillhead"]["peer_connected"] = is_peer_discovered
            peer_ip = parts.get("peer_ip", "0.0.0.0")
            if is_peer_discovered:
                gui_refs['peer_status_fillhead_var'].set(f"Peer: {peer_ip}")
            else:
                gui_refs['peer_status_fillhead_var'].set("Peer: Not Connected")

    except Exception as e:
        log_to_terminal(f"Fillhead telemetry parse error: {e}\n", gui_refs.get('terminal_cb'))


def parse_gantry_telemetry(msg, gui_refs):
    """Parses the telemetry string from the Gantry controller."""
    try:
        payload = msg.split("GANTRY_TELEM: ")[1]
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)

        # --- THIS IS THE FIX ---
        if 'fh_state_var' in gui_refs: gui_refs['fh_state_var'].set(parts.get("gantry_state", "UNKNOWN")) # <-- ADD THIS LINE

        if 'fh_state_x_var' in gui_refs: gui_refs['fh_state_x_var'].set(parts.get("x_s", "UNKNOWN"))
        if 'fh_state_y_var' in gui_refs: gui_refs['fh_state_y_var'].set(parts.get("y_s", "UNKNOWN"))
        if 'fh_state_z_var' in gui_refs: gui_refs['fh_state_z_var'].set(parts.get("z_s", "UNKNOWN"))

        axis_to_motor_map = {'x': [0], 'y': [1, 2], 'z': [3]}
        for axis_prefix, motor_indices in axis_to_motor_map.items():
            pos_val = parts.get(f'{axis_prefix}_p', '0.00')
            torque_str = parts.get(f'{axis_prefix}_t', '0.0')
            enabled_val = "Enabled" if parts.get(f'{axis_prefix}_e', '0') == "1" else "Disabled"
            homed_val = "Homed" if parts.get(f'{axis_prefix}_h', '0') == "1" else "Not Homed"
            for motor_index in motor_indices:
                if f'fh_pos_m{motor_index}_var' in gui_refs: gui_refs[f'fh_pos_m{motor_index}_var'].set(
                    f"{float(pos_val):.2f}")
                if f'fh_torque_m{motor_index}_var' in gui_refs: gui_refs[f'fh_torque_m{motor_index}_var'].set(
                    safe_float(torque_str))
                if f'fh_enabled_m{motor_index}_var' in gui_refs: gui_refs[f'fh_enabled_m{motor_index}_var'].set(
                    enabled_val)
                if f'fh_homed_m{motor_index}_var' in gui_refs: gui_refs[f'fh_homed_m{motor_index}_var'].set(homed_val)

        if 'peer_status_gantry_var' in gui_refs:
            is_peer_discovered = parts.get("pd", "0") == "1"
            devices["gantry"]["peer_connected"] = is_peer_discovered
            peer_ip = parts.get("pip", "0.0.0.0")
            if is_peer_discovered:
                gui_refs['peer_status_gantry_var'].set(f"Peer: {peer_ip}")
            else:
                gui_refs['peer_status_gantry_var'].set("Peer: Not Connected")

    except Exception as e:
        log_to_terminal(f"Gantry telemetry parse error: {e} -> on msg: {msg}\n", gui_refs.get('terminal_cb'))


def recv_loop(gui_refs):
    """Main network receive loop. Routes packets to the correct local parser."""
    terminal_cb = gui_refs.get('terminal_cb')
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode('utf-8', errors='replace').strip()
            source_ip = addr[0]
            log_telemetry = gui_refs.get('show_telemetry_var', tk.BooleanVar(value=False)).get()

            if msg == "DISCOVERY: FILLHEAD DISCOVERED":
                handle_connection("fillhead", source_ip, gui_refs)
            elif msg == "DISCOVERY: GANTRY DISCOVERED":
                handle_connection("gantry", source_ip, gui_refs)
            elif msg.startswith("FILLHEAD_TELEM: "):
                handle_connection("fillhead", source_ip, gui_refs)
                if log_telemetry:
                    log_to_terminal(f"[TELEM @{source_ip}]: {msg}", terminal_cb)
                parse_fillhead_telemetry(msg, gui_refs)
            elif msg.startswith("GANTRY_TELEM: "):
                handle_connection("gantry", source_ip, gui_refs)
                if log_telemetry:
                    log_to_terminal(f"[TELEM @{source_ip}]: {msg}", terminal_cb)
                parse_gantry_telemetry(msg, gui_refs)
            elif msg.startswith(("INFO:", "DONE:", "ERROR:", "DISCOVERY:", "FILLHEAD_DONE:", "FH_DONE:",
                                 "FILLHEAD_INFO:", "FH_INFO:", "FILLHEAD_ERROR:", "FH_ERROR:")):
                log_to_terminal(f"[STATUS @{source_ip}]: {msg}", terminal_cb)
                for key, device in devices.items():
                    if device["ip"] == source_ip:
                        device["last_rx"] = time.time()
                        break
            else:
                log_to_terminal(f"[UNHANDLED @{source_ip}]: {msg}", terminal_cb)

        except socket.timeout:
            continue
        except Exception as e:
            log_to_terminal(f"Recv_loop error: {e}\n", terminal_cb)
import socket
import threading
import time
import datetime
from gui import build_gui

CLEARCORE_PORT = 8888
CLIENT_PORT = 6272
HEARTBEAT_INTERVAL = 0.3
DISCOVERY_RETRY_INTERVAL = 2.0
TIMEOUT_THRESHOLD = 1.0
TORQUE_HISTORY_LENGTH = 200

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    sock.bind(('', CLIENT_PORT))
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(0.1)
except OSError as e:
    print(f"ERROR binding socket: {e}. Port {CLIENT_PORT} may be in use.")
    exit()

clearcore_ip = None
last_rx_time = 0
connected = False
last_ui_logic_state = None


def log_to_terminal(msg, terminal_cb_func):
    timestr = datetime.datetime.now().strftime("[%H:%M:%S.%f]")[:-3]
    if terminal_cb_func:
        terminal_cb_func(f"{timestr} {msg}\n")
    else:
        print(f"{timestr} {msg}")


def discover(terminal_cb_func):
    msg = f"DISCOVER_TELEM PORT={CLIENT_PORT}"
    try:
        sock.sendto(msg.encode(), ('255.255.255.255', CLEARCORE_PORT))
        log_to_terminal(f"Discovery: '{msg}'", terminal_cb_func)
    except Exception as e:
        log_to_terminal(f"Discovery error: {e}", terminal_cb_func)


def send_udp(msg, terminal_cb_func):
    if clearcore_ip:
        try:
            sock.sendto(msg.encode(), (clearcore_ip, CLEARCORE_PORT))
        except Exception as e:
            log_to_terminal(f"UDP send error to {clearcore_ip}: {e}", terminal_cb_func)
    else:
        log_to_terminal(f"UDP send skipped (no IP): {msg}", terminal_cb_func)


def monitor_connection(gui_refs):
    global connected, last_rx_time, clearcore_ip
    prev_conn = False
    while True:
        now = time.time();
        curr_conn = connected
        if curr_conn and (now - last_rx_time) > TIMEOUT_THRESHOLD:
            log_to_terminal("Connection timeout.", gui_refs['terminal_cb'])
            connected = False;
            clearcore_ip = None;
            curr_conn = False
            gui_refs['update_status']("🔌 Disconnected (Timeout)")
            gui_refs['update_state']("Unknown")
            gui_refs['prominent_firmware_state_var'].set("DISCONNECTED")
            if gui_refs['prominent_state_display_frame'].winfo_exists():
                gui_refs['prominent_state_display_frame'].config(bg="#500000")
                if gui_refs['prominent_state_label'].winfo_exists(): gui_refs['prominent_state_label'].config(
                    bg="#500000", fg="white")
            for i in [1, 2]: gui_refs['update_motor_status'](i, "N/A", "#808080"); gui_refs['update_enabled_status'](i,
                                                                                                                     "N/A",
                                                                                                                     "#808080")
        if curr_conn != prev_conn:
            if curr_conn: log_to_terminal(f"Connected to {clearcore_ip}", gui_refs['terminal_cb']); gui_refs[
                'update_status'](f"✅ Connected to {clearcore_ip}")
            prev_conn = curr_conn
        time.sleep(HEARTBEAT_INTERVAL)


def auto_discover_loop(terminal_cb_func):
    while True:
        if not connected: discover(terminal_cb_func)
        time.sleep(DISCOVERY_RETRY_INTERVAL)


def parse_telemetry(msg, gui_refs):  # gui_refs now contains all necessary UI elements
    global last_ui_logic_state
    try:
        parts = {};
        normalized_msg = msg.replace('%', '')
        for item in normalized_msg.split(','):
            if ':' in item: key, value = item.split(':', 1); parts[key.strip()] = value.strip()

        fw_main_state = parts.get("MAIN_STATE", "N/A")
        fw_moving_state = parts.get("MOVING_STATE", "N/A")
        fw_error_state = parts.get("ERROR_STATE", "N/A")

        # Update prominent status display
        current_status_text = "---";
        bg_color = "#444444";
        fg_color = "white"
        if fw_error_state != "ERROR_NONE" and fw_error_state != "N/A":
            current_status_text = f"{fw_error_state.replace('ERROR_', '')}"
            bg_color = "#8B0000"  # Dark Red
        elif fw_main_state == "MOVING":
            current_status_text = fw_moving_state.replace('MOVING_', '')
            bg_color = "#B8860B";
            fg_color = "white"
        elif fw_main_state == "STANDBY":
            current_status_text = "STANDBY"  # Simplified
            bg_color = "#005000"  # Dark Green
        elif fw_main_state == "TEST_STATE":
            current_status_text = "TEST MODE";
            bg_color = "#E07000";
            fg_color = "white"
        elif fw_main_state == "DISABLED":
            current_status_text = "SYSTEM DISABLED";
            bg_color = "#333333"
        else:
            current_status_text = fw_main_state

        gui_refs['prominent_firmware_state_var'].set(current_status_text.upper())
        if gui_refs['prominent_state_display_frame'].winfo_exists():
            gui_refs['prominent_state_display_frame'].config(bg=bg_color)
            if gui_refs['prominent_state_label'].winfo_exists():
                gui_refs['prominent_state_label'].config(bg=bg_color, fg=fg_color)

        # Determine UI logical state for mode buttons & jog/contextual panel enable
        ui_logic_state = "Standby"
        if fw_main_state == "STANDBY":
            ui_logic_state = "Standby"
        elif fw_main_state == "TEST_STATE":
            ui_logic_state = "Test Mode"
        elif fw_main_state == "MOVING":
            if fw_moving_state == "MOVING_JOG":
                ui_logic_state = "Jog"
            elif fw_moving_state == "MOVING_HOMING":
                ui_logic_state = "Homing Mode"
            elif fw_moving_state == "MOVING_FEEDING":
                ui_logic_state = "Feed Mode"
            else:
                ui_logic_state = "Moving"  # A generic moving state for UI
        elif fw_main_state == "ERROR":
            ui_logic_state = "Error"
        elif fw_main_state == "DISABLED":
            ui_logic_state = "Disabled"

        if ui_logic_state != last_ui_logic_state:
            gui_refs['update_state'](ui_logic_state);
            last_ui_logic_state = ui_logic_state

        t1_str = parts.get("torque1", "---");
        t1 = float(t1_str) if t1_str != "---" else 0.0
        h1 = int(parts.get("hlfb1", "0"));
        e1 = int(parts.get("enabled1", "0"))  # motorsAreEnabled
        pos_cmd1 = int(parts.get("pos_cmd1", "0"))
        t2_str = parts.get("torque2", "---");
        t2 = float(t2_str) if t2_str != "---" else 0.0
        h2 = int(parts.get("hlfb2", "0"));
        pos_cmd2 = int(parts.get("pos_cmd2", "0"))
        msteps = int(parts.get("machine_steps", "0"))

        gui_refs['commanded_steps_var'].set(str(msteps))
        ct = time.time()
        if not gui_refs['torque_times'] or ct > gui_refs['torque_times'][-1]:
            gui_refs['torque_history1'].append(t1);
            gui_refs['torque_history2'].append(t2)
            gui_refs['torque_times'].append(ct)
            while len(gui_refs['torque_history1']) > TORQUE_HISTORY_LENGTH:
                gui_refs['torque_history1'].pop(0);
                gui_refs['torque_history2'].pop(0);
                gui_refs['torque_times'].pop(0)
        if gui_refs['torque_times']: gui_refs['update_torque_plot'](t1, t2, gui_refs['torque_times'],
                                                                    gui_refs['torque_history1'],
                                                                    gui_refs['torque_history2'])
        gui_refs['update_position_cmd'](pos_cmd1, pos_cmd2)

        motors_enabled = (e1 == 1);
        en_txt, en_clr = ("Enabled", "#00ff88") if motors_enabled else ("Disabled", "#ff4444")
        motor_off_txt, motor_off_clr = ("N/A (Dis.)", "#808080")  # Shortened
        gui_refs['update_enabled_status'](1, en_txt, en_clr);
        gui_refs['update_enabled_status'](2, en_txt, en_clr)
        if motors_enabled:
            gui_refs['update_motor_status'](1, "At Pos" if h1 == 1 else "Moving/Busy", "#00bfff")  # Shortened
            gui_refs['update_motor_status'](2, "At Pos" if h2 == 1 else "Moving/Busy", "yellow")  # Shortened
        else:
            gui_refs['update_motor_status'](1, motor_off_txt, motor_off_clr);
            gui_refs['update_motor_status'](2, motor_off_txt, motor_off_clr)
    except Exception as e:
        log_to_terminal(f"Telemetry parse error: {e} in '{msg}'", gui_refs['terminal_cb'])


def recv_loop(gui_refs):
    global clearcore_ip, connected, last_rx_time
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode('utf-8', errors='replace').strip()
            if "MAIN_STATE" in msg and "MOVING_STATE" in msg:
                if not connected or clearcore_ip != addr[0]: clearcore_ip = addr[0]
                connected = True;
                last_rx_time = time.time()
                parse_telemetry(msg, gui_refs)
            else:
                if clearcore_ip and addr[0] == clearcore_ip: log_to_terminal(f"[CC@{addr[0]}]: {msg}",
                                                                             gui_refs['terminal_cb'])
        except socket.timeout:
            continue
        except Exception as e:
            log_to_terminal(f"Recv_loop error: {e}", gui_refs['terminal_cb']); time.sleep(0.1)


def main():
    gui = build_gui(
        lambda msg_to_send: send_udp(msg_to_send, gui.get('terminal_cb')),
        lambda: discover(gui.get('terminal_cb'))
    )
    threading.Thread(target=recv_loop, args=(gui,), daemon=True).start()
    threading.Thread(target=monitor_connection, args=(gui,), daemon=True).start()
    threading.Thread(target=auto_discover_loop, args=(gui.get('terminal_cb'),), daemon=True).start()
    gui['root'].mainloop()


if __name__ == "__main__":
    main()
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
    msg = f"DISCOVER_TELEM PORT={CLIENT_PORT}"  # CMD_STR_DISCOVER_TELEM
    try:
        sock.sendto(msg.encode(), ('255.255.255.255', CLEARCORE_PORT))
        log_to_terminal(f"Discovery: '{msg}' sent to broadcast", terminal_cb_func)  # Re-enabled this log
    except Exception as e:
        log_to_terminal(f"Discovery error: {e}", terminal_cb_func)


def send_udp(msg, terminal_cb_func):
    if clearcore_ip:
        try:
            sock.sendto(msg.encode(), (clearcore_ip, CLEARCORE_PORT))
            # log_to_terminal(f"Sent to {clearcore_ip}: '{msg}'", terminal_cb_func) # Optional: for debugging all sends
        except Exception as e:
            log_to_terminal(f"UDP send error to {clearcore_ip}: {e}", terminal_cb_func)
    else:
        # Log if trying to send a command other than discovery when IP is unknown
        if not msg.startswith("DISCOVER_TELEM"):
            log_to_terminal(f"Cannot send '{msg}': No ClearCore IP. Discovery active.", terminal_cb_func)
        # If you want to log that discovery is happening even when other sends are blocked:
        # else:
        #    log_to_terminal(f"Discovery active, send of '{msg}' skipped until IP is known.", terminal_cb_func)


def monitor_connection(gui_refs):
    global connected, last_rx_time, clearcore_ip, last_fw_main_state_for_gui_update
    prev_conn_status = False
    while True:
        now = time.time()
        current_conn_status = connected

        if current_conn_status and (now - last_rx_time) > TIMEOUT_THRESHOLD:
            log_to_terminal("Connection timeout.", gui_refs['terminal_cb'])
            connected = False
            current_conn_status = False  # Update for this iteration
            # No need to nullify clearcore_ip here, auto_discover will try to find it or a new one

        if current_conn_status != prev_conn_status:
            if current_conn_status:
                log_to_terminal(f"Connected to ClearCore at {clearcore_ip}", gui_refs['terminal_cb'])
                gui_refs['update_status'](f"✅ Connected to {clearcore_ip}")
            else:
                log_to_terminal("Disconnected from ClearCore.", gui_refs['terminal_cb'])
                gui_refs['update_status']("🔌 Disconnected (Timeout)")
                gui_refs['prominent_firmware_state_var'].set("DISCONNECTED")
                if gui_refs['prominent_state_display_frame'].winfo_exists():
                    gui_refs['prominent_state_display_frame'].config(bg="#500000")  # Dark Red
                if gui_refs['prominent_state_label'].winfo_exists():
                    gui_refs['prominent_state_label'].config(bg="#500000", fg="white")

                # Reset GUI fields to reflect disconnection
                gui_refs['main_state_var'].set("UNKNOWN")
                gui_refs['homing_state_var'].set("---")
                gui_refs['feed_state_var'].set("---")
                gui_refs['error_state_var'].set("---")
                gui_refs['machine_steps_var'].set("0")
                gui_refs['cartridge_steps_var'].set("0")
                gui_refs['enable_disable_var'].set("Disabled")  # Reflects inability to be enabled

                for i in [1, 2]:  # Motor 0 and Motor 1
                    gui_refs[f'motor_state{i}_var'].set("N/A")
                    gui_refs[f'enabled_state{i}_var'].set("N/A")  # Reflect actual state from telem if available
                    gui_refs[f'torque_value{i}_var'].set("--- %")
                    gui_refs[f'position_cmd{i}_var'].set("0")

                gui_refs['update_state']("UNKNOWN")  # Trigger GUI panel updates
                last_fw_main_state_for_gui_update = "UNKNOWN"

            prev_conn_status = current_conn_status
        time.sleep(HEARTBEAT_INTERVAL)


def auto_discover_loop(terminal_cb_func):
    while True:
        if not connected:
            discover(terminal_cb_func)
        time.sleep(DISCOVERY_RETRY_INTERVAL)


def parse_telemetry(msg, gui_refs):
    global last_fw_main_state_for_gui_update
    try:
        parts = {}
        # Normalize by removing '%' before splitting to handle torque values like "20.0%"
        normalized_msg = msg  # Assuming torque already comes without % or handled in gui.py
        for item in normalized_msg.split(','):
            if ':' in item:
                key, value = item.split(':', 1)
                parts[key.strip()] = value.strip()

        # Extract states from telemetry
        fw_main_state = parts.get("MAIN_STATE", "N/A")
        fw_homing_state = parts.get("HOMING_STATE", "N/A")
        fw_feed_state = parts.get("FEED_STATE", "N/A")
        fw_error_state = parts.get("ERROR_STATE", "N/A")

        # Update basic state displays in GUI
        gui_refs['main_state_var'].set(fw_main_state)
        gui_refs['homing_state_var'].set(fw_homing_state)
        gui_refs['feed_state_var'].set(fw_feed_state)
        gui_refs['error_state_var'].set(fw_error_state if fw_error_state != "ERROR_NONE" else "No Error")

        # Update prominent status display and color
        prominent_text = "---"
        bg_color = "#444444"  # Default dark grey
        fg_color = "white"

        if fw_error_state != "ERROR_NONE" and fw_error_state != "N/A":
            prominent_text = fw_error_state.replace("ERROR_", "").replace("_ABORT", " ABORT")
            bg_color = "#8B0000"  # Dark Red
        elif fw_main_state == "DISABLED_MODE":
            prominent_text = "SYSTEM DISABLED"
            bg_color = "#333333"  # Darker Grey
        elif fw_main_state == "TEST_MODE":
            prominent_text = "TEST MODE ACTIVE"
            bg_color = "#E07000"  # Orange
        elif fw_main_state == "HOMING_MODE":
            prominent_text = f"HOMING: {fw_homing_state.replace('HOMING_', '')}"
            bg_color = "#582A72"  # Purple
        elif fw_main_state == "FEED_MODE":
            prominent_text = f"FEED: {fw_feed_state.replace('FEED_', '')}"
            bg_color = "#008080"  # Cyan
        elif fw_main_state == "JOG_MODE":
            prominent_text = "JOG MODE ACTIVE"
            bg_color = "#767676"  # Grey
        elif fw_main_state == "STANDBY_MODE":
            prominent_text = "STANDBY"
            bg_color = "#005000"  # Dark Green
        else:
            prominent_text = fw_main_state  # Fallback

        gui_refs['prominent_firmware_state_var'].set(prominent_text)
        if gui_refs['prominent_state_display_frame'].winfo_exists():
            gui_refs['prominent_state_display_frame'].config(bg=bg_color)
        if gui_refs['prominent_state_label'].winfo_exists():
            gui_refs['prominent_state_label'].config(bg=bg_color, fg=fg_color)

        # Update GUI contextual panels if MAIN_STATE has changed
        if fw_main_state != last_fw_main_state_for_gui_update:
            gui_refs['update_state'](fw_main_state)
            last_fw_main_state_for_gui_update = fw_main_state

        # Motor data
        t1_str = parts.get("torque1", "---")
        # t1 = float(t1_str) if t1_str != "---" else 0.0 # Actual conversion handled by update_torque_plot
        hlfb1 = int(parts.get("hlfb1", "0"))  # 0 = busy/moving, 1 = asserted/at_pos
        enabled1_fw = int(parts.get("enabled1", "0"))  # Reflects motorsAreEnabled from firmware
        pos_cmd1 = int(parts.get("pos_cmd1", "0"))

        t2_str = parts.get("torque2", "---")
        # t2 = float(t2_str) if t2_str != "---" else 0.0
        hlfb2 = int(parts.get("hlfb2", "0"))
        # enabled2_fw = int(parts.get("enabled2", "0")) # Same as enabled1
        pos_cmd2 = int(parts.get("pos_cmd2", "0"))

        m_steps = parts.get("machine_steps", "0")
        c_steps = parts.get("cartridge_steps", "0")

        gui_refs['machine_steps_var'].set(str(m_steps))
        gui_refs['cartridge_steps_var'].set(str(c_steps))

        # Update torque plot (gui.py's update_torque_plot handles string "---")
        ct = time.time()
        if not gui_refs['torque_times'] or ct > gui_refs['torque_times'][
            -1]:  # Avoid duplicate timestamps for fast telem
            gui_refs['torque_history1'].append(float(t1_str) if t1_str != "---" else 0.0)
            gui_refs['torque_history2'].append(float(t2_str) if t2_str != "---" else 0.0)
            gui_refs['torque_times'].append(ct)
            while len(gui_refs['torque_history1']) > TORQUE_HISTORY_LENGTH:
                gui_refs['torque_history1'].pop(0)
                gui_refs['torque_history2'].pop(0)
                gui_refs['torque_times'].pop(0)

        if gui_refs['torque_times']:  # Check if there's data to plot
            # Pass raw strings to update_torque_plot, it handles "---"
            gui_refs['update_torque_plot'](t1_str, t2_str, gui_refs['torque_times'],
                                           gui_refs['torque_history1'], gui_refs['torque_history2'])

        gui_refs['update_position_cmd'](pos_cmd1, pos_cmd2)

        # Update motor enable status display and the Enable/Disable button toggle
        actual_motors_enabled = (enabled1_fw == 1)
        gui_refs['enable_disable_var'].set("Enabled" if actual_motors_enabled else "Disabled")

        en_txt_fw, en_clr_fw = ("Enabled", "#00cc66") if actual_motors_enabled else ("Disabled", "#ff4444")

        # Update individual motor enabled status (will be same for both due to global firmware flag)
        gui_refs['enabled_state1_var'].set(en_txt_fw)
        if 'enabled_status_label1' in gui_refs and gui_refs['enabled_status_label1'].winfo_exists():
            gui_refs['enabled_status_label1'].config(fg=en_clr_fw)

        gui_refs['enabled_state2_var'].set(en_txt_fw)
        if 'enabled_status_label2' in gui_refs and gui_refs['enabled_status_label2'].winfo_exists():
            gui_refs['enabled_status_label2'].config(fg=en_clr_fw)

        # Update HLFB status
        motor_off_txt, motor_off_clr = ("N/A (Dis.)", "#808080")
        if actual_motors_enabled:
            # HLFB_ASSERTED (1) typically means "in position" or "ready"
            # HLFB_NOT_ASSERTED (0) can mean "moving", "fault", "not enabled"
            # Firmware uses HlfbState values; 1 is HLFB_ASSERTED
            gui_refs['motor_state1_var'].set("Ready/At Pos" if hlfb1 == 1 else "Moving/Busy")
            if 'motor_status_label1' in gui_refs and gui_refs['motor_status_label1'].winfo_exists():
                gui_refs['motor_status_label1'].config(fg="#00bfff")

            gui_refs['motor_state2_var'].set("Ready/At Pos" if hlfb2 == 1 else "Moving/Busy")
            if 'motor_status_label2' in gui_refs and gui_refs['motor_status_label2'].winfo_exists():
                gui_refs['motor_status_label2'].config(fg="yellow")
        else:
            gui_refs['motor_state1_var'].set(motor_off_txt)
            if 'motor_status_label1' in gui_refs and gui_refs['motor_status_label1'].winfo_exists():
                gui_refs['motor_status_label1'].config(fg=motor_off_clr)
            gui_refs['motor_state2_var'].set(motor_off_txt)
            if 'motor_status_label2' in gui_refs and gui_refs['motor_status_label2'].winfo_exists():
                gui_refs['motor_status_label2'].config(fg=motor_off_clr)

    except Exception as e:
        log_to_terminal(f"Telemetry parse error: {e} in '{msg}'", gui_refs['terminal_cb'])


def recv_loop(gui_refs):
    global clearcore_ip, connected, last_rx_time
    while True:
        try:
            data, addr = sock.recvfrom(1024)  # Buffer size
            msg = data.decode('utf-8', errors='replace').strip()

            # Check for a key telemetry field to identify valid packets
            if "MAIN_STATE:" in msg and "torque1:" in msg:  # A more robust check for telemetry
                if not connected or clearcore_ip != addr[0]:
                    clearcore_ip = addr[0]  # Learn or update ClearCore IP
                    # No need to log "New ClearCore IP" here as monitor_connection handles "Connected" log
                connected = True
                last_rx_time = time.time()
                parse_telemetry(msg, gui_refs)
            else:
                # Log other messages from the known ClearCore IP
                if clearcore_ip and addr[0] == clearcore_ip:
                    log_to_terminal(f"[CC@{addr[0]}]: {msg}", gui_refs['terminal_cb'])
                # else: # Optional: Log messages from unknown IPs
                # log_to_terminal(f"[Unknown@{addr[0]}]: {msg}", gui_refs['terminal_cb'])

        except socket.timeout:
            continue  # Normal for non-blocking socket
        except Exception as e:
            log_to_terminal(f"Recv_loop error: {e}", gui_refs['terminal_cb'])
            time.sleep(0.1)  # Brief pause on other errors


def main():
    shared_gui_refs = {}

    gui = build_gui(
        lambda msg_to_send: send_udp(msg_to_send, shared_gui_refs.get('terminal_cb')),
        lambda: discover(shared_gui_refs.get('terminal_cb'))
    )
    shared_gui_refs.update(gui)

    threading.Thread(target=recv_loop, args=(gui,), daemon=True).start()
    threading.Thread(target=monitor_connection, args=(gui,), daemon=True).start()
    threading.Thread(target=auto_discover_loop, args=(gui.get('terminal_cb'),), daemon=True).start()

    gui['root'].mainloop()


if __name__ == "__main__":
    main()
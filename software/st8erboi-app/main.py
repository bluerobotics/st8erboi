import socket
import threading
import time
import datetime
from gui import build_gui

CLEARCORE_PORT = 8888
HEARTBEAT_INTERVAL = 0.02
TIMEOUT_THRESHOLD = 1.0
TORQUE_HISTORY_LENGTH = 200

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', 6272))
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.settimeout(0.1)

clearcore_ip = None
clearcore_connected = False
last_telem_time = 0

def log_to_terminal(msg, terminal_cb):
    timestr = datetime.datetime.now().strftime("[%H:%M:%S] ")
    terminal_cb(f"{timestr}{msg}\n")

def discover(terminal_cb):
    global clearcore_ip
    msg = f"DISCOVER_TELEM PORT=6272"
    try:
        destination = ('255.255.255.255', CLEARCORE_PORT)
        sock.sendto(msg.encode(), destination)
        log_to_terminal(f"Discovery packet sent: '{msg}' to {destination[0]}:{destination[1]}", terminal_cb)
    except Exception as e:
        log_to_terminal(f"Error sending discovery packet: {e}", terminal_cb)
    clearcore_ip = None

def send_udp(msg, terminal_cb):
    global clearcore_ip
    if clearcore_ip:
        try:
            log_to_terminal(f"Sending to {clearcore_ip}:{CLEARCORE_PORT} - {msg}", terminal_cb)
            sock.sendto(msg.encode(), (clearcore_ip, CLEARCORE_PORT))
        except Exception as e:
            log_to_terminal(f"UDP send error: {e}", terminal_cb)
    else:
        log_to_terminal(f"UDP send skipped (no ClearCore IP): {msg}", terminal_cb)

def send_heartbeat(terminal_cb):
    discover(terminal_cb)  # Always sends "DISCOVER_TELEM PORT=6272"

def monitor_connection(update_status_cb, update_motor_cb, terminal_cb):
    global clearcore_connected, last_telem_time
    was_connected = False
    while True:
        send_heartbeat(terminal_cb)
        time.sleep(HEARTBEAT_INTERVAL)
        if time.time() - last_telem_time > TIMEOUT_THRESHOLD:
            if clearcore_connected:
                clearcore_connected = False
                log_to_terminal("Connection lost: timed out.", terminal_cb)
                update_status_cb("🔌 Disconnected")
                update_motor_cb(1, "Unknown", "#ffffff")
                update_motor_cb(2, "Unknown", "#ffffff")
        elif clearcore_ip:
            if not was_connected:
                log_to_terminal(f"Connection established: {clearcore_ip}", terminal_cb)
                was_connected = True
            update_status_cb(f"✅ Connected to {clearcore_ip}")

def parse_telemetry(msg, update_motor_cb, update_enabled_cb, commanded_steps_var, torque_history1, torque_history2, torque_times, update_torque_plot, terminal_cb):
    try:
        parts = dict(p.strip().split(':', 1) for p in msg.replace('%','').split(',') if ':' in p)
        t1 = float(parts.get("torque1", "0").strip()) if parts.get("torque1", "---").strip() != "---" else 0.0
        h1 = int(parts.get("hlfb1", "0").strip())
        e1 = int(parts.get("enabled1", "0").strip())
        pc1 = int(parts.get("pos_cmd1", "0").strip())
        t2 = float(parts.get("torque2", "0").strip()) if parts.get("torque2", "---").strip() != "---" else 0.0
        h2 = int(parts.get("hlfb2", "0").strip())
        e2 = int(parts.get("enabled2", "0").strip())
        pc2 = int(parts.get("pos_cmd2", "0").strip())
        msteps = int(parts.get("machine_steps", "0").strip())
        csteps = int(parts.get("cartridge_steps", "0").strip())
        commanded_steps_var.set(str(msteps))

        torque_history1.append(t1)
        torque_history2.append(t2)
        torque_times.append(time.time())
        if len(torque_history1) > TORQUE_HISTORY_LENGTH:
            torque_history1.pop(0)
            torque_history2.pop(0)
            torque_times.pop(0)
        update_torque_plot(t1, t2, torque_times, torque_history1, torque_history2)

        for motor, hlfb, enabled in [(1, h1, e1), (2, h2, e2)]:
            if enabled == 0:
                update_enabled_cb(motor, "Disabled", "#ff4444")
                update_motor_cb(motor, "N/A", "#888888")
            else:
                update_enabled_cb(motor, "Enabled", "#00ff88")
                if hlfb == 1:
                    update_motor_cb(motor, "At Position", "#00bfff" if motor == 1 else "yellow")
                else:
                    update_motor_cb(motor, "Moving / Busy", "#ffff55")
    except Exception as e:
        log_to_terminal(f"Error parsing telemetry: {e}", terminal_cb)

def recv_loop(update_status_cb, update_motor_cb, update_enabled_cb, commanded_steps_var,
              torque_history1, torque_history2, torque_times, update_torque_plot, terminal_cb):
    global clearcore_ip, clearcore_connected, last_telem_time
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode().strip()
            log_to_terminal(f"Received from {addr[0]}: {msg}", terminal_cb)
            # Save IP if not set
            if clearcore_ip != addr[0]:
                clearcore_ip = addr[0]
            last_telem_time = time.time()
            clearcore_connected = True
            terminal_cb(f"[{addr[0]}]: {msg}\n")
            parse_telemetry(msg, update_motor_cb, update_enabled_cb, commanded_steps_var,
                            torque_history1, torque_history2, torque_times, update_torque_plot, terminal_cb)
        except socket.timeout:
            continue
        except Exception as e:
            log_to_terminal(f"Error in recv_loop: {e}", terminal_cb)
            terminal_cb(f"Error processing message: {e}\n")

def main():
    gui = build_gui(
        lambda msg: send_udp(msg, gui['terminal_cb']),
        lambda: discover(gui['terminal_cb']),
    )
    threading.Thread(target=recv_loop, args=(
        gui['update_status'],
        gui['update_motor_status'],
        gui['update_enabled_status'],
        gui['commanded_steps_var'],
        gui['torque_history1'],
        gui['torque_history2'],
        gui['torque_times'],
        gui['update_torque_plot'],
        gui['terminal_cb'],
    ), daemon=True).start()

    threading.Thread(target=monitor_connection, args=(
        gui['update_status'],
        gui['update_motor_status'],
        gui['terminal_cb'],
    ), daemon=True).start()

    gui['root'].mainloop()

if __name__ == "__main__":
    main()

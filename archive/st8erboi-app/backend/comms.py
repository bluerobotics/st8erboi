# backend/comms.py
import asyncio
import socket
import time
from typing import Dict, Any

# --- Configuration ---
CLEARCORE_PORT = 8888
CLIENT_PORT = 6272
DISCOVERY_INTERVAL = 2.0
TIMEOUT_THRESHOLD = 3.0

# --- Application State ---
# This dictionary is the "single source of truth" for all device and telemetry data.
# It will be managed by this module and read by the API module.
app_state: Dict[str, Any] = {
    "devices": {
        "injector": {"ip": None, "last_rx": 0, "connected": False},
        "fillhead": {"ip": None, "last_rx": 0, "connected": False}
    },
    "telemetry": {
        "injector_connected": False, "injector_ip": "0.0.0.0", "fillhead_connected": False, "fillhead_ip": "0.0.0.0",
        "fh_pos_x": 0.0, "fh_pos_y": 0.0, "fh_pos_z": 0.0, "fh_homed_x": False, "fh_homed_y": False, "fh_homed_z": False,
        "fh_torque_x": 0, "fh_torque_y": 0, "fh_torque_z": 0, "injector_pos_machine": 0.0, "injector_pos_cartridge": 0.0,
        "injector_homed": False, "injector_torque": 0, "pinch_pos_fill": 0.0, "pinch_homed_fill": False, "pinch_torque_fill": 0,
        "pinch_pos_vac": 0.0, "pinch_homed_vac": False, "pinch_torque_vac": 0, "injector_state": 'Idle', "fillhead_state": 'Idle',
        "vacuum_psig": 0.0, "heater_temp_c": 0.0
    }
}

# --- UDP Socket Setup ---
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.settimeout(0.1)
try:
    sock.bind(('', CLIENT_PORT))
except OSError as e:
    print(f"FATAL: Could not bind to port {CLIENT_PORT}. Is another instance running? Error: {e}")
    exit()

# --- Core Functions ---

def send_udp_command(ip: str, port: int, msg: str):
    """Sends a UDP message."""
    try:
        sock.sendto(msg.encode(), (ip, port))
    except Exception as e:
        print(f"UDP send error to {ip}:{port}: {e}")

def parse_injector_telemetry(payload: str):
    """Parses telemetry from the injector and updates the global state."""
    try:
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)
        app_state["telemetry"]["injector_state"] = parts.get("MAIN_STATE", "---")
        app_state["telemetry"]["injector_homed"] = parts.get("homed0", "0") == "1" and parts.get("homed1", "0") == "1"
        app_state["telemetry"]["injector_pos_machine"] = float(parts.get("machine_mm", 0.0))
        app_state["telemetry"]["injector_pos_cartridge"] = float(parts.get("cartridge_mm", 0.0))
        app_state["telemetry"]["injector_torque"] = float(parts.get('torque0', 0.0))
        app_state["telemetry"]["pinch_pos_fill"] = float(parts.get('pos_mm2', 0.0))
        app_state["telemetry"]["pinch_homed_fill"] = parts.get("homed2", "0") == "1"
        app_state["telemetry"]["pinch_torque_fill"] = float(parts.get('torque2', 0.0))
        app_state["telemetry"]["vacuum_psig"] = float(parts.get('vacuum_psig', 0.0))
        app_state["telemetry"]["heater_temp_c"] = float(parts.get("temp_c", 0.0))
    except Exception as e:
        print(f"Injector telemetry parse error: {e}")

def parse_fillhead_telemetry(payload: str):
    """Parses telemetry from the fillhead and updates the global state."""
    try:
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)
        app_state["telemetry"]["fillhead_state"] = parts.get("x_s", "UNKNOWN")
        app_state["telemetry"]["fh_pos_x"] = float(parts.get('x_p', 0.0))
        app_state["telemetry"]["fh_pos_y"] = float(parts.get('y_p', 0.0))
        app_state["telemetry"]["fh_pos_z"] = float(parts.get('z_p', 0.0))
        app_state["telemetry"]["fh_homed_x"] = parts.get("x_h", "0") == "1"
        app_state["telemetry"]["fh_homed_y"] = parts.get("y_h", "0") == "1"
        app_state["telemetry"]["fh_homed_z"] = parts.get("z_h", "0") == "1"
        app_state["telemetry"]["fh_torque_x"] = float(parts.get('x_t', 0.0))
        app_state["telemetry"]["fh_torque_y"] = float(parts.get('y_t', 0.0))
        app_state["telemetry"]["fh_torque_z"] = float(parts.get('z_t', 0.0))
    except Exception as e:
        print(f"Fillhead telemetry parse error: {e}")

# --- Background Tasks ---

async def udp_receiver():
    """Listens for UDP packets and routes them to parsers."""
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode('utf-8', errors='replace').strip()
            source_ip = addr[0]

            if msg == "DISCOVERY: INJECTOR DISCOVERED":
                app_state["devices"]["injector"]["ip"] = source_ip
            elif msg == "DISCOVERY: FILLHEAD DISCOVERED":
                app_state["devices"]["fillhead"]["ip"] = source_ip
            elif msg.startswith("INJ_TELEM_GUI:"):
                app_state["devices"]["injector"]["last_rx"] = time.time()
                parse_injector_telemetry(msg.split("INJ_TELEM_GUI:")[1])
            elif msg.startswith("FH_TELEM_GUI:"):
                app_state["devices"]["fillhead"]["last_rx"] = time.time()
                parse_fillhead_telemetry(msg.split("FH_TELEM_GUI: ")[1])
            else:
                print(f"[UNHANDLED @{source_ip}]: {msg}")

        except socket.timeout:
            pass
        except Exception as e:
            print(f"UDP Recv Error: {e}")
        await asyncio.sleep(0.01)

async def connection_monitor():
    """Periodically checks for device timeouts and sends discovery packets."""
    while True:
        now = time.time()
        for key, device in app_state["devices"].items():
            if device["ip"] and not device["connected"]:
                device["connected"] = True
                device["last_rx"] = now
                print(f"Device connected: {key} at {device['ip']}")

            if device["connected"] and (now - device["last_rx"]) > TIMEOUT_THRESHOLD:
                device["connected"] = False
                device["ip"] = None
                print(f"Device timed out: {key}")

            app_state["telemetry"][f"{key}_connected"] = device["connected"]
            app_state["telemetry"][f"{key}_ip"] = device["ip"] or "0.0.0.0"

            if not device["connected"]:
                send_udp_command('192.168.1.255', CLEARCORE_PORT, f"DISCOVER_{key.upper()} PORT={CLIENT_PORT}")
            
            if device["connected"]:
                 send_udp_command(device["ip"], CLEARCORE_PORT, "REQUEST_TELEM")

        await asyncio.sleep(DISCOVERY_INTERVAL)
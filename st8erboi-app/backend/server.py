import socket
import threading
import time
import json
from flask import Flask, request, jsonify
from flask_cors import CORS
from collections import deque

# =============================================================================
# Device and Communication Management (Consolidated)
# =============================================================================

# A thread-safe queue to hold log messages from background threads.
log_queue = deque(maxlen=100)

class Device:
    """Holds the state for a single network-connected controller."""
    def __init__(self, name):
        self.name = name
        self.ip = None
        self.last_seen = 0
        self.is_connected = False
        self.is_peer_connected = False
        self.telemetry = {}
        self.lock = threading.Lock()

    def update_telemetry(self, new_data):
        """Thread-safe update of telemetry data."""
        with self.lock:
            self.telemetry.update(new_data)
            self.is_connected = True
            self.last_seen = time.time()

    def set_connection_status(self, connected, ip=None):
        """Thread-safe update of connection status."""
        with self.lock:
            self.is_connected = connected
            if connected:
                self.ip = ip
                self.last_seen = time.time()
            else:
                self.ip = None
                self.is_peer_connected = False
                self.telemetry = {} # Clear telemetry on disconnect

    def to_dict(self):
        """Returns a dictionary representation of the device's state."""
        with self.lock:
            return {
                "name": self.name,
                "ip": self.ip,
                "isConnected": self.is_connected,
                "isPeerConnected": self.is_peer_connected,
                "lastSeen": self.last_seen,
                "telemetry": self.telemetry
            }

class DeviceManager:
    """Singleton class to manage the state of all devices."""
    _instance = None

    def __new__(cls, comms_instance=None):
        if cls._instance is None:
            cls._instance = super(DeviceManager, cls).__new__(cls)
            cls._instance.devices = {
                "injector": Device("injector"),
                "fillhead": Device("fillhead")
            }
            cls._instance.lock = threading.Lock()
            cls._instance.comms = comms_instance
        return cls._instance

    def set_comms(self, comms_instance):
        self.comms = comms_instance

    def get_device(self, name):
        return self.devices.get(name)

    def get_all_devices_status(self):
        with self.lock:
            return {name: dev.to_dict() for name, dev in self.devices.items()}
            
    def check_for_peer_connection(self):
        with self.lock:
            injector = self.get_device("injector")
            fillhead = self.get_device("fillhead")

            if not (self.comms and injector and fillhead and injector.is_connected and fillhead.is_connected):
                return

            if not injector.is_peer_connected and fillhead.ip:
                log_queue.append(f"INFO: Brokering IP. Telling Injector about Fillhead at {fillhead.ip}")
                self.comms.send_command_to_device("injector", f"SET_PEER_IP {fillhead.ip}")

            if not fillhead.is_peer_connected and injector.ip:
                log_queue.append(f"INFO: Brokering IP. Telling Fillhead about Injector at {injector.ip}")
                self.comms.send_command_to_device("fillhead", f"SET_PEER_IP {injector.ip}")

class UdpComms:
    def __init__(self, manager):
        self.CLEARCORE_PORT = 8888
        self.CLIENT_PORT = 6272
        self.BROADCAST_ADDR = '192.168.1.255'
        self.DISCOVERY_INTERVAL = 2.0
        self.TELEMETRY_INTERVAL = 0.5
        self.TIMEOUT_THRESHOLD = 3.0
        
        self.device_manager = manager
        self.sock = None
        self.is_running = False

    def setup_socket(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.settimeout(0.5)
        try:
            self.sock.bind(('', self.CLIENT_PORT))
            print(f"UDP Socket bound successfully to port {self.CLIENT_PORT}")
            return True
        except OSError as e:
            print(f"FATAL: Could not bind to port {self.CLIENT_PORT}. Error: {e}")
            return False

    def start_threads(self):
        if not self.setup_socket(): return
        self.is_running = True
        threading.Thread(target=self._discovery_and_monitoring_loop, daemon=True).start()
        threading.Thread(target=self._telemetry_requester_loop, daemon=True).start()
        threading.Thread(target=self._udp_receive_loop, daemon=True).start()
        print("UDP Communication threads started.")

    def stop_threads(self):
        self.is_running = False
        if self.sock: self.sock.close()
        print("UDP Communication threads stopped.")

    def send_command_to_device(self, device_name, command_str):
        device = self.device_manager.get_device(device_name)
        if device and device.is_connected and device.ip:
            try:
                if "REQUEST_TELEM" not in command_str:
                    log_queue.append(f"SEND -> {device.ip}:{self.CLEARCORE_PORT}: {command_str}")
                self.sock.sendto(command_str.encode(), (device.ip, self.CLEARCORE_PORT))
                return True, f"Command sent to {device_name}"
            except Exception as e:
                log_queue.append(f"ERROR sending to {device_name}: {e}")
                return False, f"Error sending command: {e}"
        return False, f"Cannot send to {device_name}: Not connected or IP unknown."

    def _discovery_and_monitoring_loop(self):
        now = time.time()
        last_discovery_times = {"injector": now - self.DISCOVERY_INTERVAL, 
                                "fillhead": now - self.DISCOVERY_INTERVAL}
        print("Discovery thread started.", flush=True)
        while self.is_running:
            try:
                now = time.time()
                for name, device in self.device_manager.devices.items():
                    time_since_last = now - last_discovery_times[name]
                    
                    # This debug logic can be noisy, optionally comment out
                    # if device.is_connected:
                    #     print(f"[DEBUG] Device '{name}' marked connected (should NOT broadcast).", flush=True)
                    # else:
                    #     print(f"[DEBUG] Device '{name}' NOT connected, time since last broadcast: {time_since_last:.1f}s", flush=True)

                    if device.is_connected and (now - device.last_seen) > self.TIMEOUT_THRESHOLD:
                        log_queue.append(f"TIMEOUT: {name.capitalize()} disconnected.")
                        device.set_connection_status(False)
                        other_name = "fillhead" if name == "injector" else "injector"
                        self.send_command_to_device(other_name, "CLEAR_PEER_IP")

                    if not device.is_connected and (time_since_last > self.DISCOVERY_INTERVAL):
                        msg = f"DISCOVER_{name.upper()} PORT={self.CLIENT_PORT}"
                        try:
                            # print(f"SENDING BROADCAST -> {self.BROADCAST_ADDR}:{self.CLEARCORE_PORT}: {msg}", flush=True)
                            log_queue.append(f"SEND -> {self.BROADCAST_ADDR}:{self.CLEARCORE_PORT}: {msg}")
                            self.sock.sendto(msg.encode(), (self.BROADCAST_ADDR, self.CLEARCORE_PORT))
                            last_discovery_times[name] = now
                        except Exception as inner_e:
                            # print(f"Inner broadcast error: {inner_e}", flush=True)
                            log_queue.append(f"Discovery broadcast error: {inner_e}")

                time.sleep(1)

            except Exception as e:
                # print(f"Outer discovery loop error: {e}", flush=True)
                log_queue.append(f"Discovery loop error: {e}")

    def _telemetry_requester_loop(self):
        while self.is_running:
            for name, device in self.device_manager.devices.items():
                if device.is_connected:
                    self.send_command_to_device(name, "REQUEST_TELEM")
            time.sleep(self.TELEMETRY_INTERVAL)

    def _udp_receive_loop(self):
        # print("UDP Receive thread started.")
        while self.is_running:
            try:
                data, addr = self.sock.recvfrom(2048)
                msg = data.decode('utf-8', errors='replace').strip()
                source_ip = addr[0]
                
                if "TELEM" not in msg:
                    log_queue.append(f"RECV <- {source_ip}: {msg}")

                if msg == "DISCOVERY: INJECTOR DISCOVERED":
                    self._handle_discovery("injector", source_ip)
                elif msg == "DISCOVERY: FILLHEAD DISCOVERED":
                    self._handle_discovery("fillhead", source_ip)
                elif msg.startswith("INJ_TELEM_GUI:"):
                    self._handle_telemetry("injector", source_ip, msg)
                elif msg.startswith("FH_TELEM_GUI:"):
                    self._handle_telemetry("fillhead", source_ip, msg)

            except socket.timeout:
                continue
            except Exception as e:
                log_queue.append(f"UDP Receive Loop Error: {e}")
                time.sleep(1)

    def _handle_discovery(self, name, ip):
        device = self.device_manager.get_device(name)
        if not device.is_connected or device.ip != ip:
            log_queue.append(f"DISCOVERY: Found {name.capitalize()} at {ip}")
            device.set_connection_status(True, ip)
            self.device_manager.check_for_peer_connection()

    def _handle_telemetry(self, name, ip, msg):
        device = self.device_manager.get_device(name)
        if not device.is_connected:
            device.set_connection_status(True, ip)
        try:
            data = self._parse_injector_telemetry(msg) if name == "injector" else self._parse_fillhead_telemetry(msg)
            device.update_telemetry(data)
            if 'isPeerDiscovered' in data and device.is_peer_connected != data['isPeerDiscovered']:
                device.is_peer_connected = data['isPeerDiscovered']
                log_queue.append(f"INFO: {name.capitalize()} peer status changed to {device.is_peer_connected}")
                self.device_manager.check_for_peer_connection()
        except Exception as e:
            log_queue.append(f"Error parsing {name} telemetry: {e} | MSG: {msg}")

    def _parse_key_value_payload(self, payload_str):
        return dict(item.split(':', 1) for item in payload_str.split(',') if ':' in item)

    def _safe_float(self, s, default=0.0):
        try: return float(s)
        except (ValueError, TypeError): return default

    def _parse_injector_telemetry(self, msg):
        payload = msg.split("INJ_TELEM_GUI:")[1]
        parts = self._parse_key_value_payload(payload)
        return { "mainState": parts.get("MAIN_STATE", "---"), "feedState": parts.get("FEED_STATE", "IDLE"), "errorState": parts.get("ERROR_STATE", "No Error"), "positionMachine": self._safe_float(parts.get("machine_mm")), "positionCartridge": self._safe_float(parts.get("cartridge_mm")), "dispensedVolume": self._safe_float(parts.get("dispensed_ml")), "temperature": self._safe_float(parts.get("temp_c")), "vacuum": self._safe_float(parts.get('vacuum_psig')), "isPeerDiscovered": parts.get("peer_disc", "0") == "1", "peerIp": parts.get("peer_ip", "0.0.0.0"), "motors": [ {"enabled": parts.get("enabled0", "0") == "1", "homed": parts.get("homed0", "0") == "1", "torque": self._safe_float(parts.get("torque0")), "position": self._safe_float(parts.get("pos_mm0"))}, {"enabled": parts.get("enabled1", "0") == "1", "homed": parts.get("homed1", "0") == "1", "torque": self._safe_float(parts.get("torque1")), "position": self._safe_float(parts.get("pos_mm1"))}, {"enabled": parts.get("enabled2", "0") == "1", "homed": parts.get("homed2", "0") == "1", "torque": self._safe_float(parts.get("torque2")), "position": self._safe_float(parts.get("pos_mm2"))} ] }

    def _parse_fillhead_telemetry(self, msg):
        # Note: The original code had a space "FH_TELEM_GUI: ".
        # Ensure this matches the device's actual output.
        # Removing the space for consistency with injector parsing.
        payload = msg.split("FH_TELEM_GUI:")[1].strip()
        parts = self._parse_key_value_payload(payload)
        return { "stateX": parts.get("x_s", "UNKNOWN"), "stateY": parts.get("y_s", "UNKNOWN"), "stateZ": parts.get("z_s", "UNKNOWN"), "isPeerDiscovered": parts.get("pd", "0") == "1", "peerIp": parts.get("pip", "0.0.0.0"), "axes": { "x": {"position": self._safe_float(parts.get("x_p")), "torque": self._safe_float(parts.get("x_t")), "enabled": parts.get("x_e", "0") == "1", "homed": parts.get("x_h", "0") == "1"}, "y": {"position": self._safe_float(parts.get("y_p")), "torque": self._safe_float(parts.get("y_t")), "enabled": parts.get("y_e", "0") == "1", "homed": parts.get("y_h", "0") == "1"}, "z": {"position": self._safe_float(parts.get("z_p")), "torque": self._safe_float(parts.get("z_t")), "enabled": parts.get("z_e", "0") == "1", "homed": parts.get("z_h", "0") == "1"} } }

# =============================================================================
# Flask App Setup and API Endpoints
# =============================================================================

device_manager = DeviceManager()
comms = UdpComms(device_manager)
device_manager.set_comms(comms)

app = Flask(__name__)
CORS(app, resources={r"/api/*": {"origins": "*"}})

@app.route('/api/devices', methods=['GET'])
def get_devices():
    """Returns device status and any new log messages."""
    logs = []
    while True:
        try:
            logs.append(log_queue.popleft())
        except IndexError:
            break
    return jsonify({
        "devices": device_manager.get_all_devices_status(),
        "logs": logs
    })

@app.route('/api/send_command', methods=['POST'])
def handle_send_command():
    data = request.get_json()
    device, command, params = data.get('device'), data.get('command'), data.get('params', {})
    if not all([device, command]): return jsonify({"error": "Device and command must be provided"}), 400
    
    command_map = { "ENABLE": "ENABLE", "DISABLE": "DISABLE", "ENABLE_PINCH": "ENABLE_PINCH", "DISABLE_PINCH": "DISABLE_PINCH", "STOP": "ABORT", "PINCH_HOME": "PINCH_HOME_MOVE", "HOME": f"MACHINE_HOME_MOVE {params.get('stroke', 50)} {params.get('rapidVel', 20)} {params.get('touchVel', 2)} {params.get('accel', 100)} {params.get('retract', 1)} {params.get('torque', 25)}", "MOVE_ABSOLUTE": f"JOG_MOVE {params.get('position', 0)} {params.get('position', 0)} {params.get('velocity', 5)} {params.get('acceleration', 25)} {params.get('torque', 20)}", "INJECT": f"INJECT_MOVE {params.get('volume')} {params.get('speed')} {params.get('accel', 5000)} {params.get('stepsPerMl')} {params.get('torque', 50)}", "PURGE": f"PURGE_MOVE {params.get('volume')} {params.get('speed')} {params.get('accel', 5000)} {params.get('stepsPerMl')} {params.get('torque', 50)}", "FH_ENABLE_X": "ENABLE_X", "FH_DISABLE_X": "DISABLE_X", "FH_ENABLE_Y": "ENABLE_Y", "FH_DISABLE_Y": "DISABLE_Y", "FH_ENABLE_Z": "ENABLE_Z", "FH_DISABLE_Z": "DISABLE_Z", "FH_HOME_X": f"HOME_X {params.get('torque', 20)} {params.get('maxDist', 420)}", "FH_HOME_Y": f"HOME_Y {params.get('torque', 20)} {params.get('maxDist', 420)}", "FH_HOME_Z": f"HOME_Z {params.get('torque', 20)} {params.get('maxDist', 420)}", "FH_MOVE_X": f"MOVE_X {params.get('distance', 10)} {params.get('velocity', 15)} {params.get('acceleration', 50)} {params.get('torque', 20)}", "FH_MOVE_Y": f"MOVE_Y {params.get('distance', 10)} {params.get('velocity', 15)} {params.get('acceleration', 50)} {params.get('torque', 20)}", "FH_MOVE_Z": f"MOVE_Z {params.get('distance', 10)} {params.get('velocity', 15)} {params.get('acceleration', 50)} {params.get('torque', 20)}", "RAW_COMMAND": params.get('raw', '') }
    raw_command = command_map.get(command)
    if not raw_command: return jsonify({"error": f"Unknown command: {command}"}), 400

    success, message = comms.send_command_to_device(device, raw_command)
    if success: return jsonify({"message": message})
    else: return jsonify({"error": message}), 500

@app.route('/api/validate_script', methods=['POST'])
def validate_script():
    return jsonify({"message": "Script validation not yet implemented."})

@app.route('/api/run_script', methods=['POST'])
def run_script():
    return jsonify({"message": "Script execution not yet implemented."})

if __name__ == '__main__':
    comms.start_threads()
    # use_reloader=False is critical when running from a script like this
    # to prevent the script from running twice.
    app.run(debug=True, port=5000, use_reloader=False)

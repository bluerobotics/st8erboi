import tkinter as tk
from tkinter import ttk
import socket
import threading
import time
import random
from collections import deque

# --- Constants ---
CLEARCORE_PORT = 8888
GUI_APP_PORT = 6272
TELEMETRY_INTERVAL = 1.0  # seconds

# Hardcoded IP addresses for the simulated devices
GANTRY_IP = "127.0.0.1"
FILLHEAD_IP = "127.0.0.1"

class DeviceSimulator(threading.Thread):
    def __init__(self, device_name, ip, port, gui_app_port):
        super().__init__()
        self.device_name = device_name
        self.ip = ip
        self.port = port
        self.gui_app_port = gui_app_port
        self.running = False
        self._stop_event = threading.Event()
        self.sock = None
        self.daemon = True
        self.state = self.initialize_state()
        self.command_queue = deque()

    def initialize_state(self):
        """Initializes the state dictionary with more realistic values based on real telemetry."""
        if self.device_name == 'gantry':
            return {
                'gantry_state': 'STANDBY', 'x_st': 'Standby', 'y_st': 'Standby', 'z_st': 'Standby',
                'x_p': 0.0, 'x_t': 0.0, 'x_e': 1, 'x_h': 0,
                'y_p': 0.0, 'y_t': 0.0, 'y_e': 1, 'y_h': 0,
                'z_p': 0.0, 'z_t': 0.0, 'z_e': 1, 'z_h': 0,
            }
        elif self.device_name == 'fillhead':
            return {
                'MAIN_STATE': 'STANDBY',
                'inj_t0': 0.0, 'inj_t1': 0.0, 'inj_h_mach': 0, 'inj_h_cart': 0,
                'inj_mach_mm': 0.0, 'inj_cart_mm': 0.0,
                'inj_cumulative_ml': 0.0, 'inj_active_ml': 0.0, 'inj_tgt_ml': 0.0,
                'enabled0': 1, 'enabled1': 1,
                'inj_valve_pos': 0.0, 'inj_valve_torque': 0.0, 'inj_valve_homed': 0, 'inj_valve_state': 0,
                'vac_valve_pos': 0.0, 'vac_valve_torque': 0.0, 'vac_valve_homed': 0, 'vac_valve_state': 0,
                'h_st': 0, 'h_sp': 70.0, 'h_pv': 25.0, 'h_op': 0.0,
                'vac_st': 0, 'vac_pv': 0.5, 'vac_sp': -14.0,
                'inj_st': 'Standby', 'inj_v_st': 'Not Homed', 'vac_v_st': 'Not Homed',
                'h_st_str': 'Off', 'vac_st_str': 'Off',
            }
        return {}

    def stop(self):
        self._stop_event.set()
        if self.sock:
            self.sock.close()

    def run(self):
        self.running = True
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            # We don't bind to a specific IP, as we'll send responses to the GUI's address
            self.sock.bind(('', self.port))
            self.sock.settimeout(0.1)
            print(f"[{self.device_name}] Simulator started, listening on port {self.port}")
        except OSError as e:
            print(f"[{self.device_name}] Error binding socket on port {self.port}: {e}")
            self.running = False
            return

        last_telemetry_time = 0
        gui_address = None

        while not self._stop_event.is_set():
            try:
                data, addr = self.sock.recvfrom(1024)
                msg = data.decode('utf-8').strip()
                print(f"[{self.device_name}] Received from {addr}: {msg}")
                gui_address = (addr[0], self.gui_app_port)

                if msg.startswith("DISCOVER_DEVICE"):
                    response = f"DISCOVERY_RESPONSE: DEVICE_ID={self.device_name}"
                    self.sock.sendto(response.encode(), gui_address)
                    print(f"[{self.device_name}] Sent discovery response to {gui_address}")
                elif msg.startswith("DISCOVER_"):
                    # This is a discovery message for another device, so ignore it.
                    pass
                else:
                    self.handle_command(msg, gui_address)

            except socket.timeout:
                pass
            except Exception as e:
                # Break the loop if the socket is closed, which causes an error.
                if self._stop_event.is_set():
                    break
                print(f"[{self.device_name}] Error in receive loop: {e}")

            # Process any pending commands/state changes
            if self.command_queue:
                cmd_func, cmd_args = self.command_queue.popleft()
                cmd_func(*cmd_args)
            
            # Simulate dynamic processes like heating
            self.update_state()

            # Check if we should stop before trying to send telemetry
            if self._stop_event.is_set():
                break

            now = time.time()
            if gui_address and (now - last_telemetry_time) > TELEMETRY_INTERVAL:
                telemetry_msg = self.generate_telemetry()
                self.sock.sendto(telemetry_msg.encode(), gui_address)
                # print(f"[{self.device_name}] Sent telemetry to {gui_address}")
                last_telemetry_time = now

        self.running = False
        print(f"[{self.device_name}] Simulator stopped.")

    def handle_command(self, msg, gui_address):
        """A more sophisticated command handler that simulates device states."""
        parts = msg.split()
        command = parts[0]
        args = parts[1:]
        
        # Default response is sent immediately for most commands
        response = f"DONE: {command}"

        try:
            if self.device_name == 'gantry':
                # --- Gantry Commands ---
                if command.startswith("MOVE_"):
                    axis = command.split('_')[1].lower()
                    dist = float(args[0])
                    duration = max(0.5, abs(dist) / 50.0) # Simulate time based on distance
                    self.set_state('gantry_state', 'MOVING', f'Moving {axis.upper()}')
                    self.command_queue.append((self.simulate_move, (axis, self.state[f'{axis}_p'] + dist, duration, gui_address, command)))
                    return # Don't send default DONE response
                
                elif command.startswith("HOME_"):
                    axis = command.split('_')[1].lower()
                    self.set_state('gantry_state', 'HOMING', f'Homing {axis.upper()}')
                    self.command_queue.append((self.simulate_homing, (axis, 2.0, gui_address, command)))
                    return # Don't send default DONE response

            elif self.device_name == 'fillhead':
                # --- Fillhead Commands ---
                if command == "JOG_MOVE":
                    self.state['inj_mach_mm'] += float(args[0])
                    self.state['inj_cart_mm'] += float(args[1])
                elif command == "MACHINE_HOME_MOVE":
                    self.set_state('MAIN_STATE', 'HOMING')
                    self.command_queue.append((self.simulate_fillhead_homing, ('machine', 2.0, gui_address, command)))
                    return
                elif command == "CARTRIDGE_HOME_MOVE":
                    self.set_state('MAIN_STATE', 'HOMING')
                    self.command_queue.append((self.simulate_fillhead_homing, ('cartridge', 2.0, gui_address, command)))
                    return
                elif command in ["INJECTION_VALVE_HOME_UNTUBED", "INJECTION_VALVE_HOME_TUBED"]:
                    self.set_state('MAIN_STATE', 'HOMING')
                    self.command_queue.append((self.simulate_fillhead_homing, ('injection_valve', 1.0, gui_address, command)))
                    return
                elif command in ["VACUUM_VALVE_HOME_UNTUBED", "VACUUM_VALVE_HOME_TUBED"]:
                    self.set_state('MAIN_STATE', 'HOMING')
                    self.command_queue.append((self.simulate_fillhead_homing, ('vacuum_valve', 1.0, gui_address, command)))
                    return
                elif command == "INJECT_MOVE":
                    vol = float(args[0])
                    self.state['inj_tgt_ml'] = vol
                    self.set_state('MAIN_STATE', 'INJECTING')
                    self.command_queue.append((self.simulate_injection, (vol, 2.0, gui_address, command)))
                    return
                elif command == "INJECTION_VALVE_CLOSE":
                    self.state['inj_valve_pos'] = 0.0
                elif command == "INJECTION_VALVE_OPEN":
                    self.state['inj_valve_pos'] = 5.0 # Assume 5mm is open
                elif command == "VACUUM_VALVE_CLOSE":
                    self.state['vac_valve_pos'] = 0.0
                elif command == "VACUUM_VALVE_OPEN":
                    self.state['vac_valve_pos'] = 5.0 # Assume 5mm is open
        
        except (IndexError, ValueError) as e:
            print(f"[{self.device_name}] Error parsing command '{msg}': {e}")
            response = f"ERROR: Invalid arguments for {command}"

        self.sock.sendto(response.encode(), gui_address)
        print(f"[{self.device_name}] Responded to {command} with '{response}'")

    def set_state(self, main_state_key, main_state_val, axis_state_val=None):
        """Helper to set device and axis states."""
        self.state[main_state_key] = main_state_val
        if axis_state_val and self.device_name == 'gantry':
            for axis in ['x', 'y', 'z']:
                self.state[f'{axis}_st'] = axis_state_val

    def simulate_move(self, axis, target_pos, duration, gui_address, command):
        """Simulates the process of a gantry move over a duration."""
        start_time = time.time()
        start_pos = self.state[f'{axis}_p']
        
        while time.time() - start_time < duration:
            elapsed = time.time() - start_time
            progress = elapsed / duration
            self.state[f'{axis}_p'] = start_pos + (target_pos - start_pos) * progress
            time.sleep(0.05) # Update rate
            if self._stop_event.is_set(): return

        self.state[f'{axis}_p'] = target_pos
        self.set_state('gantry_state', 'STANDBY', 'Standby')
        self.sock.sendto(f"DONE: {command}".encode(), gui_address)

    def simulate_homing(self, axis, duration, gui_address, command):
        """Simulates the homing process."""
        time.sleep(duration)
        if self._stop_event.is_set(): return
        self.state[f'{axis}_p'] = 0.0
        self.state[f'{axis}_h'] = 1
        self.set_state('gantry_state', 'STANDBY', 'Standby')
        self.state[f'{axis}_st'] = 'Standby' # Set specific axis to standby
        self.sock.sendto(f"DONE: {command}".encode(), gui_address)

    def simulate_fillhead_homing(self, component, duration, gui_address, command):
        """Simulates the homing process for various fillhead components."""
        time.sleep(duration)
        if self._stop_event.is_set(): return
        
        if component == 'machine':
            self.state['inj_mach_mm'] = 0.0
            self.state['inj_h_mach'] = 1
            self.state['inj_st'] = 'Standby'
        elif component == 'cartridge':
            self.state['inj_cart_mm'] = 0.0
            self.state['inj_h_cart'] = 1
            # Assuming machine homing implies cartridge is also ready to be used
            self.state['inj_st'] = 'Standby'
        elif component == 'injection_valve':
            self.state['inj_valve_pos'] = 0.0
            self.state['inj_valve_homed'] = 1
            self.state['inj_v_st'] = 'Standby'
        elif component == 'vacuum_valve':
            self.state['vac_valve_pos'] = 0.0
            self.state['vac_valve_homed'] = 1
            self.state['vac_v_st'] = 'Standby'

        self.set_state('MAIN_STATE', 'STANDBY')
        self.sock.sendto(f"DONE: {command}".encode(), gui_address)

    def simulate_injection(self, volume, duration, gui_address, command):
        """Simulates the injection process."""
        start_time = time.time()
        start_vol = self.state['inj_active_ml']
        while time.time() - start_time < duration:
            elapsed = time.time() - start_time
            progress = elapsed / duration
            self.state['inj_active_ml'] = start_vol + volume * progress
            time.sleep(0.05)
            if self._stop_event.is_set(): return

        self.state['inj_active_ml'] = 0
        self.state['inj_cumulative_ml'] += volume
        self.state['inj_tgt_ml'] = 0
        self.set_state('MAIN_STATE', 'STANDBY')
        self.sock.sendto(f"DONE: {command}".encode(), gui_address)


    def update_state(self):
        """Periodically update dynamic state values like temperature."""
        if self.device_name == 'fillhead':
            # Simulate heater PID loop
            if self.state['h_st'] == 1:
                error = self.state['h_sp'] - self.state['h_pv']
                self.state['h_op'] = min(100, max(0, error * 10)) # Simple proportional control
                self.state['h_pv'] += self.state['h_op'] * 0.01 - 0.05 # Heat up and cool down
            else:
                self.state['h_op'] = 0
                if self.state['h_pv'] > 25:
                    self.state['h_pv'] -= 0.1 # Cool down to ambient

    def generate_telemetry(self):
        if self.device_name == 'gantry':
            s = self.state
            # Slightly randomize torque for realism
            s['x_t'] = random.uniform(5, 15) if s['gantry_state'] != 'STANDBY' else 0.0
            s['y_t'] = random.uniform(5, 15) if s['gantry_state'] != 'STANDBY' else 0.0
            s['z_t'] = random.uniform(5, 15) if s['gantry_state'] != 'STANDBY' else 0.0

            return (f"GANTRY_TELEM: gantry_state:{s['gantry_state']},x_p:{s['x_p']:.2f},x_t:{s['x_t']:.2f},x_e:{s['x_e']},x_h:{s['x_h']},x_st:{s['x_st']},"
                    f"y_p:{s['y_p']:.2f},y_t:{s['y_t']:.2f},y_e:{s['y_e']},y_h:{s['y_h']},y_st:{s['y_st']},"
                    f"z_p:{s['z_p']:.2f},z_t:{s['z_t']:.2f},z_e:{s['z_e']},z_h:{s['z_h']},z_st:{s['z_st']}")
        elif self.device_name == 'fillhead':
            s = self.state
            # Simulate some dynamic values
            s['inj_t0'] = random.uniform(5, 15) if s['MAIN_STATE'] != 'STANDBY' else 0.0
            s['inj_t1'] = random.uniform(5, 15) if s['MAIN_STATE'] != 'STANDBY' else 0.0
            
            return (f"FILLHEAD_TELEM: MAIN_STATE:{s['MAIN_STATE']},"
                    f"inj_t0:{s['inj_t0']:.1f},inj_t1:{s['inj_t1']:.1f},inj_h_mach:{s['inj_h_mach']},inj_h_cart:{s['inj_h_cart']},"
                    f"inj_mach_mm:{s['inj_mach_mm']:.2f},inj_cart_mm:{s['inj_cart_mm']:.2f},"
                    f"inj_cumulative_ml:{s['inj_cumulative_ml']:.2f},inj_active_ml:{s['inj_active_ml']:.2f},inj_tgt_ml:{s['inj_tgt_ml']:.2f},"
                    f"enabled0:{s['enabled0']},enabled1:{s['enabled1']},"
                    f"inj_valve_pos:{s['inj_valve_pos']:.2f},inj_valve_torque:{s['inj_valve_torque']:.1f},inj_valve_homed:{s['inj_valve_homed']},inj_valve_state:{s['inj_valve_state']},"
                    f"vac_valve_pos:{s['vac_valve_pos']:.2f},vac_valve_torque:{s['vac_valve_torque']:.1f},vac_valve_homed:{s['vac_valve_homed']},vac_valve_state:{s['vac_valve_state']},"
                    f"h_st:{s['h_st']},h_sp:{s['h_sp']:.1f},h_pv:{s['h_pv']:.1f},h_op:{s['h_op']:.1f},"
                    f"vac_st:{s['vac_st']},vac_pv:{s['vac_pv']:.2f},vac_sp:{s['vac_sp']:.1f},"
                    f"inj_st:{s['inj_st']},inj_v_st:{s['inj_v_st']},vac_v_st:{s['vac_v_st']},h_st_str:{s['h_st_str']},vac_st_str:{s['vac_st_str']}")
        return ""


class SimulatorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("St8erBoi Device Simulator")
        self.simulators = {}

        self.gantry_var = tk.BooleanVar()
        self.fillhead_var = tk.BooleanVar()

        frame = ttk.Frame(self.root, padding="10")
        frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        ttk.Checkbutton(frame, text="Simulate Gantry", variable=self.gantry_var, command=self.toggle_gantry).grid(row=0, column=0, sticky=tk.W, pady=2)
        ttk.Label(frame, text=f"IP: {GANTRY_IP} Port: {CLEARCORE_PORT}").grid(row=0, column=1, sticky=tk.W, padx=5)

        ttk.Checkbutton(frame, text="Simulate Fillhead", variable=self.fillhead_var, command=self.toggle_fillhead).grid(row=1, column=0, sticky=tk.W, pady=2)
        ttk.Label(frame, text=f"IP: {FILLHEAD_IP} Port: {CLEARCORE_PORT}").grid(row=1, column=1, sticky=tk.W, padx=5) # Both listen on the same port

        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)

    def toggle_gantry(self):
        self.toggle_simulator('gantry', self.gantry_var.get(), GANTRY_IP, CLEARCORE_PORT)

    def toggle_fillhead(self):
        # Both simulators must listen on the same port for broadcast discovery.
        self.toggle_simulator('fillhead', self.fillhead_var.get(), FILLHEAD_IP, CLEARCORE_PORT)

    def toggle_simulator(self, name, start, ip, port):
        if start:
            if name not in self.simulators or not self.simulators[name].running:
                print(f"Starting {name} simulator...")
                sim = DeviceSimulator(name, ip, port, GUI_APP_PORT)
                self.simulators[name] = sim
                sim.start()
        else:
            if name in self.simulators and self.simulators[name].running:
                print(f"Stopping {name} simulator...")
                self.simulators[name].stop()
                self.simulators[name].join() # Wait for thread to finish

    def on_closing(self):
        print("Closing simulator...")
        for name in self.simulators:
            if self.simulators[name].running:
                self.simulators[name].stop()
                self.simulators[name].join()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = SimulatorApp(root)
    root.mainloop()

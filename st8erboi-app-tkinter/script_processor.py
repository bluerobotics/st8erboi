import time
import threading
import queue
import tkinter as tk  # For TclError and StringVar
from script_validator import COMMANDS


class ScriptRunner:
    def __init__(self, script_content, command_funcs, gui_refs, status_callback, completion_callback, message_queue,
                 line_offset=0):
        self.script_lines = script_content.splitlines()
        self.command_funcs = command_funcs
        self.gui_refs = gui_refs
        self.status_callback = status_callback
        self.completion_callback = completion_callback
        self.message_queue = message_queue
        self.line_offset = line_offset
        self.is_running = False
        self.thread = None
        self.runtime_defaults = {
            "VACUUM_CHECK_DROP": 0.1,
            "VACUUM_CHECK_DURATION": 10
        }

    def start(self):
        if self.is_running: return
        self.is_running = True
        while not self.message_queue.empty():
            try:
                self.message_queue.get_nowait()
            except queue.Empty:
                continue
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def stop(self):
        self.is_running = False

    def _get_default(self, command_name, param_index):
        param_def = COMMANDS[command_name]['params'][param_index]
        param_name = param_def['name']

        if command_name.startswith("MOVE"):
            if "Speed" in param_name: return self.runtime_defaults.get("MOVE_VEL") or param_def.get("default")
            if "Accel" in param_name: return self.runtime_defaults.get("MOVE_ACC") or param_def.get("default")
            if "Torque" in param_name: return self.runtime_defaults.get("MOVE_TORQUE") or param_def.get("default")

        if command_name == "WAIT_UNTIL_VACUUM":
            if "Target-PSI" in param_name: return self.runtime_defaults.get("VACUUM_TARGET") or param_def.get("default")
            if "Timeout" in param_name: return self.runtime_defaults.get("VACUUM_TIMEOUT") or param_def.get("default")

        if command_name == "WAIT_UNTIL_HEATER_AT_TEMP":
            if "Target-Temp" in param_name: return self.runtime_defaults.get("HEATER_TARGET") or param_def.get(
                "default")
            if "Timeout" in param_name: return self.runtime_defaults.get("HEATER_TIMEOUT") or param_def.get("default")

        return param_def.get("default")

    def _handle_wait(self, args, line_num, is_seconds=False):
        if not args:
            self.status_callback(f"Error on L{line_num}: WAIT command requires a duration.", line_num)
            self.is_running = False
            return False
        try:
            duration = float(args[0])
            duration_ms = duration * 1000 if is_seconds else duration
            unit = "s" if is_seconds else "ms"
            self.status_callback(f"L{line_num}: Waiting for {duration} {unit}...", line_num)

            start_time = time.time()
            while (time.time() - start_time) * 1000 < duration_ms:
                if not self.is_running: return False
                time.sleep(0.05)
            return True
        except ValueError:
            self.status_callback(f"Error on L{line_num}: Invalid duration for WAIT command.", line_num)
            self.is_running = False
            return False

    def _handle_wait_until_vacuum(self, args, line_num):
        try:
            target_psi = float(args[0]) if len(args) > 0 else float(self.runtime_defaults.get("VACUUM_TARGET", -14.0))
            timeout_s = float(args[1]) if len(args) > 1 else float(self.runtime_defaults.get("VACUUM_TIMEOUT", 60))
        except (ValueError, IndexError):
            self.status_callback(f"Error on L{line_num}: Invalid parameters for WAIT_UNTIL_VACUUM.", line_num)
            self.is_running = False
            return False

        start_time = time.time()
        while True:
            if not self.is_running: return False
            if time.time() - start_time > timeout_s:
                self.status_callback(f"Error on L{line_num}: Timeout waiting for vacuum target.", line_num)
                self.is_running = False
                return False

            try:
                psig_str = self.gui_refs['vacuum_psig_var'].get().split()[0]
                current_psi = float(psig_str)
                self.status_callback(f"L{line_num}: Waiting for vacuum <= {target_psi:.2f}, current: {current_psi:.2f}",
                                     line_num)
                if current_psi <= target_psi:
                    self.status_callback(f"L{line_num}: Vacuum target reached ({current_psi:.2f} PSIG).", line_num)
                    return True
            except (ValueError, IndexError, tk.TclError):
                self.status_callback(f"L{line_num}: Waiting for vacuum... (current value invalid)", line_num)

            time.sleep(0.2)

    def _handle_wait_until_heater_at_temp(self, args, line_num):
        try:
            target_temp_str = args[0] if len(args) > 0 else self.gui_refs['pid_setpoint_var'].get()
            target_temp = float(target_temp_str)
            timeout_s = float(args[1]) if len(args) > 1 else float(self.runtime_defaults.get("HEATER_TIMEOUT", 100))
        except (ValueError, IndexError, tk.TclError):
            self.status_callback(f"Error on L{line_num}: Invalid parameters for WAIT_UNTIL_HEATER_AT_TEMP.", line_num)
            self.is_running = False
            return False

        start_time = time.time()
        while True:
            if not self.is_running: return False
            if time.time() - start_time > timeout_s:
                self.status_callback(f"Error on L{line_num}: Timeout waiting for heater target.", line_num)
                self.is_running = False
                return False

            try:
                temp_str = self.gui_refs['temp_c_var'].get().split()[0]
                current_temp = float(temp_str)
                self.status_callback(f"L{line_num}: Waiting for temp ~{target_temp:.1f}C, current: {current_temp:.1f}C",
                                     line_num)
                if abs(current_temp - target_temp) <= 1.0:
                    self.status_callback(f"L{line_num}: Heater target reached ({current_temp:.1f} C).", line_num)
                    return True
            except (ValueError, IndexError, tk.TclError):
                self.status_callback(f"L{line_num}: Waiting for temp... (current value invalid)", line_num)

            time.sleep(0.2)

    def _handle_vacuum_check(self, args, line_num):
        max_increase = float(self.runtime_defaults["VACUUM_CHECK_DROP"])
        duration_s = float(self.runtime_defaults["VACUUM_CHECK_DURATION"])

        if 'vacuum_check_status_var' in self.gui_refs:
            self.gui_refs['vacuum_check_status_var'].set("N/A")

        time.sleep(0.2)

        try:
            initial_psig_str = self.gui_refs['vacuum_psig_var'].get().split()[0]
            initial_psi = float(initial_psig_str)
        except (ValueError, IndexError, tk.TclError):
            self.status_callback(f"Error on L{line_num}: Could not read initial vacuum pressure.", line_num)
            if 'vacuum_check_status_var' in self.gui_refs: self.gui_refs['vacuum_check_status_var'].set("FAIL")
            self.is_running = False
            return False

        self.status_callback(f"L{line_num}: Checking vacuum for {duration_s}s (max increase: {max_increase} PSI)...",
                             line_num)
        start_time = time.time()

        while time.time() - start_time < duration_s:
            if not self.is_running: return False
            time.sleep(0.2)

        try:
            final_psig_str = self.gui_refs['vacuum_psig_var'].get().split()[0]
            final_psi = float(final_psig_str)

            if final_psi > initial_psi + max_increase:
                self.status_callback(
                    f"Error on L{line_num}: Vacuum leak detected! ({final_psi:.2f} > {initial_psi + max_increase:.2f})",
                    line_num)
                if 'vacuum_check_status_var' in self.gui_refs: self.gui_refs['vacuum_check_status_var'].set("FAIL")
                self.is_running = False
                return False
        except (ValueError, IndexError, tk.TclError):
            self.status_callback(f"Error on L{line_num}: Could not read final vacuum pressure.", line_num)
            if 'vacuum_check_status_var' in self.gui_refs: self.gui_refs['vacuum_check_status_var'].set("FAIL")
            self.is_running = False
            return False

        self.status_callback(f"L{line_num}: Vacuum check passed.", line_num)
        if 'vacuum_check_status_var' in self.gui_refs:
            self.gui_refs['vacuum_check_status_var'].set("PASS")
        return True

    def _run(self):
        if 'vacuum_check_status_var' in self.gui_refs:
            self.gui_refs['vacuum_check_status_var'].set("N/A")

        for i, line in enumerate(self.script_lines):
            line_num = i + 1 + self.line_offset
            if not self.is_running:
                self.status_callback("Stopped", -1)
                break

            line = line.strip()
            if not line or line.startswith('#'):
                if i == len(self.script_lines) - 1: self.status_callback("Idle", -1)
                continue

            self.status_callback(f"L{line_num}: Processing '{line}'", line_num)

            sub_commands = line.split(',')
            commands_to_wait_for = []

            for sub_cmd_str in sub_commands:
                if not self.is_running: break
                sub_cmd_str = sub_cmd_str.strip()
                if not sub_cmd_str: continue

                parts = sub_cmd_str.split()
                command_word = parts[0].upper()
                args = parts[1:]
                command_info = COMMANDS.get(command_word)

                if not command_info:
                    self.status_callback(f"Error on L{line_num}: Unknown command '{command_word}'.", line_num)
                    self.is_running = False
                    break

                device = command_info['device']
                if device == "script":
                    if command_word == "WAIT_MS":
                        if not self._handle_wait(args, line_num, is_seconds=False): break
                    elif command_word == "WAIT_S":
                        if not self._handle_wait(args, line_num, is_seconds=True): break
                    elif command_word == "WAIT_UNTIL_VACUUM":
                        if not self._handle_wait_until_vacuum(args, line_num): break
                    elif command_word == "WAIT_UNTIL_HEATER_AT_TEMP":
                        if not self._handle_wait_until_heater_at_temp(args, line_num): break
                    elif command_word == "VACUUM_CHECK":
                        if not self._handle_vacuum_check(args, line_num): break
                    elif command_word == "SET_VACUUM_CHECK_PASS_PRESS_DROP":
                        if len(args) == 1: self.runtime_defaults["VACUUM_CHECK_DROP"] = args[0]
                    elif command_word == "SET_VACUUM_CHECK_DURATION":
                        if len(args) == 1: self.runtime_defaults["VACUUM_CHECK_DURATION"] = args[0]
                    elif command_word.startswith("SET_DEFAULT_"):
                        key = command_word.replace("SET_DEFAULT_", "")
                        if len(args) == 1: self.runtime_defaults[key] = args[0]
                else:
                    params_def = command_info['params']
                    full_args = list(args)

                    # CORRECTED LOGIC: Properly handle optional parameters without defaults.
                    if len(args) < len(params_def):
                        for j in range(len(args), len(params_def)):
                            param_def = params_def[j]
                            default_val = self._get_default(command_word, j)

                            if default_val is not None:
                                full_args.append(str(default_val))
                            # Only raise an error if the parameter is NOT optional.
                            # If it's optional and has no default, we send nothing for it.
                            elif not param_def.get("optional"):
                                self.status_callback(f"Error: Missing required parameter for {command_word}", line_num)
                                self.is_running = False
                                break

                    if not self.is_running: continue

                    if command_word == "HOME_X":
                        self.gui_refs['fh_homed_m0_var'].set("Not Homed")
                    elif command_word == "HOME_Y":
                        self.gui_refs['fh_homed_m1_var'].set("Not Homed")
                        self.gui_refs['fh_homed_m2_var'].set("Not Homed")
                    elif command_word == "HOME_Z":
                        self.gui_refs['fh_homed_m3_var'].set("Not Homed")
                    elif command_word == "PINCH_HOME_MOVE":
                        self.gui_refs['pinch_homed_var'].set("Not Homed")

                    final_command_str = f"{command_word} {' '.join(full_args)}" if full_args else command_word
                    send_func = self.command_funcs.get(f"send_{device}")
                    if send_func:
                        send_func(final_command_str)
                        if "MOVE" in command_word or "HOME" in command_word:
                            commands_to_wait_for.append(command_word)
                time.sleep(0.05)


            if not self.is_running: break
            if commands_to_wait_for:
                if not self._wait_for_done_messages(commands_to_wait_for, line_num):
                    self.is_running = False

            if not self.is_running: break

        self.is_running = False
        if self.completion_callback: self.completion_callback()

    def _wait_for_done_messages(self, commands, line_num, timeout_s=600):
        start_time = time.time()

        wait_list = list(commands)
        self.status_callback(f"L{line_num}: Waiting for DONE: {', '.join(wait_list)}", line_num)

        while wait_list:
            if not self.is_running:
                self.status_callback("Stopped", -1)
                return False

            if time.time() - start_time > timeout_s:
                self.status_callback(f"Error on L{line_num}: Timeout waiting for DONE: {', '.join(wait_list)}",
                                     line_num)
                return False

            try:
                msg = self.message_queue.get(timeout=0.1)

                for i in range(len(wait_list) - 1, -1, -1):
                    command_to_check = wait_list[i]
                    if command_to_check in msg:
                        wait_list.pop(i)
                        self.status_callback(f"L{line_num}: Received DONE for {command_to_check}", line_num)
                        break

            except queue.Empty:
                continue

        self.status_callback(f"L{line_num}: All motions complete.", line_num)
        return True
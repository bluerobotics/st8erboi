import time
import threading
import queue
import tkinter as tk  # For TclError and StringVar
from script_validator import COMMANDS


class ScriptRunner:
    def __init__(self, script_content, command_funcs, gui_refs, status_callback, completion_callback, message_queue,
                 line_offset=0):
        
        self.command_funcs = command_funcs
        self.gui_refs = gui_refs
        self.status_callback = status_callback
        self.completion_callback = completion_callback
        self.message_queue = message_queue
        self.line_offset = line_offset # This is the original line number of the first line of script_content
        self.is_running = False
        self.thread = None
        self.runtime_defaults = {}

        # --- New: Pre-process the script to expand loops ---
        try:
            expanded_content, self.line_map = self._expand_loops(script_content, line_offset)
            self.script_lines = expanded_content
        except Exception as e:
            self.script_lines = [f"Error processing script: {e}"]
            self.line_map = [1 + line_offset]

    def _expand_loops(self, content, start_offset):
        original_lines = content.splitlines()
        
        # Create a list of tuples: (line_content, original_line_number)
        lines_with_nums = [(line, i + 1 + start_offset) for i, line in enumerate(original_lines)]
        
        def process_block(block_with_nums):
            expanded_block = []
            i = 0
            while i < len(block_with_nums):
                line, line_num = block_with_nums[i]
                parts = line.strip().split()
                command = parts[0].upper() if parts else ""

                if command == "REPEAT":
                    count = int(parts[1])
                    nesting_level = 1
                    end_index = -1
                    
                    # Find the matching END_REPEAT
                    for j in range(i + 1, len(block_with_nums)):
                        sub_line, _ = block_with_nums[j]
                        sub_parts = sub_line.strip().split()
                        sub_command = sub_parts[0].upper() if sub_parts else ""
                        if sub_command == "REPEAT":
                            nesting_level += 1
                        elif sub_command == "END_REPEAT":
                            nesting_level -= 1
                            if nesting_level == 0:
                                end_index = j
                                break
                    
                    loop_body_with_nums = block_with_nums[i + 1 : end_index]
                    processed_body = process_block(loop_body_with_nums)
                    
                    for _ in range(count):
                        expanded_block.extend(processed_body)
                    
                    i = end_index # Move the pointer past the processed block
                
                else:
                    expanded_block.append((line, line_num))
                i += 1
            return expanded_block

        processed_lines_with_nums = process_block(lines_with_nums)
        
        final_lines = [line for line, num in processed_lines_with_nums]
        final_map = [num for line, num in processed_lines_with_nums]
        
        return final_lines, final_map

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
            end_time = start_time + (duration_ms / 1000.0)
            while time.time() < end_time:
                if not self.is_running: return False
                remaining_time = end_time - time.time()
                self.status_callback(f"L{line_num}: Waiting... {remaining_time:.1f}s remaining", line_num)
                time.sleep(0.1) # Update GUI 10 times per second
            return True
        except ValueError:
            self.status_callback(f"Error on L{line_num}: Invalid duration for WAIT command.", line_num)
            self.is_running = False
            return False

    def _handle_wait_until_vacuum(self, args, line_num):
        try:
            # New logic: Use the GUI's target var as the source of truth
            target_psi_str = args[0] if len(args) > 0 else self.gui_refs['vac_target_var'].get()
            target_psi = float(target_psi_str)
            timeout_s = float(args[1]) if len(args) > 1 else float(self.runtime_defaults.get("VACUUM_TIMEOUT", 60))
        except (ValueError, IndexError, tk.TclError):
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
            # Reverted: Now correctly uses the 'pid_setpoint_var' which is bound to the UI.
            target_temp_str = args[0] if len(args) > 0 else self.gui_refs['pid_setpoint_var'].get()
            target_temp = float(target_temp_str)
            timeout_s = float(args[1]) if len(args) > 1 else float(self.runtime_defaults.get("HEATER_TIMEOUT", 100))
        except (ValueError, IndexError, tk.TclError):
            self.status_callback(f"Error on L{line_num}: Invalid parameters for WAIT_UNTIL_HEATER_AT_TEMP.", line_num)
            self.is_running = False
            return False

        start_time = time.time()
        # Calculate the 5% tolerance band
        tolerance = target_temp * 0.05
        lower_bound = target_temp - tolerance
        upper_bound = target_temp + tolerance

        while True:
            if not self.is_running: return False
            if time.time() - start_time > timeout_s:
                self.status_callback(f"Error on L{line_num}: Timeout waiting for heater target.", line_num)
                self.is_running = False
                return False

            try:
                temp_str = self.gui_refs['temp_c_var'].get().split()[0]
                current_temp = float(temp_str)
                self.status_callback(f"L{line_num}: Waiting for temp in range [{lower_bound:.1f}..{upper_bound:.1f}]C, current: {current_temp:.1f}C",
                                     line_num)
                if lower_bound <= current_temp <= upper_bound:
                    self.status_callback(f"L{line_num}: Heater target reached ({current_temp:.1f} C).", line_num)
                    return True
            except (ValueError, IndexError, tk.TclError):
                self.status_callback(f"L{line_num}: Waiting for temp... (current value invalid)", line_num)

            time.sleep(0.2)

    def _run(self):
        for i, line in enumerate(self.script_lines):
            # MODIFIED: Use the line map to get the correct original line number
            line_num = self.line_map[i]
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
                
                # --- Special Handling for Script-Aware Commands ---
                # Update the GUI's own variables to keep them in sync with the script
                if command_word == "SET_HEATER_SETPOINT" and len(args) > 0:
                    # CORRECTED: Uses the real command and the correct GUI variable.
                    self.gui_refs['pid_setpoint_var'].set(args[0])
                if command_word == "SET_VACUUM_TARGET" and len(args) > 0:
                    self.gui_refs['vac_target_var'].set(args[0])

                if device == "script":
                    if command_word == "WAIT_MS":
                        if not self._handle_wait(args, line_num, is_seconds=False): break
                    elif command_word == "WAIT_S":
                        if not self._handle_wait(args, line_num, is_seconds=True): break
                    elif command_word == "WAIT_UNTIL_VACUUM":
                        if not self._handle_wait_until_vacuum(args, line_num): break
                    elif command_word == "WAIT_UNTIL_HEATER_AT_TEMP":
                        if not self._handle_wait_until_heater_at_temp(args, line_num): break
                    elif command_word.startswith("SET_DEFAULT_"):
                        key = command_word.replace("SET_DEFAULT_", "")
                        if len(args) == 1: self.runtime_defaults[key] = args[0]
                else:
                    params_def = command_info['params']
                    full_args = list(args)

                    if len(args) < len(params_def):
                        for j in range(len(args), len(params_def)):
                            param_def = params_def[j]
                            default_val = self._get_default(command_word, j)

                            if default_val is not None:
                                full_args.append(str(default_val))
                            elif not param_def.get("optional"):
                                self.status_callback(f"Error on L{line_num}: Missing required parameter '{param_def['name']}' for {command_word}.", line_num)
                                self.is_running = False
                                break

                    if not self.is_running: continue

                    final_command_str = f"{command_word} {' '.join(full_args)}" if full_args else command_word
                    send_func = self.command_funcs.get(f"send_{device}")
                    if send_func:
                        send_func(final_command_str)
                        # Add any command that elicits a "_DONE" response to this list.
                        if "MOVE" in command_word or \
                           "HOME" in command_word or \
                           "OPEN" in command_word or \
                           "CLOSE" in command_word or \
                           "VACUUM_LEAK_TEST" in command_word or \
                           "INJECT_STATOR" in command_word or \
                           "INJECT_ROTOR" in command_word:
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

                # Check for failure messages first
                if "FAILED" in msg:
                    for command_to_check in wait_list:
                        is_failure = False
                        if command_to_check == "VACUUM_LEAK_TEST" and "LEAK_TEST" in msg:
                            is_failure = True
                        elif command_to_check in msg:
                            is_failure = True

                        if is_failure:
                            self.status_callback(
                                f"Error on L{line_num}: Received FAILURE for {command_to_check}. Message: {msg}",
                                line_num)
                            return False

                # Check for success messages
                for i in range(len(wait_list) - 1, -1, -1):
                    command_to_check = wait_list[i]
                    is_complete = False

                    # --- FINAL FIX ---
                    # Added specific checks for pinch valve homing commands, as their "DONE"
                    # messages don't contain the original command string.
                    if command_to_check == "VACUUM_LEAK_TEST" and "LEAK_TEST" in msg and "PASSED" in msg:
                        is_complete = True
                    elif (command_to_check == "INJECTION_VALVE_HOME_UNTUBED" or command_to_check == "INJECTION_VALVE_HOME_TUBED") and "inj_valve" in msg and "DONE" in msg:
                        is_complete = True
                    elif (command_to_check == "VACUUM_VALVE_HOME_UNTUBED" or command_to_check == "VACUUM_VALVE_HOME_TUBED") and "vac_valve" in msg and "DONE" in msg:
                        is_complete = True
                    # --- FIX for Valve Open/Close ---
                    # These also have unique DONE messages that don't contain the command name.
                    elif (command_to_check == "INJECTION_VALVE_OPEN" or command_to_check == "INJECTION_VALVE_CLOSE") and "inj_valve" in msg and "DONE" in msg:
                        is_complete = True
                    elif (command_to_check == "VACUUM_VALVE_OPEN" or command_to_check == "VACUUM_VALVE_CLOSE") and "vac_valve" in msg and "DONE" in msg:
                        is_complete = True
                    # Generic fallback for other commands like MOVE_X, HOME_Y, etc.
                    elif command_to_check in msg and "DONE" in msg:
                        is_complete = True
                    # -----------------

                    if is_complete:
                        wait_list.pop(i)
                        self.status_callback(f"L{line_num}: Received DONE for {command_to_check}", line_num)
                        break

            except queue.Empty:
                continue

        self.status_callback(f"L{line_num}: All operations complete.", line_num)
        return True
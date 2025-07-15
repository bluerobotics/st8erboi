import tkinter as tk
from tkinter import ttk, filedialog, scrolledtext, messagebox
import time
import threading
from script_validator import COMMANDS, validate_script


# Custom Text Widget with Line Numbers
class TextLineNumbers(tk.Canvas):
    def __init__(self, *args, **kwargs):
        tk.Canvas.__init__(self, *args, **kwargs)
        self.textwidget = None

    def attach(self, text_widget):
        self.textwidget = text_widget
        self.redraw()

    def redraw(self, *args):
        self.delete("all")
        i = self.textwidget.index("@0,0")
        while True:
            dline = self.textwidget.dlineinfo(i)
            if dline is None: break
            y = dline[1]
            linenum = str(i).split(".")[0]
            self.create_text(2, y, anchor="nw", text=linenum, fill="#6c757d", font=("Consolas", 10))
            i = self.textwidget.index(f"{i}+1line")


class CustomText(tk.Text):
    def __init__(self, *args, **kwargs):
        tk.Text.__init__(self, *args, **kwargs)
        self._orig = self._w + "_orig"
        self.tk.call("rename", self._w, self._orig)
        self.tk.createcommand(self._w, self._proxy)

    def _proxy(self, *args):
        cmd = (self._orig,) + args
        try:
            result = self.tk.call(cmd)
        except tk.TclError:
            return None

        if (args[0] in ("insert", "delete", "replace") or
                args[0:3] == ("mark", "set", "insert") or
                args[0:2] == ("xview", "moveto") or
                args[0:2] == ("xview", "scroll") or
                args[0:2] == ("yview", "moveto") or
                args[0:2] == ("yview", "scroll")
        ):
            self.event_generate("<<Change>>", when="tail")
        return result


class TextWithLineNumbers(tk.Frame):
    def __init__(self, *args, **kwargs):
        tk.Frame.__init__(self, *args, **kwargs)
        self.text = CustomText(self, wrap=tk.WORD, bg="#1b1e2b", fg="white", insertbackground="white",
                               font=("Consolas", 10), undo=True, selectbackground="#0078d7", selectforeground="white")
        self.linenumbers = TextLineNumbers(self, width=30, bg="#2a2d3b", highlightthickness=0)
        self.linenumbers.attach(self.text)

        self.vsb = tk.Scrollbar(self, orient="vertical", command=self.text.yview)
        self.text.configure(yscrollcommand=self.vsb.set)

        self.linenumbers.pack(side="left", fill="y")
        self.vsb.pack(side="right", fill="y")
        self.text.pack(side="right", fill="both", expand=True)

        self.text.bind("<<Change>>", self._on_change)
        self.text.bind("<Configure>", self._on_change)

    def _on_change(self, event):
        self.linenumbers.redraw()

    def get(self, *args, **kwargs): return self.text.get(*args, **kwargs)

    def insert(self, *args, **kwargs): return self.text.insert(*args, **kwargs)

    def delete(self, *args, **kwargs): return self.text.delete(*args, **kwargs)

    def tag_config(self, *args, **kwargs): return self.text.tag_config(*args, **kwargs)

    def tag_add(self, *args, **kwargs): return self.text.tag_add(*args, **kwargs)

    def tag_remove(self, *args, **kwargs): return self.text.tag_remove(*args, **kwargs)

    def index(self, *args, **kwargs): return self.text.index(*args, **kwargs)

    @property
    def edit_reset(self): return self.text.edit_reset

    def bind(self, *args, **kwargs): self.text.bind(*args, **kwargs)


class ScriptRunner:
    def __init__(self, script_content, command_funcs, gui_refs, status_callback, completion_callback, line_offset=0):
        self.script_lines = script_content.splitlines()
        self.command_funcs = command_funcs
        self.gui_refs = gui_refs
        self.status_callback = status_callback
        self.completion_callback = completion_callback
        self.line_offset = line_offset
        self.is_running = False
        self.thread = None
        self.default_move_params = {'vel': '50', 'acc': '200', 'torque': '25'}
        self.default_vacuum_target = -14.0
        self.default_vacuum_timeout = 60.0
        self.default_heater_target = 70.0
        self.default_heater_timeout = 120.0

    def start(self):
        if self.is_running: return
        self.is_running = True
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def stop(self):
        self.is_running = False

    def _is_fillhead_busy(self):
        try:
            x_state = self.gui_refs['fh_state_x_var'].get()
            y_state = self.gui_refs['fh_state_y_var'].get()
            z_state = self.gui_refs['fh_state_z_var'].get()
            return "Moving" in (x_state, y_state, z_state) or "Homing" in (x_state, y_state, z_state)
        except (KeyError, tk.TclError):
            return False

    def _is_injector_busy(self):
        try:
            for i in range(1, 4):
                motor_state_var = self.gui_refs.get(f'motor_state{i}')
                if motor_state_var and "Busy" in motor_state_var.get():
                    return True
        except (KeyError, tk.TclError, AttributeError):
            return False
        return False

    def _wait_for_idle(self, devices_to_check, line_num, timeout_s=600):
        self.status_callback(f"Waiting for move on {', '.join(devices_to_check)}...", line_num)
        start_time = time.time()
        while time.time() - start_time < timeout_s:
            if not self.is_running: return False
            all_idle = not any(
                (device == 'FILLHEAD' and self._is_fillhead_busy()) or
                (device == 'INJECTOR' and self._is_injector_busy())
                for device in devices_to_check
            )
            if all_idle:
                self.status_callback("Move(s) complete.", line_num)
                return True
            time.sleep(0.1)
        self.status_callback("Error: Timed out waiting for move to complete.", line_num)
        return False

    def _wait_for_vacuum(self, target_psi, timeout_sec, line_num):
        start_time = time.time()
        self.status_callback(f"Waiting for vacuum to reach {target_psi} PSIG...", line_num)
        while time.time() - start_time < timeout_sec:
            if not self.is_running: return False
            try:
                vac_str = self.gui_refs.get('vacuum_psig_var', tk.StringVar(value='0.0 PSIG')).get()
                current_psi = float(vac_str.split()[0])
                if current_psi <= target_psi:
                    self.status_callback(f"Vacuum target of {target_psi} PSIG reached.", line_num)
                    return True
            except (ValueError, IndexError, tk.TclError):
                pass
            time.sleep(0.2)
        self.status_callback(f"Error: Timed out waiting for vacuum target {target_psi} PSIG.", line_num)
        return False

    def _wait_for_heater(self, target_temp, timeout_sec, line_num):
        start_time = time.time()
        self.status_callback(f"Waiting for heater to reach {target_temp}°C...", line_num)
        tolerance = 1.0
        while time.time() - start_time < timeout_sec:
            if not self.is_running: return False
            try:
                temp_str = self.gui_refs.get('temp_c_var', tk.StringVar(value='0.0 °C')).get()
                current_temp = float(temp_str.split()[0])
                if abs(current_temp - target_temp) <= tolerance:
                    self.status_callback(f"Heater target of {target_temp}°C reached.", line_num)
                    return True
            except (ValueError, IndexError, tk.TclError):
                pass
            time.sleep(0.5)
        self.status_callback(f"Error: Timed out waiting for heater target {target_temp}°C.", line_num)
        return False

    def _run(self):
        for i, line in enumerate(self.script_lines):
            line_num = i + 1 + self.line_offset
            if not self.is_running:
                self.status_callback("Stopped", -1)
                break

            line = line.strip()
            if not line or line.startswith('#'):
                if i == len(self.script_lines) - 1: self.status_callback("Idle", -1)
                continue

            parts = line.split()
            command_word = parts[0].upper()
            args = parts[1:]

            self.status_callback(f"L{line_num}: Executing '{line}'", line_num)

            # --- Handle Script-Control Commands ---
            if command_word.startswith("SET_DEFAULT_"):
                if len(args) != 1:
                    self.status_callback(f"Error on L{line_num}: SET_DEFAULT requires 1 argument.", line_num)
                    self.is_running = False
                else:
                    param_map = {
                        "SET_DEFAULT_MOVE_VEL": (self.default_move_params, 'vel'),
                        "SET_DEFAULT_MOVE_ACC": (self.default_move_params, 'acc'),
                        "SET_DEFAULT_MOVE_TORQUE": (self.default_move_params, 'torque'),
                        "SET_DEFAULT_VACUUM_TARGET": (self, 'default_vacuum_target'),
                        "SET_DEFAULT_VACUUM_TIMEOUT": (self, 'default_vacuum_timeout'),
                        "SET_DEFAULT_HEATER_TIMEOUT": (self, 'default_heater_timeout'),
                    }
                    if command_word in param_map:
                        target_obj, key = param_map[command_word]
                        try:
                            value = float(args[0])
                            if isinstance(target_obj, dict):
                                target_obj[key] = str(value)
                            else:
                                setattr(target_obj, key, value)
                            self.status_callback(f"Set {command_word.replace('SET_DEFAULT_', '')} to {args[0]}",
                                                 line_num)
                        except ValueError:
                            self.status_callback(f"Error on L{line_num}: Invalid number for {command_word}.", line_num)
                            self.is_running = False
                    else:
                        self.status_callback(f"Error on L{line_num}: Unknown SET command.", line_num)
                        self.is_running = False

            elif command_word == "WAIT":
                try:
                    time.sleep(float(args[0]) / 1000.0)
                except (ValueError, IndexError):
                    self.status_callback(f"Error on L{line_num}: Invalid WAIT format.", line_num)
                    self.is_running = False

            elif command_word == "WAIT_UNTIL_VACUUM":
                try:
                    target_psi = float(args[0]) if args else self.default_vacuum_target
                    timeout = float(args[1]) if len(args) > 1 else self.default_vacuum_timeout
                    if not self._wait_for_vacuum(target_psi, timeout, line_num): self.is_running = False
                except (ValueError, IndexError):
                    self.status_callback(f"Error on L{line_num}: Invalid args for {command_word}.", line_num)
                    self.is_running = False

            elif command_word == "WAIT_UNTIL_HEATER_AT_TEMP":
                try:
                    target_temp = float(args[0]) if args else self.default_heater_target
                    timeout = float(args[1]) if len(args) > 1 else self.default_heater_timeout
                    if not self._wait_for_heater(target_temp, timeout, line_num): self.is_running = False
                except (ValueError, IndexError):
                    self.status_callback(f"Error on L{line_num}: Invalid args for {command_word}.", line_num)
                    self.is_running = False

            # --- Handle Device Commands ---
            else:
                command_info = COMMANDS.get(command_word)
                if not command_info:
                    self.status_callback(f"Error on L{line_num}: Unknown command '{command_word}'.", line_num)
                    self.is_running = False
                else:
                    device = command_info['device']
                    send_func = self.command_funcs.get(f"send_{device}") if device in ['injector', 'fillhead'] else None

                    if send_func:
                        send_func(line)
                        is_motion = "MOVE" in command_word or "HOME" in command_word or "INJECT" in command_word or "PURGE" in command_word
                        if is_motion:
                            if not self._wait_for_idle({device.upper()}, line_num): self.is_running = False
                    elif device == 'both':
                        self.command_funcs.get("send_injector")(line)
                        self.command_funcs.get("send_fillhead")(line)
                    else:
                        self.status_callback(f"Error on L{line_num}: Invalid device '{device}'.", line_num)
                        self.is_running = False

            if not self.is_running: break

        self.is_running = False
        if self.completion_callback: self.completion_callback()


class ValidationResultsWindow(tk.Toplevel):
    def __init__(self, parent, errors):
        super().__init__(parent)
        self.title("Validation Results")
        self.geometry("600x300")
        self.configure(bg="#2a2d3b")

        self.transient(parent)
        self.grab_set()

        text_area = scrolledtext.ScrolledText(self, wrap=tk.WORD, bg="#1b1e2b", fg="white", font=("Consolas", 10))
        text_area.pack(expand=True, fill="both", padx=10, pady=10)

        if not errors:
            text_area.insert(tk.END, "Validation successful! No errors found.")
        else:
            for error in errors:
                text_area.insert(tk.END, f"Line {error['line']}: {error['error']}\n")

        text_area.config(state=tk.DISABLED)

        close_button = ttk.Button(self, text="Close", command=self.destroy)
        close_button.pack(pady=5)


def create_command_reference(parent, script_editor_widget):
    ref_frame = tk.LabelFrame(parent, text="Command Reference", bg="#2a2d3b", fg="white", padx=5, pady=5)
    tree = ttk.Treeview(ref_frame, columns=('device', 'params', 'desc'), show='tree headings')
    tree.heading('#0', text='Command')
    tree.heading('device', text='Device')
    tree.heading('params', text='Parameters')
    tree.heading('desc', text='Description')
    tree.column('#0', width=150, anchor='w');
    tree.column('device', width=80, anchor='w')
    tree.column('params', width=200, anchor='w');
    tree.column('desc', width=250, anchor='w')

    style = ttk.Style()
    style.configure("Treeview", background="#1b1e2b", foreground="white", fieldbackground="#1b1e2b",
                    font=('Segoe UI', 8))
    style.configure("Treeview.Heading", font=('Segoe UI', 9, 'bold'))
    style.map("Treeview", background=[('selected', '#0078d7')])

    for cmd, details in sorted(COMMANDS.items(), key=lambda item: (item[1]['device'], item[0])):
        device = details['device']
        params = " ".join([p['name'] for p in details['params']])
        desc = details['help']
        tree.insert('', 'end', text=cmd, values=(device.capitalize(), params, desc))

    tree.pack(fill=tk.BOTH, expand=True)

    def add_to_script(event=None):
        selected_item = tree.focus()
        if not selected_item: return
        command = tree.item(selected_item, "text")
        script_editor_widget.insert(tk.INSERT, f"{command} ")

    tree.bind("<Double-1>", add_to_script)
    return ref_frame


def create_status_overview(parent, gui_refs):
    status_container = tk.Frame(parent, bg="#21232b")
    fh_readout_frame = tk.LabelFrame(status_container, text="Fillhead Position", bg="#2a2d3b", fg="white", padx=5,
                                     pady=5)
    fh_readout_frame.pack(fill=tk.X, expand=False, pady=(5, 5), padx=0)

    font_readout_label = ("Segoe UI", 10, "bold");
    font_readout_value = ("Consolas", 24, "bold");
    font_homed_status = ("Segoe UI", 8, "italic")
    vars = {
        'fh_x_pos': tk.StringVar(value='0.00'), 'fh_y_pos': tk.StringVar(value='0.00'),
        'fh_z_pos': tk.StringVar(value='0.00'),
        'fh_x_homed': tk.StringVar(value='Not Homed'), 'fh_y_homed': tk.StringVar(value='Not Homed'),
        'fh_z_homed': tk.StringVar(value='Not Homed'),
        'inj_state': tk.StringVar(value='---'), 'inj_temp': tk.StringVar(value='---'),
        'inj_vac': tk.StringVar(value='---'), 'inj_vol': tk.StringVar(value='---'),
    }
    xyz_frame = tk.Frame(fh_readout_frame, bg=fh_readout_frame['bg']);
    xyz_frame.pack(fill=tk.X, expand=True)
    for i, axis in enumerate(['X', 'Y', 'Z']):
        color = ['cyan', 'yellow', '#ff8888'][i]
        axis_frame = tk.Frame(xyz_frame, bg=xyz_frame['bg']);
        axis_frame.pack(side=tk.LEFT, expand=True, fill=tk.X)
        tk.Label(axis_frame, text=f"{axis} (mm)", bg=axis_frame['bg'], fg=color, font=font_readout_label).pack()
        tk.Label(axis_frame, textvariable=vars[f'fh_{axis.lower()}_pos'], bg=axis_frame['bg'], fg=color,
                 font=font_readout_value).pack()
        homed_label = tk.Label(axis_frame, textvariable=vars[f'fh_{axis.lower()}_homed'], bg=axis_frame['bg'],
                               font=font_homed_status);
        homed_label.pack()

        def make_tracer(var, label):
            def tracer(*args): label.config(fg="lightgreen" if var.get() == "Homed" else "orange")

            return tracer

        vars[f'fh_{axis.lower()}_homed'].trace_add('write', make_tracer(vars[f'fh_{axis.lower()}_homed'], homed_label))
        make_tracer(vars[f'fh_{axis.lower()}_homed'], homed_label)()

    inj_frame = tk.LabelFrame(status_container, text="Injector Status", bg="#2a2d3b", fg="#aaddff", padx=5, pady=5)
    inj_frame.pack(fill=tk.X, expand=True, pady=2, padx=0);
    inj_frame.grid_columnconfigure(1, weight=1)
    tk.Label(inj_frame, text="State:", bg=inj_frame['bg'], fg="white").grid(row=0, column=0, sticky='e')
    tk.Label(inj_frame, textvariable=vars['inj_state'], bg=inj_frame['bg'], fg="cyan").grid(row=0, column=1, sticky='w')
    tk.Label(inj_frame, text="Temp:", bg=inj_frame['bg'], fg="white").grid(row=1, column=0, sticky='e')
    tk.Label(inj_frame, textvariable=vars['inj_temp'], bg=inj_frame['bg'], fg="orange").grid(row=1, column=1,
                                                                                             sticky='w')
    tk.Label(inj_frame, text="Vacuum:", bg=inj_frame['bg'], fg="white").grid(row=0, column=2, sticky='e', padx=(10, 0))
    tk.Label(inj_frame, textvariable=vars['inj_vac'], bg=inj_frame['bg'], fg="lightblue").grid(row=0, column=3,
                                                                                               sticky='w')
    tk.Label(inj_frame, text="Dispensed:", bg=inj_frame['bg'], fg="white").grid(row=1, column=2, sticky='e',
                                                                                padx=(10, 0))
    tk.Label(inj_frame, textvariable=vars['inj_vol'], bg=inj_frame['bg'], fg="lightgreen").grid(row=1, column=3,
                                                                                                sticky='w')

    def update_loop():
        try:
            vars['inj_state'].set(gui_refs.get('main_state_var', tk.StringVar(value='N/A')).get())
            vars['inj_temp'].set(gui_refs.get('temp_c_var', tk.StringVar(value='N/A')).get())
            vars['inj_vac'].set(gui_refs.get('vacuum_psig_var', tk.StringVar(value='N/A')).get())
            vars['inj_vol'].set(gui_refs.get('inject_dispensed_ml_var', tk.StringVar(value='N/A')).get())
            vars['fh_x_pos'].set(f"{float(gui_refs.get('fh_pos_m0_var', tk.StringVar(value='0.0')).get()):.2f}")
            vars['fh_y_pos'].set(f"{float(gui_refs.get('fh_pos_m1_var', tk.StringVar(value='0.0')).get()):.2f}")
            vars['fh_z_pos'].set(f"{float(gui_refs.get('fh_pos_m3_var', tk.StringVar(value='0.0')).get()):.2f}")
            vars['fh_x_homed'].set(gui_refs.get('fh_homed_m0_var', tk.StringVar(value='Not Homed')).get())
            vars['fh_y_homed'].set(gui_refs.get('fh_homed_m1_var', tk.StringVar(value='Not Homed')).get())
            vars['fh_z_homed'].set(gui_refs.get('fh_homed_m3_var', tk.StringVar(value='Not Homed')).get())
        except (ValueError, tk.TclError, AttributeError):
            pass
        status_container.after(250, update_loop)

    status_container.after(250, update_loop)
    return status_container


def create_scripting_tab(notebook, command_funcs, shared_gui_refs):
    scripting_tab = tk.Frame(notebook, bg="#21232b")
    notebook.add(scripting_tab, text='  Scripting  ')
    script_runner = None
    last_exec_highlight = -1
    last_selection_highlight = 1

    paned_window = ttk.PanedWindow(scripting_tab, orient=tk.HORIZONTAL)
    paned_window.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
    left_pane = tk.Frame(paned_window, bg="#21232b")
    paned_window.add(left_pane, weight=3)
    right_pane = tk.Frame(paned_window, bg="#21232b")
    paned_window.add(right_pane, weight=2)

    control_frame = tk.Frame(left_pane, bg="#2a2d3b")
    control_frame.pack(fill=tk.X, pady=(5, 0))
    editor_frame = tk.LabelFrame(left_pane, text="Script Editor", bg="#2a2d3b", fg="white", padx=5, pady=5)
    editor_frame.pack(fill=tk.BOTH, expand=True, pady=5)

    script_editor = TextWithLineNumbers(editor_frame)
    script_editor.pack(fill="both", expand=True)
    script_editor.tag_config("exec_highlight", background="yellow", foreground="black")
    script_editor.tag_config("selection_highlight", background="#003366")
    script_editor.tag_config("error_highlight", background="#552222")
    script_editor.text.tag_lower("selection_highlight", "sel")
    script_editor.insert(tk.END,
                         "# Example Script (Prefixes are no longer needed!)\n\n# Turn on vacuum and check for leaks\nVACUUM_ON\nWAIT 1000\nVACUUM_CHECK 0.5 10\n\n# Home the Z-axis\nHOME_Z 15 50\n")

    status_overview_widget = create_status_overview(right_pane, shared_gui_refs)
    status_overview_widget.pack(fill=tk.X, expand=False, pady=(5, 0), padx=5)
    command_ref_widget = create_command_reference(right_pane, script_editor.text)
    command_ref_widget.pack(fill=tk.BOTH, expand=True, pady=5, padx=5)

    def set_buttons_state(state):
        for btn in [run_button, run_from_line_button, step_button, stop_button, load_button, save_button,
                    validate_button]:
            btn.config(state=state)

    def status_callback_handler(message, line_num):
        nonlocal last_exec_highlight
        status_var.set(message)
        script_editor.tag_remove("exec_highlight", "1.0", tk.END)
        if line_num != -1:
            script_editor.tag_add("exec_highlight", f"{line_num}.0", f"{line_num}.end")
            last_exec_highlight = line_num
        else:
            last_exec_highlight = -1

    def on_run_finished():
        set_buttons_state(tk.NORMAL)
        status_callback_handler("Idle", -1)

    def on_step_finished():
        set_buttons_state(tk.NORMAL)
        current_line_num = int(last_selection_highlight)
        status_var.set(f"Step complete. Next line: {current_line_num + 1}")
        update_selection_highlight(current_line_num + 1)

    def run_script_from_content(content, line_offset=0, is_step=False):
        nonlocal script_runner
        set_buttons_state(tk.DISABLED);
        stop_button.config(state=tk.NORMAL)
        completion_callback = on_step_finished if is_step else on_run_finished
        script_runner = ScriptRunner(content, command_funcs, shared_gui_refs, status_callback_handler,
                                     completion_callback, line_offset)
        script_runner.start()

    def get_current_line_info():
        try:
            line_num = int(last_selection_highlight)
            content = script_editor.get(f"{line_num}.0", f"{line_num}.end")
            line_offset = line_num - 1
            return content, line_offset
        except (tk.TclError, ValueError, TypeError):
            status_callback_handler("Error: No line selected.", -1)
            return None, None

    def run_full_script():
        update_selection_highlight(1)
        script_content = script_editor.get("1.0", tk.END)
        run_script_from_content(script_content)

    def run_from_line():
        content, line_offset = get_current_line_info()
        if content is None: return
        all_lines = script_editor.get("1.0", tk.END).splitlines()
        content_from_line = "\n".join(all_lines[line_offset:])
        run_script_from_content(content_from_line, line_offset)

    def step_line():
        content, line_offset = get_current_line_info()
        if content is not None and content.strip():
            run_script_from_content(content, line_offset, is_step=True)

    def stop_script():
        if script_runner: script_runner.stop()
        on_run_finished()

    def load_script():
        filepath = filedialog.askopenfilename(title="Open Script File",
                                              filetypes=(("Text files", "*.txt"), ("All files", "*.*")))
        if not filepath: return
        with open(filepath, 'r') as f:
            script_editor.delete('1.0', tk.END);
            script_editor.insert('1.0', f.read())
        script_editor.edit_reset();
        update_selection_highlight(1)

    def save_script():
        filepath = filedialog.asksaveasfilename(title="Save Script As", defaultextension=".txt",
                                                filetypes=(("Text files", "*.txt"), ("All files", "*.*")))
        if not filepath: return
        with open(filepath, 'w') as f: f.write(script_editor.get('1.0', tk.END))
        status_var.set(f"Saved to {filepath.split('/')[-1]}")

    def validate_script_ui():
        script_content = script_editor.get("1.0", tk.END)
        errors = validate_script(script_content)

        # Clear previous error highlights
        script_editor.tag_remove("error_highlight", "1.0", tk.END)

        if not errors:
            messagebox.showinfo("Validation Success", "Script is valid!")
            status_var.set("Validation Successful")
        else:
            ValidationResultsWindow(scripting_tab, errors)
            status_var.set(f"{len(errors)} error(s) found.")
            for error in errors:
                line_num = error['line']
                script_editor.tag_add("error_highlight", f"{line_num}.0", f"{line_num}.end")

    def update_selection_highlight(line_num):
        nonlocal last_selection_highlight
        if last_selection_highlight != -1:
            script_editor.tag_remove("selection_highlight", f"{last_selection_highlight}.0",
                                     f"{last_selection_highlight}.end")
        script_editor.tag_add("selection_highlight", f"{line_num}.0", f"{line_num}.end")
        last_selection_highlight = line_num

    def on_line_click(event):
        index = script_editor.text.index(f"@{event.x},{event.y}")
        line_num = int(index.split('.')[0])
        update_selection_highlight(line_num)

    script_editor.bind("<Button-1>", on_line_click)
    update_selection_highlight(1)

    btn_container = tk.Frame(control_frame, bg="#2a2d3b")
    btn_container.pack(fill=tk.X, padx=5, pady=5)
    exec_frame = tk.LabelFrame(btn_container, text="Execution", bg="#2a2d3b", fg="white", padx=5, pady=5)
    exec_frame.pack(side=tk.LEFT, padx=(0, 5))
    run_button = ttk.Button(exec_frame, text="Run Script", command=run_full_script, style="Green.TButton")
    run_button.pack(side=tk.LEFT)
    run_from_line_button = ttk.Button(exec_frame, text="Run From Line", command=run_from_line)
    run_from_line_button.pack(side=tk.LEFT, padx=5)
    step_button = ttk.Button(exec_frame, text="Step Line", command=step_line)
    step_button.pack(side=tk.LEFT)

    file_frame = tk.LabelFrame(btn_container, text="File & Validation", bg="#2a2d3b", fg="white", padx=5, pady=5)
    file_frame.pack(side=tk.LEFT, padx=5)
    load_button = ttk.Button(file_frame, text="Load", command=load_script)
    load_button.pack(side=tk.LEFT)
    save_button = ttk.Button(file_frame, text="Save", command=save_script)
    save_button.pack(side=tk.LEFT, padx=5)
    validate_button = ttk.Button(file_frame, text="Validate", command=validate_script_ui)
    validate_button.pack(side=tk.LEFT)

    control_frame_btns = tk.LabelFrame(btn_container, text="Control", bg="#2a2d3b", fg="white", padx=5, pady=5)
    control_frame_btns.pack(side=tk.LEFT, padx=5)
    stop_button = ttk.Button(control_frame_btns, text="Stop", command=stop_script, style="Red.TButton")
    stop_button.pack(side=tk.LEFT)
    status_var = tk.StringVar(value="Idle")
    status_label = tk.Label(btn_container, textvariable=status_var, bg=btn_container['bg'], fg="cyan", anchor='w')
    status_label.pack(side=tk.LEFT, padx=10, fill=tk.X, expand=True)

    return {}

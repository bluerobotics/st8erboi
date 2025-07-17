import tkinter as tk
from tkinter import ttk, filedialog, scrolledtext, messagebox
import queue
import threading
import os
from script_validator import COMMANDS, validate_script
from script_processor import ScriptRunner


# --- Autocomplete Popup ---
class AutocompletePopup(tk.Toplevel):
    def __init__(self, parent, text_widget, *args, **kwargs):
        super().__init__(parent, *args, **kwargs)
        self.overrideredirect(True)
        self.text_widget = text_widget

        self.listbox = tk.Listbox(self, bg="#3c3f41", fg="white", selectbackground="#0078d7", exportselection=False)
        self.listbox.pack(expand=True, fill="both")

        self.listbox.bind("<ButtonRelease-1>", self.on_select)
        self.listbox.bind("<Return>", self.on_select)

        self.withdraw()

    def show(self, x, y, suggestions):
        if not suggestions:
            self.withdraw()
            return

        self.listbox.delete(0, tk.END)
        for s in suggestions:
            self.listbox.insert(tk.END, s)

        self.listbox.config(height=min(len(suggestions), 8))
        self.geometry(f"+{x}+{y}")
        self.deiconify()
        self.lift()
        self.focus_set()
        self.listbox.selection_set(0)

    def on_select(self, event=None):
        if not self.listbox.curselection():
            self.withdraw()
            return

        selected = self.listbox.get(self.listbox.curselection())
        self.text_widget.autocomplete(selected)
        self.withdraw()

    def on_key(self, event):
        if event.keysym == "Down":
            current = self.listbox.curselection()
            if current and current[0] < self.listbox.size() - 1:
                self.listbox.selection_clear(0, tk.END)
                self.listbox.selection_set(current[0] + 1)
        elif event.keysym == "Up":
            current = self.listbox.curselection()
            if current and current[0] > 0:
                self.listbox.selection_clear(0, tk.END)
                self.listbox.selection_set(current[0] - 1)


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

        if (args[0] in ("insert", "delete", "replace")):
            self.event_generate("<<Modified>>", when="tail")

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

        self.autocomplete_popup = AutocompletePopup(self, self)
        self.text.bind("<KeyRelease>", self._on_key_release)
        self.text.bind("<FocusOut>", lambda e: self.autocomplete_popup.withdraw())
        self.text.bind("<Escape>", lambda e: self.autocomplete_popup.withdraw())
        self.autocomplete_popup.bind("<Key>", self.autocomplete_popup.on_key)

    def _on_key_release(self, event):
        if event.char.isalnum() or event.keysym in ('BackSpace', 'Delete'):
            self.show_autocomplete()

    def show_autocomplete(self):
        word_start = self.text.index("insert wordstart")
        word_end = self.text.index("insert")
        current_word = self.text.get(word_start, word_end)

        if len(current_word) > 0:
            suggestions = [cmd for cmd in COMMANDS if cmd.startswith(current_word.upper())]
            if suggestions:
                bbox = self.text.bbox(tk.INSERT)
                if bbox:
                    x, y, _, _ = bbox
                    self.autocomplete_popup.show(self.winfo_rootx() + x, self.winfo_rooty() + y + 20, suggestions)
            else:
                self.autocomplete_popup.withdraw()
        else:
            self.autocomplete_popup.withdraw()

    def autocomplete(self, selected_command):
        word_start = self.text.index("insert wordstart")
        word_end = self.text.index("insert")

        self.text.delete(word_start, word_end)
        self.text.insert(tk.INSERT, selected_command + " ")

    def _on_change(self, event):
        self.linenumbers.redraw()

    def get(self, *args, **kwargs):
        return self.text.get(*args, **kwargs)

    def insert(self, *args, **kwargs):
        return self.text.insert(*args, **kwargs)

    def delete(self, *args, **kwargs):
        return self.text.delete(*args, **kwargs)

    def tag_config(self, *args, **kwargs):
        return self.text.tag_config(*args, **kwargs)

    def tag_add(self, *args, **kwargs):
        return self.text.tag_add(*args, **kwargs)

    def tag_remove(self, *args, **kwargs):
        return self.text.tag_remove(*args, **kwargs)

    def index(self, *args, **kwargs):
        return self.text.index(*args, **kwargs)

    @property
    def edit_reset(self):
        return self.text.edit_reset

    def bind(self, *args, **kwargs):
        self.text.bind(*args, **kwargs)


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

    context_menu = tk.Menu(parent, tearoff=0)

    def get_selected_command():
        selected_item = tree.focus()
        if not selected_item: return None
        return tree.item(selected_item, "text")

    def copy_command():
        command = get_selected_command()
        if command:
            ref_frame.clipboard_clear()
            ref_frame.clipboard_append(command)

    def add_to_script():
        command = get_selected_command()
        if command:
            script_editor_widget.insert(tk.INSERT, f"{command} ")

    context_menu.add_command(label="Copy Command", command=copy_command)
    context_menu.add_command(label="Add to Script", command=add_to_script)

    def show_context_menu(event):
        iid = tree.identify_row(event.y)
        if iid:
            tree.selection_set(iid)
            tree.focus(iid)
            context_menu.post(event.x_root, event.y_root)

    tree.bind("<Button-3>", show_context_menu)
    tree.bind("<Double-1>", lambda e: add_to_script())
    return ref_frame


def create_status_overview(parent, gui_refs):
    """Creates the beautiful status overview panel for the scripting tab."""

    def create_rounded_rect(canvas, x1, y1, x2, y2, radius, **kwargs):
        points = [x1 + radius, y1, x1 + radius, y1, x2 - radius, y1, x2 - radius, y1, x2, y1, x2, y1 + radius, x2,
                  y1 + radius, x2, y2 - radius, x2, y2 - radius, x2, y2, x2 - radius, y2, x2 - radius, y2, x1 + radius,
                  y2, x1 + radius, y2, x1, y2, x1, y2 - radius, x1, y2 - radius, x1, y1 + radius, x1, y1 + radius, x1,
                  y1]
        return canvas.create_polygon(points, **kwargs, smooth=True)

    status_container = tk.Frame(parent, bg="#21232b")

    font_title = ("Segoe UI", 14, "bold")
    font_readout_label = ("Segoe UI", 10)
    font_readout_value = ("Consolas", 28, "bold")
    font_homed_status = ("Segoe UI", 9, "italic")
    font_status_label = ("Segoe UI", 11)
    font_status_value = ("Segoe UI", 11, "bold")

    bg_color = "#21232b"
    card_color = "#2a2d3b"
    title_color = "#ffffff"
    label_color = "#adb5bd"

    fh_card_frame = tk.Frame(status_container, bg=bg_color)
    fh_card_frame.pack(fill=tk.X, pady=(5, 10))

    fh_canvas = tk.Canvas(fh_card_frame, bg=bg_color, highlightthickness=0, height=120)
    fh_canvas.pack(fill=tk.X)

    create_rounded_rect(fh_canvas, 2, 2, 600, 118, radius=20, fill=card_color)

    fh_title = tk.Label(fh_card_frame, text="Fillhead Position", bg=card_color, fg=title_color, font=font_title)
    fh_canvas.create_window(20, 25, window=fh_title, anchor="w")

    xyz_frame = tk.Frame(fh_card_frame, bg=card_color)
    fh_canvas.create_window(20, 50, window=xyz_frame, anchor="nw")

    inj_card_frame = tk.Frame(status_container, bg=bg_color)
    inj_card_frame.pack(fill=tk.X)

    inj_canvas = tk.Canvas(inj_card_frame, bg=bg_color, highlightthickness=0, height=150)
    inj_canvas.pack(fill=tk.X)

    create_rounded_rect(inj_canvas, 2, 2, 600, 148, radius=20, fill=card_color)

    inj_title = tk.Label(inj_card_frame, text="Injector Status", bg=card_color, fg="#aaddff", font=font_title)
    inj_canvas.create_window(20, 25, window=inj_title, anchor="w")

    inj_grid_frame = tk.Frame(inj_card_frame, bg=card_color)
    inj_canvas.create_window(20, 50, window=inj_grid_frame, anchor="nw")

    inj_grid_frame.grid_columnconfigure(1, weight=1)
    inj_grid_frame.grid_columnconfigure(3, weight=1)

    vars = {
        'fh_x_pos': tk.StringVar(value='0.00'), 'fh_y_pos': tk.StringVar(value='0.00'),
        'fh_z_pos': tk.StringVar(value='0.00'),
        'fh_x_homed': tk.StringVar(value='Not Homed'), 'fh_y_homed': tk.StringVar(value='Not Homed'),
        'fh_z_homed': tk.StringVar(value='Not Homed'),
        'inj_state': tk.StringVar(value='---'), 'inj_temp': tk.StringVar(value='---'),
        'inj_vac': tk.StringVar(value='---'),
        'inj_heater_status': tk.StringVar(value='OFF'),
        'inj_dispensed_feed': tk.StringVar(value='---'),
        'inj_dispensed_total': tk.StringVar(value='---'),
    }

    for i, axis in enumerate(['X', 'Y', 'Z']):
        color = ['cyan', 'yellow', '#ff8888'][i]
        axis_frame = tk.Frame(xyz_frame, bg=card_color)
        axis_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=20)
        tk.Label(axis_frame, text=f"{axis} (mm)", bg=card_color, fg=color, font=font_readout_label).pack()
        tk.Label(axis_frame, textvariable=vars[f'fh_{axis.lower()}_pos'], bg=card_color, fg=color,
                 font=font_readout_value).pack()
        homed_label = tk.Label(axis_frame, textvariable=vars[f'fh_{axis.lower()}_homed'], bg=card_color,
                               font=font_homed_status)
        homed_label.pack()

        def make_tracer(var, label):
            def tracer(*args): label.config(fg="lightgreen" if var.get() == "Homed" else "orange")

            return tracer

        vars[f'fh_{axis.lower()}_homed'].trace_add('write', make_tracer(vars[f'fh_{axis.lower()}_homed'], homed_label))
        make_tracer(vars[f'fh_{axis.lower()}_homed'], homed_label)()

    tk.Label(inj_grid_frame, text="State:", bg=card_color, fg=label_color, font=font_status_label).grid(row=0, column=0,
                                                                                                        sticky='e',
                                                                                                        padx=5, pady=2)
    tk.Label(inj_grid_frame, textvariable=vars['inj_state'], bg=card_color, fg="cyan", font=font_status_value).grid(
        row=0, column=1, sticky='w')

    tk.Label(inj_grid_frame, text="Temp:", bg=card_color, fg=label_color, font=font_status_label).grid(row=0, column=2,
                                                                                                       sticky='e',
                                                                                                       padx=15, pady=2)
    tk.Label(inj_grid_frame, textvariable=vars['inj_temp'], bg=card_color, fg="orange", font=font_status_value).grid(
        row=0, column=3, sticky='w')

    tk.Label(inj_grid_frame, text="Heater:", bg=card_color, fg=label_color, font=font_status_label).grid(row=1,
                                                                                                         column=2,
                                                                                                         sticky='e',
                                                                                                         padx=15,
                                                                                                         pady=2)
    tk.Label(inj_grid_frame, textvariable=vars['inj_heater_status'], bg=card_color, fg="lightgreen",
             font=font_status_value).grid(row=1, column=3, sticky='w')

    tk.Label(inj_grid_frame, text="Vacuum:", bg=card_color, fg=label_color, font=font_status_label).grid(row=1,
                                                                                                         column=0,
                                                                                                         sticky='e',
                                                                                                         padx=5, pady=2)
    tk.Label(inj_grid_frame, textvariable=vars['inj_vac'], bg=card_color, fg="lightblue", font=font_status_value).grid(
        row=1, column=1, sticky='w')

    tk.Label(inj_grid_frame, text="Vac Check:", bg=card_color, fg=label_color, font=font_status_label).grid(row=2,
                                                                                                            column=0,
                                                                                                            sticky='e',
                                                                                                            padx=5,
                                                                                                            pady=2)
    vac_check_label = tk.Label(inj_grid_frame, textvariable=gui_refs['vacuum_check_status_var'], bg=card_color,
                               font=font_status_value)
    vac_check_label.grid(row=2, column=1, sticky='w')

    def update_vac_check_color(*args):
        status = gui_refs['vacuum_check_status_var'].get()
        color = "white"
        if status == "PASS":
            color = "lightgreen"
        elif status == "FAIL":
            color = "orange red"
        vac_check_label.config(fg=color)

    gui_refs['vacuum_check_status_var'].trace_add('write', update_vac_check_color)
    update_vac_check_color()

    tk.Label(inj_grid_frame, text="Dispensed (Feed):", bg=card_color, fg=label_color, font=font_status_label).grid(
        row=3, column=0, sticky='e', padx=5, pady=2)
    tk.Label(inj_grid_frame, textvariable=vars['inj_dispensed_feed'], bg=card_color, fg="lightgreen",
             font=font_status_value).grid(row=3, column=1, sticky='w')

    tk.Label(inj_grid_frame, text="Dispensed (Total):", bg=card_color, fg=label_color, font=font_status_label).grid(
        row=3, column=2, sticky='e', padx=15, pady=2)
    tk.Label(inj_grid_frame, textvariable=vars['inj_dispensed_total'], bg=card_color, fg="lightgreen",
             font=font_status_value).grid(row=3, column=3, sticky='w')

    def update_loop():
        try:
            vars['inj_state'].set(gui_refs.get('main_state_var', tk.StringVar(value='N/A')).get())
            vars['inj_temp'].set(gui_refs.get('temp_c_var', tk.StringVar(value='N/A')).get())
            vars['inj_vac'].set(gui_refs.get('vacuum_psig_var', tk.StringVar(value='N/A')).get())
            vars['inj_dispensed_feed'].set(gui_refs.get('inject_dispensed_ml_var', tk.StringVar(value='N/A')).get())
            vars['inj_dispensed_total'].set(gui_refs.get('cartridge_steps_var', tk.StringVar(value='N/A')).get())
            vars['inj_heater_status'].set(gui_refs.get('heater_mode_var', tk.StringVar(value='OFF')).get())

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
    current_filepath = None

    message_queue = queue.Queue()
    original_terminal_cb = shared_gui_refs.get('terminal_cb')

    if 'vacuum_check_status_var' not in shared_gui_refs:
        shared_gui_refs['vacuum_check_status_var'] = tk.StringVar(value='N/A')

    def terminal_wrapper(message):
        if "DONE:" in message:
            message_queue.put(message)
        if original_terminal_cb:
            original_terminal_cb(message)

    shared_gui_refs['terminal_cb'] = terminal_wrapper

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
                         "# Example Script\n# Type commands here or load a file.\n")

    status_overview_widget = create_status_overview(right_pane, shared_gui_refs)
    status_overview_widget.pack(fill=tk.X, expand=False, pady=(5, 0), padx=5)
    command_ref_widget = create_command_reference(right_pane, script_editor.text)
    command_ref_widget.pack(fill=tk.BOTH, expand=True, pady=5, padx=5)

    def update_window_title():
        root = notebook.winfo_toplevel()
        filename = "Untitled"
        if current_filepath:
            filename = os.path.basename(current_filepath)

        modified_star = "*" if script_editor.text.edit_modified() else ""
        root.title(f"{filename}{modified_star} - Multi-Device Controller")

    def on_script_modified(event=None):
        update_window_title()

    script_editor.text.bind("<<Modified>>", on_script_modified)

    def set_buttons_state(state):
        for btn in [run_button, run_from_line_button, step_button, stop_button, load_button, save_button,
                    save_as_button, new_button,
                    validate_button]:
            btn.config(state=state)

    def status_callback_handler(message, line_num):
        nonlocal last_exec_highlight
        status_var.set(message)
        if last_exec_highlight != line_num:
            if last_exec_highlight != -1:
                script_editor.tag_remove("exec_highlight", f"{last_exec_highlight}.0", f"{last_exec_highlight}.end")
            if line_num != -1:
                script_editor.tag_add("exec_highlight", f"{line_num}.0", f"{line_num}.end")
            last_exec_highlight = line_num

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
                                     completion_callback, message_queue, line_offset)
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

    def check_script_validity(show_success=False):
        script_content = script_editor.get("1.0", tk.END)
        errors = validate_script(script_content)
        script_editor.tag_remove("error_highlight", "1.0", tk.END)

        if errors:
            ValidationResultsWindow(scripting_tab, errors)
            status_var.set(f"{len(errors)} error(s) found.")
            for error in errors:
                line_num = error['line']
                script_editor.tag_add("error_highlight", f"{line_num}.0", f"{line_num}.end")
            return False
        else:
            if show_success:
                messagebox.showinfo("Validation Success", "Script is valid!")
            status_var.set("Validation Successful")
            return True

    def run_full_script():
        if not check_script_validity(): return
        update_selection_highlight(1)
        script_content = script_editor.get("1.0", tk.END)
        run_script_from_content(script_content)

    def run_from_line():
        if not check_script_validity(): return
        content, line_offset = get_current_line_info()
        if content is None: return
        all_lines = script_editor.get("1.0", tk.END).splitlines()
        content_from_line = "\n".join(all_lines[line_offset:])
        run_script_from_content(content_from_line, line_offset)

    def step_line():
        if not check_script_validity(): return
        try:
            start_line_num = int(last_selection_highlight)
        except (ValueError, TypeError):
            start_line_num = 1

        all_lines = script_editor.get("1.0", tk.END).splitlines()
        next_valid_line_num = -1
        next_valid_line_content = ""

        for i in range(start_line_num - 1, len(all_lines)):
            line_content = all_lines[i].strip()
            if line_content and not line_content.startswith('#'):
                next_valid_line_num = i + 1
                next_valid_line_content = all_lines[i]
                break

        if next_valid_line_num != -1:
            update_selection_highlight(next_valid_line_num)
            run_script_from_content(next_valid_line_content, next_valid_line_num - 1, is_step=True)
        else:
            status_var.set("End of script reached.")
            on_run_finished()

    def stop_script():
        if script_runner: script_runner.stop()
        on_run_finished()

    def check_unsaved_changes():
        if not script_editor.text.edit_modified():
            return True

        response = messagebox.askyesnocancel(
            "Unsaved Changes",
            "You have unsaved changes. Do you want to save them?"
        )

        if response is True:
            return save_script()
        elif response is False:
            return True
        else:
            return False

    def new_script():
        nonlocal current_filepath
        if not check_unsaved_changes():
            return
        script_editor.delete('1.0', tk.END)
        script_editor.text.edit_modified(False)
        current_filepath = None
        update_window_title()
        update_selection_highlight(1)

    def load_script():
        nonlocal current_filepath
        if not check_unsaved_changes():
            return

        filepath = filedialog.askopenfilename(
            title="Open Script File",
            filetypes=(("Text files", "*.txt"), ("All files", "*.*"))
        )
        if not filepath:
            return

        try:
            with open(filepath, 'r') as f:
                script_editor.delete('1.0', tk.END)
                script_editor.insert('1.0', f.read())

            current_filepath = filepath
            script_editor.text.edit_modified(False)
            update_window_title()
            update_selection_highlight(1)
            status_var.set(f"Loaded {os.path.basename(current_filepath)}")
        except Exception as e:
            messagebox.showerror("Load Error", f"Could not load file:\n{e}")

    def save_script():
        nonlocal current_filepath
        if current_filepath:
            try:
                with open(current_filepath, 'w') as f:
                    f.write(script_editor.get('1.0', tk.END))
                script_editor.text.edit_modified(False)
                update_window_title()
                status_var.set(f"Saved to {os.path.basename(current_filepath)}")
                return True
            except Exception as e:
                messagebox.showerror("Save Error", f"Could not save file:\n{e}")
                return False
        else:
            return save_script_as()

    def save_script_as():
        nonlocal current_filepath
        filepath = filedialog.asksaveasfilename(
            title="Save Script As",
            defaultextension=".txt",
            filetypes=(("Text files", "*.txt"), ("All files", "*.*"))
        )
        if not filepath:
            return False

        current_filepath = filepath
        return save_script()

    def validate_script_ui():
        check_script_validity(show_success=True)

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
    new_button = ttk.Button(file_frame, text="New", command=new_script)
    new_button.pack(side=tk.LEFT)
    load_button = ttk.Button(file_frame, text="Load", command=load_script)
    load_button.pack(side=tk.LEFT, padx=5)
    save_button = ttk.Button(file_frame, text="Save", command=save_script)
    save_button.pack(side=tk.LEFT)
    save_as_button = ttk.Button(file_frame, text="Save As", command=save_script_as)
    save_as_button.pack(side=tk.LEFT, padx=5)
    validate_button = ttk.Button(file_frame, text="Validate", command=validate_script_ui)
    validate_button.pack(side=tk.LEFT)

    control_frame_btns = tk.LabelFrame(btn_container, text="Control", bg="#2a2d3b", fg="white", padx=5, pady=5)
    control_frame_btns.pack(side=tk.LEFT, padx=5)
    stop_button = ttk.Button(control_frame_btns, text="Stop", command=stop_script, style="Red.TButton")
    stop_button.pack(side=tk.LEFT)
    status_var = tk.StringVar(value="Idle")
    status_label = tk.Label(btn_container, textvariable=status_var, bg=btn_container['bg'], fg="cyan", anchor='w')
    status_label.pack(side=tk.LEFT, padx=10, fill=tk.X, expand=True)

    update_window_title()

    return {}

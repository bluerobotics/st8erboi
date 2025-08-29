import tkinter as tk
from tkinter import ttk
import math


# This file now consolidates all logic for creating the manual control panels
# for both the Fillhead and the gantry devices.

# --- Motor Power Controls ---
def create_motor_power_controls(parent, command_funcs, shared_gui_refs):
    """Creates the central motor power enable/disable controls with dynamic color feedback."""
    power_frame = tk.LabelFrame(parent, text="Motor Power", bg="#2a2d3b", fg="white", padx=5, pady=5)
    power_frame.pack(fill=tk.X, pady=5, padx=5, anchor='n')

    power_frame.grid_columnconfigure((0, 1), weight=1)

    send_fillhead_cmd = command_funcs['send_fillhead']
    send_gantry_cmd = command_funcs['send_gantry']

    def make_style_tracer(button_map):
        def tracer(*args):
            for btn, var, en_style, dis_style in button_map:
                is_enabled = var.get() == "Enabled"
                btn.config(style=en_style if is_enabled else dis_style)

        return tracer

    fillhead_frame = tk.Frame(power_frame, bg=power_frame['bg'])
    fillhead_frame.grid(row=0, column=0, sticky='ew', padx=(0, 2))
    fillhead_frame.grid_columnconfigure((0, 1), weight=1)

    btn_enable_fillhead = ttk.Button(fillhead_frame, text="Enable Fillhead", style='Small.TButton',
                                     command=lambda: send_fillhead_cmd("ENABLE"))
    btn_enable_fillhead.grid(row=0, column=0, sticky='ew', padx=(0, 1))
    btn_disable_fillhead = ttk.Button(fillhead_frame, text="Disable Fillhead", style='Small.TButton',
                                      command=lambda: send_fillhead_cmd("DISABLE"))
    btn_disable_fillhead.grid(row=0, column=1, sticky='ew', padx=(1, 0))

    def fillhead_state_tracer(*args):
        m0_enabled = shared_gui_refs['enabled_state1_var'].get() == "Enabled"
        m1_enabled = shared_gui_refs['enabled_state2_var'].get() == "Enabled"

        all_enabled = m0_enabled and m1_enabled
        all_disabled = not m0_enabled and not m1_enabled

        if all_enabled:
            btn_enable_fillhead.config(style="Green.TButton")
            btn_disable_fillhead.config(style="Small.TButton")
        elif all_disabled:
            btn_enable_fillhead.config(style="Small.TButton")
            btn_disable_fillhead.config(style="Red.TButton")
        else:
            btn_enable_fillhead.config(style="Yellow.TButton")
            btn_disable_fillhead.config(style="Yellow.TButton")

    shared_gui_refs['enabled_state1_var'].trace_add('write', fillhead_state_tracer)
    shared_gui_refs['enabled_state2_var'].trace_add('write', fillhead_state_tracer)
    fillhead_state_tracer()

    ttk.Separator(power_frame, orient='horizontal').grid(row=1, column=0, columnspan=2, sticky='ew', pady=5)

    fh_frame = tk.Frame(power_frame, bg=power_frame['bg'])
    fh_frame.grid(row=2, column=0, columnspan=2, sticky='ew')
    fh_frame.grid_columnconfigure((0, 1, 2), weight=1)

    btn_enable_x = ttk.Button(fh_frame, text="Enable X", style='Small.TButton',
                              command=lambda: send_gantry_cmd("ENABLE_X"))
    btn_enable_x.grid(row=0, column=0, sticky='ew', padx=(0, 2))
    btn_disable_x = ttk.Button(fh_frame, text="Disable X", style='Small.TButton',
                               command=lambda: send_gantry_cmd("DISABLE_X"))
    btn_disable_x.grid(row=1, column=0, sticky='ew', padx=(0, 2), pady=2)
    x_tracer = make_style_tracer(
        [(btn_enable_x, shared_gui_refs['fh_enabled_m0_var'], 'Green.TButton', 'Small.TButton'),
         (btn_disable_x, shared_gui_refs['fh_enabled_m0_var'], 'Small.TButton', 'Red.TButton')])
    shared_gui_refs['fh_enabled_m0_var'].trace_add('write', x_tracer)
    x_tracer()

    btn_enable_y = ttk.Button(fh_frame, text="Enable Y", style='Small.TButton',
                              command=lambda: send_gantry_cmd("ENABLE_Y"))
    btn_enable_y.grid(row=0, column=1, sticky='ew', padx=2)
    btn_disable_y = ttk.Button(fh_frame, text="Disable Y", style='Small.TButton',
                               command=lambda: send_gantry_cmd("DISABLE_Y"))
    btn_disable_y.grid(row=1, column=1, sticky='ew', padx=2, pady=2)
    y_tracer = make_style_tracer(
        [(btn_enable_y, shared_gui_refs['fh_enabled_m1_var'], 'Green.TButton', 'Small.TButton'),
         (btn_disable_y, shared_gui_refs['fh_enabled_m1_var'], 'Small.TButton', 'Red.TButton')])
    shared_gui_refs['fh_enabled_m1_var'].trace_add('write', y_tracer)
    y_tracer()

    btn_enable_z = ttk.Button(fh_frame, text="Enable Z", style='Small.TButton',
                              command=lambda: send_gantry_cmd("ENABLE_Z"))
    btn_enable_z.grid(row=0, column=2, sticky='ew', padx=(2, 0))
    btn_disable_z = ttk.Button(fh_frame, text="Disable Z", style='Small.TButton',
                               command=lambda: send_gantry_cmd("DISABLE_Z"))
    btn_disable_z.grid(row=1, column=2, sticky='ew', padx=(2, 0), pady=2)
    z_tracer = make_style_tracer(
        [(btn_enable_z, shared_gui_refs['fh_enabled_m3_var'], 'Green.TButton', 'Small.TButton'),
         (btn_disable_z, shared_gui_refs['fh_enabled_m3_var'], 'Small.TButton', 'Red.TButton')])
    shared_gui_refs['fh_enabled_m3_var'].trace_add('write', z_tracer)
    z_tracer()


def create_pinch_valve_controls(parent, command_funcs):
    """Creates controls for opening and closing the pinch valves."""
    send_fillhead_cmd = command_funcs['send_fillhead']
    frame = tk.LabelFrame(parent, text="Pinch Valve Control", bg="#2a2d3b", fg="#E0B0FF", font=("Segoe UI", 9, "bold"), padx=5, pady=5)
    frame.pack(fill=tk.X, pady=5, padx=5, anchor='n')
    frame.grid_columnconfigure((0, 1), weight=1)

    # Injection Valve
    ttk.Button(frame, text="Open Injection Valve", style='Small.TButton', command=lambda: send_fillhead_cmd("INJECTION_VALVE_OPEN")).grid(row=0, column=0, sticky='ew', padx=(0, 2), pady=1)
    ttk.Button(frame, text="Close Injection Valve", style='Small.TButton', command=lambda: send_fillhead_cmd("INJECTION_VALVE_CLOSE")).grid(row=1, column=0, sticky='ew', padx=(0, 2))

    # Vacuum Valve
    ttk.Button(frame, text="Open Vacuum Valve", style='Small.TButton', command=lambda: send_fillhead_cmd("VACUUM_VALVE_OPEN")).grid(row=0, column=1, sticky='ew', padx=(2, 0), pady=1)
    ttk.Button(frame, text="Close Vacuum Valve", style='Small.TButton', command=lambda: send_fillhead_cmd("VACUUM_VALVE_CLOSE")).grid(row=1, column=1, sticky='ew', padx=(2, 0))


def create_heater_controls(parent, command_funcs, shared_gui_refs):
    """Creates controls for the heater."""
    send_fillhead_cmd = command_funcs['send_fillhead']
    frame = tk.LabelFrame(parent, text="Heater Control", bg="#2a2d3b", fg="#FFD700", font=("Segoe UI", 9, "bold"), padx=5, pady=5)
    frame.pack(fill=tk.X, pady=5, padx=5, anchor='n')

    # On/Off Buttons
    btn_frame = tk.Frame(frame, bg=frame['bg'])
    btn_frame.pack(fill=tk.X, pady=(0, 5))
    btn_frame.grid_columnconfigure((0, 1), weight=1)
    ttk.Button(btn_frame, text="Heater On", style='Green.TButton', command=lambda: send_fillhead_cmd("HEATER_ON")).grid(row=0, column=0, sticky='ew', padx=(0, 2))
    ttk.Button(btn_frame, text="Heater Off", style='Red.TButton', command=lambda: send_fillhead_cmd("HEATER_OFF")).grid(row=0, column=1, sticky='ew', padx=(2, 0))

    # Parameter Entries
    param_frame = tk.Frame(frame, bg=frame['bg'])
    param_frame.pack(fill=tk.X)
    param_frame.grid_columnconfigure(1, weight=1)

    # Setpoint
    tk.Label(param_frame, text="Setpoint (°C):", bg=frame['bg'], fg='white').grid(row=0, column=0, sticky='e', padx=(0, 5))
    entry_setpoint = ttk.Entry(param_frame, textvariable=shared_gui_refs['heater_setpoint_var'], width=8)
    entry_setpoint.grid(row=0, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_HEATER_SETPOINT {shared_gui_refs['heater_setpoint_var'].get()}")).grid(row=0, column=2, sticky='ew', padx=2)

    # Gains
    tk.Label(param_frame, text="Gains (P, I, D):", bg=frame['bg'], fg='white').grid(row=1, column=0, sticky='e', padx=(0, 5))
    gains_frame = tk.Frame(param_frame, bg=frame['bg'])
    gains_frame.grid(row=1, column=1, sticky='ew', pady=1)
    gains_frame.grid_columnconfigure((0,1,2), weight=1)
    ttk.Entry(gains_frame, textvariable=shared_gui_refs['heater_kp_var'], width=5).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=(0, 2))
    ttk.Entry(gains_frame, textvariable=shared_gui_refs['heater_ki_var'], width=5).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
    ttk.Entry(gains_frame, textvariable=shared_gui_refs['heater_kd_var'], width=5).pack(side=tk.LEFT, expand=True, fill=tk.X, padx=(2, 0))
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_HEATER_GAINS {shared_gui_refs['heater_kp_var'].get()} {shared_gui_refs['heater_ki_var'].get()} {shared_gui_refs['heater_kd_var'].get()}")).grid(row=1, column=2, sticky='ew', padx=2)


def create_vacuum_controls(parent, command_funcs, shared_gui_refs):
    """Creates controls for the vacuum system."""
    send_fillhead_cmd = command_funcs['send_fillhead']
    frame = tk.LabelFrame(parent, text="Vacuum Control", bg="#2a2d3b", fg="#ADD8E6", font=("Segoe UI", 9, "bold"), padx=5, pady=5)
    frame.pack(fill=tk.X, pady=5, padx=5, anchor='n')

    # On/Off/Test Buttons
    btn_frame = tk.Frame(frame, bg=frame['bg'])
    btn_frame.pack(fill=tk.X, pady=(0, 5))
    btn_frame.grid_columnconfigure((0, 1, 2), weight=1)
    ttk.Button(btn_frame, text="Vacuum On", style='Green.TButton', command=lambda: send_fillhead_cmd("VACUUM_ON")).grid(row=0, column=0, sticky='ew', padx=(0, 2))
    ttk.Button(btn_frame, text="Vacuum Off", style='Red.TButton', command=lambda: send_fillhead_cmd("VACUUM_OFF")).grid(row=0, column=1, sticky='ew', padx=2)
    ttk.Button(btn_frame, text="Leak Test", style='Blue.TButton', command=lambda: send_fillhead_cmd("VACUUM_LEAK_TEST")).grid(row=0, column=2, sticky='ew', padx=(2, 0))

    # Parameter Entries
    param_frame = tk.Frame(frame, bg=frame['bg'])
    param_frame.pack(fill=tk.X, pady=2)
    param_frame.grid_columnconfigure(1, weight=1)

    # Target
    tk.Label(param_frame, text="Target (PSIG):", bg=frame['bg'], fg='white').grid(row=0, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_target_var'], width=8).grid(row=0, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_VACUUM_TARGET {shared_gui_refs['vac_target_var'].get()}")).grid(row=0, column=2, sticky='ew', padx=2)

    # Timeout
    tk.Label(param_frame, text="Timeout (s):", bg=frame['bg'], fg='white').grid(row=1, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_timeout_var'], width=8).grid(row=1, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_VACUUM_TIMEOUT_S {shared_gui_refs['vac_timeout_var'].get()}")).grid(row=1, column=2, sticky='ew', padx=2)
    
    # Leak Test Delta
    tk.Label(param_frame, text="Leak Δ (PSIG):", bg=frame['bg'], fg='white').grid(row=2, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_leak_delta_var'], width=8).grid(row=2, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_LEAK_TEST_DELTA {shared_gui_refs['vac_leak_delta_var'].get()}")).grid(row=2, column=2, sticky='ew', padx=2)
    
    # Leak Test Duration
    tk.Label(param_frame, text="Leak Dura. (s):", bg=frame['bg'], fg='white').grid(row=3, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_leak_duration_var'], width=8).grid(row=3, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_LEAK_TEST_DURATION_S {shared_gui_refs['vac_leak_duration_var'].get()}")).grid(row=3, column=2, sticky='ew', padx=2)


# --- Main Collapsible Panel Creation ---
def create_manual_controls_panel(parent, command_funcs, shared_gui_refs):
    """Creates the scrollable panel containing all manual device controls."""
    from fillhead_controls import create_fillhead_ancillary_controls
    from gantry_controls import create_gantry_ancillary_controls

    canvas = tk.Canvas(parent, bg="#2a2d3b", highlightthickness=0)
    scrollbar = ttk.Scrollbar(parent, orient="vertical", command=canvas.yview)
    scrollable_frame = tk.Frame(canvas, bg="#2a2d3b")

    def on_canvas_configure(event):
        canvas.itemconfig(window_id, width=event.width)

    scrollable_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))

    def _on_mousewheel(event):
        canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

    canvas.bind_all("<MouseWheel>", _on_mousewheel)
    scrollable_frame.bind("<MouseWheel>", _on_mousewheel)

    window_id = canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
    canvas.configure(yscrollcommand=scrollbar.set)
    canvas.bind("<Configure>", on_canvas_configure)

    canvas.pack(side="left", fill="both", expand=True)
    scrollbar.pack(side="right", fill="y")

    ui_elements = {}

    create_motor_power_controls(scrollable_frame, command_funcs, shared_gui_refs)

    # Separator
    ttk.Separator(scrollable_frame, orient='horizontal').pack(fill=tk.X, pady=10, padx=5)

    create_pinch_valve_controls(scrollable_frame, command_funcs)
    create_heater_controls(scrollable_frame, command_funcs, shared_gui_refs)
    create_vacuum_controls(scrollable_frame, command_funcs, shared_gui_refs)

    # Separator
    ttk.Separator(scrollable_frame, orient='horizontal').pack(fill=tk.X, pady=10, padx=5)

    create_fillhead_ancillary_controls(scrollable_frame, command_funcs['send_fillhead'], shared_gui_refs, ui_elements)
    # --- MODIFIED: Added the missing 'shared_gui_refs' argument ---
    create_gantry_ancillary_controls(scrollable_frame, command_funcs['send_gantry'], shared_gui_refs)


def create_manual_controls_display(parent, command_funcs, shared_gui_refs):
    """Creates the main expandable container for the manual controls."""
    main_container = tk.Frame(parent, bg="#21232b")

    content_panel = tk.Frame(main_container, width=350, bg="#2a2d3b")
    content_panel.pack_propagate(False)
    create_manual_controls_panel(content_panel, command_funcs, shared_gui_refs)

    trigger_canvas = tk.Canvas(main_container, width=30, bg="#4a5568", highlightthickness=1,
                               highlightbackground="#646d7e")
    trigger_canvas.pack(side=tk.LEFT, fill=tk.Y)

    def draw_trigger_text(text):
        trigger_canvas.delete("all")
        trigger_canvas.create_text(15, 150, text=text, angle=90, font=("Segoe UI", 10, "bold"), fill="white",
                                   anchor="center")

    def toggle_panel():
        if content_panel.winfo_ismapped():
            content_panel.pack_forget()
            draw_trigger_text("Show Manual Controls")
        else:
            content_panel.pack(side=tk.LEFT, fill=tk.Y)
            draw_trigger_text("Hide Manual Controls")

    draw_trigger_text("Show Manual Controls")
    trigger_canvas.bind("<Button-1>", lambda e: toggle_panel())
    trigger_canvas.bind("<Enter>", lambda e: trigger_canvas.config(bg="#5a6268"))
    trigger_canvas.bind("<Leave>", lambda e: trigger_canvas.config(bg="#4a5568"))

    return {'main_container': main_container}

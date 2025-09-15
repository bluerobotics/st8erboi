import tkinter as tk
from tkinter import ttk
import theme


# This file now consolidates all logic for creating the manual control panels
# for both the Fillhead and the gantry devices.

# --- Motor Power Controls ---
def create_motor_power_controls(parent, command_funcs, shared_gui_refs):
    """Creates the central motor power enable/disable controls."""
    power_frame = ttk.LabelFrame(parent, text="Motor Power", style='Card.TLabelframe', padding=6)
    power_frame.pack(fill=tk.X, pady=4, padx=6, anchor='n')
    power_frame.grid_columnconfigure((0, 1), weight=1)

    send_fillhead_cmd = command_funcs['send_fillhead']
    send_gantry_cmd = command_funcs['send_gantry']

    def make_style_tracer(button_map):
        def tracer(*args):
            for btn, var, en_style, dis_style in button_map:
                is_enabled = var.get() == "Enabled"
                btn.config(style=en_style if is_enabled else dis_style)

        return tracer

    fillhead_frame = ttk.Frame(power_frame, style='TFrame')
    fillhead_frame.grid(row=0, column=0, sticky='ew', padx=(0, 2))
    fillhead_frame.grid_columnconfigure((0, 1), weight=1)

    btn_enable_fillhead = ttk.Button(fillhead_frame, text="Enable Fillhead", style='Small.TButton', command=lambda: send_fillhead_cmd("ENABLE"))
    btn_enable_fillhead.grid(row=0, column=0, sticky='ew', padx=(0, 1))
    btn_disable_fillhead = ttk.Button(fillhead_frame, text="Disable Fillhead", style='Small.TButton', command=lambda: send_fillhead_cmd("DISABLE"))
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

    fh_frame = ttk.Frame(power_frame, style='TFrame')
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
    frame = ttk.LabelFrame(parent, text="Pinch Valves", style='Card.TLabelframe', padding=6)
    frame.pack(fill=tk.X, pady=4, padx=6, anchor='n')
    frame.grid_columnconfigure((0, 1), weight=1)

    # Injection Valve
    ttk.Button(frame, text="Inj Open", style='Green.TButton', command=lambda: send_fillhead_cmd("INJECTION_VALVE_OPEN")).grid(row=0, column=0, sticky='ew', padx=(0, 2), pady=1)
    ttk.Button(frame, text="Inj Close", style='Red.TButton', command=lambda: send_fillhead_cmd("INJECTION_VALVE_CLOSE")).grid(row=1, column=0, sticky='ew', padx=(0, 2))

    # Vacuum Valve
    ttk.Button(frame, text="Vac Open", style='Green.TButton', command=lambda: send_fillhead_cmd("VACUUM_VALVE_OPEN")).grid(row=0, column=1, sticky='ew', padx=(2, 0), pady=1)
    ttk.Button(frame, text="Vac Close", style='Red.TButton', command=lambda: send_fillhead_cmd("VACUUM_VALVE_CLOSE")).grid(row=1, column=1, sticky='ew', padx=(2, 0))


def create_heater_controls(parent, command_funcs, shared_gui_refs):
    """Creates controls for the heater."""
    send_fillhead_cmd = command_funcs['send_fillhead']
    frame = ttk.LabelFrame(parent, text="Heater", style='Card.TLabelframe', padding=6)
    frame.pack(fill=tk.X, pady=4, padx=6, anchor='n')

    # On/Off Buttons
    btn_frame = ttk.Frame(frame, style='TFrame')
    btn_frame.pack(fill=tk.X, pady=(0, 4))
    btn_frame.grid_columnconfigure((0, 1), weight=1)
    ttk.Button(btn_frame, text="On", style='Green.TButton', width=3, command=lambda: send_fillhead_cmd("HEATER_ON")).grid(row=0, column=0, sticky='e', padx=(0, 2))
    ttk.Button(btn_frame, text="Off", style='Red.TButton', width=3, command=lambda: send_fillhead_cmd("HEATER_OFF")).grid(row=0, column=1, sticky='w', padx=(2, 0))

    # Parameter Entries
    param_frame = ttk.Frame(frame, style='TFrame')
    param_frame.pack(fill=tk.X)
    param_frame.grid_columnconfigure(1, weight=1)

    # Setpoint
    ttk.Label(param_frame, text="Setpoint (°C):", style='Subtle.TLabel', padding=5).grid(row=0, column=0, sticky='e', padx=(0, 5))
    entry_setpoint = ttk.Entry(param_frame, textvariable=shared_gui_refs['heater_setpoint_var'], width=8, style='TEntry')
    entry_setpoint.grid(row=0, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_HEATER_SETPOINT {shared_gui_refs['heater_setpoint_var'].get()}")).grid(row=0, column=2, sticky='ew', padx=2)

    # Gains
    ttk.Label(param_frame, text="Gains (P, I, D):", style='Subtle.TLabel', padding=5).grid(row=1, column=0, sticky='e', padx=(0, 5))
    gains_frame = ttk.Frame(param_frame, style='TFrame')
    gains_frame.grid(row=1, column=1, sticky='ew', pady=1)
    gains_frame.grid_columnconfigure((0,1,2), weight=1)
    ttk.Entry(gains_frame, textvariable=shared_gui_refs['heater_kp_var'], width=5, style='TEntry').pack(side=tk.LEFT, expand=True, fill=tk.X, padx=(0, 2))
    ttk.Entry(gains_frame, textvariable=shared_gui_refs['heater_ki_var'], width=5, style='TEntry').pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
    ttk.Entry(gains_frame, textvariable=shared_gui_refs['heater_kd_var'], width=5, style='TEntry').pack(side=tk.LEFT, expand=True, fill=tk.X, padx=(2, 0))
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_HEATER_GAINS {shared_gui_refs['heater_kp_var'].get()} {shared_gui_refs['heater_ki_var'].get()} {shared_gui_refs['heater_kd_var'].get()}")).grid(row=1, column=2, sticky='ew', padx=2)


def create_vacuum_controls(parent, command_funcs, shared_gui_refs):
    """Creates controls for the vacuum system."""
    send_fillhead_cmd = command_funcs['send_fillhead']
    frame = ttk.LabelFrame(parent, text="Vacuum", style='Card.TLabelframe', padding=6)
    frame.pack(fill=tk.X, pady=4, padx=6, anchor='n')

    # On/Off/Test Buttons
    btn_frame = ttk.Frame(frame, style='TFrame')
    btn_frame.pack(fill=tk.X, pady=(0, 4))
    btn_frame.grid_columnconfigure((0, 1, 2), weight=1)
    ttk.Button(btn_frame, text="On", style='Green.TButton', width=3, command=lambda: send_fillhead_cmd("VACUUM_ON")).grid(row=0, column=0, sticky='ew', padx=(0, 2))
    ttk.Button(btn_frame, text="Off", style='Red.TButton', width=3, command=lambda: send_fillhead_cmd("VACUUM_OFF")).grid(row=0, column=1, sticky='ew', padx=2)
    ttk.Button(btn_frame, text="Leak", style='Blue.TButton', width=4, command=lambda: send_fillhead_cmd("VACUUM_LEAK_TEST")).grid(row=0, column=2, sticky='ew', padx=(2, 0))

    # Parameter Entries
    param_frame = ttk.Frame(frame, style='TFrame')
    param_frame.pack(fill=tk.X, pady=2)
    param_frame.grid_columnconfigure(1, weight=1)

    # Target
    ttk.Label(param_frame, text="Target (PSIG):", style='Subtle.TLabel', padding=5).grid(row=0, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_target_var'], width=8, style='TEntry').grid(row=0, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_VACUUM_TARGET {shared_gui_refs['vac_target_var'].get()}")).grid(row=0, column=2, sticky='ew', padx=2)

    # Timeout
    ttk.Label(param_frame, text="Timeout (s):", style='Subtle.TLabel', padding=5).grid(row=1, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_timeout_var'], width=8, style='TEntry').grid(row=1, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_VACUUM_TIMEOUT_S {shared_gui_refs['vac_timeout_var'].get()}")).grid(row=1, column=2, sticky='ew', padx=2)
    
    # Leak Test Delta
    ttk.Label(param_frame, text="Leak Δ (PSIG):", style='Subtle.TLabel', padding=5).grid(row=2, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_leak_delta_var'], width=8, style='TEntry').grid(row=2, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_LEAK_TEST_DELTA {shared_gui_refs['vac_leak_delta_var'].get()}")).grid(row=2, column=2, sticky='ew', padx=2)
    
    # Leak Test Duration
    ttk.Label(param_frame, text="Leak Dura. (s):", style='Subtle.TLabel', padding=5).grid(row=3, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_leak_duration_var'], width=8, style='TEntry').grid(row=3, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_LEAK_TEST_DURATION_S {shared_gui_refs['vac_leak_duration_var'].get()}")).grid(row=3, column=2, sticky='ew', padx=2)


def create_fillhead_jog_controls(parent, send_cmd_func, variables):
    """Creates a themed jog panel for the Fillhead."""
    frame = ttk.LabelFrame(parent, text="Fillhead Jog", style='Card.TLabelframe', padding=8)
    frame.pack(fill=tk.X, expand=False, pady=(6, 0), padx=6)

    params_frame = ttk.Frame(frame, style='Card.TFrame')
    params_frame.pack(fill=tk.X, pady=(0, 6))
    params_frame.grid_columnconfigure((0, 1), weight=1)

    ttk.Label(params_frame, text="Dist (mm)", style='Subtle.TLabel').grid(row=0, column=0, sticky="w")
    ttk.Label(params_frame, text="Vel (mm/s)", style='Subtle.TLabel').grid(row=0, column=1, sticky="w")
    ttk.Entry(params_frame, textvariable=variables['jog_dist_mm_var'], width=8).grid(row=1, column=0, sticky="ew", padx=(0, 6))
    ttk.Entry(params_frame, textvariable=variables['jog_velocity_var'], width=8).grid(row=1, column=1, sticky="ew")

    ttk.Label(params_frame, text="Accel (mm/s²)", style='Subtle.TLabel').grid(row=2, column=0, sticky="w", pady=(4, 0))
    ttk.Label(params_frame, text="Torque (%)", style='Subtle.TLabel').grid(row=2, column=1, sticky="w", pady=(4, 0))
    ttk.Entry(params_frame, textvariable=variables['jog_acceleration_var'], width=8).grid(row=3, column=0, sticky="ew", padx=(0, 6))
    ttk.Entry(params_frame, textvariable=variables['jog_torque_percent_var'], width=8).grid(row=3, column=1, sticky="ew")

    btn_frame = ttk.Frame(frame, style='Card.TFrame')
    btn_frame.pack(fill=tk.X)
    btn_frame.grid_columnconfigure((0, 1, 2), weight=1)
    
    # M0 Jog
    m0_frame = ttk.Frame(btn_frame, style='TFrame')
    m0_frame.grid(row=1, column=0, padx=1, sticky='ew')
    ttk.Label(btn_frame, text="M0").grid(row=0, column=0)
    ttk.Button(m0_frame, text="\u25b2", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"JOG_MOVE {variables['jog_dist_mm_var'].get()} 0 {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}")).pack()
    ttk.Button(m0_frame, text="\u25bc", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"JOG_MOVE -{variables['jog_dist_mm_var'].get()} 0 {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}")).pack()

    # M1 Jog
    m1_frame = ttk.Frame(btn_frame, style='TFrame')
    m1_frame.grid(row=1, column=1, padx=1, sticky='ew')
    ttk.Label(btn_frame, text="M1").grid(row=0, column=1)
    ttk.Button(m1_frame, text="\u25b2", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"JOG_MOVE 0 {variables['jog_dist_mm_var'].get()} {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}")).pack()
    ttk.Button(m1_frame, text="\u25bc", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"JOG_MOVE 0 -{variables['jog_dist_mm_var'].get()} {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}")).pack()

    # Both Jog
    both_frame = ttk.Frame(btn_frame, style='TFrame')
    both_frame.grid(row=1, column=2, padx=1, sticky='ew')
    ttk.Label(btn_frame, text="Both").grid(row=0, column=2)
    ttk.Button(both_frame, text="\u25b2", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"JOG_MOVE {variables['jog_dist_mm_var'].get()} {variables['jog_dist_mm_var'].get()} {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}")).pack()
    ttk.Button(both_frame, text="\u25bc", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"JOG_MOVE -{variables['jog_dist_mm_var'].get()} -{variables['jog_dist_mm_var'].get()} {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}")).pack()


def create_gantry_jog_controls(parent, send_cmd_func, variables):
    """Creates a themed jog panel for the Gantry."""
    frame = ttk.LabelFrame(parent, text="Gantry Jog", style='Card.TLabelframe', padding=8)
    frame.pack(fill=tk.X, expand=False, pady=6, padx=6)

    params_frame = ttk.Frame(frame, style='Card.TFrame')
    params_frame.pack(fill=tk.X, pady=(0, 6))
    params_frame.grid_columnconfigure((0, 1), weight=1)

    ttk.Label(params_frame, text="Dist (mm)", style='Subtle.TLabel').grid(row=0, column=0, sticky="w")
    ttk.Label(params_frame, text="Speed (mm/s)", style='Subtle.TLabel').grid(row=0, column=1, sticky="w")
    ttk.Entry(params_frame, textvariable=variables['fh_jog_dist_mm_var'], width=8).grid(row=1, column=0, sticky="ew", padx=(0, 6))
    ttk.Entry(params_frame, textvariable=variables['fh_jog_vel_mms_var'], width=8).grid(row=1, column=1, sticky="ew")

    ttk.Label(params_frame, text="Accel (mm/s²)", style='Subtle.TLabel').grid(row=2, column=0, sticky="w", pady=(4, 0))
    ttk.Label(params_frame, text="Torque (%)", style='Subtle.TLabel').grid(row=2, column=1, sticky="w", pady=(4, 0))
    ttk.Entry(params_frame, textvariable=variables['fh_jog_accel_mms2_var'], width=8).grid(row=3, column=0, sticky="ew", padx=(0, 6))
    ttk.Entry(params_frame, textvariable=variables['fh_jog_torque_var'], width=8).grid(row=3, column=1, sticky="ew")

    btn_frame = ttk.Frame(frame, style='Card.TFrame')
    btn_frame.pack(fill=tk.X)
    btn_frame.grid_columnconfigure((0, 1, 2), weight=1)

    # X Jog
    x_frame = ttk.Frame(btn_frame, style='TFrame')
    x_frame.grid(row=1, column=0, padx=1, sticky='ew')
    ttk.Label(btn_frame, text="X").grid(row=0, column=0)
    ttk.Button(x_frame, text="+", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"MOVE_X {variables['fh_jog_dist_mm_var'].get()} {variables['fh_jog_vel_mms_var'].get()} {variables['fh_jog_accel_mms2_var'].get()} {variables['fh_jog_torque_var'].get()}")).pack()
    ttk.Button(x_frame, text="-", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"MOVE_X -{variables['fh_jog_dist_mm_var'].get()} {variables['fh_jog_vel_mms_var'].get()} {variables['fh_jog_accel_mms2_var'].get()} {variables['fh_jog_torque_var'].get()}")).pack()

    # Y Jog
    y_frame = ttk.Frame(btn_frame, style='TFrame')
    y_frame.grid(row=1, column=1, padx=1, sticky='ew')
    ttk.Label(btn_frame, text="Y").grid(row=0, column=1)
    ttk.Button(y_frame, text="+", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"MOVE_Y {variables['fh_jog_dist_mm_var'].get()} {variables['fh_jog_vel_mms_var'].get()} {variables['fh_jog_accel_mms2_var'].get()} {variables['fh_jog_torque_var'].get()}")).pack()
    ttk.Button(y_frame, text="-", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"MOVE_Y -{variables['fh_jog_dist_mm_var'].get()} {variables['fh_jog_vel_mms_var'].get()} {variables['fh_jog_accel_mms2_var'].get()} {variables['fh_jog_torque_var'].get()}")).pack()

    # Z Jog
    z_frame = ttk.Frame(btn_frame, style='TFrame')
    z_frame.grid(row=1, column=2, padx=1, sticky='ew')
    ttk.Label(btn_frame, text="Z").grid(row=0, column=2)
    ttk.Button(z_frame, text="+", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"MOVE_Z {variables['fh_jog_dist_mm_var'].get()} {variables['fh_jog_vel_mms_var'].get()} {variables['fh_jog_accel_mms2_var'].get()} {variables['fh_jog_torque_var'].get()}")).pack()
    ttk.Button(z_frame, text="-", style='Small.TButton', width=2, command=lambda: send_cmd_func(f"MOVE_Z -{variables['fh_jog_dist_mm_var'].get()} {variables['fh_jog_vel_mms_var'].get()} {variables['fh_jog_accel_mms2_var'].get()} {variables['fh_jog_torque_var'].get()}")).pack()


def create_manual_controls_display(parent, command_funcs, shared_gui_refs):
    """Creates the main container for the manual controls, with all functionality restored and themed."""
    main_container = ttk.Frame(parent, style='TFrame')
    
    # --- Initialize all missing variables ---
    if 'jog_dist_mm_var' not in shared_gui_refs: shared_gui_refs['jog_dist_mm_var'] = tk.StringVar(value="1.0")
    if 'jog_velocity_var' not in shared_gui_refs: shared_gui_refs['jog_velocity_var'] = tk.StringVar(value="5.0")
    if 'jog_acceleration_var' not in shared_gui_refs: shared_gui_refs['jog_acceleration_var'] = tk.StringVar(value="25.0")
    if 'jog_torque_percent_var' not in shared_gui_refs: shared_gui_refs['jog_torque_percent_var'] = tk.StringVar(value="20.0")
    if 'jog_pinch_dist_mm_var' not in shared_gui_refs: shared_gui_refs['jog_pinch_dist_mm_var'] = tk.StringVar(value="1.0")
    if 'fh_jog_dist_mm_var' not in shared_gui_refs: shared_gui_refs['fh_jog_dist_mm_var'] = tk.StringVar(value="10.0")
    if 'fh_jog_vel_mms_var' not in shared_gui_refs: shared_gui_refs['fh_jog_vel_mms_var'] = tk.StringVar(value="15.0")
    if 'fh_jog_accel_mms2_var' not in shared_gui_refs: shared_gui_refs['fh_jog_accel_mms2_var'] = tk.StringVar(value="50.0")
    if 'fh_jog_torque_var' not in shared_gui_refs: shared_gui_refs['fh_jog_torque_var'] = tk.StringVar(value="20")
    if 'fh_home_distance_mm_var' not in shared_gui_refs: shared_gui_refs['fh_home_distance_mm_var'] = tk.StringVar(value="1250.0")


    # --- Motor Power ---
    create_motor_power_controls(main_container, command_funcs, shared_gui_refs)
    ttk.Separator(main_container, orient='horizontal').pack(fill=tk.X, pady=10, padx=5)

    # --- HEATER (moved up for visibility) ---
    create_heater_controls(main_container, command_funcs, shared_gui_refs)
    ttk.Separator(main_container, orient='horizontal').pack(fill=tk.X, pady=8, padx=5)

    # --- JOGGING ---
    create_fillhead_jog_controls(main_container, command_funcs['send_fillhead'], shared_gui_refs)
    create_gantry_jog_controls(main_container, command_funcs['send_gantry'], shared_gui_refs)
    
    # --- HOMING ---
    homing_frame = ttk.LabelFrame(main_container, text="Homing", style='Card.TLabelframe', padding=6)
    homing_frame.pack(fill=tk.X, expand=False, pady=4, padx=6)

    # Main Homing buttons
    main_home_frame = ttk.Frame(homing_frame, style='TFrame')
    main_home_frame.pack(fill=tk.X, expand=True, pady=(0, 5))
    main_home_frame.grid_columnconfigure((0,1), weight=1)
    ttk.Button(main_home_frame, text="Machine", style='Yellow.TButton', command=lambda: command_funcs['send_fillhead']("MACHINE_HOME_MOVE")).grid(row=0, column=0, sticky='ew', padx=(0,2))
    ttk.Button(main_home_frame, text="Cartridge", style='Yellow.TButton', command=lambda: command_funcs['send_fillhead']("CARTRIDGE_HOME_MOVE")).grid(row=0, column=1, sticky='ew', padx=(2,0))

    ttk.Separator(homing_frame, orient='horizontal').pack(fill=tk.X, pady=5)

    # Pinch Valve Homing
    pv_home_frame = ttk.Frame(homing_frame, style='TFrame')
    pv_home_frame.pack(fill=tk.X, expand=True, pady=(0, 5))
    pv_home_frame.grid_columnconfigure((0,1), weight=1)
    
    ttk.Button(pv_home_frame, text="Inj\nUntubed", style='Blue.TButton', command=lambda: command_funcs['send_fillhead']("INJECTION_VALVE_HOME_UNTUBED")).grid(row=0, column=0, sticky="ew", padx=(0, 2), pady=(0,2))
    ttk.Button(pv_home_frame, text="Vac\nUntubed", style='Blue.TButton', command=lambda: command_funcs['send_fillhead']("VACUUM_VALVE_HOME_UNTUBED")).grid(row=0, column=1, sticky="ew", padx=(2,0), pady=(0,2))
    ttk.Button(pv_home_frame, text="Inj\nTubed", style='Blue.TButton', command=lambda: command_funcs['send_fillhead']("INJECTION_VALVE_HOME_TUBED")).grid(row=1, column=0, sticky="ew", padx=(0, 2))
    ttk.Button(pv_home_frame, text="Vac\nTubed", style='Blue.TButton', command=lambda: command_funcs['send_fillhead']("VACUUM_VALVE_HOME_TUBED")).grid(row=1, column=1, sticky="ew", padx=(2, 0))

    ttk.Separator(homing_frame, orient='horizontal').pack(fill=tk.X, pady=5)

    # Gantry Homing
    gantry_home_frame = ttk.Frame(homing_frame, style='TFrame')
    gantry_home_frame.pack(fill=tk.X, expand=True)
    gantry_home_frame.grid_columnconfigure((0,1,2), weight=1)
    ttk.Button(gantry_home_frame, text="X", style='Gray.TButton', command=lambda: command_funcs['send_gantry']("HOME_X")).grid(row=0, column=0, sticky='ew', padx=(0,2))
    ttk.Button(gantry_home_frame, text="Y", style='Gray.TButton', command=lambda: command_funcs['send_gantry']("HOME_Y")).grid(row=0, column=1, sticky='ew', padx=(2,2))
    ttk.Button(gantry_home_frame, text="Z", style='Gray.TButton', command=lambda: command_funcs['send_gantry']("HOME_Z")).grid(row=0, column=2, sticky='ew', padx=(2,0))

    ttk.Separator(main_container, orient='horizontal').pack(fill=tk.X, pady=10, padx=5)
    create_pinch_valve_controls(main_container, command_funcs)
    create_vacuum_controls(main_container, command_funcs, shared_gui_refs)
    
    return {'main_container': main_container}

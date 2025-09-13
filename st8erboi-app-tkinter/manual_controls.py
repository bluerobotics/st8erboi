import tkinter as tk
from tkinter import ttk
import theme


# This file now consolidates all logic for creating the manual control panels
# for both the Fillhead and the gantry devices.

# --- Motor Power Controls ---
def create_motor_power_controls(parent, command_funcs, shared_gui_refs):
    """Creates the central motor power enable/disable controls."""
    power_frame = ttk.LabelFrame(parent, text="Motor Power", style='TFrame', padding=5)
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
    frame = ttk.LabelFrame(parent, text="Pinch Valve Control", style='TFrame', padding=5)
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
    frame = ttk.LabelFrame(parent, text="Heater Control", style='TFrame', padding=5)
    frame.pack(fill=tk.X, pady=5, padx=5, anchor='n')

    # On/Off Buttons
    btn_frame = ttk.Frame(frame, style='TFrame')
    btn_frame.pack(fill=tk.X, pady=(0, 5))
    btn_frame.grid_columnconfigure((0, 1), weight=1)
    ttk.Button(btn_frame, text="Heater On", style='Green.TButton', command=lambda: send_fillhead_cmd("HEATER_ON")).grid(row=0, column=0, sticky='ew', padx=(0, 2))
    ttk.Button(btn_frame, text="Heater Off", style='Red.TButton', command=lambda: send_fillhead_cmd("HEATER_OFF")).grid(row=0, column=1, sticky='ew', padx=(2, 0))

    # Parameter Entries
    param_frame = ttk.Frame(frame, style='TFrame')
    param_frame.pack(fill=tk.X)
    param_frame.grid_columnconfigure(1, weight=1)

    # Setpoint
    ttk.Label(param_frame, text="Setpoint (°C):", style='TLabel', padding=5).grid(row=0, column=0, sticky='e', padx=(0, 5))
    entry_setpoint = ttk.Entry(param_frame, textvariable=shared_gui_refs['heater_setpoint_var'], width=8, style='TEntry')
    entry_setpoint.grid(row=0, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_HEATER_SETPOINT {shared_gui_refs['heater_setpoint_var'].get()}")).grid(row=0, column=2, sticky='ew', padx=2)

    # Gains
    ttk.Label(param_frame, text="Gains (P, I, D):", style='TLabel', padding=5).grid(row=1, column=0, sticky='e', padx=(0, 5))
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
    frame = ttk.LabelFrame(parent, text="Vacuum Control", style='TFrame', padding=5)
    frame.pack(fill=tk.X, pady=5, padx=5, anchor='n')

    # On/Off/Test Buttons
    btn_frame = ttk.Frame(frame, style='TFrame')
    btn_frame.pack(fill=tk.X, pady=(0, 5))
    btn_frame.grid_columnconfigure((0, 1, 2), weight=1)
    ttk.Button(btn_frame, text="Vacuum On", style='Green.TButton', command=lambda: send_fillhead_cmd("VACUUM_ON")).grid(row=0, column=0, sticky='ew', padx=(0, 2))
    ttk.Button(btn_frame, text="Vacuum Off", style='Red.TButton', command=lambda: send_fillhead_cmd("VACUUM_OFF")).grid(row=0, column=1, sticky='ew', padx=2)
    ttk.Button(btn_frame, text="Leak Test", style='Blue.TButton', command=lambda: send_fillhead_cmd("VACUUM_LEAK_TEST")).grid(row=0, column=2, sticky='ew', padx=(2, 0))

    # Parameter Entries
    param_frame = ttk.Frame(frame, style='TFrame')
    param_frame.pack(fill=tk.X, pady=2)
    param_frame.grid_columnconfigure(1, weight=1)

    # Target
    ttk.Label(param_frame, text="Target (PSIG):", style='TLabel', padding=5).grid(row=0, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_target_var'], width=8, style='TEntry').grid(row=0, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_VACUUM_TARGET {shared_gui_refs['vac_target_var'].get()}")).grid(row=0, column=2, sticky='ew', padx=2)

    # Timeout
    ttk.Label(param_frame, text="Timeout (s):", style='TLabel', padding=5).grid(row=1, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_timeout_var'], width=8, style='TEntry').grid(row=1, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_VACUUM_TIMEOUT_S {shared_gui_refs['vac_timeout_var'].get()}")).grid(row=1, column=2, sticky='ew', padx=2)
    
    # Leak Test Delta
    ttk.Label(param_frame, text="Leak Δ (PSIG):", style='TLabel', padding=5).grid(row=2, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_leak_delta_var'], width=8, style='TEntry').grid(row=2, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_LEAK_TEST_DELTA {shared_gui_refs['vac_leak_delta_var'].get()}")).grid(row=2, column=2, sticky='ew', padx=2)
    
    # Leak Test Duration
    ttk.Label(param_frame, text="Leak Dura. (s):", style='TLabel', padding=5).grid(row=3, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(param_frame, textvariable=shared_gui_refs['vac_leak_duration_var'], width=8, style='TEntry').grid(row=3, column=1, sticky='ew', pady=1)
    ttk.Button(param_frame, text="Set", style='Small.TButton', width=4, command=lambda: send_fillhead_cmd(f"SET_LEAK_TEST_DURATION_S {shared_gui_refs['vac_leak_duration_var'].get()}")).grid(row=3, column=2, sticky='ew', padx=2)


def create_themed_jog_panel(parent, send_cmd_func, variables, config):
    """Helper to create a standardized, themed jog panel."""
    frame = ttk.LabelFrame(parent, text=config['title'], style='TFrame', padding=10)
    frame.pack(fill=tk.X, expand=False, pady=(5, 0), padx=5)
    
    params_frame = ttk.Frame(frame, style='TFrame')
    params_frame.pack(fill=tk.X, pady=(0, 5))
    
    col_count = len(config['params'])
    params_frame.grid_columnconfigure(tuple(range(col_count * 2)), weight=1)

    for i, p_config in enumerate(config['params']):
        ttk.Label(params_frame, text=p_config['label']).grid(row=0, column=i*2, sticky="e", padx=(0, 5))
        ttk.Entry(params_frame, textvariable=variables[p_config['var']], width=8).grid(row=0, column=i*2+1, sticky="ew", padx=(0, 10))

    btn_frame = ttk.Frame(frame, style='TFrame')
    btn_frame.pack(fill=tk.X, pady=2)
    btn_frame.grid_columnconfigure(tuple(range(len(config['buttons']))), weight=1)

    for i, b_config in enumerate(config['buttons']):
        cmd = b_config['cmd_lambda'](variables)
        ttk.Button(btn_frame, text=b_config['text'], style='Small.TButton', command=lambda c=cmd: send_cmd_func(c)).grid(row=b_config['row'], column=i, sticky='ew', padx=1, pady=1)

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

    # --- JOGGING ---
    # Fillhead Jog
    fillhead_jog_config = {
        'title': "Fillhead Jog",
        'params': [
            {'label': "Dist(mm):", 'var': 'jog_dist_mm_var'},
            {'label': "Vel(mm/s):", 'var': 'jog_velocity_var'},
            {'label': "Accel(mm/s²):", 'var': 'jog_acceleration_var'},
            {'label': "Torque(%):", 'var': 'jog_torque_percent_var'}
        ],
        'buttons': [
            {'text': "▲ M0", 'row': 0, 'cmd_lambda': lambda v: f"JOG_MOVE {v['jog_dist_mm_var'].get()} 0 {v['jog_velocity_var'].get()} {v['jog_acceleration_var'].get()} {v['jog_torque_percent_var'].get()}"},
            {'text': "▲ M1", 'row': 0, 'cmd_lambda': lambda v: f"JOG_MOVE 0 {v['jog_dist_mm_var'].get()} {v['jog_velocity_var'].get()} {v['jog_acceleration_var'].get()} {v['jog_torque_percent_var'].get()}"},
            {'text': "▲ Both", 'row': 0, 'cmd_lambda': lambda v: f"JOG_MOVE {v['jog_dist_mm_var'].get()} {v['jog_dist_mm_var'].get()} {v['jog_velocity_var'].get()} {v['jog_acceleration_var'].get()} {v['jog_torque_percent_var'].get()}"},
            {'text': "▼ M0", 'row': 1, 'cmd_lambda': lambda v: f"JOG_MOVE -{v['jog_dist_mm_var'].get()} 0 {v['jog_velocity_var'].get()} {v['jog_acceleration_var'].get()} {v['jog_torque_percent_var'].get()}"},
            {'text': "▼ M1", 'row': 1, 'cmd_lambda': lambda v: f"JOG_MOVE 0 -{v['jog_dist_mm_var'].get()} {v['jog_velocity_var'].get()} {v['jog_acceleration_var'].get()} {v['jog_torque_percent_var'].get()}"},
            {'text': "▼ Both", 'row': 1, 'cmd_lambda': lambda v: f"JOG_MOVE -{v['jog_dist_mm_var'].get()} -{v['jog_dist_mm_var'].get()} {v['jog_velocity_var'].get()} {v['jog_acceleration_var'].get()} {v['jog_torque_percent_var'].get()}"},
        ]
    }
    create_themed_jog_panel(main_container, command_funcs['send_fillhead'], shared_gui_refs, fillhead_jog_config)

    # Gantry Jog
    gantry_jog_config = {
        'title': "Gantry Jog",
        'params': [
            {'label': "Dist(mm):", 'var': 'fh_jog_dist_mm_var'},
            {'label': "Speed(mm/s):", 'var': 'fh_jog_vel_mms_var'},
            {'label': "Accel(mm/s²):", 'var': 'fh_jog_accel_mms2_var'},
            {'label': "Torque(%):", 'var': 'fh_jog_torque_var'}
        ],
        'buttons': [
            {'text': "X-", 'row': 0, 'cmd_lambda': lambda v: f"MOVE_X -{v['fh_jog_dist_mm_var'].get()} {v['fh_jog_vel_mms_var'].get()} {v['fh_jog_accel_mms2_var'].get()} {v['fh_jog_torque_var'].get()}"},
            {'text': "Y-", 'row': 0, 'cmd_lambda': lambda v: f"MOVE_Y -{v['fh_jog_dist_mm_var'].get()} {v['fh_jog_vel_mms_var'].get()} {v['fh_jog_accel_mms2_var'].get()} {v['fh_jog_torque_var'].get()}"},
            {'text': "Z-", 'row': 0, 'cmd_lambda': lambda v: f"MOVE_Z -{v['fh_jog_dist_mm_var'].get()} {v['fh_jog_vel_mms_var'].get()} {v['fh_jog_accel_mms2_var'].get()} {v['fh_jog_torque_var'].get()}"},
            {'text': "X+", 'row': 1, 'cmd_lambda': lambda v: f"MOVE_X {v['fh_jog_dist_mm_var'].get()} {v['fh_jog_vel_mms_var'].get()} {v['fh_jog_accel_mms2_var'].get()} {v['fh_jog_torque_var'].get()}"},
            {'text': "Y+", 'row': 1, 'cmd_lambda': lambda v: f"MOVE_Y {v['fh_jog_dist_mm_var'].get()} {v['fh_jog_vel_mms_var'].get()} {v['fh_jog_accel_mms2_var'].get()} {v['fh_jog_torque_var'].get()}"},
            {'text': "Z+", 'row': 1, 'cmd_lambda': lambda v: f"MOVE_Z {v['fh_jog_dist_mm_var'].get()} {v['fh_jog_vel_mms_var'].get()} {v['fh_jog_accel_mms2_var'].get()} {v['fh_jog_torque_var'].get()}"},
        ]
    }
    create_themed_jog_panel(main_container, command_funcs['send_gantry'], shared_gui_refs, gantry_jog_config)
    
    # --- HOMING ---
    homing_frame = ttk.LabelFrame(main_container, text="Homing", style='TFrame', padding=10)
    homing_frame.pack(fill=tk.X, expand=False, pady=5, padx=5)

    # Main Homing buttons
    main_home_frame = ttk.Frame(homing_frame, style='TFrame')
    main_home_frame.pack(fill=tk.X, expand=True, pady=(0, 5))
    main_home_frame.grid_columnconfigure((0,1), weight=1)
    ttk.Button(main_home_frame, text="Machine Home", style='Small.TButton', command=lambda: command_funcs['send_fillhead']("MACHINE_HOME_MOVE")).grid(row=0, column=0, sticky='ew', padx=(0,2))
    ttk.Button(main_home_frame, text="Cartridge Home", style='Small.TButton', command=lambda: command_funcs['send_fillhead']("CARTRIDGE_HOME_MOVE")).grid(row=0, column=1, sticky='ew', padx=(2,0))

    ttk.Separator(homing_frame, orient='horizontal').pack(fill=tk.X, pady=5)

    # Pinch Valve Homing
    pv_home_frame = ttk.Frame(homing_frame, style='TFrame')
    pv_home_frame.pack(fill=tk.X, expand=True, pady=(0, 5))
    pv_home_frame.grid_columnconfigure((0,1), weight=1)
    
    ttk.Button(pv_home_frame, text="Home Inj (Untubed)", style='Small.TButton', command=lambda: command_funcs['send_fillhead']("INJECTION_VALVE_HOME_UNTUBED")).grid(row=0, column=0, sticky="ew", padx=(0, 2), pady=(0,2))
    ttk.Button(pv_home_frame, text="Home Vac (Untubed)", style='Small.TButton', command=lambda: command_funcs['send_fillhead']("VACUUM_VALVE_HOME_UNTUBED")).grid(row=0, column=1, sticky="ew", padx=(2,0), pady=(0,2))
    ttk.Button(pv_home_frame, text="Home Inj (Tubed)", style='Small.TButton', command=lambda: command_funcs['send_fillhead']("INJECTION_VALVE_HOME_TUBED")).grid(row=1, column=0, sticky="ew", padx=(0, 2))
    ttk.Button(pv_home_frame, text="Home Vac (Tubed)", style='Small.TButton', command=lambda: command_funcs['send_fillhead']("VACUUM_VALVE_HOME_TUBED")).grid(row=1, column=1, sticky="ew", padx=(2, 0))

    ttk.Separator(homing_frame, orient='horizontal').pack(fill=tk.X, pady=5)

    # Gantry Homing
    gantry_home_frame = ttk.Frame(homing_frame, style='TFrame')
    gantry_home_frame.pack(fill=tk.X, expand=True)
    gantry_home_frame.grid_columnconfigure((0,1,2), weight=1)
    ttk.Button(gantry_home_frame, text="Home X", style='Small.TButton', command=lambda: command_funcs['send_gantry'](f"HOME_X {shared_gui_refs['fh_home_distance_mm_var'].get()}")).grid(row=0, column=0, sticky='ew', padx=(0,2))
    ttk.Button(gantry_home_frame, text="Home Y", style='Small.TButton', command=lambda: command_funcs['send_gantry'](f"HOME_Y {shared_gui_refs['fh_home_distance_mm_var'].get()}")).grid(row=0, column=1, sticky='ew', padx=(2,2))
    ttk.Button(gantry_home_frame, text="Home Z", style='Small.TButton', command=lambda: command_funcs['send_gantry'](f"HOME_Z {shared_gui_refs['fh_home_distance_mm_var'].get()}")).grid(row=0, column=2, sticky='ew', padx=(2,0))

    ttk.Separator(main_container, orient='horizontal').pack(fill=tk.X, pady=10, padx=5)
    create_pinch_valve_controls(main_container, command_funcs)
    create_heater_controls(main_container, command_funcs, shared_gui_refs)
    create_vacuum_controls(main_container, command_funcs, shared_gui_refs)
    
    return {'main_container': main_container}

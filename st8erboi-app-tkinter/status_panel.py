import tkinter as tk
from tkinter import ttk
import theme

def create_status_bar(parent, shared_gui_refs):
    """
    Creates the vertically stacked left status bar with Connection, Axis, and System status.
    """
    status_bar_container = ttk.Frame(parent, style='TFrame')
    status_bar_container.pack(side=tk.TOP, fill=tk.X, expand=False)

    # --- Connection Status ---
    conn_frame = ttk.LabelFrame(status_bar_container, text="Connection Status", style='TFrame', padding=10)
    conn_frame.pack(side=tk.TOP, fill="x", pady=(0, 10))
    
    gantry_conn_label = ttk.Label(conn_frame, textvariable=shared_gui_refs['status_var_gantry'])
    gantry_conn_label.pack(anchor='w')
    
    fillhead_conn_label = ttk.Label(conn_frame, textvariable=shared_gui_refs['status_var_fillhead'])
    fillhead_conn_label.pack(anchor='w')

    # Dynamic status coloring
    shared_gui_refs['status_var_gantry'].trace_add("write", 
        lambda *args: gantry_conn_label.config(
            foreground=theme.SUCCESS_GREEN if "Connected" in shared_gui_refs['status_var_gantry'].get() else theme.ERROR_RED
        )
    )
    shared_gui_refs['status_var_fillhead'].trace_add("write", 
        lambda *args: fillhead_conn_label.config(
            foreground=theme.SUCCESS_GREEN if "Connected" in shared_gui_refs['status_var_fillhead'].get() else theme.ERROR_RED
        )
    )

    # --- Axis Status Display ---
    axis_status_frame = ttk.Frame(status_bar_container, style='TFrame', padding=10)
    axis_status_frame.pack(side=tk.TOP, fill="x", expand=True, pady=(0, 10))

    # --- Font Definitions ---
    font_large_readout = ("JetBrains Mono", 28, "bold")
    font_medium_readout = ("JetBrains Mono", 16, "bold")
    font_small_readout = ("JetBrains Mono", 14, "bold")
    bar_height = 55
    small_bar_height = 40

    # --- Helper Functions ---
    def make_homed_tracer(var, label_to_color):
        def tracer(*args):
            is_homed = var.get() == "Homed"
            color = theme.SUCCESS_GREEN if is_homed else theme.ERROR_RED
            label_to_color.config(foreground=color)
        return tracer

    def make_torque_tracer(double_var, string_var):
        def tracer(*args):
            try:
                val = double_var.get()
                string_var.set(f"{int(val)}%")
            except (tk.TclError, ValueError):
                string_var.set("ERR")
        return tracer

    def create_torque_widget(parent, torque_dv, height):
        torque_frame = ttk.Frame(parent, height=height, width=30, style='TFrame')
        torque_frame.pack_propagate(False)
        torque_sv = tk.StringVar()
        torque_frame.tracer = make_torque_tracer(torque_dv, torque_sv)
        torque_dv.trace_add('write', torque_frame.tracer)
        pbar = ttk.Progressbar(torque_frame, variable=torque_dv, maximum=100, orient=tk.VERTICAL)
        pbar.pack(fill=tk.BOTH, expand=True)
        label = ttk.Label(torque_frame, textvariable=torque_sv, font=("JetBrains Mono", 7, "bold"), anchor='center')
        label.place(relx=0.5, rely=0.5, anchor='center')
        torque_frame.tracer()
        return torque_frame

    # --- Gantry Axes (X, Y, Z) ---
    gantry_axes_data = [
        {'label': 'X', 'pos_var': 'fh_pos_m0_var', 'homed_var': 'fh_homed_m0_var', 'torque_var': 'fh_torque_m0_var'},
        {'label': 'Y', 'pos_var': 'fh_pos_m1_var', 'homed_var': 'fh_homed_m1_var', 'torque_var': 'fh_torque_m1_var'},
        {'label': 'Z', 'pos_var': 'fh_pos_m3_var', 'homed_var': 'fh_homed_m3_var', 'torque_var': 'fh_torque_m3_var'}
    ]

    for axis_info in gantry_axes_data:
        axis_frame = ttk.Frame(axis_status_frame, style='TFrame')
        axis_frame.pack(anchor='w', pady=4, fill='x')
        axis_frame.grid_columnconfigure(1, weight=1)

        axis_label = ttk.Label(axis_frame, text=f"{axis_info['label']}:", width=5, anchor='w', font=font_large_readout)
        axis_label.grid(row=0, column=0, sticky='w', padx=(0, 5))

        ttk.Label(axis_frame, textvariable=shared_gui_refs[axis_info['pos_var']], font=font_large_readout, anchor='e').grid(row=0, column=1, sticky='ew', padx=(0, 10))
        
        torque_widget = create_torque_widget(axis_frame, shared_gui_refs[axis_info['torque_var']], bar_height)
        torque_widget.grid(row=0, column=2, sticky='ns')

        homed_var = shared_gui_refs[axis_info['homed_var']]
        axis_label.tracer = make_homed_tracer(homed_var, axis_label)
        homed_var.trace_add('write', axis_label.tracer)
        axis_label.tracer()

    # --- Injector Distance Display (No border) ---
    injector_dist_frame = ttk.Frame(axis_status_frame, style='TFrame')
    injector_dist_frame.pack(fill='x', pady=(10, 5))
    injector_dist_frame.grid_columnconfigure(1, weight=1)

    machine_label = ttk.Label(injector_dist_frame, text="Machine:", font=font_medium_readout)
    machine_label.grid(row=0, column=0, sticky='w')
    ttk.Label(injector_dist_frame, textvariable=shared_gui_refs['machine_steps_var'], font=font_medium_readout, anchor='e').grid(row=0, column=1, sticky='ew', padx=5)

    cartridge_label = ttk.Label(injector_dist_frame, text="Cartridge:", font=font_medium_readout)
    cartridge_label.grid(row=1, column=0, sticky='w')
    ttk.Label(injector_dist_frame, textvariable=shared_gui_refs['cartridge_steps_var'], font=font_medium_readout, anchor='e').grid(row=1, column=1, sticky='ew', padx=5)

    injector_torque_widget = create_torque_widget(injector_dist_frame, shared_gui_refs['torque0_var'], bar_height)
    injector_torque_widget.grid(row=0, column=2, rowspan=2, sticky='ns', padx=(10, 0))

    # Create and attach independent tracers for machine and cartridge homing status.
    machine_homed_var = shared_gui_refs['homed0_var']
    machine_label.tracer = make_homed_tracer(machine_homed_var, machine_label)
    machine_homed_var.trace_add('write', machine_label.tracer)
    machine_label.tracer()

    cartridge_homed_var = shared_gui_refs['homed1_var']
    cartridge_label.tracer = make_homed_tracer(cartridge_homed_var, cartridge_label)
    cartridge_homed_var.trace_add('write', cartridge_label.tracer)
    cartridge_label.tracer()

    # --- Pinch Axes ---
    pinch_axes_data = [
        {'label': 'Inj Valve', 'pos_var': 'inj_valve_pos_var', 'homed_var': 'inj_valve_homed_var', 'torque_var': 'torque2_var'},
        {'label': 'Vac Valve', 'pos_var': 'vac_valve_pos_var', 'homed_var': 'vac_valve_homed_var', 'torque_var': 'torque3_var'},
    ]

    for axis_info in pinch_axes_data:
        axis_frame = ttk.Frame(axis_status_frame, style='TFrame')
        axis_frame.pack(anchor='w', pady=2, fill='x')
        axis_frame.grid_columnconfigure(1, weight=1)

        axis_label = ttk.Label(axis_frame, text=f"{axis_info['label']}:", anchor='w', font=font_small_readout)
        axis_label.grid(row=0, column=0, sticky='w', padx=(0, 5))

        ttk.Label(axis_frame, textvariable=shared_gui_refs[axis_info['pos_var']], font=font_small_readout, anchor='e').grid(row=0, column=1, sticky='ew', padx=(0, 10))

        torque_widget = create_torque_widget(axis_frame, shared_gui_refs[axis_info['torque_var']], small_bar_height)
        torque_widget.grid(row=0, column=2, sticky='ns')

        homed_var = shared_gui_refs[axis_info['homed_var']]
        axis_label.tracer = make_homed_tracer(homed_var, axis_label)
        homed_var.trace_add('write', axis_label.tracer)
        axis_label.tracer()

    # --- Consolidated System Status Display ---
    sys_status_frame = ttk.Frame(status_bar_container, style='TFrame', padding=10)
    sys_status_frame.pack(side=tk.TOP, fill="x", expand=True, pady=(10, 0))
    status_grid = ttk.Frame(sys_status_frame, style='TFrame')
    status_grid.pack(fill="x", expand=True)
    status_grid.grid_columnconfigure(1, weight=1)
    
    font_status_label = theme.FONT_NORMAL
    font_status_value = theme.FONT_BOLD
    font_state_value = ("JetBrains Mono", 14, "bold")

    ttk.Label(status_grid, text="Fillhead State:", font=font_status_label).grid(row=0, column=0, sticky='e', padx=5, pady=3)
    ttk.Label(status_grid, textvariable=shared_gui_refs['main_state_var'], foreground=theme.PRIMARY_ACCENT, font=font_state_value).grid(row=0, column=1, sticky='w')
    
    ttk.Label(status_grid, text="Gantry State:", font=font_status_label).grid(row=1, column=0, sticky='e', padx=5, pady=3)
    ttk.Label(status_grid, textvariable=shared_gui_refs['fh_state_var'], foreground=theme.WARNING_YELLOW, font=font_state_value).grid(row=1, column=1, sticky='w')
    
    # --- Gantry Axis States ---
    gantry_states_frame = ttk.Frame(status_grid, style='TFrame')
    gantry_states_frame.grid(row=2, column=0, columnspan=2, sticky='ew', pady=2, padx=20)
    gantry_states_frame.grid_columnconfigure((0, 1, 2), weight=1)
    
    ttk.Label(gantry_states_frame, text="X:", foreground=theme.WARNING_YELLOW).grid(row=0, column=0, sticky='e')
    ttk.Label(gantry_states_frame, textvariable=shared_gui_refs['fh_state_x_var']).grid(row=0, column=1, sticky='w')
    ttk.Label(gantry_states_frame, text="Y:", foreground=theme.WARNING_YELLOW).grid(row=1, column=0, sticky='e')
    ttk.Label(gantry_states_frame, textvariable=shared_gui_refs['fh_state_y_var']).grid(row=1, column=1, sticky='w')
    ttk.Label(gantry_states_frame, text="Z:", foreground=theme.WARNING_YELLOW).grid(row=2, column=0, sticky='e')
    ttk.Label(gantry_states_frame, textvariable=shared_gui_refs['fh_state_z_var']).grid(row=2, column=1, sticky='w')

    ttk.Separator(status_grid, orient='horizontal').grid(row=3, column=0, columnspan=2, sticky='ew', pady=8)
    
    # --- Fillhead Component States ---
    fillhead_states_frame = ttk.Frame(status_grid, style='TFrame')
    fillhead_states_frame.grid(row=4, column=0, columnspan=2, sticky='ew', pady=(2, 0), padx=20)
    fillhead_states_frame.grid_columnconfigure(1, weight=1)

    # Data for fillhead components
    fillhead_components = [
        ("Injector:", 'fillhead_injector_state_var'),
        ("Inject Valve:", 'fillhead_inj_valve_state_var'),
        ("Vac Valve:", 'fillhead_vac_valve_state_var'),
        ("Heater:", 'fillhead_heater_state_var'),
        ("Vacuum:", 'fillhead_vacuum_state_var')
    ]

    for i, (label_text, var_key) in enumerate(fillhead_components):
        ttk.Label(fillhead_states_frame, text=label_text, foreground=theme.PRIMARY_ACCENT).grid(row=i, column=0, sticky='e', padx=(0, 5))
        ttk.Label(fillhead_states_frame, textvariable=shared_gui_refs[var_key]).grid(row=i, column=1, sticky='w')

    ttk.Separator(status_grid, orient='horizontal').grid(row=5, column=0, columnspan=2, sticky='ew', pady=8)

    # System Readouts
    readouts = [
        ("Vacuum:", 'vacuum_psig_var', theme.PRIMARY_ACCENT),
        ("Heater Temp:", 'temp_c_var', theme.WARNING_YELLOW),
        ("Dispensed (Active):", 'inject_active_ml_var', theme.SUCCESS_GREEN),
        ("Dispensed (Total):", 'inject_cumulative_ml_var', theme.FG_COLOR)
    ]

    for i, (label_text, var_key, color) in enumerate(readouts):
        ttk.Label(status_grid, text=label_text, font=font_status_label).grid(row=i + 6, column=0, sticky='e', padx=5, pady=2)
        ttk.Label(status_grid, textvariable=shared_gui_refs[var_key], foreground=color, font=font_status_value).grid(row=i + 6, column=1, sticky='w')

    return status_bar_container

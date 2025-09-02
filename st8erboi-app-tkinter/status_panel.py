import tkinter as tk
from tkinter import ttk


def create_status_bar(parent, shared_gui_refs):
    """
    Creates the vertically stacked left status bar with Connection, Axis, and System status.
    """
    status_bar_container = tk.Frame(parent, bg="#21232b")
    status_bar_container.pack(side=tk.TOP, fill=tk.X, expand=False)

    # --- Connection Status ---
    conn_frame = tk.LabelFrame(status_bar_container, text="Connection Status", bg="#2a2d3b", fg="white", padx=10,
                               pady=5, font=("Segoe UI", 10, "bold"))
    conn_frame.pack(side=tk.TOP, fill="x", pady=(0, 10))
    # MODIFIED: Changed 'status_var_injector' to 'status_var_fillhead' to fix the KeyError.
    tk.Label(conn_frame, textvariable=shared_gui_refs['status_var_fillhead'], bg=conn_frame['bg'], fg="white",
             font=("Segoe UI", 9)).pack(anchor='w')
    tk.Label(conn_frame, textvariable=shared_gui_refs['status_var_gantry'], bg=conn_frame['bg'], fg="white",
             font=("Segoe UI", 9)).pack(anchor='w')

    # --- Axis Status Display ---
    axis_status_frame = tk.LabelFrame(status_bar_container, text="Axis Status", bg="#2a2d3b", fg="white", padx=10,
                                      pady=10, font=("Segoe UI", 10, "bold"))
    axis_status_frame.pack(side=tk.TOP, fill="x", expand=True, pady=(0, 10))

    # --- Font and Size Definitions ---
    font_large_readout = ("Consolas", 32, "bold")
    font_medium_readout = ("Consolas", 18, "bold")
    font_small_readout = ("Consolas", 16, "bold")
    bar_height = 55
    small_bar_height = 40

    # --- Helper Functions ---
    def make_homed_tracer(var, label_to_color):
        def tracer(*args):
            is_homed = var.get() == "Homed"
            color = "lightgreen" if is_homed else "#db2828"
            label_to_color.config(fg=color)

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
        torque_frame = tk.Frame(parent, bg=parent['bg'], height=height, width=30)
        torque_frame.pack_propagate(False)
        torque_sv = tk.StringVar()
        torque_frame.tracer = make_torque_tracer(torque_dv, torque_sv)
        torque_dv.trace_add('write', torque_frame.tracer)
        pbar = ttk.Progressbar(torque_frame, variable=torque_dv, maximum=100, orient=tk.VERTICAL,
                               style="Green.Vertical.TProgressbar")
        pbar.pack(fill=tk.BOTH, expand=True)
        label = tk.Label(torque_frame, textvariable=torque_sv, bg="#333745", fg="white", font=("Segoe UI", 7, "bold"))
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
        axis_frame = tk.Frame(axis_status_frame, bg=axis_status_frame['bg'])
        axis_frame.pack(anchor='w', pady=4, fill='x')
        axis_frame.grid_columnconfigure(1, weight=1)

        axis_label = tk.Label(axis_frame, text=f"{axis_info['label']}:", width=5, anchor='w',
                              bg=axis_status_frame['bg'], font=font_large_readout)
        axis_label.grid(row=0, column=0, sticky='w', padx=(0, 5))

        tk.Label(axis_frame, textvariable=shared_gui_refs[axis_info['pos_var']], bg=axis_status_frame['bg'], fg="white",
                 font=font_large_readout, anchor='e').grid(row=0, column=1, sticky='ew', padx=(0, 10))

        torque_widget = create_torque_widget(axis_frame, shared_gui_refs[axis_info['torque_var']], bar_height)
        torque_widget.grid(row=0, column=2, sticky='ns')

        homed_var = shared_gui_refs[axis_info['homed_var']]
        axis_label.tracer = make_homed_tracer(homed_var, axis_label)
        homed_var.trace_add('write', axis_label.tracer)
        axis_label.tracer()

    # --- Injector Distance Display (No border) ---
    injector_dist_frame = tk.Frame(axis_status_frame, bg="#2a2d3b")
    injector_dist_frame.pack(fill='x', pady=(10, 5))
    injector_dist_frame.grid_columnconfigure(1, weight=1)

    machine_label = tk.Label(injector_dist_frame, text="Machine:", bg=injector_dist_frame['bg'],
                             font=font_medium_readout)
    machine_label.grid(row=0, column=0, sticky='w')
    tk.Label(injector_dist_frame, textvariable=shared_gui_refs['machine_steps_var'], bg=injector_dist_frame['bg'],
             fg='white', font=font_medium_readout, anchor='e').grid(row=0, column=1, sticky='ew', padx=5)

    cartridge_label = tk.Label(injector_dist_frame, text="Cartridge:", bg=injector_dist_frame['bg'],
                               font=font_medium_readout)
    cartridge_label.grid(row=1, column=0, sticky='w')
    tk.Label(injector_dist_frame, textvariable=shared_gui_refs['inject_dispensed_ml_var'], bg=injector_dist_frame['bg'],
             fg='white', font=font_medium_readout, anchor='e').grid(row=1, column=1, sticky='ew', padx=5)

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
    # MODIFIED: Updated to use new, specific variable names for pinch valves.
    pinch_axes_data = [
        {'label': 'Inj Valve', 'pos_var': 'inj_valve_pos_var', 'homed_var': 'inj_valve_homed_var',
         'torque_var': 'torque2_var'},
        {'label': 'Vac Valve', 'pos_var': 'vac_valve_pos_var', 'homed_var': 'vac_valve_homed_var',
         'torque_var': 'torque3_var'},
    ]

    for axis_info in pinch_axes_data:
        axis_frame = tk.Frame(axis_status_frame, bg=axis_status_frame['bg'])
        axis_frame.pack(anchor='w', pady=2, fill='x')
        axis_frame.grid_columnconfigure(1, weight=1)

        axis_label = tk.Label(axis_frame, text=f"{axis_info['label']}:", anchor='w', bg=axis_status_frame['bg'],
                              font=font_small_readout)
        axis_label.grid(row=0, column=0, sticky='w', padx=(0, 5))

        tk.Label(axis_frame, textvariable=shared_gui_refs[axis_info['pos_var']], bg=axis_status_frame['bg'], fg="white",
                 font=font_small_readout, anchor='e').grid(row=0, column=1, sticky='ew', padx=(0, 10))

        torque_widget = create_torque_widget(axis_frame, shared_gui_refs[axis_info['torque_var']], small_bar_height)
        torque_widget.grid(row=0, column=2, sticky='ns')

        homed_var = shared_gui_refs[axis_info['homed_var']]
        axis_label.tracer = make_homed_tracer(homed_var, axis_label)
        homed_var.trace_add('write', axis_label.tracer)
        axis_label.tracer()

    # --- Consolidated System Status Display ---
    sys_status_frame = tk.LabelFrame(status_bar_container, text="System Status", bg="#2a2d3b", fg="white", padx=10,
                                     pady=10, font=("Segoe UI", 10, "bold"))
    sys_status_frame.pack(side=tk.TOP, fill="x", expand=True, pady=(10, 0))
    status_grid = tk.Frame(sys_status_frame, bg=sys_status_frame['bg'])
    status_grid.pack(fill="x", expand=True)
    status_grid.grid_columnconfigure(1, weight=1)
    font_status_label = ("Segoe UI", 10)
    font_status_value = ("Segoe UI", 10, "bold")
    font_state_value = ("Segoe UI", 14, "bold")

    tk.Label(status_grid, text="Fillhead State:", bg=sys_status_frame['bg'], fg="white", font=font_status_label).grid(
        row=0, column=0, sticky='e', padx=5, pady=3)
    tk.Label(status_grid, textvariable=shared_gui_refs['main_state_var'], bg=sys_status_frame['bg'], fg="cyan",
             font=font_state_value).grid(row=0, column=1, sticky='w')
    tk.Label(status_grid, text="Gantry State:", bg=sys_status_frame['bg'], fg="white", font=font_status_label).grid(
        row=1, column=0, sticky='e', padx=5, pady=3)
    tk.Label(status_grid, textvariable=shared_gui_refs['fh_state_var'], bg=sys_status_frame['bg'], fg="yellow",
             font=font_state_value).grid(row=1, column=1, sticky='w')
    ttk.Separator(status_grid, orient='horizontal').grid(row=2, column=0, columnspan=2, sticky='ew', pady=8)
    tk.Label(status_grid, text="Vacuum:", bg=sys_status_frame['bg'], fg="white", font=font_status_label).grid(row=3,
                                                                                                              column=0,
                                                                                                              sticky='e',
                                                                                                              padx=5,
                                                                                                              pady=2)
    tk.Label(status_grid, textvariable=shared_gui_refs['vacuum_psig_var'], bg=sys_status_frame['bg'], fg="#aaddff",
             font=font_status_value).grid(row=3, column=1, sticky='w')
    tk.Label(status_grid, text="Heater Temp:", bg=sys_status_frame['bg'], fg="white", font=font_status_label).grid(
        row=4, column=0, sticky='e', padx=5, pady=2)
    tk.Label(status_grid, textvariable=shared_gui_refs['temp_c_var'], bg=sys_status_frame['bg'], fg="orange",
             font=font_status_value).grid(row=4, column=1, sticky='w')
    tk.Label(status_grid, text="Dispensed (ml):", bg=sys_status_frame['bg'], fg="white", font=font_status_label).grid(
        row=5, column=0, sticky='e', padx=5, pady=2)
    tk.Label(status_grid, textvariable=shared_gui_refs['cartridge_steps_var'], bg=sys_status_frame['bg'],
             fg="lightgreen", font=font_status_value).grid(row=5, column=1, sticky='w')

    return status_bar_container

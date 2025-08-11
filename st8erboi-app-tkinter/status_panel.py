import tkinter as tk
from tkinter import ttk


def create_status_bar(parent, shared_gui_refs):
    """
    Creates the vertically stacked left status bar with Connection, Axis, and System status.
    """
    status_bar_container = tk.Frame(parent, bg="#21232b")
    status_bar_container.pack(side=tk.TOP, fill=tk.X, expand=False)

    # --- Initialize required StringVars ---
    if 'fh_pos_m0_var' not in shared_gui_refs: shared_gui_refs['fh_pos_m0_var'] = tk.StringVar(value="0.00")
    if 'fh_pos_m1_var' not in shared_gui_refs: shared_gui_refs['fh_pos_m1_var'] = tk.StringVar(value="0.00")
    if 'fh_pos_m3_var' not in shared_gui_refs: shared_gui_refs['fh_pos_m3_var'] = tk.StringVar(value="0.00")
    if 'pinch_pos_mm_var' not in shared_gui_refs: shared_gui_refs['pinch_pos_mm_var'] = tk.StringVar(value="---")
    if 'homed0_var' not in shared_gui_refs: shared_gui_refs['homed0_var'] = tk.StringVar(value="Not Homed")
    if 'homed1_var' not in shared_gui_refs: shared_gui_refs['homed1_var'] = tk.StringVar(value="Not Homed")
    if 'fh_homed_m0_var' not in shared_gui_refs: shared_gui_refs['fh_homed_m0_var'] = tk.StringVar(value="Not Homed")
    if 'fh_homed_m1_var' not in shared_gui_refs: shared_gui_refs['fh_homed_m1_var'] = tk.StringVar(value="Not Homed")
    if 'fh_homed_m3_var' not in shared_gui_refs: shared_gui_refs['fh_homed_m3_var'] = tk.StringVar(value="Not Homed")
    if 'pinch_homed_var' not in shared_gui_refs: shared_gui_refs['pinch_homed_var'] = tk.StringVar(value="Not Homed")
    if 'main_state_var' not in shared_gui_refs: shared_gui_refs['main_state_var'] = tk.StringVar(value='---')
    if 'fh_state_var' not in shared_gui_refs: shared_gui_refs['fh_state_var'] = tk.StringVar(value='Idle')
    if 'vacuum_psig_var' not in shared_gui_refs: shared_gui_refs['vacuum_psig_var'] = tk.StringVar(value='---')
    if 'cartridge_steps_var' not in shared_gui_refs: shared_gui_refs['cartridge_steps_var'] = tk.StringVar(value='---')
    if 'temp_c_var' not in shared_gui_refs: shared_gui_refs['temp_c_var'] = tk.StringVar(value='---')
    if 'machine_steps_var' not in shared_gui_refs: shared_gui_refs['machine_steps_var'] = tk.StringVar(value='---')
    if 'inject_dispensed_ml_var' not in shared_gui_refs: shared_gui_refs['inject_dispensed_ml_var'] = tk.StringVar(
        value='---')
    if 'vac_pinch_pos_mm_var' not in shared_gui_refs: shared_gui_refs['vac_pinch_pos_mm_var'] = tk.StringVar(
        value='---')
    if 'vac_pinch_homed_var' not in shared_gui_refs: shared_gui_refs['vac_pinch_homed_var'] = tk.StringVar(
        value='Not Homed')
    if 'torque3_var' not in shared_gui_refs: shared_gui_refs['torque3_var'] = tk.DoubleVar(value=0.0)

    # --- Connection Status ---
    conn_frame = tk.LabelFrame(status_bar_container, text="Connection Status", bg="#2a2d3b", fg="white", padx=10,
                               pady=5, font=("Segoe UI", 10, "bold"))
    conn_frame.pack(side=tk.TOP, fill="x", pady=(0, 10))
    tk.Label(conn_frame, textvariable=shared_gui_refs['status_var_injector'], bg=conn_frame['bg'], fg="white",
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

    def make_injector_axis_tracer(var1, var2, labels_to_color):
        def tracer(*args):
            is_homed = var1.get() == "Homed" and var2.get() == "Homed"
            color = "lightgreen" if is_homed else "#db2828"
            for label in labels_to_color:
                label.config(fg=color)

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

    # Shared torque bar for the injector axis
    injector_torque_widget = create_torque_widget(injector_dist_frame, shared_gui_refs['torque0_var'], bar_height)
    injector_torque_widget.grid(row=0, column=2, rowspan=2, sticky='ns', padx=(10, 0))

    # Shared homing tracer for the injector axis labels
    injector_tracer = make_injector_axis_tracer(shared_gui_refs['homed0_var'], shared_gui_refs['homed1_var'],
                                                [machine_label, cartridge_label])
    shared_gui_refs['homed0_var'].trace_add('write', injector_tracer)
    shared_gui_refs['homed1_var'].trace_add('write', injector_tracer)
    injector_tracer()

    # --- Pinch Axes ---
    pinch_axes_data = [
        {'label': 'Fill Pinch', 'pos_var': 'pinch_pos_mm_var', 'homed_var': 'pinch_homed_var',
         'torque_var': 'torque2_var'},
        {'label': 'Vac Pinch', 'pos_var': 'vac_pinch_pos_mm_var', 'homed_var': 'vac_pinch_homed_var',
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

    tk.Label(status_grid, text="Injector State:", bg=sys_status_frame['bg'], fg="white", font=font_status_label).grid(
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
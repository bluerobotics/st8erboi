import tkinter as tk
from tkinter import ttk

def create_status_bar(parent, shared_gui_refs):
    """
    Creates the vertically stacked left status bar with Connection, Fillhead, and System status.
    """
    status_bar_container = tk.Frame(parent, bg="#21232b")
    status_bar_container.pack(side=tk.TOP, fill=tk.X, expand=False)

    # --- Initialize required StringVars ---
    if 'fh_pos_m0_var' not in shared_gui_refs: shared_gui_refs['fh_pos_m0_var'] = tk.StringVar(value="0.00")
    if 'fh_pos_m1_var' not in shared_gui_refs: shared_gui_refs['fh_pos_m1_var'] = tk.StringVar(value="0.00")
    if 'fh_pos_m3_var' not in shared_gui_refs: shared_gui_refs['fh_pos_m3_var'] = tk.StringVar(value="0.00")
    if 'fh_homed_m0_var' not in shared_gui_refs: shared_gui_refs['fh_homed_m0_var'] = tk.StringVar(value="Not Homed")
    if 'fh_homed_m1_var' not in shared_gui_refs: shared_gui_refs['fh_homed_m1_var'] = tk.StringVar(value="Not Homed")
    if 'fh_homed_m3_var' not in shared_gui_refs: shared_gui_refs['fh_homed_m3_var'] = tk.StringVar(value="Not Homed")
    if 'main_state_var' not in shared_gui_refs: shared_gui_refs['main_state_var'] = tk.StringVar(value='---')
    if 'fh_state_var' not in shared_gui_refs: shared_gui_refs['fh_state_var'] = tk.StringVar(value='Idle')
    if 'vacuum_psig_var' not in shared_gui_refs: shared_gui_refs['vacuum_psig_var'] = tk.StringVar(value='---')
    if 'cartridge_steps_var' not in shared_gui_refs: shared_gui_refs['cartridge_steps_var'] = tk.StringVar(value='---')
    if 'temp_c_var' not in shared_gui_refs: shared_gui_refs['temp_c_var'] = tk.StringVar(value='---')

    # --- Connection Status ---
    conn_frame = tk.LabelFrame(status_bar_container, text="Connection Status", bg="#2a2d3b", fg="white", padx=10, pady=5, font=("Segoe UI", 10, "bold"))
    conn_frame.pack(side=tk.TOP, fill="x", pady=(0, 10))
    tk.Label(conn_frame, textvariable=shared_gui_refs['status_var_injector'], bg=conn_frame['bg'], fg="white", font=("Segoe UI", 9)).pack(anchor='w')
    tk.Label(conn_frame, textvariable=shared_gui_refs['status_var_fillhead'], bg=conn_frame['bg'], fg="white", font=("Segoe UI", 9)).pack(anchor='w')

    # --- Large Fillhead Position Display ---
    fh_frame = tk.LabelFrame(status_bar_container, text="Fillhead Position (mm)", bg="#2a2d3b", fg="white", padx=10, pady=10, font=("Segoe UI", 10, "bold"))
    fh_frame.pack(side=tk.TOP, fill="x", expand=True, pady=(0, 10))

    font_large_readout = ("Consolas", 36, "bold")
    font_homed_status = ("Segoe UI", 10, "italic")

    for i, axis in enumerate(['X', 'Y', 'Z']):
        motor_index = i if i < 2 else 3
        color = ['cyan', 'yellow', '#ff8888'][i]
        axis_frame = tk.Frame(fh_frame, bg=fh_frame['bg'])
        axis_frame.pack(anchor='w', pady=2, fill='x')
        pos_frame = tk.Frame(axis_frame, bg=fh_frame['bg'])
        pos_frame.pack(side=tk.LEFT, fill='x', expand=True)
        tk.Label(pos_frame, text=f"{axis}:", bg=fh_frame['bg'], fg=color, font=font_large_readout).pack(side=tk.LEFT)
        tk.Label(pos_frame, textvariable=shared_gui_refs[f'fh_pos_m{motor_index}_var'], bg=fh_frame['bg'], fg="white", font=font_large_readout, width=6, anchor='e').pack(side=tk.LEFT, expand=True, fill='x')
        homed_label = tk.Label(axis_frame, textvariable=shared_gui_refs[f'fh_homed_m{motor_index}_var'], bg=fh_frame['bg'], font=font_homed_status, padx=10)
        homed_label.pack(side=tk.RIGHT)

        def make_tracer(var, label):
            def tracer(*args):
                label.config(fg="lightgreen" if var.get() == "Homed" else "orangered")
            return tracer
        var_name = f'fh_homed_m{motor_index}_var'
        if var_name in shared_gui_refs:
            var = shared_gui_refs[var_name]
            var.trace_add('write', make_tracer(var, homed_label))
            make_tracer(var, homed_label)()

    # --- Consolidated System Status Display ---
    sys_status_frame = tk.LabelFrame(status_bar_container, text="System Status", bg="#2a2d3b", fg="white", padx=10, pady=10, font=("Segoe UI", 10, "bold"))
    sys_status_frame.pack(side=tk.TOP, fill="x", expand=True)
    status_grid = tk.Frame(sys_status_frame, bg=sys_status_frame['bg'])
    status_grid.pack(fill="x", expand=True)
    status_grid.grid_columnconfigure(1, weight=1)
    font_status_label = ("Segoe UI", 10)
    font_status_value = ("Segoe UI", 10, "bold")
    font_state_value = ("Segoe UI", 14, "bold")

    tk.Label(status_grid, text="Injector State:", bg=sys_status_frame['bg'], fg="white", font=font_status_label).grid(row=0, column=0, sticky='e', padx=5, pady=3)
    tk.Label(status_grid, textvariable=shared_gui_refs['main_state_var'], bg=sys_status_frame['bg'], fg="cyan", font=font_state_value).grid(row=0, column=1, sticky='w')
    tk.Label(status_grid, text="Fillhead State:", bg=sys_status_frame['bg'], fg="white", font=font_status_label).grid(row=1, column=0, sticky='e', padx=5, pady=3)
    tk.Label(status_grid, textvariable=shared_gui_refs['fh_state_var'], bg=sys_status_frame['bg'], fg="yellow", font=font_state_value).grid(row=1, column=1, sticky='w')
    ttk.Separator(status_grid, orient='horizontal').grid(row=2, column=0, columnspan=2, sticky='ew', pady=8)
    tk.Label(status_grid, text="Vacuum:", bg=sys_status_frame['bg'], fg="white", font=font_status_label).grid(row=3, column=0, sticky='e', padx=5, pady=2)
    tk.Label(status_grid, textvariable=shared_gui_refs['vacuum_psig_var'], bg=sys_status_frame['bg'], fg="#aaddff", font=font_status_value).grid(row=3, column=1, sticky='w')
    tk.Label(status_grid, text="Heater Temp:", bg=sys_status_frame['bg'], fg="white", font=font_status_label).grid(row=4, column=0, sticky='e', padx=5, pady=2)
    tk.Label(status_grid, textvariable=shared_gui_refs['temp_c_var'], bg=sys_status_frame['bg'], fg="orange", font=font_status_value).grid(row=4, column=1, sticky='w')
    tk.Label(status_grid, text="Dispensed (ml):", bg=sys_status_frame['bg'], fg="white", font=font_status_label).grid(row=5, column=0, sticky='e', padx=5, pady=2)
    tk.Label(status_grid, textvariable=shared_gui_refs['cartridge_steps_var'], bg=sys_status_frame['bg'], fg="lightgreen", font=font_status_value).grid(row=5, column=1, sticky='w')

    return status_bar_container

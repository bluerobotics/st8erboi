import tkinter as tk
from tkinter import ttk
import theme

def create_status_bar(parent, shared_gui_refs):
    status_bar_container = ttk.Frame(parent, style='TFrame')
    status_bar_container.pack(side=tk.TOP, fill=tk.X, expand=False)

    # --- Font Definitions ---
    font_large_readout = ("JetBrains Mono", 28, "bold")
    font_medium_readout = ("JetBrains Mono", 16, "bold")
    font_small_readout = ("JetBrains Mono", 14, "bold")
    font_header = ("JetBrains Mono", 14, "bold")
    font_icon = ("JetBrains Mono", 12) # Smaller font for the icon
    font_ip = ("JetBrains Mono", 9)      # Small font for the IP address
    font_status_label = theme.FONT_NORMAL
    font_status_value = theme.FONT_BOLD
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

    def make_state_tracer(var, label_to_color):
        def tracer(*args):
            state = var.get().upper()
            color = theme.FG_COLOR  # Default
            if "STANDBY" in state: color = theme.SUCCESS_GREEN
            elif "BUSY" in state: color = theme.BUSY_BLUE
            elif "ERROR" in state: color = theme.ERROR_RED
            label_to_color.config(foreground=color)
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

    def create_device_frame(parent, title, state_var, conn_var):
        # Create an outer frame that will act as the border.
        outer_container = ttk.Frame(parent, style='CardBorder.TFrame', padding=1)
        outer_container.pack(side=tk.TOP, fill="x", expand=True, pady=(0, 8))

        # The inner container holds all the content and has the card background color.
        container = ttk.Frame(outer_container, style='Card.TFrame', padding=10)
        container.pack(fill='x', expand=True)

        # --- Header ---
        header_frame = ttk.Frame(container, style='Card.TFrame')
        header_frame.pack(fill='x', expand=True, anchor='n')

        conn_icon_label = ttk.Label(header_frame, text="🔌", font=font_icon, style='Subtle.TLabel')
        conn_icon_label.pack(side=tk.LEFT, padx=(0, 2))
        
        title_label = ttk.Label(header_frame, text=title, font=font_header, foreground=theme.PRIMARY_ACCENT, style='Subtle.TLabel')
        title_label.pack(side=tk.LEFT, padx=(0, 5))

        ip_label = ttk.Label(header_frame, text="", font=font_ip, style='Subtle.TLabel')
        ip_label.pack(side=tk.LEFT, anchor='sw', pady=(0, 2))

        state_label = ttk.Label(header_frame, textvariable=state_var, font=font_header, style='Subtle.TLabel')
        state_label.pack(side=tk.RIGHT)
        
        state_label.tracer = make_state_tracer(state_var, state_label)
        state_var.trace_add('write', state_label.tracer)
        state_label.tracer()

        def conn_tracer(*args):
            full_status = conn_var.get()
            is_connected = "Connected" in full_status
            
            ip_address = ""
            if is_connected:
                try:
                    ip_address = full_status.split('(')[1].split(')')[0]
                except IndexError:
                    ip_address = "?.?.?.?"

            conn_icon_label.config(text="✅" if is_connected else "🔌", foreground=theme.SUCCESS_GREEN if is_connected else theme.ERROR_RED)
            ip_label.config(text=ip_address)

        conn_var.trace_add("write", conn_tracer)
        conn_tracer()
        
        # All content goes into this frame.
        content_frame = ttk.Frame(container, style='Card.TFrame')
        content_frame.pack(fill='x', expand=True)
        
        return content_frame

    # --- Gantry Panel ---
    gantry_content = create_device_frame(status_bar_container, "Gantry", shared_gui_refs['fh_state_var'], shared_gui_refs['status_var_gantry'])
    gantry_axes_data = [
        {'label': 'X', 'pos_var': 'fh_pos_m0_var', 'homed_var': 'fh_homed_m0_var', 'torque_var': 'fh_torque_m0_var', 'state_var': 'fh_state_x_var'},
        {'label': 'Y', 'pos_var': 'fh_pos_m1_var', 'homed_var': 'fh_homed_m1_var', 'torque_var': 'fh_torque_m1_var', 'state_var': 'fh_state_y_var'},
        {'label': 'Z', 'pos_var': 'fh_pos_m3_var', 'homed_var': 'fh_homed_m3_var', 'torque_var': 'fh_torque_m3_var', 'state_var': 'fh_state_z_var'}
    ]
    for axis_info in gantry_axes_data:
        axis_frame = ttk.Frame(gantry_content, style='Card.TFrame')
        axis_frame.pack(anchor='w', pady=4, fill='x')
        axis_frame.grid_columnconfigure(2, weight=1)
        axis_label = ttk.Label(axis_frame, text=f"{axis_info['label']}:", width=3, anchor='w', font=font_large_readout, style='Subtle.TLabel')
        axis_label.grid(row=0, column=0, sticky='w', padx=(0, 5))
        
        state_label = ttk.Label(axis_frame, textvariable=shared_gui_refs[axis_info['state_var']], font=("JetBrains Mono", 10, "bold"), style='Subtle.TLabel')
        state_label.grid(row=0, column=1, sticky='w', padx=(0, 10))

        ttk.Label(axis_frame, textvariable=shared_gui_refs[axis_info['pos_var']], font=font_large_readout, anchor='e', style='Subtle.TLabel').grid(row=0, column=2, sticky='ew', padx=(0, 10))
        
        torque_widget = create_torque_widget(axis_frame, shared_gui_refs[axis_info['torque_var']], bar_height)
        torque_widget.grid(row=0, column=3, sticky='ns')

        homed_var = shared_gui_refs[axis_info['homed_var']]
        axis_label.tracer = make_homed_tracer(homed_var, axis_label)
        homed_var.trace_add('write', axis_label.tracer)
        axis_label.tracer()

    # --- Fillhead Panel ---
    fillhead_content = create_device_frame(status_bar_container, "Fillhead", shared_gui_refs['main_state_var'], shared_gui_refs['status_var_fillhead'])
    injector_dist_frame = ttk.Frame(fillhead_content, style='Card.TFrame')
    injector_dist_frame.pack(fill='x', pady=(0, 5)) # Increased top padding
    injector_dist_frame.grid_columnconfigure(1, weight=1)
    machine_label = ttk.Label(injector_dist_frame, text="Machine:", font=font_medium_readout, style='Subtle.TLabel')
    machine_label.grid(row=0, column=0, sticky='w')
    ttk.Label(injector_dist_frame, textvariable=shared_gui_refs['machine_steps_var'], font=font_medium_readout, anchor='e', style='Subtle.TLabel').grid(row=0, column=1, sticky='ew', padx=5)
    cartridge_label = ttk.Label(injector_dist_frame, text="Cartridge:", font=font_medium_readout, style='Subtle.TLabel')
    cartridge_label.grid(row=1, column=0, sticky='w')
    ttk.Label(injector_dist_frame, textvariable=shared_gui_refs['cartridge_steps_var'], font=font_medium_readout, anchor='e', style='Subtle.TLabel').grid(row=1, column=1, sticky='ew', padx=5)
    injector_torque_widget = create_torque_widget(injector_dist_frame, shared_gui_refs['torque0_var'], bar_height)
    injector_torque_widget.grid(row=0, column=2, rowspan=2, sticky='ns', padx=(10, 0))
    machine_homed_var = shared_gui_refs['homed0_var']
    machine_label.tracer = make_homed_tracer(machine_homed_var, machine_label)
    machine_homed_var.trace_add('write', machine_label.tracer)
    machine_label.tracer()
    cartridge_homed_var = shared_gui_refs['homed1_var']
    cartridge_label.tracer = make_homed_tracer(cartridge_homed_var, cartridge_label)
    cartridge_homed_var.trace_add('write', cartridge_label.tracer)
    cartridge_label.tracer()

    pinch_axes_data = [
        {'label': 'Inj Valve', 'pos_var': 'inj_valve_pos_var', 'homed_var': 'inj_valve_homed_var', 'torque_var': 'torque2_var'},
        {'label': 'Vac Valve', 'pos_var': 'vac_valve_pos_var', 'homed_var': 'vac_valve_homed_var', 'torque_var': 'torque3_var'},
    ]
    for axis_info in pinch_axes_data:
        axis_frame = ttk.Frame(fillhead_content, style='Card.TFrame')
        axis_frame.pack(anchor='w', pady=2, fill='x')
        axis_frame.grid_columnconfigure(1, weight=1)
        axis_label = ttk.Label(axis_frame, text=f"{axis_info['label']}:", anchor='w', font=font_small_readout, style='Subtle.TLabel')
        axis_label.grid(row=0, column=0, sticky='w', padx=(0, 5))
        ttk.Label(axis_frame, textvariable=shared_gui_refs[axis_info['pos_var']], font=font_small_readout, anchor='e', style='Subtle.TLabel').grid(row=0, column=1, sticky='ew', padx=(0, 10))
        torque_widget = create_torque_widget(axis_frame, shared_gui_refs[axis_info['torque_var']], small_bar_height)
        torque_widget.grid(row=0, column=2, sticky='ns')
        homed_var = shared_gui_refs[axis_info['homed_var']]
        axis_label.tracer = make_homed_tracer(homed_var, axis_label)
        homed_var.trace_add('write', axis_label.tracer)
        axis_label.tracer()

    ttk.Separator(fillhead_content, orient='horizontal').pack(fill='x', pady=8)

    # --- Fillhead Component States ---
    fillhead_states_frame = ttk.Frame(fillhead_content, style='Card.TFrame')
    fillhead_states_frame.pack(fill='x', expand=True, pady=(0, 8), padx=20)
    fillhead_states_frame.grid_columnconfigure(1, weight=1)

    fillhead_components = [
        ("Injector:", 'fillhead_injector_state_var'),
        ("Inject Valve:", 'fillhead_inj_valve_state_var'),
        ("Vac Valve:", 'fillhead_vac_valve_state_var'),
        ("Heater:", 'fillhead_heater_state_var'),
        ("Vacuum:", 'fillhead_vacuum_state_var')
    ]

    for i, (label_text, var_key) in enumerate(fillhead_components):
        ttk.Label(fillhead_states_frame, text=label_text, foreground=theme.PRIMARY_ACCENT, style='Subtle.TLabel').grid(row=i, column=0, sticky='e', padx=(0, 5))
        ttk.Label(fillhead_states_frame, textvariable=shared_gui_refs[var_key], style='Subtle.TLabel').grid(row=i, column=1, sticky='w')

    readouts_grid = ttk.Frame(fillhead_content, style='Card.TFrame')
    readouts_grid.pack(fill='x', expand=True)
    readouts_grid.grid_columnconfigure(1, weight=1)
    readouts = [
        ("Vacuum:", 'vacuum_psig_var', theme.PRIMARY_ACCENT),
        ("Heater Temp:", 'heater_display_var', theme.WARNING_YELLOW),
        ("Dispensed (Active):", 'inject_active_ml_var', theme.SUCCESS_GREEN),
        ("Dispensed (Total):", 'inject_cumulative_ml_var', theme.FG_COLOR)
    ]
    for i, (label_text, var_key, color) in enumerate(readouts):
        label = ttk.Label(readouts_grid, text=label_text, font=font_status_label, style='Subtle.TLabel')
        label.grid(row=i, column=0, sticky='e', padx=5, pady=2)

        value_label = ttk.Label(readouts_grid, textvariable=shared_gui_refs[var_key], font=font_status_value, style='Subtle.TLabel')
        value_label.config(foreground=color)
        value_label.grid(row=i, column=1, sticky='w')

    return status_bar_container

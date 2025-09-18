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
    font_injector_readout = ("JetBrains Mono", 12, "bold")
    font_header = ("JetBrains Mono", 14, "bold")
    font_icon = ("JetBrains Mono", 12) # Smaller font for the icon
    font_ip = ("JetBrains Mono", 9)      # Small font for the IP address
    font_status_label = theme.FONT_NORMAL
    font_status_value = theme.FONT_BOLD
    bar_height = 55
    small_bar_height = 20

    # --- Helper Functions ---
    def make_homed_tracer(var, label_to_color):
        def tracer(*args):
            state = var.get()
            if state == 'Homed':
                label_to_color.config(foreground=theme.SUCCESS_GREEN)
            else:
                label_to_color.config(foreground=theme.ERROR_RED)
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

    def make_on_off_tracer(var, *labels_with_colors):
        # labels_with_colors is a list of tuples: (label, on_color, off_color)
        def tracer(*args):
            state = var.get().upper()
            is_on = "ON" in state or "ACTIVE" in state
            for label, on_color, off_color in labels_with_colors:
                color = on_color if is_on else off_color
                label.config(foreground=color)
        return tracer

    def make_heater_value_tracer(var, label_to_color):
        def tracer(*args):
            try:
                value_str = var.get().split()[0]
                temp = float(value_str)
                color = theme.SUCCESS_GREEN if 0 <= temp <= 200 else theme.FG_COLOR
                label_to_color.config(foreground=color)
            except (ValueError, IndexError):
                label_to_color.config(foreground=theme.FG_COLOR)
        return tracer

    def make_vacuum_value_tracer(var, label_to_color):
        def tracer(*args):
            try:
                value_str = var.get().split()[0]
                pressure = float(value_str)
                color = theme.SUCCESS_GREEN if -15 <= pressure <= 1 else theme.FG_COLOR
                label_to_color.config(foreground=color)
            except (ValueError, IndexError):
                label_to_color.config(foreground=theme.FG_COLOR)
        return tracer

    def create_torque_widget(parent, torque_dv, height):
        torque_frame = ttk.Frame(parent, height=height, width=30, style='TFrame')
        torque_frame.pack_propagate(False)
        torque_sv = tk.StringVar()
        torque_frame.tracer = make_torque_tracer(torque_dv, torque_sv)
        torque_dv.trace_add('write', torque_frame.tracer)
        pbar = ttk.Progressbar(torque_frame, variable=torque_dv, maximum=100, orient=tk.VERTICAL, style='Card.Vertical.TProgressbar')
        pbar.pack(fill=tk.BOTH, expand=True)
        label = ttk.Label(torque_frame, textvariable=torque_sv, font=("JetBrains Mono", 7, "bold"), anchor='center', style='Subtle.TLabel')
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
    
    # --- Injector, Machine, Cartridge, Dispensing Layout ---
    injector_container = ttk.Frame(fillhead_content, style='CardBorder.TFrame')
    injector_container.pack(anchor='w', pady=(10, 5), fill='x')

    content_grid = ttk.Frame(injector_container, style='Card.TFrame', padding=5)
    content_grid.pack(fill='both', expand=True, padx=1, pady=1)

    content_grid.grid_columnconfigure(1, weight=1)
    content_grid.grid_columnconfigure(3, weight=1)

    # --- Row 0 & 1: Injector and Machine/Cartridge ---
    # Injector Status (spans 2 rows)
    inj_frame = ttk.Frame(content_grid, style='Card.TFrame')
    inj_frame.grid(row=0, column=0, columnspan=2, rowspan=2, sticky='nsew')
    ttk.Label(inj_frame, text="Injector:", font=font_injector_readout, style='Subtle.TLabel').pack(anchor='w')
    injector_state_label = ttk.Label(inj_frame, textvariable=shared_gui_refs['fillhead_injector_state_var'], font=font_injector_readout, style='Subtle.TLabel')
    injector_state_label.pack(anchor='w')
    injector_state_label.tracer = make_state_tracer(shared_gui_refs['fillhead_injector_state_var'], injector_state_label)
    shared_gui_refs['fillhead_injector_state_var'].trace_add('write', injector_state_label.tracer)

    # Machine Position (Row 0)
    machine_label = ttk.Label(content_grid, text="Machine:", font=font_injector_readout, style='Subtle.TLabel')
    machine_label.grid(row=0, column=2, sticky='w', padx=(10, 5))
    ttk.Label(content_grid, textvariable=shared_gui_refs['machine_steps_var'], font=font_injector_readout, style='Subtle.TLabel', anchor='e').grid(row=0, column=3, sticky='ew')
    machine_homed_var = shared_gui_refs['homed0_var']
    machine_label.tracer = make_homed_tracer(machine_homed_var, machine_label)
    machine_homed_var.trace_add('write', machine_label.tracer)
    machine_label.tracer()
    
    # Cartridge Position (Row 1)
    cartridge_label = ttk.Label(content_grid, text="Cartridge:", font=font_injector_readout, style='Subtle.TLabel')
    cartridge_label.grid(row=1, column=2, sticky='w', padx=(10, 5))
    ttk.Label(content_grid, textvariable=shared_gui_refs['cartridge_steps_var'], font=font_injector_readout, style='Subtle.TLabel', anchor='e').grid(row=1, column=3, sticky='ew')
    cartridge_homed_var = shared_gui_refs['homed1_var']
    cartridge_label.tracer = make_homed_tracer(cartridge_homed_var, cartridge_label)
    cartridge_homed_var.trace_add('write', cartridge_label.tracer)
    cartridge_label.tracer()

    # --- Row 2: Total Dispensed ---
    total_disp_frame = ttk.Frame(content_grid, style='Card.TFrame')
    total_disp_frame.grid(row=2, column=0, columnspan=4, sticky='ew')
    total_disp_frame.grid_columnconfigure(1, weight=1)
    ttk.Label(total_disp_frame, text="Total Dispensed:", font=font_injector_readout, style='Subtle.TLabel').grid(row=0, column=0, sticky='w')
    ttk.Label(total_disp_frame, textvariable=shared_gui_refs['total_dispensed_var'], font=font_injector_readout, style='Subtle.TLabel', anchor='e').grid(row=0, column=1, sticky='ew')

    # --- Row 3: Cycle Dispensed ---
    cycle_disp_frame = ttk.Frame(content_grid, style='Card.TFrame')
    cycle_disp_frame.grid(row=3, column=0, columnspan=4, sticky='ew')
    cycle_disp_frame.grid_columnconfigure(1, weight=1)
    ttk.Label(cycle_disp_frame, text="Cycle Dispensed:", font=font_injector_readout, style='Subtle.TLabel').grid(row=0, column=0, sticky='w')
    ttk.Label(cycle_disp_frame, textvariable=shared_gui_refs['cycle_dispensed_var'], foreground=theme.SUCCESS_GREEN, font=font_injector_readout, style='Subtle.TLabel', anchor='e').grid(row=0, column=1, sticky='ew')

    # --- Torque Bar ---
    injector_torque_widget = create_torque_widget(content_grid, shared_gui_refs['torque0_var'], 115) # Height adjusted
    injector_torque_widget.grid(row=0, column=4, rowspan=4, sticky='ns', padx=(10, 0)) # Rowspan adjusted
    
    # Separator
    ttk.Separator(fillhead_content, orient='horizontal').pack(fill='x', pady=8, padx=10)
    
    pinch_axes_data = [
        {'label': 'Inj Valve', 'pos_var': 'inj_valve_pos_var', 'homed_var': 'inj_valve_homed_var', 'torque_var': 'torque2_var', 'state_var': 'fillhead_inj_valve_state_var'},
        {'label': 'Vac Valve', 'pos_var': 'vac_valve_pos_var', 'homed_var': 'vac_valve_homed_var', 'torque_var': 'torque3_var', 'state_var': 'fillhead_vac_valve_state_var'},
    ]

    for axis_info in pinch_axes_data:
        axis_frame = ttk.Frame(fillhead_content, style='Card.TFrame')
        axis_frame.pack(anchor='w', pady=2, fill='x')
        axis_frame.grid_columnconfigure(2, weight=1)
        axis_label = ttk.Label(axis_frame, text=f"{axis_info['label']}:", anchor='w', font=font_small_readout, style='Subtle.TLabel')
        axis_label.grid(row=0, column=0, sticky='w', padx=(0, 5))
        
        ttk.Label(axis_frame, textvariable=shared_gui_refs[axis_info['state_var']], font=font_small_readout, style='Subtle.TLabel').grid(row=0, column=1, sticky='w', padx=(0, 10))

        ttk.Label(axis_frame, textvariable=shared_gui_refs[axis_info['pos_var']], font=font_small_readout, anchor='e', style='Subtle.TLabel').grid(row=0, column=2, sticky='ew', padx=(0, 10))
        
        torque_widget = create_torque_widget(axis_frame, shared_gui_refs[axis_info['torque_var']], small_bar_height)
        torque_widget.grid(row=0, column=3, rowspan=1, sticky='ns', padx=(10, 0))

        # Apply homing status color tracer to the valve label
        homed_var = shared_gui_refs[axis_info['homed_var']]
        axis_label.tracer = make_homed_tracer(homed_var, axis_label)
        homed_var.trace_add('write', axis_label.tracer)
        axis_label.tracer() # Initial call to set color

    # Separator
    ttk.Separator(fillhead_content, orient='horizontal').pack(fill='x', pady=8, padx=10)
    
    # --- System Readouts ---
    # Vacuum
    vac_frame = ttk.Frame(fillhead_content, style='Card.TFrame')
    vac_frame.pack(anchor='w', pady=2, fill='x')
    vac_frame.grid_columnconfigure(2, weight=1)

    vac_label = ttk.Label(vac_frame, text="Vacuum:", anchor='w', font=font_small_readout, style='Subtle.TLabel')
    vac_label.grid(row=0, column=0, sticky='w', padx=(0, 5))
    
    vac_status_label = ttk.Label(vac_frame, textvariable=shared_gui_refs['fillhead_vacuum_state_var'], font=font_small_readout, style='Subtle.TLabel')
    vac_status_label.grid(row=0, column=1, sticky='w', padx=(0,10))
    
    # Color for the "ON/OFF" status
    vac_status_tracer = make_on_off_tracer(
        shared_gui_refs['fillhead_vacuum_state_var'],
        (vac_status_label, theme.SUCCESS_GREEN, theme.COMMENT_COLOR)
    )
    shared_gui_refs['fillhead_vacuum_state_var'].trace_add('write', vac_status_tracer)
    vac_status_tracer()

    # Color for the "Vacuum:" label based on its value
    vac_value_tracer = make_vacuum_value_tracer(shared_gui_refs['vacuum_psig_var'], vac_label)
    shared_gui_refs['vacuum_psig_var'].trace_add('write', vac_value_tracer)
    vac_value_tracer()
    
    ttk.Label(vac_frame, textvariable=shared_gui_refs['vacuum_psig_var'], font=font_small_readout, foreground=theme.PRIMARY_ACCENT, anchor='e', style='Subtle.TLabel').grid(row=0, column=2, sticky='ew', padx=(0, 10))


    # Heater
    heater_frame = ttk.Frame(fillhead_content, style='Card.TFrame')
    heater_frame.pack(anchor='w', pady=2, fill='x')
    heater_frame.grid_columnconfigure(2, weight=1)

    heater_label = ttk.Label(heater_frame, text="Heater:", anchor='w', font=font_small_readout, style='Subtle.TLabel')
    heater_label.grid(row=0, column=0, sticky='w', padx=(0, 5))

    heater_status_label = ttk.Label(heater_frame, textvariable=shared_gui_refs['fillhead_heater_state_var'], font=font_small_readout, style='Subtle.TLabel')
    heater_status_label.grid(row=0, column=1, sticky='w', padx=(0,10))

    # Color for the "ON/OFF" status
    heater_status_tracer = make_on_off_tracer(
        shared_gui_refs['fillhead_heater_state_var'],
        (heater_status_label, theme.SUCCESS_GREEN, theme.COMMENT_COLOR)
    )
    shared_gui_refs['fillhead_heater_state_var'].trace_add('write', heater_status_tracer)
    heater_status_tracer()

    # Color for the "Heater:" label based on its value
    heater_value_tracer = make_heater_value_tracer(shared_gui_refs['temp_c_var'], heater_label)
    shared_gui_refs['temp_c_var'].trace_add('write', heater_value_tracer)
    heater_value_tracer()

    ttk.Label(heater_frame, textvariable=shared_gui_refs['heater_display_var'], font=font_small_readout, foreground=theme.WARNING_YELLOW, anchor='e', style='Subtle.TLabel').grid(row=0, column=2, sticky='ew', padx=(0, 10))
        

    return status_bar_container

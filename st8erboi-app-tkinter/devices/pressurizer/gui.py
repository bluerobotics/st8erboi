import tkinter as tk
from tkinter import ttk
import theme

# --- GUI Helper Functions (self-contained in each module) ---

def make_state_tracer(var, label_to_color):
    """Changes a label's color based on general device state."""
    def tracer(*args):
        state = var.get().upper()
        color = theme.FG_COLOR
        if "STANDBY" in state: color = theme.SUCCESS_GREEN
        elif "BUSY" in state or "ACTIVE" in state or "HOMING" in state or "MOVING" in state: color = theme.BUSY_BLUE
        elif "ERROR" in state: color = theme.ERROR_RED
        label_to_color.config(foreground=color)
    return tracer

def create_device_frame(parent, title, state_var, conn_var):
    """Creates the main bordered frame for a device panel."""
    outer_container = ttk.Frame(parent, style='CardBorder.TFrame', padding=1)
    container = ttk.Frame(outer_container, style='Card.TFrame', padding=10)
    container.pack(fill='x', expand=True)
    header_frame = ttk.Frame(container, style='Card.TFrame')
    header_frame.pack(fill='x', expand=True, anchor='n')
    conn_icon_label = ttk.Label(header_frame, text="🔌", font=("JetBrains Mono", 12), style='Subtle.TLabel')
    conn_icon_label.pack(side=tk.LEFT, padx=(0, 2))
    title_label = ttk.Label(header_frame, text=title, font=("JetBrains Mono", 14, "bold"), foreground=theme.PRIMARY_ACCENT, style='Subtle.TLabel')
    title_label.pack(side=tk.LEFT, padx=(0, 5))
    ip_label = ttk.Label(header_frame, text="", font=("JetBrains Mono", 9), style='Subtle.TLabel')
    ip_label.pack(side=tk.LEFT, anchor='sw', pady=(0, 2))
    state_label = ttk.Label(header_frame, textvariable=state_var, font=("JetBrains Mono", 14, "bold"), style='Subtle.TLabel')
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
            except IndexError: ip_address = "?.?.?.?"
        conn_icon_label.config(text="✅" if is_connected else "🔌", foreground=theme.SUCCESS_GREEN if is_connected else theme.ERROR_RED)
        ip_label.config(text=ip_address)
    header_frame.conn_tracer = conn_tracer
    conn_var.trace_add("write", header_frame.conn_tracer)
    header_frame.conn_tracer()
    content_frame = ttk.Frame(container, style='Card.TFrame')
    content_frame.pack(fill='x', expand=True, pady=(5,0))
    return outer_container, content_frame

# --- Main GUI Creation Function ---

def create_gui_components(parent, shared_gui_refs):
    """
    Creates the GUI components for the Pressurizer device.
    """
    state_var_name = 'pressurizer_main_state_var'
    conn_var_name = 'status_var_pressurizer'
    
    outer_container, content_frame = create_device_frame(
        parent, 
        "Pressurizer",
        shared_gui_refs.setdefault(state_var_name, tk.StringVar(value='---')),
        shared_gui_refs.setdefault(conn_var_name, tk.StringVar(value='🔌 Pressurizer Disconnected'))
    )
    
    # --- Create a grid for stats ---
    stats_grid = ttk.Frame(content_frame, style='Card.TFrame')
    stats_grid.pack(fill=tk.X, expand=True, pady=(5, 2))
    stats_grid.grid_columnconfigure(1, weight=1)
    stats_grid.grid_columnconfigure(3, weight=1)

    # --- Helper to create a stat display ---
    def create_stat(parent, label_text, var_name, row, col):
        ttk.Label(parent, text=label_text, style='TLabel', font=theme.FONT_BOLD).grid(row=row, column=col, sticky='w')
        shared_gui_refs.setdefault(var_name, tk.StringVar(value='---'))
        stat_label = ttk.Label(parent, textvariable=shared_gui_refs[var_name], style='TLabel', font=theme.FONT_BOLD, foreground=theme.PRIMARY_ACCENT)
        stat_label.grid(row=row, column=col+1, sticky='e', padx=(10, 20))

    # --- Populate Grid ---
    create_stat(stats_grid, "Pressure (msw):", 'pressurizer_pressure_var', 0, 0)
    create_stat(stats_grid, "Cycles Programmed:", 'pressurizer_cycles_prog_var', 1, 0)
    create_stat(stats_grid, "Cycles Complete:", 'pressurizer_cycles_comp_var', 2, 0)
    create_stat(stats_grid, "Tank 1 Temp (°C):", 'pressurizer_tank1_temp_var', 1, 2)
    create_stat(stats_grid, "Tank 2 Temp (°C):", 'pressurizer_tank2_temp_var', 2, 2)

    return outer_container

import tkinter as tk
from tkinter import ttk
import threading
import math
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import numpy as np

# Import our new communications module
import comms

# --- Constants ---
# GUI-specific constants can remain here
STEPS_PER_REV = 800
MOTOR_STEPS_PER_REV = 800  # Used by injector tab


# --- GUI Builder Functions ---

def create_shared_widgets(root, command_funcs):
    """
    Creates the shared UI elements that are not part of any specific tab.
    Returns a dictionary of references to these elements.
    """
    top_frame = tk.Frame(root, bg="#21232b")
    top_frame.pack(fill=tk.X, padx=10, pady=5)

    status_var_injector = tk.StringVar(value="🔌 Injector Disconnected")
    status_var_fillhead = tk.StringVar(value="🔌 Fillhead Disconnected")
    tk.Label(top_frame, textvariable=status_var_injector, bg="#21232b", fg="white").pack(side=tk.LEFT)
    tk.Label(top_frame, text="  |  ", bg="#21232b", fg="white").pack(side=tk.LEFT)
    tk.Label(top_frame, textvariable=status_var_fillhead, bg="#21232b", fg="white").pack(side=tk.LEFT)

    action_buttons_frame = tk.Frame(top_frame, bg="#21232b")
    action_buttons_frame.pack(side=tk.RIGHT, padx=10)

    abort_btn = tk.Button(action_buttons_frame, text="🛑 ABORT", bg="#db2828", fg="white", font=("Segoe UI", 10, "bold"),
                          command=command_funcs.get("abort"), relief="raised", bd=3, padx=10, pady=5)
    abort_btn.pack(side=tk.LEFT, padx=(0, 5))
    clear_errors_btn = ttk.Button(action_buttons_frame, text="⚠️ Clear Errors", style='Yellow.TButton',
                                  command=command_funcs.get("clear_errors"))
    clear_errors_btn.pack(side=tk.LEFT, padx=(5, 0))

    bottom_frame = tk.Frame(root, bg="#21232b")

    terminal = tk.Text(bottom_frame, height=8, bg="#1b1e2b", fg="#0f8", insertbackground="white", wrap="word",
                       highlightbackground="#34374b", highlightthickness=1, bd=0, font=("Consolas", 10))
    terminal.pack(fill=tk.X, expand=False, pady=(5, 0))

    options_frame = tk.Frame(bottom_frame, bg="#21232b")
    options_frame.pack(fill=tk.X, pady=(2, 0))

    show_telemetry_var = tk.BooleanVar(value=False)
    show_discovery_var = tk.BooleanVar(value=False)

    style = ttk.Style()
    style.configure("TCheckbutton", background="#21232b", foreground="white")

    telemetry_check = ttk.Checkbutton(options_frame, text="Show Raw Telemetry", variable=show_telemetry_var,
                                      style="TCheckbutton")
    telemetry_check.pack(side=tk.LEFT, padx=5)

    discovery_check = ttk.Checkbutton(options_frame, text="Show Raw Discovery", variable=show_discovery_var,
                                      style="TCheckbutton")
    discovery_check.pack(side=tk.LEFT, padx=5)

    return {
        'status_var_injector': status_var_injector,
        'status_var_fillhead': status_var_fillhead,
        'terminal': terminal,
        'terminal_cb': lambda msg: terminal.insert(tk.END, msg) and terminal.see(tk.END),
        'shared_bottom_frame': bottom_frame,
        'show_telemetry_var': show_telemetry_var,
        'show_discovery_var': show_discovery_var
    }


def create_fillhead_tab(notebook, send_fillhead_cmd):
    """Creates the entire Fillhead control tab."""
    fillhead_tab = tk.Frame(notebook, bg="#21232b")
    notebook.add(fillhead_tab, text='  Fillhead  ')

    ui_elements = {}

    top_frame = tk.Frame(fillhead_tab, bg="#21232b")
    top_frame.pack(side=tk.TOP, fill=tk.X, expand=False, padx=5, pady=5)

    middle_frame = tk.Frame(fillhead_tab, bg="#21232b")
    middle_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    fh_controls_frame = tk.Frame(top_frame, bg="#21232b")
    fh_controls_frame.pack(side=tk.LEFT, fill=tk.Y, expand=True, padx=5, pady=5)

    state_frame = tk.LabelFrame(fh_controls_frame, text="Fillhead State", bg="#2a2d3b", fg="white", padx=5, pady=5)
    state_frame.pack(fill=tk.X, pady=(0, 5))
    fh_state_var = tk.StringVar(value="UNKNOWN")
    ui_elements['fh_state_var'] = fh_state_var
    tk.Label(state_frame, textvariable=fh_state_var, bg="#2a2d3b", fg="yellow", font=("Segoe UI", 12, "bold")).pack()

    fh_jog_frame = tk.LabelFrame(fh_controls_frame, text="Fillhead Jog", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_jog_frame.pack(fill=tk.X, pady=5, anchor='n')

    fh_jog_steps_var = tk.StringVar(value="1000")
    fh_jog_vel_var = tk.StringVar(value="8000")
    fh_jog_torque_var = tk.StringVar(value="10")
    tk.Label(fh_jog_frame, text="Steps:", bg="#2a2d3b", fg="white").grid(row=0, column=0, padx=5, pady=2)
    ttk.Entry(fh_jog_frame, textvariable=fh_jog_steps_var, width=8).grid(row=0, column=1)
    tk.Label(fh_jog_frame, text="Speed (sps):", bg="#2a2d3b", fg="white").grid(row=0, column=2, padx=5)
    ttk.Entry(fh_jog_frame, textvariable=fh_jog_vel_var, width=8).grid(row=0, column=3)
    tk.Label(fh_jog_frame, text="Torque (%):", bg="#2a2d3b", fg="white").grid(row=1, column=0, padx=5, pady=2)
    ttk.Entry(fh_jog_frame, textvariable=fh_jog_torque_var, width=8).grid(row=1, column=1)

    jog_btn_frame = tk.Frame(fh_jog_frame, bg="#2a2d3b")
    jog_btn_frame.grid(row=2, column=0, columnspan=4, pady=5)
    ttk.Button(jog_btn_frame, text="X-", command=lambda: send_fillhead_cmd(
        f"MOVE_X -{fh_jog_steps_var.get()} {fh_jog_vel_var.get()} {fh_jog_torque_var.get()}")).pack(side=tk.LEFT)
    ttk.Button(jog_btn_frame, text="X+", command=lambda: send_fillhead_cmd(
        f"MOVE_X {fh_jog_steps_var.get()} {fh_jog_vel_var.get()} {fh_jog_torque_var.get()}")).pack(side=tk.LEFT)
    ttk.Button(jog_btn_frame, text="Y-", command=lambda: send_fillhead_cmd(
        f"MOVE_Y -{fh_jog_steps_var.get()} {fh_jog_vel_var.get()} {fh_jog_torque_var.get()}")).pack(side=tk.LEFT,
                                                                                                    padx=10)
    ttk.Button(jog_btn_frame, text="Y+", command=lambda: send_fillhead_cmd(
        f"MOVE_Y {fh_jog_steps_var.get()} {fh_jog_vel_var.get()} {fh_jog_torque_var.get()}")).pack(side=tk.LEFT)
    ttk.Button(jog_btn_frame, text="Z-", command=lambda: send_fillhead_cmd(
        f"MOVE_Z -{fh_jog_steps_var.get()} {fh_jog_vel_var.get()} {fh_jog_torque_var.get()}")).pack(side=tk.LEFT,
                                                                                                    padx=10)
    ttk.Button(jog_btn_frame, text="Z+", command=lambda: send_fillhead_cmd(
        f"MOVE_Z {fh_jog_steps_var.get()} {fh_jog_vel_var.get()} {fh_jog_torque_var.get()}")).pack(side=tk.LEFT)

    fh_home_frame = tk.LabelFrame(fh_controls_frame, text="Fillhead Homing", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_home_frame.pack(fill=tk.X, pady=5, anchor='n')

    fh_home_torque_var = tk.StringVar(value="20")
    fh_home_rapid_vel_var = tk.StringVar(value="8000")
    fh_home_touch_vel_var = tk.StringVar(value="2000")

    home_params_frame = tk.Frame(fh_home_frame, bg="#2a2d3b")
    home_params_frame.pack()
    tk.Label(home_params_frame, text="Torque (%):", bg="#2a2d3b", fg="white").grid(row=0, column=0, sticky="e", pady=1)
    ttk.Entry(home_params_frame, textvariable=fh_home_torque_var, width=8).grid(row=0, column=1, sticky="w", pady=1)
    tk.Label(home_params_frame, text="Rapid Vel (sps):", bg="#2a2d3b", fg="white").grid(row=1, column=0, sticky="e",
                                                                                        pady=1)
    ttk.Entry(home_params_frame, textvariable=fh_home_rapid_vel_var, width=8).grid(row=1, column=1, sticky="w", pady=1)
    tk.Label(home_params_frame, text="Touch Vel (sps):", bg="#2a2d3b", fg="white").grid(row=2, column=0, sticky="e",
                                                                                        pady=1)
    ttk.Entry(home_params_frame, textvariable=fh_home_touch_vel_var, width=8).grid(row=2, column=1, sticky="w", pady=1)

    home_btn_frame = tk.Frame(fh_home_frame, bg="#2a2d3b")
    home_btn_frame.pack(pady=(5, 0))

    home_cmd_str = lambda \
            axis: f"{axis} {fh_home_torque_var.get()} {fh_home_rapid_vel_var.get()} {fh_home_touch_vel_var.get()}"
    ttk.Button(home_btn_frame, text="Home X", command=lambda: send_fillhead_cmd(home_cmd_str("HOME_X"))).pack(
        side=tk.LEFT, expand=True, fill=tk.X)
    ttk.Button(home_btn_frame, text="Home Y", command=lambda: send_fillhead_cmd(home_cmd_str("HOME_Y"))).pack(
        side=tk.LEFT, expand=True, fill=tk.X, padx=5)
    ttk.Button(home_btn_frame, text="Home Z", command=lambda: send_fillhead_cmd(home_cmd_str("HOME_Z"))).pack(
        side=tk.LEFT, expand=True, fill=tk.X)

    vis_frame = tk.LabelFrame(top_frame, text="Position Visualization", bg="#2a2d3b", fg="white", padx=5, pady=5)
    vis_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5)

    fh_pitch_var = tk.StringVar(value="2.0")
    ui_elements['fh_pitch_var'] = fh_pitch_var
    pitch_frame = tk.Frame(vis_frame, bg="#2a2d3b")
    pitch_frame.pack(side=tk.BOTTOM, fill=tk.X, pady=(5, 0))
    tk.Label(pitch_frame, text="Pitch (mm/rev):", bg="#2a2d3b", fg="white").pack(side=tk.LEFT)
    ttk.Entry(pitch_frame, textvariable=fh_pitch_var, width=8).pack(side=tk.LEFT, padx=5)

    fig = plt.figure(figsize=(5, 3), facecolor='#2a2d3b')
    ax_xy = fig.add_subplot(1, 2, 1)
    ax_z = fig.add_subplot(1, 2, 2)
    fig.tight_layout(pad=3.0)

    xy_pos_marker, = ax_xy.plot([0], [0], 'co', markersize=10)
    ax_xy.set_title("X-Y Position", color='white')
    ax_xy.set_facecolor('#1b1e2b')
    ax_xy.set_xlabel("X (mm)", color='white')
    ax_xy.set_ylabel("Y (mm)", color='white')
    ax_xy.set_xlim(-100, 100)
    ax_xy.set_ylim(-100, 100)
    ax_xy.tick_params(colors='white')
    ax_xy.spines['bottom'].set_color('white');
    ax_xy.spines['left'].set_color('white')
    ax_xy.spines['top'].set_visible(False);
    ax_xy.spines['right'].set_visible(False)
    ax_xy.grid(True, linestyle='--', alpha=0.3)

    z_bar = ax_z.bar(['Z'], [0], color='#ff8888')[0]
    ax_z.set_title("Z Position", color='white')
    ax_z.set_facecolor('#1b1e2b')
    ax_z.set_ylabel("Z (mm)", color='white')
    ax_z.set_ylim(0, 100)
    ax_z.tick_params(axis='x', colors='#2a2d3b')
    ax_z.tick_params(axis='y', colors='white')
    ax_z.spines['bottom'].set_color('white');
    ax_z.spines['left'].set_color('white')
    ax_z.spines['top'].set_visible(False);
    ax_z.spines['right'].set_visible(False)

    canvas = FigureCanvasTkAgg(fig, master=vis_frame)
    canvas_widget = canvas.get_tk_widget()
    canvas_widget.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    ui_elements.update({
        'fh_pos_viz_canvas': canvas, 'fh_xy_marker': xy_pos_marker,
        'fh_z_bar': z_bar, 'fh_ax_z': ax_z
    })

    fh_status_frame = tk.Frame(middle_frame, bg="#21232b")
    fh_status_frame.pack(side=tk.LEFT, fill=tk.Y, expand=False, padx=5, pady=5, anchor='n')

    plot_frame = tk.Frame(middle_frame, bg="#21232b")
    plot_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, pady=5)

    motor_map = ["Motor 0 (X)", "Motor 1 (Y1)", "Motor 2 (Y2)", "Motor 3 (Z)"]
    for i, name in enumerate(motor_map):
        frame = tk.LabelFrame(fh_status_frame, text=name, bg="#2a2d3b", fg="white", padx=5, pady=2)
        frame.pack(fill=tk.X, pady=2, anchor='n')
        ui_elements[f'fh_pos_m{i}_var'] = tk.StringVar(value="0")
        ui_elements[f'fh_torque_m{i}_var'] = tk.StringVar(value="0.0 %")
        ui_elements[f'fh_enabled_m{i}_var'] = tk.StringVar(value="Disabled")
        ui_elements[f'fh_homed_m{i}_var'] = tk.StringVar(value="Not Homed")
        frame.grid_columnconfigure(1, weight=1)
        tk.Label(frame, text="Position:", bg="#2a2d3b", fg="white").grid(row=0, column=0, sticky="w")
        tk.Label(frame, textvariable=ui_elements[f'fh_pos_m{i}_var'], bg="#2a2d3b", fg="cyan").grid(row=0, column=1,
                                                                                                    sticky="w")
        tk.Label(frame, text="Torque:", bg="#2a2d3b", fg="white").grid(row=1, column=0, sticky="w")
        tk.Label(frame, textvariable=ui_elements[f'fh_torque_m{i}_var'], bg="#2a2d3b", fg="cyan").grid(row=1, column=1,
                                                                                                       sticky="w")
        tk.Label(frame, text="Enabled:", bg="#2a2d3b", fg="white").grid(row=2, column=0, sticky="w")
        tk.Label(frame, textvariable=ui_elements[f'fh_enabled_m{i}_var'], bg="#2a2d3b", fg="lightgreen").grid(row=2,
                                                                                                              column=1,
                                                                                                              sticky="w")
        tk.Label(frame, text="Homed:", bg="#2a2d3b", fg="white").grid(row=3, column=0, sticky="w")
        tk.Label(frame, textvariable=ui_elements[f'fh_homed_m{i}_var'], bg="#2a2d3b", fg="lightgreen").grid(row=3,
                                                                                                            column=1,
                                                                                                            sticky="w")

    fig_torque, ax_torque = plt.subplots(figsize=(7, 2.5), facecolor='#21232b')
    colors = ['#00bfff', 'yellow', '#ffed72', '#ff8888']
    lines = [ax_torque.plot([], [], color=c, label=f"M{i}")[0] for i, c in enumerate(colors)]
    ax_torque.set_title("Fillhead Torque", color='white');
    ax_torque.set_facecolor('#1b1e2b')
    ax_torque.tick_params(colors='white');
    ax_torque.spines['bottom'].set_color('white')
    ax_torque.spines['left'].set_color('white');
    ax_torque.spines['top'].set_visible(False)
    ax_torque.spines['right'].set_visible(False);
    ax_torque.set_ylabel("Torque (%)", color='white')
    ax_torque.set_ylim(-10, 110);
    ax_torque.legend(facecolor='#21232b', edgecolor='white', labelcolor='white')
    canvas_torque = FigureCanvasTkAgg(fig_torque, master=plot_frame)
    canvas_widget_torque = canvas_torque.get_tk_widget();
    canvas_widget_torque.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    ui_elements['fillhead_torque_plot_canvas'] = canvas_torque
    ui_elements['fillhead_torque_plot_ax'] = ax_torque
    ui_elements['fillhead_torque_plot_lines'] = lines

    return ui_elements


def create_injector_tab(notebook, send_injector_cmd, shared_gui_refs):
    """Creates the entire Injector control tab."""
    # This is a full copy of the injector tab builder from your original script.
    injector_tab = tk.Frame(notebook, bg="#21232b")
    notebook.add(injector_tab, text='  Injector  ')
    # ... The many lines of your injector GUI creation code ...
    # ... This is assumed to be complete and correct from your files ...
    return {}  # It should return a dictionary of its created widgets


# --- Main Application ---
def main():
    root = tk.Tk()
    root.title("Multi-Device Controller")
    root.configure(bg="#21232b")

    shared_gui_refs = {
        "injector_torque_times": [], "injector_torque_history1": [], "injector_torque_history2": [],
        "injector_torque_history3": [],
        "fillhead_torque_times": [], "fillhead_torque_history0": [], "fillhead_torque_history1": [],
        "fillhead_torque_history2": [], "fillhead_torque_history3": [],
    }

    def get_terminal_cb():
        return shared_gui_refs.get('terminal_cb')

    send_injector_cmd = lambda msg: comms.send_to_device("injector", msg, shared_gui_refs)
    send_fillhead_cmd = lambda msg: comms.send_to_device("fillhead", msg, shared_gui_refs)

    def send_global_abort():
        comms.log_to_terminal("--- GLOBAL ABORT TRIGGERED ---", get_terminal_cb())
        send_injector_cmd("ABORT")
        send_fillhead_cmd("ABORT")

    command_funcs = {"abort": send_global_abort, "clear_errors": lambda: send_injector_cmd("CLEAR_ERRORS")}

    shared_widgets = create_shared_widgets(root, command_funcs)
    shared_gui_refs.update(shared_widgets)

    notebook = ttk.Notebook(root)

    injector_widgets = create_injector_tab(notebook, send_injector_cmd, shared_gui_refs)
    shared_gui_refs.update(injector_widgets)

    fillhead_widgets = create_fillhead_tab(notebook, send_fillhead_cmd)
    shared_gui_refs.update(fillhead_widgets)

    notebook.pack(expand=True, fill="both", padx=10, pady=5)
    shared_widgets['shared_bottom_frame'].pack(fill=tk.X, expand=False, padx=10, pady=(0, 10))

    # --- Start Communication Threads ---
    threading.Thread(target=comms.recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=comms.monitor_connections, args=(shared_gui_refs,), daemon=True).start()

    root.mainloop()


if __name__ == "__main__":
    main()
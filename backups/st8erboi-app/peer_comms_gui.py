import tkinter as tk
from tkinter import ttk
import comms  # Used for the send_to_device function reference


def create_peer_comms_tab(notebook, command_funcs):
    """
    Creates a dedicated tab for testing inter-device communication.
    """
    peer_tab = tk.Frame(notebook, bg="#21232b")
    notebook.add(peer_tab, text='  Peer Comms  ')

    send_injector_cmd = command_funcs.get("send_injector")
    send_fillhead_cmd = command_funcs.get("send_fillhead")

    # --- Frame for Injector -> Fillhead commands ---
    inj_to_fh_frame = tk.LabelFrame(peer_tab, text=" Test Commands: Injector -> Fillhead ",
                                    bg="#2a2d3b", fg="#aaddff", font=("Segoe UI", 10, "bold"),
                                    padx=10, pady=10)
    inj_to_fh_frame.pack(fill=tk.X, expand=False, padx=10, pady=(10, 5))

    # --- MOVE Command ---
    move_frame = tk.Frame(inj_to_fh_frame, bg=inj_to_fh_frame['bg'])
    move_frame.pack(fill=tk.X)

    tk.Label(move_frame, text="MOVE_X", bg=move_frame['bg'], fg='white').grid(row=0, column=0, padx=(0, 10))

    dist_var = tk.StringVar(value="15.0")
    vel_var = tk.StringVar(value="25.0")
    accel_var = tk.StringVar(value="100.0")
    torque_var = tk.StringVar(value="20")

    tk.Label(move_frame, text="Dist(mm):", bg=move_frame['bg'], fg='white').grid(row=0, column=1)
    ttk.Entry(move_frame, textvariable=dist_var, width=8).grid(row=0, column=2, padx=(0, 10))

    tk.Label(move_frame, text="Vel(mm/s):", bg=move_frame['bg'], fg='white').grid(row=0, column=3)
    ttk.Entry(move_frame, textvariable=vel_var, width=8).grid(row=0, column=4, padx=(0, 10))

    tk.Label(move_frame, text="Accel(mm/s²):", bg=move_frame['bg'], fg='white').grid(row=0, column=5)
    ttk.Entry(move_frame, textvariable=accel_var, width=8).grid(row=0, column=6, padx=(0, 10))

    tk.Label(move_frame, text="Torque(%):", bg=move_frame['bg'], fg='white').grid(row=0, column=7)
    ttk.Entry(move_frame, textvariable=torque_var, width=8).grid(row=0, column=8, padx=(0, 10))

    move_cmd_str = lambda: f"MOVE_X {dist_var.get()} {vel_var.get()} {accel_var.get()} {torque_var.get()}"
    ttk.Button(move_frame, text="Send MOVE_X to Fillhead", command=lambda: send_fillhead_cmd(move_cmd_str())).grid(
        row=0, column=9, padx=10)

    # --- HOME Command ---
    home_frame = tk.Frame(inj_to_fh_frame, bg=inj_to_fh_frame['bg'])
    home_frame.pack(fill=tk.X, pady=(10, 0))

    tk.Label(home_frame, text="HOME_Y", bg=home_frame['bg'], fg='white').grid(row=0, column=0, padx=(0, 10))

    home_torque_var = tk.StringVar(value="20")
    home_dist_var = tk.StringVar(value="120.0")

    tk.Label(home_frame, text="Torque(%):", bg=home_frame['bg'], fg='white').grid(row=0, column=1)
    ttk.Entry(home_frame, textvariable=home_torque_var, width=8).grid(row=0, column=2, padx=(0, 10))

    tk.Label(home_frame, text="Max Dist(mm):", bg=home_frame['bg'], fg='white').grid(row=0, column=3)
    ttk.Entry(home_frame, textvariable=home_dist_var, width=8).grid(row=0, column=4, padx=(0, 10))

    home_cmd_str = lambda: f"HOME_Y {home_torque_var.get()} {home_dist_var.get()}"
    ttk.Button(home_frame, text="Send HOME_Y to Fillhead", command=lambda: send_fillhead_cmd(home_cmd_str())).grid(
        row=0, column=9, padx=10)

    # --- Frame for Fillhead -> Injector commands (Placeholder) ---
    fh_to_inj_frame = tk.LabelFrame(peer_tab, text=" Test Commands: Fillhead -> Injector (Example) ",
                                    bg="#2a2d3b", fg="#aaddff", font=("Segoe UI", 10, "bold"),
                                    padx=10, pady=10)
    fh_to_inj_frame.pack(fill=tk.X, expand=False, padx=10, pady=10)

    tk.Label(fh_to_inj_frame, text="This section is a placeholder for sending commands to the Injector.",
             bg=fh_to_inj_frame['bg'], fg='white').pack()

    return {}  # This tab doesn't need to return any UI element references for now
import tkinter as tk
from tkinter import ttk


def create_shared_widgets(root, command_funcs):
    """
    Creates the shared UI elements that are not part of any specific tab.
    Returns a dictionary of references to these elements.
    """
    top_frame = tk.Frame(root, bg="#21232b")
    top_frame.pack(fill=tk.X, padx=10, pady=5)

    # --- Main Status Frame ---
    status_frame = tk.Frame(top_frame, bg="#21232b")
    status_frame.pack(side=tk.LEFT, anchor='n')

    status_var_injector = tk.StringVar(value="🔌 Injector Disconnected")
    status_var_fillhead = tk.StringVar(value="🔌 Fillhead Disconnected")

    tk.Label(status_frame, textvariable=status_var_injector, bg="#21232b", fg="white",
             font=("Segoe UI", 8, "bold")).pack(side=tk.TOP, anchor='w')
    tk.Label(status_frame, textvariable=status_var_fillhead, bg="#21232b", fg="white",
             font=("Segoe UI", 8, "bold")).pack(side=tk.TOP, anchor='w')

    # --- Peer Status Frame ---
    peer_status_frame = tk.Frame(top_frame, bg="#21232b")
    peer_status_frame.pack(side=tk.LEFT, anchor='n', padx=20)

    peer_status_injector = tk.StringVar(value="Peer: ---")
    peer_status_fillhead = tk.StringVar(value="Peer: ---")

    tk.Label(peer_status_frame, text="Injector Peer Status:", bg="#21232b", fg="white", font=("Segoe UI", 8)).pack(side=tk.TOP, anchor='w')
    tk.Label(peer_status_frame, textvariable=peer_status_injector, bg="#21232b", fg="#aaddff", font=("Segoe UI", 8)).pack(side=tk.TOP, anchor='w')
    tk.Label(peer_status_frame, text="Fillhead Peer Status:", bg="#21232b", fg="white", font=("Segoe UI", 8)).pack(side=tk.TOP, anchor='w', pady=(5, 0))
    tk.Label(peer_status_frame, textvariable=peer_status_fillhead, bg="#21232b", fg="#aaddff", font=("Segoe UI", 8)).pack(side=tk.TOP, anchor='w')

    action_buttons_frame = tk.Frame(top_frame, bg="#21232b")
    action_buttons_frame.pack(side=tk.RIGHT, padx=10)

    abort_btn = tk.Button(action_buttons_frame, text="🛑 ABORT", bg="#db2828", fg="white", font=("Segoe UI", 9, "bold"),
                          command=command_funcs.get("abort"), relief="raised", bd=3, padx=10, pady=2)
    abort_btn.pack(side=tk.LEFT, padx=(0, 5))
    clear_errors_btn = ttk.Button(action_buttons_frame, text="⚠️ Clear Errors", style='Yellow.TButton',
                                  command=command_funcs.get("clear_errors"))
    clear_errors_btn.pack(side=tk.LEFT, padx=(5, 0), ipady=3)

    bottom_frame = tk.Frame(root, bg="#21232b")

    terminal = tk.Text(bottom_frame, height=8, bg="#1b1e2b", fg="#0f8", insertbackground="white", wrap="word",
                       highlightbackground="#34374b", highlightthickness=1, bd=0, font=("Consolas", 9))
    terminal.pack(fill=tk.X, expand=False, pady=(5, 0))

    options_frame = tk.Frame(bottom_frame, bg="#21232b")
    options_frame.pack(fill=tk.X, pady=(2, 0))

    show_telemetry_var = tk.BooleanVar(value=False)
    show_discovery_var = tk.BooleanVar(value=False)

    style = ttk.Style()
    style.configure("TCheckbutton", background="#21232b", foreground="white", font=("Segoe UI", 8))
    style.map("TCheckbutton", background=[('active', '#21232b')])


    telemetry_check = ttk.Checkbutton(options_frame, text="Show Raw Telemetry", variable=show_telemetry_var,
                                      style="TCheckbutton")
    telemetry_check.pack(side=tk.LEFT, padx=5)

    discovery_check = ttk.Checkbutton(options_frame, text="Show Raw Discovery", variable=show_discovery_var,
                                      style="TCheckbutton")
    discovery_check.pack(side=tk.LEFT, padx=5)

    return {
        'status_var_injector': status_var_injector,
        'status_var_fillhead': status_var_fillhead,
        'peer_status_injector_var': peer_status_injector,
        'peer_status_fillhead_var': peer_status_fillhead,
        'terminal': terminal,
        'terminal_cb': lambda msg: terminal.insert(tk.END, msg) and terminal.see(tk.END),
        'shared_bottom_frame': bottom_frame,
        'show_telemetry_var': show_telemetry_var,
        'show_discovery_var': show_discovery_var
    }
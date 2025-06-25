# shared_gui.py
import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg


def create_shared_widgets(root, send_injector_cmd):
    # Top frame with connection status and abort buttons
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
                          command=lambda: send_injector_cmd("ABORT"), relief="raised", bd=3, padx=10, pady=5)
    abort_btn.pack(side=tk.LEFT, padx=(0, 5))
    clear_errors_btn = ttk.Button(action_buttons_frame, text="⚠️ Clear Errors", style='Yellow.TButton',
                                  command=lambda: send_injector_cmd("CLEAR_ERRORS"))
    clear_errors_btn.pack(side=tk.LEFT, padx=(5, 0))

    bottom_frame = tk.Frame(root, bg="#21232b")

    # Torque Plot
    fig, ax = plt.subplots(figsize=(7, 2.8), facecolor='#21232b')
    line1, = ax.plot([], [], color='#00bfff', label="M0");
    line2, = ax.plot([], [], color='yellow', label="M1")
    line3, = ax.plot([], [], color='#ff8888', label="M2 (Pinch)")
    ax.set_facecolor('#1b1e2b');
    ax.tick_params(axis='x', colors='white');
    ax.tick_params(axis='y', colors='white')
    ax.spines['bottom'].set_color('white');
    ax.spines['left'].set_color('white');
    ax.spines['top'].set_visible(False);
    ax.spines['right'].set_visible(False)
    ax.set_ylabel("Torque (%)", color='white');
    ax.set_ylim(-10, 110);
    ax.legend(facecolor='#21232b', edgecolor='white', labelcolor='white')
    canvas = FigureCanvasTkAgg(fig, master=bottom_frame)
    canvas_widget = canvas.get_tk_widget()
    canvas_widget.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    # Terminal Log
    terminal = tk.Text(bottom_frame, height=8, bg="#1b1e2b", fg="#0f8", insertbackground="white", wrap="word",
                       highlightbackground="#34374b", highlightthickness=1, bd=0, font=("Consolas", 10))
    terminal.pack(fill=tk.X, expand=False, pady=(5, 0))

    return {
        'status_var_injector': status_var_injector,
        'status_var_fillhead': status_var_fillhead,
        'terminal': terminal,
        'terminal_cb': lambda msg: terminal.insert(tk.END, msg) and terminal.see(tk.END),
        'shared_bottom_frame': bottom_frame,
        'torque_plot_canvas': canvas,
        'torque_plot_ax': ax,
        'torque_plot_lines': [line1, line2, line3]
    }
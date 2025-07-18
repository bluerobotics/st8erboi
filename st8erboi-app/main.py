import tkinter as tk
from tkinter import ttk
import threading
import comms
from scripting_gui import create_scripting_area, create_manual_controls_display
from status_panel import create_status_bar

GUI_UPDATE_INTERVAL_MS = 100


def create_shared_widgets(root, command_funcs, shared_gui_refs):
    """
    Creates the shared UI elements that are not part of any specific tab.
    This is now primarily the bottom terminal display.
    Returns a dictionary of references to these elements.
    """
    if 'peer_status_injector_var' not in shared_gui_refs: shared_gui_refs['peer_status_injector_var'] = tk.StringVar(
        value='Peer: ---')
    if 'peer_status_fillhead_var' not in shared_gui_refs: shared_gui_refs['peer_status_fillhead_var'] = tk.StringVar(
        value='Peer: ---')

    # --- Bottom Frame (Terminal) ---
    bottom_frame = tk.Frame(root, bg="#21232b")

    terminal_container = tk.Frame(bottom_frame)
    terminal_container.pack(fill=tk.X, expand=True, pady=(5, 0))
    terminal_container.grid_rowconfigure(0, weight=1)
    terminal_container.grid_columnconfigure(0, weight=1)

    terminal = tk.Text(terminal_container, height=8, bg="#1b1e2b", fg="#0f8", insertbackground="white", wrap="word",
                       highlightbackground="#34374b", highlightthickness=1, bd=0, font=("Consolas", 9))
    terminal.grid(row=0, column=0, sticky="nsew")

    scrollbar = ttk.Scrollbar(terminal_container, command=terminal.yview)
    scrollbar.grid(row=0, column=1, sticky='ns')
    terminal['yscrollcommand'] = scrollbar.set

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
        'terminal': terminal,
        'terminal_cb': lambda msg: terminal.insert(tk.END, msg) and terminal.see(tk.END),
        'shared_bottom_frame': bottom_frame,
        'show_telemetry_var': show_telemetry_var,
        'show_discovery_var': show_discovery_var
    }


def configure_styles():
    """
    Configures all the ttk styles for the application for a modern look.
    """
    style = ttk.Style()
    try:
        style.theme_use('clam')
    except tk.TclError:
        print("Warning: 'clam' theme not available. Using default.")

    style.layout('TButton',
                 [('Button.button', {'children': [('Button.focus', {'children': [
                     ('Button.padding', {'children': [('Button.label', {'side': 'left', 'expand': 1})]})]})]})])

    # Enable/Disable Buttons
    style.configure("Green.TButton", background='#21ba45', foreground='white', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Green.TButton", background=[('pressed', '#198734'), ('active', '#25cc4c')])

    style.configure("Red.TButton", background='#db2828', foreground='white', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Red.TButton", background=[('pressed', '#b32424'), ('active', '#e03d3d')])

    style.configure("Neutral.TButton", background='#4a5568', foreground='white', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Neutral.TButton", background=[('pressed', '#2d3748'), ('active', '#5a6268')])

    style.configure("Yellow.TButton", background='#f2c037', foreground='black', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Yellow.TButton", background=[('pressed', '#d69e2e'), ('active', '#f6e05e')])

    # General Purpose Buttons
    style.configure("Small.TButton", background='#718096', foreground='white', font=('Segoe UI', 8), borderwidth=0)
    style.map("Small.TButton", background=[('pressed', '#4a5568'), ('active', '#8a97aa')])

    # Jog Buttons
    style.configure("Jog.TButton", background='#4299e1', foreground='white', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0, padding=5)
    style.map("Jog.TButton", background=[('pressed', '#2b6cb0'), ('active', '#63b3ed')])

    # Feed/Purge Buttons
    style.configure("Start.TButton", background='#48bb78', foreground='white', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Start.TButton", background=[('pressed', '#38a169'), ('active', '#68d391')])

    style.configure("Pause.TButton", background='#f6ad55', foreground='white', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Pause.TButton", background=[('pressed', '#dd6b20'), ('active', '#fbd38d')])

    style.configure("Resume.TButton", background='#4299e1', foreground='white', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Resume.TButton", background=[('pressed', '#2b6cb0'), ('active', '#63b3ed')])

    style.configure("Cancel.TButton", background='#f56565', foreground='white', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Cancel.TButton", background=[('pressed', '#c53030'), ('active', '#fc8181')])


def main():
    root = tk.Tk()
    root.title("Multi-Device Controller")
    root.configure(bg="#21232b")
    root.geometry("1600x950")

    configure_styles()

    shared_gui_refs = {
        'status_var_injector': tk.StringVar(value='🔌 Injector Disconnected'),
        'status_var_fillhead': tk.StringVar(value='🔌 Fillhead Disconnected')
    }

    send_injector_cmd = lambda msg: comms.send_to_device("injector", msg, shared_gui_refs)
    send_fillhead_cmd = lambda msg: comms.send_to_device("fillhead", msg, shared_gui_refs)

    def send_global_abort():
        if 'terminal_cb' in shared_gui_refs:
            comms.log_to_terminal("--- GLOBAL ABORT TRIGGERED ---", shared_gui_refs.get('terminal_cb'))
        send_injector_cmd("ABORT")
        send_fillhead_cmd("ABORT")

    command_funcs = {
        "abort": send_global_abort,
        "clear_errors": lambda: send_injector_cmd("CLEAR_ERRORS"),
        "send_injector": send_injector_cmd,
        "send_fillhead": send_fillhead_cmd
    }

    # --- Main Layout Frames ---
    left_bar_frame = tk.Frame(root, bg="#21232b", width=350)
    left_bar_frame.pack(side=tk.LEFT, fill=tk.Y, padx=(10, 0), pady=10)
    left_bar_frame.pack_propagate(False)

    main_content_frame = tk.Frame(root, bg="#21232b")
    main_content_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

    # --- Populate Left Bar ---
    create_status_bar(left_bar_frame, shared_gui_refs)
    manual_control_widgets = create_manual_controls_display(left_bar_frame, command_funcs, shared_gui_refs)
    shared_gui_refs.update(manual_control_widgets)

    abort_btn = tk.Button(left_bar_frame, text="🛑 ABORT MOVE", bg="#db2828", fg="white", font=("Segoe UI", 10, "bold"),
                          command=command_funcs.get("abort"), relief="raised", bd=3, padx=10, pady=5)
    abort_btn.pack(side=tk.BOTTOM, fill=tk.X, pady=(10, 0), ipady=5)

    # --- Populate Main Content Area ---
    shared_widgets = create_shared_widgets(main_content_frame, command_funcs, shared_gui_refs)
    shared_gui_refs.update(shared_widgets)

    scripting_widgets = create_scripting_area(main_content_frame, command_funcs, shared_gui_refs)
    shared_gui_refs.update(scripting_widgets)

    shared_widgets['shared_bottom_frame'].pack(side=tk.BOTTOM, fill=tk.X, expand=False, padx=10, pady=(0, 10))

    def telemetry_requester_loop():
        if comms.devices["injector"]["connected"]:
            comms.send_to_device("injector", "REQUEST_TELEM", shared_gui_refs)
        if comms.devices["fillhead"]["connected"]:
            comms.send_to_device("fillhead", "REQUEST_TELEM", shared_gui_refs)
        root.after(GUI_UPDATE_INTERVAL_MS, telemetry_requester_loop)

    threading.Thread(target=comms.recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=comms.monitor_connections, args=(shared_gui_refs,), daemon=True).start()
    root.after(1000, telemetry_requester_loop)

    root.mainloop()


if __name__ == "__main__":
    main()

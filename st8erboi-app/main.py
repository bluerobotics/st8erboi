import tkinter as tk
from tkinter import ttk
import threading
import comms
import sv_ttk  # Import the new theme library
from scripting_gui import create_scripting_interface
from manual_controls import create_manual_controls_display
from status_panel import create_status_bar
from terminal import create_terminal_panel
from styles import configure_styles
from top_menu import create_top_menu

GUI_UPDATE_INTERVAL_MS = 100


def main():
    """
    Initializes the main application window, creates the primary UI layout,
    and starts the communication threads.
    """
    root = tk.Tk()
    root.title("Multi-Device Controller")

    # Apply the theme BEFORE creating any widgets
    sv_ttk.set_theme("dark")

    root.geometry("1600x950")

    # Configure custom styles after the theme is set
    configure_styles()

    # --- Shared State and Functions ---
    shared_gui_refs = {
        'status_var_injector': tk.StringVar(value='🔌 Injector Disconnected'),
        'status_var_fillhead': tk.StringVar(value='🔌 Fillhead Disconnected'),
        'enabled_state1_var': tk.StringVar(value='Disabled'),
        'enabled_state2_var': tk.StringVar(value='Disabled'),
        'enabled_state3_var': tk.StringVar(value='Disabled'),
        'fh_enabled_m0_var': tk.StringVar(value='Disabled'),
        'fh_enabled_m1_var': tk.StringVar(value='Disabled'),
        'fh_enabled_m2_var': tk.StringVar(value='Disabled'),
        'fh_enabled_m3_var': tk.StringVar(value='Disabled'),
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
    # The left bar holds status and manual controls.
    left_bar_frame = ttk.Frame(root, width=350)
    left_bar_frame.pack(side=tk.LEFT, fill=tk.Y, padx=(10, 0), pady=10)
    left_bar_frame.pack_propagate(False)

    # The terminal is packed at the bottom of the root window.
    terminal_widgets = create_terminal_panel(root, shared_gui_refs)
    shared_gui_refs.update(terminal_widgets)
    terminal_widgets['terminal_frame'].pack(side=tk.BOTTOM, fill=tk.X, expand=False, pady=(0, 10))

    # The main content frame holds the scripting interface.
    main_content_frame = ttk.Frame(root)
    main_content_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

    # --- Populate UI Components ---

    # 1. Scripting Interface (must be created before menu to get file commands)
    scripting_widgets = create_scripting_interface(main_content_frame, command_funcs, shared_gui_refs)
    shared_gui_refs.update(scripting_widgets)

    # 2. Top Menu Bar (created first, but needs file_commands)
    file_commands = scripting_widgets.get('file_commands', {})
    menu_widgets = create_top_menu(root, file_commands)

    # Pass the recent_files_menu object to the scripting interface so it can be updated
    if 'update_recent_menu_callback' in scripting_widgets:
        scripting_widgets['update_recent_menu_callback'](menu_widgets['recent_files_menu'])

    # 3. Status Panel
    status_widgets = create_status_bar(left_bar_frame, shared_gui_refs)
    shared_gui_refs.update(status_widgets)

    # 4. Manual Controls Panel
    manual_control_widgets = create_manual_controls_display(left_bar_frame, command_funcs, shared_gui_refs)
    shared_gui_refs.update(manual_control_widgets)

    # 5. Abort Button
    abort_btn = ttk.Button(left_bar_frame, text="🛑 ABORT MOVE", style="Red.TButton",
                           command=command_funcs.get("abort"))
    abort_btn.pack(side=tk.BOTTOM, fill=tk.X, pady=(10, 0), ipady=5, padx=5)

    # --- Start Communication Threads ---
    threading.Thread(target=comms.recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=comms.monitor_connections, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=comms.telemetry_requester_loop, args=(shared_gui_refs, GUI_UPDATE_INTERVAL_MS),
                     daemon=True).start()

    root.mainloop()


if __name__ == "__main__":
    main()

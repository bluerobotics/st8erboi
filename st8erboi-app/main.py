import tkinter as tk
from tkinter import ttk
import threading
import comms
from shared_gui import create_shared_widgets
from scripting_gui import create_main_view

GUI_UPDATE_INTERVAL_MS = 100

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
    style.configure("Green.TButton", background='#21ba45', foreground='white', font=('Segoe UI', 9, 'bold'), borderwidth=0)
    style.map("Green.TButton", background=[('pressed', '#198734'), ('active', '#25cc4c')])

    style.configure("Red.TButton", background='#db2828', foreground='white', font=('Segoe UI', 9, 'bold'), borderwidth=0)
    style.map("Red.TButton", background=[('pressed', '#b32424'), ('active', '#e03d3d')])

    style.configure("Neutral.TButton", background='#4a5568', foreground='white', font=('Segoe UI', 9, 'bold'), borderwidth=0)
    style.map("Neutral.TButton", background=[('pressed', '#2d3748'), ('active', '#5a6268')])

    # General Purpose Buttons
    style.configure("Small.TButton", background='#718096', foreground='white', font=('Segoe UI', 8), borderwidth=0)
    style.map("Small.TButton", background=[('pressed', '#4a5568'), ('active', '#8a97aa')])

    # Jog Buttons
    style.configure("Jog.TButton", background='#4299e1', foreground='white', font=('Segoe UI', 9, 'bold'), borderwidth=0, padding=5)
    style.map("Jog.TButton", background=[('pressed', '#2b6cb0'), ('active', '#63b3ed')])

    # Feed/Purge Buttons
    style.configure("Start.TButton", background='#48bb78', foreground='white', font=('Segoe UI', 9, 'bold'), borderwidth=0)
    style.map("Start.TButton", background=[('pressed', '#38a169'), ('active', '#68d391')])

    style.configure("Pause.TButton", background='#f6ad55', foreground='white', font=('Segoe UI', 9, 'bold'), borderwidth=0)
    style.map("Pause.TButton", background=[('pressed', '#dd6b20'), ('active', '#fbd38d')])

    style.configure("Resume.TButton", background='#4299e1', foreground='white', font=('Segoe UI', 9, 'bold'), borderwidth=0)
    style.map("Resume.TButton", background=[('pressed', '#2b6cb0'), ('active', '#63b3ed')])

    style.configure("Cancel.TButton", background='#f56565', foreground='white', font=('Segoe UI', 9, 'bold'), borderwidth=0)
    style.map("Cancel.TButton", background=[('pressed', '#c53030'), ('active', '#fc8181')])


def main():
    root = tk.Tk()
    root.title("Multi-Device Controller")
    root.configure(bg="#21232b")
    root.geometry("1400x950")  # Set a default size for the application window

    configure_styles()

    shared_gui_refs = {}

    send_injector_cmd = lambda msg: comms.send_to_device("injector", msg, shared_gui_refs)
    send_fillhead_cmd = lambda msg: comms.send_to_device("fillhead", msg, shared_gui_refs)

    def send_global_abort():
        comms.log_to_terminal("--- GLOBAL ABORT TRIGGERED ---", shared_gui_refs.get('terminal_cb'))
        send_injector_cmd("ABORT")
        send_fillhead_cmd("ABORT")

    command_funcs = {
        "abort": send_global_abort,
        "clear_errors": lambda: send_injector_cmd("CLEAR_ERRORS"),
        "send_injector": send_injector_cmd,
        "send_fillhead": send_fillhead_cmd
    }

    # Create shared widgets like the terminal first
    shared_widgets = create_shared_widgets(root, command_funcs)
    shared_gui_refs.update(shared_widgets)

    # Create the main application view which now contains everything else
    main_view_widgets = create_main_view(root, command_funcs, shared_gui_refs)
    shared_gui_refs.update(main_view_widgets)

    # Pack the shared bottom frame (terminal, etc.) at the bottom
    shared_widgets['shared_bottom_frame'].pack(side=tk.BOTTOM, fill=tk.X, expand=False, padx=10, pady=(0, 10))

    def update_torque_bar(canvas, bar_item, text_item, torque_val, max_height=100):
        if not all([canvas, bar_item, text_item]): return
        if max_height <= 20: max_height = 100
        clamped_torque = max(0, min(100, torque_val))
        bar_height = (clamped_torque / 100.0) * (max_height - 20)
        y0 = max_height - 10 - bar_height
        y1 = max_height - 10
        canvas.coords(bar_item, 10, y0, 30, y1)
        canvas.itemconfig(text_item, text=f"{torque_val:.1f}%")
        color = '#21ba45'
        if torque_val > 85: color = '#db2828'
        elif torque_val > 60: color = '#f2c037'
        canvas.itemconfig(bar_item, fill=color)

    def update_injector_torque_bars():
        for i in range(1, 4):
            try:
                torque_str = shared_gui_refs.get(f'torque_value{i}_var').get()
                torque_val = float(torque_str)
                canvas = shared_gui_refs.get(f'injector_torque_canvas{i}')
                update_torque_bar(
                    canvas,
                    shared_gui_refs.get(f'injector_torque_bar{i}'),
                    shared_gui_refs.get(f'injector_torque_text{i}'),
                    torque_val,
                    max_height=canvas.winfo_height()
                )
            except (ValueError, AttributeError, tk.TclError, KeyError):
                continue

    def update_fillhead_torque_bars():
        for i in range(4):
            try:
                torque_str = shared_gui_refs.get(f'fh_torque_m{i}_var').get()
                torque_val = float(torque_str)
                canvas = shared_gui_refs.get(f'fh_torque_canvas{i}')
                update_torque_bar(
                    canvas,
                    shared_gui_refs.get(f'fh_torque_bar{i}'),
                    shared_gui_refs.get(f'fh_torque_text{i}'),
                    torque_val,
                    max_height=canvas.winfo_height()
                )
            except (ValueError, AttributeError, tk.TclError, KeyError):
                continue

    def periodic_gui_updater():
        update_injector_torque_bars()
        update_fillhead_torque_bars()
        root.after(GUI_UPDATE_INTERVAL_MS, periodic_gui_updater)

    def telemetry_requester_loop():
        if comms.devices["injector"]["connected"]:
            comms.send_to_device("injector", "REQUEST_TELEM", shared_gui_refs)
        if comms.devices["fillhead"]["connected"]:
            comms.send_to_device("fillhead", "REQUEST_TELEM", shared_gui_refs)
        root.after(GUI_UPDATE_INTERVAL_MS, telemetry_requester_loop)

    # Start threads and loops
    threading.Thread(target=comms.recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=comms.monitor_connections, args=(shared_gui_refs,), daemon=True).start()
    root.after(250, periodic_gui_updater)
    root.after(1000, telemetry_requester_loop)

    root.mainloop()

if __name__ == "__main__":
    main()

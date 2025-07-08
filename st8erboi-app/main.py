import tkinter as tk
from tkinter import ttk
import threading
import time
import datetime
import math
# Removed matplotlib and numpy imports

# Import our communications module
import comms

# Import GUI builder functions from their separate files
from shared_gui import create_shared_widgets
from injector_gui import create_injector_tab
from fillhead_gui import create_fillhead_tab
from peer_comms_gui import create_peer_comms_tab

# --- CONFIGURATION CONSTANTS ---
GUI_UPDATE_INTERVAL_MS = 100  # Update all lightweight UI elements 10 times/sec


# --- UPDATED: More robust style configuration ---
def configure_styles():
    """Defines custom colors and styles for ttk widgets."""
    style = ttk.Style()

    try:
        style.theme_use('clam')
    except tk.TclError:
        print("Warning: 'clam' theme not available. Using default.")

    style.layout('TButton',
                 [('Button.button', {'children':
                                         [('Button.focus', {'children':
                                                                [('Button.padding', {'children':
                                                                                         [('Button.label',
                                                                                           {'side': 'left',
                                                                                            'expand': 1})]
                                                                                     })]
                                                            })]
                                     })]
                 )
    style.configure("Green.TButton",
                    background='#21ba45',
                    foreground='white',
                    font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Green.TButton",
              background=[('pressed', '#198734'), ('active', '#25cc4c')])

    style.configure("Red.TButton",
                    background='#db2828',
                    foreground='white',
                    font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Red.TButton",
              background=[('pressed', '#b32424'), ('active', 'red')])

    style.configure("Neutral.TButton",
                    background='#495057',
                    foreground='#f8f9fa',
                    borderwidth=0)
    style.map("Neutral.TButton",
              background=[('pressed', '#343a40'), ('active', '#5a6268')])


# --- Main Application ---
def main():
    root = tk.Tk()
    root.title("Multi-Device Controller")
    root.configure(bg="#21232b")

    configure_styles()

    # This dictionary no longer needs to store plot history
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

    shared_widgets = create_shared_widgets(root, command_funcs)
    shared_gui_refs.update(shared_widgets)

    notebook = ttk.Notebook(root)

    injector_widgets = create_injector_tab(notebook, send_injector_cmd, shared_gui_refs)
    shared_gui_refs.update(injector_widgets)

    fillhead_widgets = create_fillhead_tab(notebook, send_fillhead_cmd, shared_gui_refs)
    shared_gui_refs.update(fillhead_widgets)

    peer_comms_widgets = create_peer_comms_tab(notebook, command_funcs)
    shared_gui_refs.update(peer_comms_widgets)

    notebook.pack(expand=True, fill="both", padx=10, pady=5)
    shared_widgets['shared_bottom_frame'].pack(fill=tk.X, expand=False, padx=10, pady=(0, 10))

    def update_torque_bar(canvas, bar_item, text_item, torque_val):
        """Updates a single torque bar and its text."""
        if not all([canvas, bar_item, text_item]):
            return

        # Clamp torque value between 0 and 100 for bar display
        clamped_torque = max(0, min(100, torque_val))

        # Calculate bar height (canvas is 100px high, from y=10 to y=110)
        bar_height = (clamped_torque / 100.0) * 100
        y0 = 110 - bar_height

        # Update bar coordinates
        canvas.coords(bar_item, 10, y0, 30, 110)

        # Update text
        canvas.itemconfig(text_item, text=f"{torque_val:.1f}%")

        # Update color based on value
        if torque_val > 85:
            color = '#db2828'  # Red
        elif torque_val > 60:
            color = '#f2c037'  # Yellow
        else:
            color = '#21ba45'  # Green
        canvas.itemconfig(bar_item, fill=color)

    def update_injector_torque_bars():
        for i in range(1, 4):
            try:
                torque_str = shared_gui_refs.get(f'torque_value{i}_var').get()
                torque_val = float(torque_str)
                update_torque_bar(
                    shared_gui_refs.get(f'injector_torque_canvas{i}'),
                    shared_gui_refs.get(f'injector_torque_bar{i}'),
                    shared_gui_refs.get(f'injector_torque_text{i}'),
                    torque_val
                )
            except (ValueError, AttributeError, tk.TclError):
                continue  # Skip if widgets or vars don't exist yet

    def update_fillhead_torque_bars():
        for i in range(4):
            try:
                torque_str = shared_gui_refs.get(f'fh_torque_m{i}_var').get()
                torque_val = float(torque_str)
                update_torque_bar(
                    shared_gui_refs.get(f'fh_torque_canvas{i}'),
                    shared_gui_refs.get(f'fh_torque_bar{i}'),
                    shared_gui_refs.get(f'fh_torque_text{i}'),
                    torque_val
                )
            except (ValueError, AttributeError, tk.TclError):
                continue

    def periodic_gui_updater():
        """Handles all lightweight GUI updates."""
        update_injector_torque_bars()
        update_fillhead_torque_bars()
        # The position plot was removed, so this is no longer needed
        # update_fillhead_position_plot()
        root.after(GUI_UPDATE_INTERVAL_MS, periodic_gui_updater)

    def telemetry_requester_loop():
        """Requests new data from devices."""
        if comms.devices["injector"]["connected"]:
            comms.send_to_device("injector", "REQUEST_TELEM", shared_gui_refs)
        if comms.devices["fillhead"]["connected"]:
            comms.send_to_device("fillhead", "REQUEST_TELEM", shared_gui_refs)
        root.after(GUI_UPDATE_INTERVAL_MS, telemetry_requester_loop)

    # --- Start all loops ---
    threading.Thread(target=comms.recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=comms.monitor_connections, args=(shared_gui_refs,), daemon=True).start()

    root.after(250, periodic_gui_updater)
    root.after(1000, telemetry_requester_loop)

    root.mainloop()


if __name__ == "__main__":
    main()

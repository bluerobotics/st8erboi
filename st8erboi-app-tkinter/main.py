import tkinter as tk
from tkinter import ttk
import threading
import comms
from scripting_gui import create_scripting_interface
from manual_controls import create_manual_controls_display
from status_panel import create_status_bar
from terminal import create_terminal_panel
from styles import configure_styles
from top_menu import create_top_menu
import json
import os

GUI_UPDATE_INTERVAL_MS = 100


class MainApplication:
    def __init__(self, root):
        self.root = root
        self.root.title("Multi-Device Controller")
        self.root.configure(bg="#21232b")
        
        # --- Maximize Window on Start ---
        self.root.state('zoomed')

        self.autosave_var = tk.BooleanVar(value=True)
        self.shared_gui_refs = self.initialize_shared_variables()
        
        # The command_funcs need to be initialized after comms functions are available
        self.command_funcs = self.initialize_command_functions()
        self.shared_gui_refs['command_funcs'] = self.command_funcs

        self.setup_styles()
        self.create_widgets()

        # --- Bind the unsaved changes check to the window's close button ---
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        
        self.load_last_script()

    def initialize_shared_variables(self):
        """
        Initializes the shared state and functions for the application.
        """
        # --- Shared State and Functions ---
        shared_gui_refs = {
            # --- Connection and Global State ---
            'status_var_fillhead': tk.StringVar(value='🔌 Fillhead Disconnected'),
            'status_var_gantry': tk.StringVar(value='🔌 Gantry Disconnected'),
            'main_state_var': tk.StringVar(value='---'),
            'feed_state_var': tk.StringVar(value='---'),
            'error_state_var': tk.StringVar(value='No Error'),

            # --- Fillhead Enabled States ---
            'enabled_state1_var': tk.StringVar(value='Disabled'),
            'enabled_state2_var': tk.StringVar(value='Disabled'),
            'enabled_state3_var': tk.StringVar(value='Disabled'),

            # --- Fillhead Position, Homing, and Dispensing ---
            'pos_mm0_var': tk.StringVar(value='---'),
            'pos_mm1_var': tk.StringVar(value='---'),
            'homed0_var': tk.StringVar(value='Not Homed'),
            'homed1_var': tk.StringVar(value='Not Homed'),
            'machine_steps_var': tk.StringVar(value='---'),
            'cartridge_steps_var': tk.StringVar(value='---'),
            'heater_mode_var': tk.StringVar(value='---'),
            'vacuum_state_var': tk.StringVar(value='---'),
            'inject_cumulative_ml_var': tk.StringVar(value='---'),
            'inject_active_ml_var': tk.StringVar(value='---'),

            # --- Fillhead Component States ---
            'fillhead_injector_state_var': tk.StringVar(value='---'),
            'fillhead_inj_valve_state_var': tk.StringVar(value='---'),
            'fillhead_vac_valve_state_var': tk.StringVar(value='---'),
            'fillhead_heater_state_var': tk.StringVar(value='---'),
            'fillhead_vacuum_state_var': tk.StringVar(value='---'),

            # --- Fillhead Torque Values ---
            'torque0_var': tk.DoubleVar(value=0.0),
            'torque1_var': tk.DoubleVar(value=0.0),
            'torque2_var': tk.DoubleVar(value=0.0),
            'torque3_var': tk.DoubleVar(value=0.0),

            # --- Gantry Enabled States ---
            'fh_enabled_m0_var': tk.StringVar(value='Disabled'),
            'fh_enabled_m1_var': tk.StringVar(value='Disabled'),
            'fh_enabled_m2_var': tk.StringVar(value='Disabled'),
            'fh_enabled_m3_var': tk.StringVar(value='Disabled'),

            # --- Gantry Position and Homing ---
            'fh_pos_m0_var': tk.StringVar(value='---'),
            'fh_pos_m1_var': tk.StringVar(value='---'),
            'fh_pos_m2_var': tk.StringVar(value='---'),
            'fh_pos_m3_var': tk.StringVar(value='---'),
            'fh_homed_m0_var': tk.StringVar(value='Not Homed'),
            'fh_homed_m1_var': tk.StringVar(value='Not Homed'),
            'fh_homed_m2_var': tk.StringVar(value='Not Homed'),
            'fh_homed_m3_var': tk.StringVar(value='Not Homed'),

            # --- Gantry Axis States ---
            'fh_state_x_var': tk.StringVar(value='---'),
            'fh_state_y_var': tk.StringVar(value='---'),
            'fh_state_z_var': tk.StringVar(value='---'),
            'fh_state_var': tk.StringVar(value='---'), # Used by status_panel

            # --- Gantry Torque Values ---
            'fh_torque_m0_var': tk.DoubleVar(value=0.0),
            'fh_torque_m1_var': tk.DoubleVar(value=0.0),
            'fh_torque_m2_var': tk.DoubleVar(value=0.0),
            'fh_torque_m3_var': tk.DoubleVar(value=0.0),

            # --- Fillhead Pinch Valve Variables ---
            'inj_valve_pos_var': tk.StringVar(value='---'),
            'inj_valve_homed_var': tk.StringVar(value='Not Homed'),
            'vac_valve_pos_var': tk.StringVar(value='---'),
            'vac_valve_homed_var': tk.StringVar(value='Not Homed'),

            # --- System Status Variables (Heater, Vacuum, etc.) ---
            'vacuum_psig_var': tk.StringVar(value='0.00 PSIG'),
            'temp_c_var': tk.StringVar(value='0.0 °C'),
            'pid_setpoint_var': tk.StringVar(value='25.0'), # Default setpoint

            # Heater variables
            'heater_setpoint_var': tk.StringVar(value='70.0'),
            'heater_kp_var': tk.StringVar(value='60.0'),
            'heater_ki_var': tk.StringVar(value='2.5'),
            'heater_kd_var': tk.StringVar(value='40.0'),

            # Vacuum variables
            'vac_target_var': tk.StringVar(value='-14.0'),
            'vac_timeout_var': tk.StringVar(value='30'),
            'vac_leak_delta_var': tk.StringVar(value='0.1'),
            'vac_leak_duration_var': tk.StringVar(value='10')
        }

        send_fillhead_cmd = lambda msg: comms.send_to_device("fillhead", msg, shared_gui_refs)
        send_gantry_cmd = lambda msg: comms.send_to_device("gantry", msg, shared_gui_refs)

        def send_global_abort():
            if 'terminal_cb' in shared_gui_refs:
                comms.log_to_terminal("--- GLOBAL ABORT TRIGGERED ---", shared_gui_refs.get('terminal_cb'))
            send_fillhead_cmd("ABORT")
            send_gantry_cmd("ABORT")

        command_funcs = {
            "abort": send_global_abort,
            "clear_errors": lambda: send_fillhead_cmd("CLEAR_ERRORS"),
            "send_fillhead": send_fillhead_cmd,
            "send_gantry": send_gantry_cmd
        }
        return shared_gui_refs

    def initialize_command_functions(self):
        send_fillhead_cmd = lambda msg: comms.send_to_device("fillhead", msg, self.shared_gui_refs)
        send_gantry_cmd = lambda msg: comms.send_to_device("gantry", msg, self.shared_gui_refs)

        def send_global_abort():
            if 'terminal_cb' in self.shared_gui_refs:
                comms.log_to_terminal("--- GLOBAL ABORT TRIGGERED ---", self.shared_gui_refs.get('terminal_cb'))
            send_fillhead_cmd("ABORT")
            send_gantry_cmd("ABORT")

        return {
            "abort": send_global_abort,
            "clear_errors": lambda: send_fillhead_cmd("CLEAR_ERRORS"),
            "send_fillhead": send_fillhead_cmd,
            "send_gantry": send_gantry_cmd
        }

    def setup_styles(self):
        """
        Configures the application's styles.
        """
        configure_styles()

    def create_widgets(self):
        """
        Creates the main UI layout and populates it with components.
        """
        # --- Main Layout Frames ---
        left_bar_frame = tk.Frame(self.root, bg="#21232b", width=350)
        left_bar_frame.pack(side=tk.LEFT, fill=tk.Y, padx=(10, 0), pady=10)
        left_bar_frame.pack_propagate(False)

        manual_control_widgets = create_manual_controls_display(self.root, self.command_funcs, self.shared_gui_refs)
        manual_control_widgets['main_container'].pack(side=tk.LEFT, fill=tk.Y, pady=10, padx=(10, 0))

        terminal_widgets = create_terminal_panel(self.root, self.shared_gui_refs)
        self.shared_gui_refs.update(terminal_widgets)
        terminal_widgets['terminal_frame'].pack(side=tk.BOTTOM, fill=tk.X, expand=False, pady=(0, 10))

        main_content_frame = tk.Frame(self.root, bg="#21232b")
        main_content_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # --- Populate UI Components ---
        self.scripting_gui_refs = create_scripting_interface(main_content_frame, self.command_funcs, self.shared_gui_refs, self.autosave_var)
        self.shared_gui_refs.update(self.scripting_gui_refs)

        file_commands = self.scripting_gui_refs.get('file_commands', {})
        menu_widgets = create_top_menu(self.root, file_commands, self.autosave_var)
        self.recent_files_menu = menu_widgets['recent_files_menu']
        self.scripting_gui_refs['update_recent_menu_callback'](self.recent_files_menu)

        status_widgets = create_status_bar(left_bar_frame, self.shared_gui_refs)
        self.shared_gui_refs.update(status_widgets)

        abort_btn = ttk.Button(left_bar_frame, text="🛑 ABORT ALL", style="Abort.TButton", command=self.command_funcs.get("abort"))
        abort_btn.pack(side=tk.BOTTOM, fill=tk.X, pady=(10, 0))

    def setup_menu(self):
        """
        Sets up the top menu bar.
        """
        pass # This method is not fully implemented in the original file, so it's left empty.

    def on_closing(self):
        """
        Handles the window close event, checking for unsaved changes before exiting.
        """
        # The check_unsaved function from scripting_gui shows a dialog and returns True if it's safe to close
        if self.scripting_gui_refs['check_unsaved']():
            self.root.destroy()

    def load_last_script(self):
        """
        Loads the most recently opened script on startup if one exists.
        """
        try:
            with open("recent_files.json", 'r') as f:
                recent_files = json.load(f)
            if recent_files:
                last_script_path = recent_files[0]
                if os.path.exists(last_script_path):
                    # We need to call this after the main loop has started processing events
                    self.root.after(100, lambda: self.scripting_gui_refs['load_specific_script'](last_script_path))
        except (FileNotFoundError, json.JSONDecodeError, IndexError):
            # No recent files, or file is corrupt. Do nothing.
            pass

    def run(self):
        """
        Starts the communication threads and the main event loop.
        """
        # --- Start Communication Threads ---
        threading.Thread(target=comms.recv_loop, args=(self.shared_gui_refs,), daemon=True).start()
        threading.Thread(target=comms.monitor_connections, args=(self.shared_gui_refs,), daemon=True).start()

        self.root.mainloop()


def main():
    """
    Initializes the main application window, creates the primary UI layout,
    and starts the communication threads.
    """
    root = tk.Tk()
    app = MainApplication(root)
    app.run()


if __name__ == "__main__":
    main()
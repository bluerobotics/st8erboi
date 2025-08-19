import tkinter as tk
from tkinter import ttk


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

    # Abort Button Style
    style.configure("Abort.TButton",
                    background='#c53030',
                    foreground='white',
                    font=('Segoe UI', 12, 'bold'),
                    borderwidth=2,
                    padding=(10, 8))
    style.map("Abort.TButton",
              background=[('pressed', '#9b2c2c'), ('active', '#e53e3e')],
              relief=[('pressed', 'sunken'), ('!pressed', 'raised')])

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

    # --- New Toggle Button Style for Single Block ---
    style.configure("OrangeToggle.TButton",
                    foreground="white",
                    background="#718096",  # Default OFF color
                    font=('Segoe UI', 9, 'bold'),
                    padding=5)
    style.map("OrangeToggle.TButton",
              background=[('selected', '#f2c037'), ('active', '#8a97aa')],
              foreground=[('selected', 'black')]
              )

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

    # TCheckbutton style from terminal.py (and others)
    style.configure("TCheckbutton", background="#21232b", foreground="white", font=("Segoe UI", 8))
    style.map("TCheckbutton", background=[('active', '#21232b')])

    # Treeview styles from scripting_gui
    style.configure("Treeview", background="#1b1e2b", foreground="white", fieldbackground="#1b1e2b",
                    font=('Segoe UI', 8));
    style.configure("Treeview.Heading", font=('Segoe UI', 9, 'bold'));
    style.map("Treeview", background=[('selected', '#0078d7')])

    # Progress Bar for Torque (Horizontal)
    style.configure("Torque.Horizontal.TProgressbar",
                    troughcolor='#333745',
                    background='#4299e1',
                    thickness=15,
                    borderwidth=0)
    style.map("Torque.Horizontal.TProgressbar",
              background=[('active', '#63b3ed')])

    # Progress Bar for Torque (Vertical)
    style.configure("Green.Vertical.TProgressbar",
                    troughcolor='#333745',
                    background='#21ba45',
                    thickness=25,
                    borderwidth=0)
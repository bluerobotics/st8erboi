import socket
import threading
import tkinter as tk
from tkinter import messagebox

CLEARCORE_PORT = 8888
TERMINAL_PORT = 9999
BUFFER_SIZE = 1024

clearcore_ip = None
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', TERMINAL_PORT))

def receive_loop():
    global clearcore_ip
    while True:
        try:
            data, addr = sock.recvfrom(BUFFER_SIZE)
            msg = data.decode().strip()
            if msg == "CLEARCORE_ACK":
                clearcore_ip = addr[0]
                status_label.config(text=f"Connected to {clearcore_ip}")
            elif msg.startswith("torque:"):
                parts = msg.split(',')
                torque_label.config(text=parts[0].strip())

                hlfb = parts[1].split(':')[1].strip()
                enabled = parts[2].split(':')[1].strip()

                if enabled == "0":
                    motor_status.config(text="Motor: DISABLED", bg="red")
                elif hlfb == "1":
                    motor_status.config(text="Motor: ENABLED (In Position)", bg="green")
                else:
                    motor_status.config(text="Motor: ENABLED (Moving)", bg="yellow")

            else:
                log_box.insert(tk.END, f"{msg}\n")
                log_box.see(tk.END)
        except Exception as e:
            print(f"Receive error: {e}")

def discover_clearcore():
    msg = f"DISCOVER_CLEARCORE:PORT={TERMINAL_PORT}"
    bcast = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    bcast.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    bcast.sendto(msg.encode(), ('<broadcast>', CLEARCORE_PORT))
    bcast.close()

def send_udp_command(command):
    if clearcore_ip:
        sock.sendto(command.encode(), (clearcore_ip, CLEARCORE_PORT))
    else:
        messagebox.showerror("Not connected", "ClearCore not discovered yet.")

def send_revs(cmd_type):
    try:
        revs = int(rev_entry.get().strip())
        send_udp_command(f"{cmd_type} {revs}")
    except ValueError:
        messagebox.showerror("Invalid Input", "Enter a valid integer.")

def reset_motor():
    send_udp_command("RESET")

def update_ppr(selection):
    send_udp_command(f"PPR {selection}")

def abort_move():
    send_udp_command("ABORT")

def disable_motor():
    send_udp_command("DISABLE")

def enable_motor():
    send_udp_command("ENABLE")

# GUI
root = tk.Tk()
root.title("ClearCore Control Panel")
root.geometry("470x480")

frame = tk.Frame(root)
frame.pack(pady=10)

tk.Label(frame, text="Revolutions:").grid(row=0, column=0, padx=5)
rev_entry = tk.Entry(frame, width=10)
rev_entry.grid(row=0, column=1, padx=5)
tk.Button(frame, text="Send FAST", command=lambda: send_revs("FAST")).grid(row=0, column=2, padx=5)
tk.Button(frame, text="Send SLOW", command=lambda: send_revs("SLOW")).grid(row=1, column=2, padx=5)

tk.Label(frame, text="PPR:").grid(row=2, column=0, padx=5)
ppr_var = tk.StringVar(value="6400")
ppr_menu = tk.OptionMenu(frame, ppr_var, "6400", "800", command=update_ppr)
ppr_menu.grid(row=2, column=1, padx=5)

tk.Button(root, text="Reset Motor", command=reset_motor, width=20).pack(pady=5)
tk.Button(root, text="Rediscover", command=discover_clearcore, width=20).pack()

tk.Button(root, text="Abort Move", command=abort_move, width=20).pack(pady=5)
tk.Button(root, text="Disable Motor", command=disable_motor, width=20).pack(pady=2)
tk.Button(root, text="Enable Motor", command=enable_motor, width=20).pack(pady=2)

status_label = tk.Label(root, text="Not connected", fg="blue")
status_label.pack(pady=4)

torque_label = tk.Label(root, text="torque: --", fg="green")
torque_label.pack()

motor_status = tk.Label(root, text="Motor: ---", bg="gray", fg="black", width=30)
motor_status.pack(pady=4)

log_box = tk.Text(root, height=10, width=55)
log_box.pack(pady=5)

threading.Thread(target=receive_loop, daemon=True).start()
discover_clearcore()
root.mainloop()

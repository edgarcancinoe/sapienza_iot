import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

# === CONFIGURATION ===
PORT                = '/dev/cu.usbserial-0001'
BAUD_RATE           = 115200
WINDOW_DURATION_SEC = 4.0
SERIAL_TIMEOUT      = 0.002     
MAX_INFO_LINES      = 15        

# === SERIAL SETUP ===
ser = serial.Serial(PORT, BAUD_RATE, timeout=SERIAL_TIMEOUT)
ser.reset_input_buffer()

# === DATA BUFFERS & STATE ===
times      = deque()
values     = deque()
info_lines = deque(maxlen=MAX_INFO_LINES)

# === PLOT SETUP ===
plt.style.use('dark_background')
fig = plt.figure(figsize=(12, 4))
fig.subplots_adjust(left=0.05, right=0.98)  # reduce left margin, tighten overall layout

# Make console area wider and reduce spacing
gs = fig.add_gridspec(1, 2, width_ratios=[3.5, 1.5], wspace=0.03)

ax_plot = fig.add_subplot(gs[0, 0])
line, = ax_plot.plot([], [], lw=1, marker='o', markersize=4,
                     markerfacecolor='none', color='w', markeredgecolor='w')
ax_plot.set_xlim(0, WINDOW_DURATION_SEC)
ax_plot.set_ylim(-30, 30)
ax_plot.set_xlabel("Time (s)")
ax_plot.set_ylabel("Signal")
ax_plot.set_title("Live Signal")

ax_console = fig.add_subplot(gs[0, 1])
ax_console.axis('off')
text_info_console = ax_console.text(
    0.01, 0.99, "", transform=ax_console.transAxes,
    va='top', ha='left', fontsize=9, color='c', family='monospace'
)
text_avg = ax_console.text(
    0.01, 0.01, "Measured sampling frequency: -- Hz", transform=ax_console.transAxes,
    va='bottom', ha='left', fontsize=10, color='y', family='monospace'
)

def read_serial():
    raw = ser.readline().decode('utf-8', 'ignore').strip()
    if not raw:
        return None, None, None
    
    if raw.startswith("ESP-ROM:"):
        return "REBOOT", None, None
    
    if raw.startswith("[INFO]"):
        if raw.startswith("[INFO] Component"):
            return None, None, None
        return "INFO", "> " + raw[len("[INFO]"):].strip(), None
    
    if raw.startswith("#TS:"):
        try:
            ts_part, msg = raw.split("\t", 1)
            t_s = float(ts_part.split(":", 1)[1]) / 1e6
            if msg.startswith("#SAMPLE:"):
                try:
                    v = float(msg.split("\t", 1)[1])
                except ValueError:
                    return None, None, None 
                return "SAMPLE", t_s, v
        except ValueError:
            pass
    return None, None, None

def init():
    line.set_data([], [])
    text_info_console.set_text("")
    text_avg.set_text("Measured sampling frequency: -- Hz")
    return line, text_info_console, text_avg

def update(frame):
    # drain serial
    while True:
        kind, a, b = read_serial()
        if kind is None:
            break
        if kind == "REBOOT":
            times.clear()
            values.clear()
            info_lines.clear()
            line.set_data([], [])
            text_info_console.set_text("[REBOOT detected]")
            text_avg.set_text("Measured sampling frequency: -- Hz")
            continue
        if kind == "INFO":
            info_lines.append(a)
        elif kind == "SAMPLE":
            t_s, v = a, b
            if times and t_s < times[-1]:
                times.clear()
                values.clear()
                line.set_data([], [])
            times.append(t_s)
            values.append(v)

    # update console text
    text_info_console.set_text("\n".join(info_lines))

    # average fs
    if len(times) >= 2:
        duration = times[-1] - times[0]
        avg_fs = (len(times)-1) / duration if duration > 0 else 0.0
        text_avg.set_text(f"Measured sampling frequency: {avg_fs:.1f} Hz")
    else:
        text_avg.set_text("Measured sampling frequency: -- Hz")

    # prune old points
    if times:
        t_latest = times[-1]
        t_cutoff = t_latest - WINDOW_DURATION_SEC
        while times and times[0] < t_cutoff:
            times.popleft()
            values.popleft()
        # Plot using actual timestamps
        xs = list(times)
        line.set_data(xs, list(values))
        # Slide x-axis to show [t_latest - WINDOW_DURATION_SEC, t_latest]
        ax_plot.set_xlim(t_cutoff, t_latest)
        ymin, ymax = min(values), max(values)
        if ymin != ymax:
            ax_plot.set_ylim(ymin * 1.1, ymax * 1.1)

    return line, text_info_console, text_avg

ani = animation.FuncAnimation(
    fig, update, init_func=init,
    interval=20,
    blit=False
)

plt.show()
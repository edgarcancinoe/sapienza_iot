import json
import time
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import paho.mqtt.client as mqtt

# === CONFIGURATION ===
MQTT_BROKER         = "localhost"
MQTT_PORT           = 1883
MQTT_TOPIC          = "iot/aggregate"
WINDOW_DURATION_SEC = 4.0     # show last 10 seconds
MAX_POINTS          = 1000     # max points to keep

# === DATA BUFFERS ===
times     = deque(maxlen=MAX_POINTS)
values    = deque(maxlen=MAX_POINTS)
intervals = deque(maxlen=MAX_POINTS - 1)  # time between timestamps

# === MQTT CALLBACKS ===
def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker, result code:", rc)
    client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
        avg = float(payload.get("average", 0.0))
        ts_device = float(payload.get("timeStamp", 0)) / 1e6  # µs → s

        # reset detection
        if times and ts_device < times[-1]:
            times.clear()
            values.clear()
            intervals.clear()

        # calculate time difference
        if times:
            dt_us = (ts_device - times[-1]) * 1e6  # µs
            if dt_us > 0:
                intervals.append(dt_us)

        times.append(ts_device)
        values.append(avg)
    except Exception as e:
        print("Malformed message:", e, msg.payload)

# === SETUP MQTT CLIENT ===
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_start()

# === SETUP PLOT ===
plt.style.use('dark_background')
fig, ax = plt.subplots(figsize=(10, 4))
line, = ax.plot([], [], lw=1, marker='o', markersize=4,
                markerfacecolor='none', color='w', markeredgecolor='w')
text_deltat = ax.text(0.02, 0.95, "", transform=ax.transAxes,
                      color='y', fontsize=10, va='top')
ax.set_xlabel("Time (s)")
ax.set_ylabel("Average Value")
ax.set_title("Live Average from MQTT")
ax.set_xlim(0, WINDOW_DURATION_SEC)
plt.tight_layout()

def init():
    line.set_data([], [])
    text_deltat.set_text("")
    return line, text_deltat

def update(frame):
    if not times:
        return line, text_deltat

    # prune old data
    t_latest = times[-1]
    t_cutoff = t_latest - WINDOW_DURATION_SEC
    while times and times[0] < t_cutoff:
        times.popleft()
        values.popleft()
        if intervals:
            intervals.popleft()

    # update line with dots
    xs = [t - t_cutoff for t in times]
    line.set_data(xs, values)

    # show average interval
    if intervals:
        avg_interval = sum(intervals) / len(intervals) / 1e3  # convert to ms
        text_deltat.set_text(f"Avg Δt: {avg_interval:.1f} ms")
    else:
        text_deltat.set_text("Avg Δt: -- µs")

    # dynamic y-limits
    ymin, ymax = min(values), max(values)
    if ymin != ymax:
        ax.set_ylim(ymin * 1.1, ymax * 1.1)

    return line, text_deltat

ani = animation.FuncAnimation(
    fig, update, init_func=init,
    interval=200,  # ms between frames
    blit=False
)

plt.show()
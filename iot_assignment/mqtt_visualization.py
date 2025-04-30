import json
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import paho.mqtt.client as mqtt

# === CONFIGURATION ===
MQTT_BROKER         = "localhost"
MQTT_PORT           = 1883
MQTT_TOPIC          = "iot/aggregate"
WINDOW_DURATION_SEC = 4.0    
MAX_POINTS          = 1000  

# === DATA BUFFERS ===
# times holds device timestamps in seconds (float)
times     = deque(maxlen=MAX_POINTS)
values    = deque(maxlen=MAX_POINTS)
intervals = deque(maxlen=MAX_POINTS-1) 

# absolute timestamp of the very first sample
ts_start = None

# === MQTT CALLBACKS ===
def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker, result code:", rc)
    client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    global ts_start
    try:
        payload   = json.loads(msg.payload.decode())
        avg       = float(payload.get("average", 0.0))
        ts_device = float(payload.get("timeStamp", 0)) / 1e6  # µs → s

        # initialize ts_start at first sample
        if ts_start is None:
            ts_start = ts_device

        if times and ts_device < times[-1]:
            times.clear()
            values.clear()
            intervals.clear()
            ts_start = ts_device

        # compute inter-arrival Δt in ms
        if times:
            dt_ms = (ts_device - times[-1]) * 1e3
            if dt_ms > 0:
                intervals.append(dt_ms)

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
                markerfacecolor='none', markeredgecolor='w')
text_deltat = ax.text(0.02, 0.95, "", transform=ax.transAxes,
                      color='y', fontsize=10, va='top')

ax.set_xlabel("Time since start (s)")
ax.set_ylabel("Average Value")
ax.set_title("Live Average from MQTT")
ax.set_xlim(0, WINDOW_DURATION_SEC)  # initial limits

plt.tight_layout()

def init():
    line.set_data([], [])
    text_deltat.set_text("")
    return line, text_deltat

def update(frame):
    if not times:
        return line, text_deltat

    # optionally prune old data (buffer maxlen handles memory)
    while len(times) > MAX_POINTS:
        times.popleft()
        values.popleft()
        if intervals:
            intervals.popleft()

    # compute x = seconds since very first sample
    xs = [t - ts_start for t in times]
    line.set_data(xs, values)

    # grow x-axis to include the newest point
    if xs:
        ax.set_xlim(0, xs[-1])

    # show average Δt in ms
    if intervals:
        avg_dt = sum(intervals) / len(intervals)
        text_deltat.set_text(f"Avg Δt: {avg_dt:.1f} ms")
    else:
        text_deltat.set_text("Avg Δt: -- ms")

    # dynamic y-limits
    ymin, ymax = min(values), max(values)
    if ymin != ymax:
        ax.set_ylim(ymin * 1.1, ymax * 1.1)

    return line, text_deltat

ani = animation.FuncAnimation(
    fig, update, init_func=init,
    interval=200,  # redraw every 200 ms
    blit=False
)

plt.show()
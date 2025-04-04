import serial
import matplotlib.pyplot as plt
import numpy as np
from collections import deque
import time

# === CONFIGURATION ===
PORT = '/dev/cu.usbserial-0001'
BAUD_RATE = 115200
FFT_SIZE = 256
BINS_TO_PLOT = FFT_SIZE // 2
SAMPLE_WINDOW = 256
AGG_WINDOW = 100  # how many aggregate values to keep in the plot

# === SETUP PLOT ===
plt.ion()
fig, (ax_fft, ax_time, ax_agg) = plt.subplots(3, 1, figsize=(10, 8), sharex=False)

sampling_freq = 50.0  # Default, will be updated

# Frequency axis
def get_freqs(sampling_freq, fft_size, bins_to_plot):
    return [i * (sampling_freq / fft_size) for i in range(bins_to_plot)]

freqs = get_freqs(sampling_freq, FFT_SIZE, BINS_TO_PLOT)
bars = ax_fft.bar(freqs, [0]*BINS_TO_PLOT, width=(freqs[1] - freqs[0]))
ax_fft.set_xlabel("Frequency (Hz)")
ax_fft.set_ylabel("Magnitude")
ax_fft.set_title("FFT Output")

# Time-domain signal plot
sample_buffer = deque([0]*SAMPLE_WINDOW, maxlen=SAMPLE_WINDOW)
line_time, = ax_time.plot(range(SAMPLE_WINDOW), sample_buffer)
ax_time.set_ylabel("Sample Value")
ax_time.set_xlabel("Sample Index")
ax_time.set_title("Signal Samples (Time Domain)")

# Aggregated values plot
agg_values = deque([0]*AGG_WINDOW, maxlen=AGG_WINDOW)
agg_times = deque(range(AGG_WINDOW), maxlen=AGG_WINDOW)
agg_line, = ax_agg.plot(agg_times, agg_values, 'o-', color='orange')
ax_agg.set_title("Aggregated Average Values")
ax_agg.set_ylabel("Avg Value")
ax_agg.set_xlabel("Time Index")

# Text label for most recent average
agg_text = ax_time.text(0.01, 0.95, "Average: --", transform=ax_time.transAxes,
                        verticalalignment='top', fontsize=10, color='blue')

# === SETUP SERIAL ===
ser = serial.Serial(PORT, BAUD_RATE, timeout=1)
last_line_time = time.time()

# === MAIN LOOP ===
try:
    while True:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        
        if not line:
            if time.time() - last_line_time > 3:
                print("[WARNING] No data received for 3 seconds.")
                last_line_time = time.time()
            continue

        last_line_time = time.time()

        if line.startswith("#SAMPLING_FREQ:"):
            try:
                sampling_freq = float(line.split(":")[1])
                print(f"[INFO] New sampling frequency: {sampling_freq:.2f} Hz")
                freqs = get_freqs(sampling_freq, FFT_SIZE, BINS_TO_PLOT)
                for bar, freq in zip(bars, freqs):
                    bar.set_x(freq)
                ax_fft.set_xlim(freqs[0], freqs[-1])
            except ValueError:
                continue

        elif line.startswith("#SAMPLE:"):
            try:
                val = float(line.split('\t')[1])
                sample_buffer.append(val)
                line_time.set_ydata(sample_buffer)
                ax_time.set_ylim(-max(abs(max(sample_buffer)), abs(min(sample_buffer))) * 1.1,
                                 max(abs(max(sample_buffer)), abs(min(sample_buffer))) * 1.1)
            except Exception as e:
                print(f"[ERROR] SAMPLE parse: {e}")
                continue

        elif line.startswith("#AGGREGATE:"):
            try:
                agg_val = float(line.split('\t')[1])
                agg_text.set_text(f"Average: {agg_val:.2f}")
                agg_values.append(agg_val)
                agg_times.append(agg_times[-1] + 1 if agg_times else 0)
                agg_line.set_data(agg_times, agg_values)
                ax_agg.set_xlim(max(0, agg_times[0]), agg_times[-1])
                ax_agg.set_ylim(min(agg_values)*0.9, max(agg_values)*1.1)
            except Exception as e:
                print(f"[ERROR] AGGREGATE parse: {e}")
                continue

        elif line.startswith("[INFO]") or line.startswith("[WARNING]") or line.startswith("[DEBUG]"):
            print(line)
            continue

        elif line.startswith("#COMPONENT:"):
            try:
                _, freq, mag = line.split('\t')
                print(f"[COMPONENT] Frequency: {float(freq):.2f} Hz | Magnitude: {float(mag):.2f}")
            except ValueError:
                continue

        elif '\t' in line:
            try:
                values = list(map(float, line.split('\t')))
                if len(values) != BINS_TO_PLOT:
                    continue
                for bar, val in zip(bars, values):
                    bar.set_height(val)
                ax_fft.set_ylim(0, max(values) * 1.1)
            except ValueError as e:
                print(f"[ERROR] FFT parse: {e} | line: {line}")
                continue

        plt.pause(0.001)

except KeyboardInterrupt:
    print("Exiting...")
    ser.close()
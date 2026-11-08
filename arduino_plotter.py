"""
Arduino serial logger
- Press Enter to start logging
- Press Enter again to stop
- Saves a CSV and shows a graph

Arduino side: print one line per reading, numbers separated by commas, e.g.
    Serial.print(tempC); Serial.print(","); Serial.println(rawADC);
Labels like "temp:23.1,raw:512" (Serial Plotter style) also work.

Usage:
    pip install pyserial matplotlib
    python arduino_logger.py            # auto-detects the Arduino port
    python arduino_logger.py COM3       # or give the port yourself
    python arduino_logger.py /dev/cu.usbmodem1101 115200   # port + baud
"""

import csv
import sys
import threading
import time
from datetime import datetime

import matplotlib.pyplot as plt
import serial
import serial.tools.list_ports

BAUD_DEFAULT = 115200


def find_port():
    """Pick the most likely Arduino port, or ask if unsure."""
    ports = list(serial.tools.list_ports.comports())
    keywords = ("arduino", "usbmodem", "usbserial", "ch340", "wchusb", "acm")
    for p in ports:
        text = f"{p.device} {p.description}".lower()
        if any(k in text for k in keywords):
            return p.device
    if not ports:
        sys.exit("No serial ports found. Is the Arduino plugged in?")
    print("Couldn't auto-detect the Arduino. Available ports:")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device} - {p.description}")
    return ports[int(input("Pick a number: "))].device


def parse_line(line):
    """Turn '23.1,512' or 'temp:23.1,raw:512' into names and float values."""
    names, values = [], []
    for i, part in enumerate(line.split(",")):
        part = part.strip()
        if ":" in part:
            name, part = part.split(":", 1)
        else:
            name = f"value{i + 1}"
        values.append(float(part))  # raises ValueError on junk lines
        names.append(name.strip())
    return names, values


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else BAUD_DEFAULT

    print(f"Opening {port} at {baud} baud...")
    ser = serial.Serial(port, baud, timeout=0.5)
    time.sleep(2)  # opening the port resets most Arduinos; give it time to boot

    input("Ready. Press Enter to START logging...")
    ser.reset_input_buffer()  # drop anything printed before we started

    rows, names = [], None
    stop = threading.Event()
    t0 = time.time()

    def reader():
        nonlocal names
        while not stop.is_set():
            raw = ser.readline().decode(errors="ignore").strip()
            if not raw:
                continue
            try:
                line_names, values = parse_line(raw)
            except ValueError:
                continue  # skip partial or non-numeric lines
            if names is None:
                names = line_names
            if len(values) != len(names):
                continue  # skip lines with a different number of values
            t = time.time() - t0
            rows.append([t] + values)
            print(f"{t:7.2f} s  " + "  ".join(f"{n}={v:g}" for n, v in zip(names, values)))

    thread = threading.Thread(target=reader, daemon=True)
    thread.start()

    input("Logging... press Enter to STOP.\n")
    stop.set()
    thread.join()
    ser.close()

    if not rows:
        print("No data received. Check the port, baud rate, and what the Arduino is printing.")
        return

    filename = datetime.now().strftime("log_%Y-%m-%d_%H-%M-%S.csv")
    with open(filename, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["time_s"] + names)
        w.writerows(rows)
    print(f"Saved {len(rows)} readings to {filename}")

    times = [r[0] for r in rows]
    for i, name in enumerate(names):
        plt.plot(times, [r[i + 1] for r in rows], label=name)
    plt.xlabel("Time (s)")
    plt.ylabel("Value")
    plt.title(filename)
    if len(names) > 1:
        plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()

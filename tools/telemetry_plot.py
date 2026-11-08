#!/usr/bin/env python3
# Author: Raguram Kasthurisamy
"""Live plot of the ECU's control-loop telemetry.

Opens the board's serial port, turns on the telemetry stream, and plots target/actual
speed, duty and motor current as they arrive. Optionally logs every sample to CSV and
sends console commands first, so a step response can be captured in one go:

    python tools/telemetry_plot.py /dev/ttyACM0 --send "sim on" --send "speed 120" --csv step.csv

The firmware prints lines of the form
    $T,<ms>,<target_rpm>,<setpoint_rpm>,<speed_rpm>,<duty>,<current_a>,<mode>
and everything else on the console is ignored.
"""

import argparse
import collections
import csv
import sys
import time

FIELDS = ("t_ms", "target_rpm", "setpoint_rpm", "speed_rpm", "duty", "current_a", "mode")


def parse_line(line):
    """Returns a dict for a telemetry line, or None for anything else."""
    line = line.strip()
    if not line.startswith("$T,"):
        return None
    parts = line[3:].split(",")
    if len(parts) != len(FIELDS):
        return None
    try:
        return dict(zip(FIELDS, (float(p) for p in parts)))
    except ValueError:
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("port", help="serial port, e.g. /dev/ttyACM0 or COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--rate", type=int, default=50, help="telemetry rate in Hz (1-100)")
    parser.add_argument("--window", type=float, default=10.0, help="seconds of history to show")
    parser.add_argument("--csv", help="also write every sample to this CSV file")
    parser.add_argument("--send", action="append", default=[],
                        help="console command to send after starting the stream (repeatable)")
    args = parser.parse_args()

    try:
        import serial  # pyserial
        import matplotlib.pyplot as plt
        from matplotlib.animation import FuncAnimation
    except ImportError as error:
        sys.exit(f"missing dependency ({error}); run: pip install -r tools/requirements.txt")

    port = serial.Serial(args.port, args.baud, timeout=0.02)

    def command(text):
        port.write((text + "\r").encode())
        time.sleep(0.05)

    command(f"stream {args.rate}")
    for text in args.send:
        command(text)

    history = {name: collections.deque(maxlen=int(args.window * args.rate)) for name in FIELDS}
    csv_file = open(args.csv, "w", newline="") if args.csv else None
    writer = csv.DictWriter(csv_file, fieldnames=FIELDS) if csv_file else None
    if writer:
        writer.writeheader()

    figure, (ax_speed, ax_duty, ax_current) = plt.subplots(3, 1, sharex=True, figsize=(10, 8))
    (line_target,) = ax_speed.plot([], [], "--", label="target")
    (line_setpoint,) = ax_speed.plot([], [], label="setpoint (ramped)")
    (line_speed,) = ax_speed.plot([], [], label="measured")
    (line_duty,) = ax_duty.plot([], [])
    (line_current,) = ax_current.plot([], [])
    ax_speed.set_ylabel("rpm")
    ax_speed.legend(loc="upper left")
    ax_duty.set_ylabel("duty")
    ax_current.set_ylabel("current (A)")
    ax_current.set_xlabel("time (s)")
    for axis in (ax_speed, ax_duty, ax_current):
        axis.grid(True)
    pending = ""

    def update(_frame):
        nonlocal pending
        pending += port.read(port.in_waiting or 1).decode(errors="replace")
        *lines, pending = pending.split("\n")
        for text in lines:
            sample = parse_line(text)
            if sample is None:
                continue
            for name in FIELDS:
                history[name].append(sample[name])
            if writer:
                writer.writerow(sample)
        if history["t_ms"]:
            t = [ms / 1000.0 for ms in history["t_ms"]]
            line_target.set_data(t, history["target_rpm"])
            line_setpoint.set_data(t, history["setpoint_rpm"])
            line_speed.set_data(t, history["speed_rpm"])
            line_duty.set_data(t, history["duty"])
            line_current.set_data(t, history["current_a"])
            for axis in (ax_speed, ax_duty, ax_current):
                axis.relim()
                axis.autoscale_view()
        return line_target, line_setpoint, line_speed, line_duty, line_current

    animation = FuncAnimation(figure, update, interval=50, cache_frame_data=False)
    try:
        plt.show()
    finally:
        del animation
        command("stream off")
        port.close()
        if csv_file:
            csv_file.close()


if __name__ == "__main__":
    main()

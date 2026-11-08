#!/usr/bin/env python3
# Author: Raghuram Kasturi Sami
# Copyright (c) 2026 Raghuram Kasturi Sami
"""Watch and command the ECU over CAN with a USB-CAN adapter.

Decodes every ECU frame with protocol/ecu.dbc. With --speed it also acts as the commanding
node: it sends ECU_Command at 20 Hz with a changing alive counter and a CRC, exactly as the
ECU expects, and sends Stop on exit.

    python tools/can_monitor.py --interface gs_usb --channel 0                  # CANable (candleLight)
    python tools/can_monitor.py --interface slcan --channel /dev/ttyACM1        # slcan firmware
    python tools/can_monitor.py --interface socketcan --channel can0 --speed 120 --duration 10
"""

import argparse
import pathlib
import sys
import time

DBC_PATH = pathlib.Path(__file__).resolve().parent.parent / "protocol" / "ecu.dbc"
PRINT_INTERVAL_S = 0.2  # status frames arrive at 100 Hz; print a readable subset


def crc8_sae_j1850(data):
    """CRC-8 SAE J1850 (poly 0x1D, init 0xFF, final XOR 0xFF), same as the firmware."""
    crc = 0xFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1D) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ 0xFF


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--interface", default="gs_usb", help="python-can interface name")
    parser.add_argument("--channel", default="0", help="python-can channel")
    parser.add_argument("--bitrate", type=int, default=500000)
    parser.add_argument("--speed", type=float, help="command this speed in rpm while running")
    parser.add_argument("--duration", type=float, help="stop after this many seconds")
    args = parser.parse_args()

    try:
        import can
        import cantools
    except ImportError as error:
        sys.exit(f"missing dependency ({error}); run: pip install -r tools/requirements.txt")

    database = cantools.database.load_file(str(DBC_PATH))
    command = database.get_message_by_name("ECU_Command")
    bus = can.Bus(interface=args.interface, channel=args.channel, bitrate=args.bitrate)
    counter = 0
    next_command = time.monotonic()
    last_print = {}
    end = time.monotonic() + args.duration if args.duration else None

    def send_command(mode, speed):
        nonlocal counter
        counter = (counter + 1) & 0xFF
        payload = bytearray(command.encode({"Mode": mode, "SpeedTarget": speed, "DutyTarget": 0,
                                            "AliveCounter": counter, "Crc": 0}))
        payload[7] = crc8_sae_j1850(payload[:7])
        bus.send(can.Message(arbitration_id=command.frame_id, data=payload, is_extended_id=False))

    try:
        while end is None or time.monotonic() < end:
            if args.speed is not None and time.monotonic() >= next_command:
                send_command(2, args.speed)
                next_command += 0.05  # 20 Hz, well inside the ECU's 250 ms timeout
            message = bus.recv(timeout=0.01)
            if message is None or message.is_extended_id:
                continue
            try:
                definition = database.get_message_by_frame_id(message.arbitration_id)
            except KeyError:
                continue
            now = time.monotonic()
            if now - last_print.get(definition.name, 0.0) < PRINT_INTERVAL_S:
                continue
            last_print[definition.name] = now
            decoded = definition.decode(message.data)
            fields = "  ".join(f"{key}={value}" for key, value in decoded.items())
            print(f"{message.timestamp:12.3f}  {definition.name:<17} {fields}")
    except KeyboardInterrupt:
        pass
    finally:
        if args.speed is not None:
            send_command(0, 0.0)
        bus.shutdown()


if __name__ == "__main__":
    main()

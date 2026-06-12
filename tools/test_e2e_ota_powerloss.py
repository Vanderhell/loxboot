#!/usr/bin/env python3
"""
Manual ESP32-S3 OTA power-loss/disconnect harness.

This script does not fake evidence. It guides the operator through one
disconnect scenario at a time and records the observed post-reconnect state.

Usage:
    python tools/test_e2e_ota_powerloss.py --port COM19 --firmware idf_project/build_no_confirm/loxboot_esp32.bin --scenario after_commit_before_reboot

Scenarios:
    before_hello
    during_first_write_before_erase
    during_erase_or_first_write
    during_middle_write
    after_all_write_before_commit
    during_commit
    after_commit_before_reboot
    during_reboot

The recommended firmware for rollback-oriented scenarios is the
auto-confirm-disabled build produced with:
    -DLOXBOOT_ESP32_AUTO_CONFIRM=0
"""

import argparse
import os
import struct
import sys
import time
import zlib
import serial

sys.path.insert(0, ".")
from test_e2e_ota import (  # noqa: E402
    CMD_COMMIT,
    CMD_HELLO,
    CMD_REBOOT,
    CMD_STATUS,
    CMD_WRITE,
    RSP_OK,
    RSP_STATUS,
    SLOT_STATE,
    crc32,
    encode_frame,
    read_frame,
    send_recv,
    upload_firmware,
    wait_hello,
)


SCENARIOS = {
    "before_hello": "Disconnect power before the first HELLO attempt.",
    "during_first_write_before_erase": "Disconnect power immediately before the first WRITE is sent.",
    "during_erase_or_first_write": "Disconnect power while the first WRITE is in flight.",
    "during_middle_write": "Disconnect power while a later WRITE is in flight.",
    "after_all_write_before_commit": "Disconnect power after all WRITE commands finish, before COMMIT.",
    "during_commit": "Disconnect power while COMMIT is in flight.",
    "after_commit_before_reboot": "Disconnect power after COMMIT succeeds, before REBOOT.",
    "during_reboot": "Disconnect power while REBOOT is in flight.",
}


def prompt(msg: str) -> None:
    input(f"\n{msg}\nPress Enter when ready...")


def print_status(payload: bytes, label: str) -> int:
    if len(payload) != 4:
        print(f"  INFO  {label}: invalid status payload ({len(payload)} bytes)")
        return -1
    slot_a = SLOT_STATE.get(payload[0], f"0x{payload[0]:02X}")
    slot_b = SLOT_STATE.get(payload[1], f"0x{payload[1]:02X}")
    active = payload[2]
    reason = payload[3]
    print(f"  INFO  {label}: slot_a={slot_a} slot_b={slot_b} active={active} reason=0x{reason:02X}")
    return active


def wait_for_status(port: serial.Serial, timeout: float = 35.0) -> bytes:
    payload = wait_hello(port, timeout=timeout)
    cmd, body = send_recv(port, encode_frame(CMD_STATUS), timeout=5.0)
    if cmd != RSP_STATUS:
        raise RuntimeError(f"STATUS returned 0x{cmd:02X}")
    return body


def main() -> int:
    parser = argparse.ArgumentParser(description="Manual ESP32-S3 OTA power-loss harness")
    parser.add_argument("--port", default="COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--firmware", default="idf_project/build_no_confirm/loxboot_esp32.bin")
    parser.add_argument("--scenario", choices=sorted(SCENARIOS), required=True)
    args = parser.parse_args()

    if not os.path.exists(args.firmware):
        print(f"ERROR: firmware not found: {args.firmware}")
        return 1

    with open(args.firmware, "rb") as f:
        firmware = f.read()

    print(f"Port:     {args.port} @ {args.baud} baud")
    print(f"Firmware: {args.firmware} ({len(firmware)} bytes, CRC32=0x{crc32(firmware):08X})")
    print(f"Scenario: {args.scenario}")
    print(f"Meaning:  {SCENARIOS[args.scenario]}")

    with serial.Serial(args.port, args.baud, timeout=2) as port:
        try:
            if args.scenario == "before_hello":
                prompt("Disconnect power now, then reconnect and let the board boot back up.")
                payload = wait_for_status(port)
                print_status(payload, "post-reconnect")
                return 0

            payload = wait_for_status(port)
            initial_active = print_status(payload, "initial")
            if initial_active < 0:
                return 1

            if args.scenario == "during_first_write_before_erase":
                prompt("Disconnect power now, before the first WRITE is sent.")
                try:
                    send_recv(port, encode_frame(CMD_HELLO), timeout=5.0)
                except Exception:
                    pass
                try:
                    send_recv(port, encode_frame(CMD_WRITE, struct.pack('<I', 0) + firmware[:1020]), timeout=5.0)
                except Exception as e:
                    print(f"  INFO  WRITE interrupted as expected: {e}")
                payload = wait_for_status(port)
                print_status(payload, "post-reconnect")
                return 0

            if args.scenario == "during_erase_or_first_write":
                prompt("Disconnect power now while the first WRITE is in flight.")
                try:
                    send_recv(port, encode_frame(CMD_HELLO), timeout=5.0)
                except Exception:
                    pass
                try:
                    send_recv(port, encode_frame(CMD_WRITE, struct.pack('<I', 0) + firmware[:1020]), timeout=5.0)
                except Exception as e:
                    print(f"  INFO  First WRITE interrupted as expected: {e}")
                payload = wait_for_status(port)
                print_status(payload, "post-reconnect")
                return 0

            if args.scenario == "during_middle_write":
                send_recv(port, encode_frame(CMD_HELLO), timeout=5.0)
                upload_firmware(port, firmware[:2040])
                prompt("Disconnect power now while the next WRITE is in flight.")
                try:
                    send_recv(port, encode_frame(CMD_WRITE, struct.pack('<I', 2040) + firmware[2040:3060]), timeout=5.0)
                except Exception as e:
                    print(f"  INFO  Middle WRITE interrupted as expected: {e}")
                payload = wait_for_status(port)
                print_status(payload, "post-reconnect")
                return 0

            if args.scenario == "after_all_write_before_commit":
                send_recv(port, encode_frame(CMD_HELLO), timeout=5.0)
                upload_firmware(port, firmware)
                prompt("Disconnect power now before COMMIT is sent.")
                payload = wait_for_status(port)
                print_status(payload, "post-reconnect")
                return 0

            if args.scenario == "during_commit":
                send_recv(port, encode_frame(CMD_HELLO), timeout=5.0)
                upload_firmware(port, firmware)
                prompt("Disconnect power now while COMMIT is in flight.")
                try:
                    send_recv(port, encode_frame(CMD_COMMIT, struct.pack('<II', len(firmware), crc32(firmware))), timeout=10.0)
                except Exception as e:
                    print(f"  INFO  COMMIT interrupted as expected: {e}")
                payload = wait_for_status(port)
                print_status(payload, "post-reconnect")
                return 0

            if args.scenario == "after_commit_before_reboot":
                send_recv(port, encode_frame(CMD_HELLO), timeout=5.0)
                upload_firmware(port, firmware)
                cmd, _ = send_recv(port, encode_frame(CMD_COMMIT, struct.pack('<II', len(firmware), crc32(firmware))), timeout=10.0)
                if cmd != RSP_OK:
                    raise RuntimeError(f"COMMIT returned 0x{cmd:02X}")
                prompt("Disconnect power now before REBOOT is sent.")
                payload = wait_for_status(port)
                print_status(payload, "post-reconnect")
                return 0

            if args.scenario == "during_reboot":
                send_recv(port, encode_frame(CMD_HELLO), timeout=5.0)
                upload_firmware(port, firmware)
                cmd, _ = send_recv(port, encode_frame(CMD_COMMIT, struct.pack('<II', len(firmware), crc32(firmware))), timeout=10.0)
                if cmd != RSP_OK:
                    raise RuntimeError(f"COMMIT returned 0x{cmd:02X}")
                prompt("Disconnect power now while REBOOT is in flight.")
                try:
                    send_recv(port, encode_frame(CMD_REBOOT), timeout=10.0)
                except Exception as e:
                    print(f"  INFO  REBOOT interrupted as expected: {e}")
                payload = wait_for_status(port)
                print_status(payload, "post-reconnect")
                return 0

        except KeyboardInterrupt:
            print("\nAborted by user.")
            return 130
        except Exception as e:
            print(f"ERROR: {e}")
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

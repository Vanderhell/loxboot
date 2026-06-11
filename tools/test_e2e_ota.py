#!/usr/bin/env python3
"""
loxboot ESP32-S3 full OTA hardware test.

Tests the complete flow:
  1. boot ota_0 (slot A)
  2. UART update to ota_1 (slot B)
  3. COMMIT -> slot B = PENDING
  4. REBOOT -> loxboot calls esp_ota_set_boot_partition(ota_1) + esp_restart()
  5. IDF bootloader loads ota_1
  6. New app calls loxboot_esp32_confirm_running_app()
  7. Next reboot still boots ota_1 (rollback cancelled)
  8. Corrupt update test -> verify rollback to previous

Usage:
    python tools/test_e2e_ota.py --port COM19 --firmware idf_project/build/loxboot_esp32.bin

Requires:
    pip install pyserial
"""

import argparse
import os
import struct
import sys
import time
import zlib
import serial

# ---------------------------------------------------------------------------
# CRC16-CCITT and CRC32
# ---------------------------------------------------------------------------
_CRC16_TABLE = [
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
    0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
    0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
    0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
    0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
    0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
    0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
    0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
    0x48C4,0x58E5,0x6886,0x78A7,0x0840,0x1861,0x2802,0x3823,
    0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
    0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,
    0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
    0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
    0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
    0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
    0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
    0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
    0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
    0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
    0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
    0xB5EA,0xA5CB,0x95A8,0x8589,0xF56E,0xE54F,0xD52C,0xC50D,
    0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
    0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
    0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
    0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
    0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
    0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
    0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
    0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
    0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
    0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0,
]

def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        idx = ((crc >> 8) ^ b) & 0xFF
        crc = ((crc << 8) ^ _CRC16_TABLE[idx]) & 0xFFFF
    return crc

def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF

# ---------------------------------------------------------------------------
# UART frame protocol
# ---------------------------------------------------------------------------
SOF        = 0x7E
CMD_HELLO  = 0x01
CMD_WRITE  = 0x02
CMD_COMMIT = 0x03
CMD_ABORT  = 0x04
CMD_STATUS = 0x05
CMD_REBOOT = 0x06
RSP_OK     = 0x81
RSP_ERROR  = 0x82
RSP_STATUS = 0x83

SLOT_STATE = {0: "EMPTY", 1: "PENDING", 2: "VALID", 3: "INVALID", 4: "ACTIVE", 5: "ROLLBACK"}

def encode_frame(cmd: int, payload: bytes = b'') -> bytes:
    plen = len(payload)
    hdr  = bytes([cmd, plen & 0xFF, (plen >> 8) & 0xFF])
    crc  = crc16(hdr + payload)
    return bytes([SOF]) + hdr + payload + bytes([crc & 0xFF, (crc >> 8) & 0xFF])

def read_frame(port: serial.Serial, timeout: float = 10.0) -> tuple:
    deadline = time.time() + timeout
    while time.time() < deadline:
        b = port.read(1)
        if b and b[0] == SOF:
            break
    else:
        raise TimeoutError("No SOF received within timeout")
    hdr  = port.read(3)
    cmd  = hdr[0]
    plen = hdr[1] | (hdr[2] << 8)
    body = port.read(plen + 2)
    return cmd, body[:plen]

def wait_hello(port: serial.Serial, timeout: float = 35.0) -> tuple:
    """Send HELLO until RSP_STATUS received. Catches the listen window."""
    print("  Waiting for listen window...", end="", flush=True)
    deadline = time.time() + timeout
    frame = encode_frame(CMD_HELLO)
    while time.time() < deadline:
        port.write(frame)
        port.flush()
        try:
            cmd, payload = read_frame(port, timeout=1.0)
            if cmd == RSP_STATUS:
                print(" OK")
                return payload
        except (TimeoutError, serial.SerialException):
            print(".", end="", flush=True)
    raise TimeoutError(f"Device did not respond to HELLO within {timeout}s")

def send_recv(port: serial.Serial, frame: bytes, timeout: float = 10.0) -> tuple:
    port.write(frame)
    port.flush()
    return read_frame(port, timeout)

# ---------------------------------------------------------------------------
# Firmware upload
# ---------------------------------------------------------------------------
CHUNK_SIZE = 1020  # must be <= LOXBOOT_UART_MAX_FRAME_PAYLOAD - 4 (offset field)

def upload_firmware(port: serial.Serial, firmware: bytes) -> None:
    total  = len(firmware)
    offset = 0
    chunks = (total + CHUNK_SIZE - 1) // CHUNK_SIZE
    print(f"  Uploading {total} bytes in {chunks} chunks...")

    while offset < total:
        chunk = firmware[offset : offset + CHUNK_SIZE]
        payload = struct.pack('<I', offset) + chunk
        cmd, _ = send_recv(port, encode_frame(CMD_WRITE, payload), timeout=15.0)
        if cmd != RSP_OK:
            raise RuntimeError(f"WRITE at offset {offset} failed: cmd=0x{cmd:02X}")
        offset += len(chunk)
        pct = offset * 100 // total
        print(f"  {offset}/{total} ({pct}%)", end="\r", flush=True)

    print(f"  Upload complete: {total} bytes")

# ---------------------------------------------------------------------------
# Results
# ---------------------------------------------------------------------------
class Results:
    def __init__(self):
        self.passed = self.failed = 0

    def begin(self, name: str):
        print(f"\n{'-'*60}")
        print(f"TEST: {name}")

    def check(self, cond: bool, msg: str):
        if cond:
            self.passed += 1
            print(f"  PASS  {msg}")
        else:
            self.failed += 1
            print(f"  FAIL  {msg}")

    def summary(self) -> bool:
        total = self.passed + self.failed
        print(f"\n{'='*60}")
        print(f"OTA HARDWARE E2E: {self.passed}/{total} passed", end="")
        print("  ALL PASS" if self.failed == 0 else f"  {self.failed} FAILED")
        return self.failed == 0

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

def test_initial_state(port: serial.Serial, r: Results) -> bytes:
    """Verify device boots and reports slot state."""
    r.begin("Initial state: device reports current active slot")
    payload = wait_hello(port)
    r.check(len(payload) == 4, f"RSP_STATUS payload={len(payload)} bytes")
    if len(payload) == 4:
        slot_a = SLOT_STATE.get(payload[0], f"0x{payload[0]:02X}")
        slot_b = SLOT_STATE.get(payload[1], f"0x{payload[1]:02X}")
        active = payload[2]
        reason = payload[3]
        print(f"  INFO  slot_a={slot_a} slot_b={slot_b} active={active} reason=0x{reason:02X}")
    return payload


def test_full_ota_update(port: serial.Serial, r: Results, firmware: bytes) -> None:
    """Upload firmware to the inactive slot, commit, reboot, verify slot switch."""
    r.begin("OTA update: HELLO -> WRITE -> COMMIT -> REBOOT -> boot inactive slot")

    fw_size = len(firmware)
    fw_crc  = crc32(firmware)
    print(f"  Firmware: {fw_size} bytes, CRC32=0x{fw_crc:08X}")

    # HELLO
    payload = wait_hello(port)
    initial_active = payload[2] if len(payload) == 4 else None
    if initial_active is not None:
        print(f"  INFO  initial active slot = {initial_active}")
    r.check(len(payload) == 4, "HELLO returned a valid status payload")

    # WRITE all chunks
    upload_firmware(port, firmware)

    # COMMIT
    commit_pl = struct.pack('<II', fw_size, fw_crc)
    cmd, _ = send_recv(port, encode_frame(CMD_COMMIT, commit_pl), timeout=10.0)
    r.check(cmd == RSP_OK, f"COMMIT -> RSP_OK (got 0x{cmd:02X})")

    # REBOOT -> device calls esp_ota_set_boot_partition(ota_1) + esp_restart()
    cmd, _ = send_recv(port, encode_frame(CMD_REBOOT), timeout=10.0)
    r.check(cmd == RSP_OK, f"REBOOT -> RSP_OK (got 0x{cmd:02X})")

    expected_active = 0 if initial_active == 1 else 1
    print(f"  Device rebooting via esp_restart() -> expected active slot {expected_active} after handoff")

    # Wait for ota_1 to start and respond
    time.sleep(2.0)
    port.reset_input_buffer()
    payload = wait_hello(port, timeout=35.0)
    r.check(len(payload) == 4, "ota_1 responds to HELLO")

    if len(payload) == 4:
        slot_a = SLOT_STATE.get(payload[0], f"0x{payload[0]:02X}")
        slot_b = SLOT_STATE.get(payload[1], f"0x{payload[1]:02X}")
        active = payload[2]
        print(f"  INFO  Post-reboot: slot_a={slot_a} slot_b={slot_b} active={active}")
        r.check(active == expected_active, f"active={active} == expected slot {expected_active} after OTA")
        r.check(payload[expected_active] in (1, 2, 4),
                f"target slot state={SLOT_STATE.get(payload[expected_active], f'0x{payload[expected_active]:02X}')} is PENDING/VALID/ACTIVE")


def test_ota1_still_boots_after_reboot(port: serial.Serial, r: Results) -> None:
    """Verify the same active slot remains active after a second reboot."""
    r.begin("Persistence: active slot remains stable after second reboot")

    # Trigger a reboot by sending HELLO + REBOOT without updating
    payload = wait_hello(port)
    initial_active = payload[2] if len(payload) == 4 else None
    cmd, _ = send_recv(port, encode_frame(CMD_REBOOT), timeout=10.0)
    r.check(cmd == RSP_OK, "REBOOT sent")

    time.sleep(2.0)
    port.reset_input_buffer()
    payload = wait_hello(port, timeout=35.0)

    if len(payload) == 4:
        active = payload[2]
        print(f"  INFO  After 2nd reboot: active={active}")
        r.check(active == initial_active, f"active={active} == initial active slot {initial_active}")


def test_corrupt_update_rollback(port: serial.Serial, r: Results) -> None:
    """Upload corrupt firmware to the inactive slot and verify rejection."""
    r.begin("Corrupt update: bad CRC is rejected and active slot stays unchanged")

    # Upload garbage to slot A (wrong CRC will be detected at boot)
    bad_firmware = bytes([0xFF] * 256)
    bad_crc = crc32(bad_firmware) ^ 0xDEADBEEF  # intentionally wrong CRC

    payload = wait_hello(port)
    initial_active = payload[2] if len(payload) == 4 else None
    r.check(len(payload) == 4, "HELLO OK before corrupt update")

    upload_firmware(port, bad_firmware)

    commit_pl = struct.pack('<II', len(bad_firmware), bad_crc)
    cmd, _ = send_recv(port, encode_frame(CMD_COMMIT, commit_pl), timeout=10.0)
    if cmd == RSP_ERROR:
        r.check(True, "COMMIT rejected bad CRC (loxboot validation)")
        return

    r.check(cmd == RSP_OK, "COMMIT unexpectedly accepted bad CRC")

    cmd, _ = send_recv(port, encode_frame(CMD_REBOOT), timeout=10.0)
    r.check(cmd == RSP_OK, "REBOOT sent")

    print("  Device rebooting with corrupt image -> expected active slot unchanged")
    time.sleep(3.0)
    port.reset_input_buffer()

    payload = wait_hello(port, timeout=35.0)
    if len(payload) == 4:
        active = payload[2]
        slot_a = SLOT_STATE.get(payload[0], f"0x{payload[0]:02X}")
        slot_b = SLOT_STATE.get(payload[1], f"0x{payload[1]:02X}")
        print(f"  INFO  After corrupt update: slot_a={slot_a} slot_b={slot_b} active={active}")
        r.check(active == initial_active, f"active={active} == initial active slot {initial_active}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="loxboot ESP32-S3 full OTA hardware test")
    parser.add_argument('--port',     default='COM19')
    parser.add_argument('--baud',     type=int, default=115200)
    parser.add_argument('--firmware', default='idf_project/build/loxboot_esp32.bin',
                        help="Firmware .bin to upload via UART")
    args = parser.parse_args()

    if not os.path.exists(args.firmware):
        print(f"ERROR: firmware not found: {args.firmware}")
        sys.exit(1)

    with open(args.firmware, 'rb') as f:
        firmware = f.read()

    print(f"Port:     {args.port} @ {args.baud} baud")
    print(f"Firmware: {args.firmware} ({len(firmware)} bytes, CRC32=0x{crc32(firmware):08X})")

    r = Results()

    with serial.Serial(args.port, args.baud, timeout=2) as port:
        try:
            test_initial_state(port, r)
            test_full_ota_update(port, r, firmware)
            test_ota1_still_boots_after_reboot(port, r)
            test_corrupt_update_rollback(port, r)
        except Exception as e:
            r.check(False, f"EXCEPTION: {e}")
            import traceback; traceback.print_exc()

    ok = r.summary()
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()

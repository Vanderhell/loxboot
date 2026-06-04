#!/usr/bin/env python3
"""
loxboot hardware E2E test — drives real loxboot over serial port.

Usage:
    python tools/test_e2e_serial.py --port COM19 [--baud 115200]
"""

import argparse
import struct
import sys
import time
import zlib
import serial

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

def crc16(data):
    crc = 0xFFFF
    for b in data:
        idx = ((crc >> 8) ^ b) & 0xFF
        crc = ((crc << 8) ^ _CRC16_TABLE[idx]) & 0xFFFF
    return crc

def crc32(data):
    return zlib.crc32(data) & 0xFFFFFFFF

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

def encode_frame(cmd, payload=b''):
    plen = len(payload)
    hdr = bytes([cmd, plen & 0xFF, (plen >> 8) & 0xFF])
    crc = crc16(hdr + payload)
    return bytes([SOF]) + hdr + payload + bytes([crc & 0xFF, (crc >> 8) & 0xFF])

def read_frame(port, timeout=5.0):
    deadline = time.time() + timeout
    # Wait for SOF
    while time.time() < deadline:
        b = port.read(1)
        if b and b[0] == SOF:
            break
    else:
        raise TimeoutError("No SOF received")
    hdr = port.read(3)
    if len(hdr) < 3:
        raise TimeoutError("Header truncated")
    cmd  = hdr[0]
    plen = hdr[1] | (hdr[2] << 8)
    body = port.read(plen + 2)
    return cmd, body[:plen]

def send_recv(port, frame, timeout=5.0):
    port.write(frame)
    port.flush()
    return read_frame(port, timeout)

def wait_for_hello_window(port, timeout=10.0):
    """
    Send HELLO repeatedly until we get RSP_STATUS back.
    loxboot boots, listens 3s, restarts on no valid slot — cycle ~3.2s.
    We just hammer HELLO until the device catches one in its listen window.
    """
    port.reset_input_buffer()
    deadline = time.time() + timeout
    frame = encode_frame(CMD_HELLO)
    while time.time() < deadline:
        port.write(frame)
        port.flush()
        try:
            cmd, payload = read_frame(port, timeout=0.5)
            if cmd == RSP_STATUS:
                return payload
        except Exception:
            pass
    raise TimeoutError("Device did not respond to HELLO within timeout")

class Results:
    def __init__(self):
        self.passed = self.failed = 0

    def begin(self, name):
        print(f"\n{'-'*60}")
        print(f"TEST: {name}")

    def check(self, cond, msg):
        if cond:
            self.passed += 1
            print(f"  PASS  {msg}")
        else:
            self.failed += 1
            print(f"  FAIL  {msg}")

    def summary(self):
        total = self.passed + self.failed
        print(f"\n{'='*60}")
        print(f"HARDWARE E2E: {self.passed}/{total} passed", end="")
        print("  ALL PASS" if self.failed == 0 else f"  {self.failed} FAILED")
        return self.failed == 0


def run_tests(port_name, baud):
    r = Results()

    with serial.Serial(port_name, baud, timeout=5) as port:
        print(f"Connected: {port_name} @ {baud} baud")

        # ---- Test 1: HELLO ----
        r.begin("HELLO -> RSP_STATUS (device listening)")
        try:
            payload = wait_for_hello_window(port)
            r.check(len(payload) == 4, f"payload={len(payload)} bytes == 4")
            if len(payload) == 4:
                print(f"  INFO  slot_a=0x{payload[0]:02X} slot_b=0x{payload[1]:02X} "
                      f"active={payload[2]} reason=0x{payload[3]:02X}")
            r.check(True, "RSP_STATUS received")
        except Exception as e:
            r.check(False, f"Exception: {e}")

        # ---- Test 2: WRITE before HELLO rejected ----
        r.begin("WRITE before HELLO -> RSP_ERROR")
        try:
            # Send REBOOT to end current session and trigger device restart
            try: send_recv(port, encode_frame(CMD_REBOOT), timeout=2)
            except Exception: pass
            # Wait for device to reboot and start new listen window
            time.sleep(3.0)
            port.reset_input_buffer()
            # Send WRITE immediately — before sending HELLO — session should reject it
            pl = struct.pack('<I', 0) + b'\xAA\xBB'
            port.write(encode_frame(CMD_WRITE, pl))
            port.flush()
            cmd, _ = read_frame(port, timeout=10)
            r.check(cmd == RSP_ERROR, f"cmd=0x{cmd:02X} == RSP_ERROR(0x82)")
        except Exception as e:
            r.check(False, f"Exception: {e}")

        # ---- Test 3: Corrupt frame rejected ----
        r.begin("Corrupt frame (bad CRC) -> RSP_ERROR")
        try:
            wait_for_hello_window(port)
            frame = bytearray(encode_frame(CMD_STATUS))
            frame[-1] ^= 0xFF  # corrupt CRC
            port.write(bytes(frame))
            port.flush()
            cmd, _ = read_frame(port, timeout=5)
            r.check(cmd == RSP_ERROR, f"cmd=0x{cmd:02X} == RSP_ERROR(0x82)")
        except Exception as e:
            r.check(False, f"Exception: {e}")

        # ---- Test 4: COMMIT size mismatch ----
        r.begin("COMMIT size mismatch -> RSP_ERROR")
        try:
            wait_for_hello_window(port)
            firmware = b'\xDE\xAD\xBE\xEF'
            send_recv(port, encode_frame(CMD_WRITE, struct.pack('<I', 0) + firmware), timeout=5)
            commit = encode_frame(CMD_COMMIT, struct.pack('<II', 8, crc32(firmware)))
            cmd, _ = send_recv(port, commit, timeout=5)
            r.check(cmd == RSP_ERROR, f"cmd=0x{cmd:02X} == RSP_ERROR")
        except Exception as e:
            r.check(False, f"Exception: {e}")

        # ---- Test 5: STATUS command ----
        r.begin("STATUS returns current state")
        try:
            wait_for_hello_window(port)
            cmd, payload = send_recv(port, encode_frame(CMD_STATUS), timeout=5)
            r.check(cmd == RSP_STATUS, "STATUS -> RSP_STATUS")
            r.check(len(payload) == 4, f"payload={len(payload)} == 4")
        except Exception as e:
            r.check(False, f"Exception: {e}")

        # ---- Test 6: Full update flow ----
        r.begin("Full update: HELLO -> WRITE -> COMMIT -> REBOOT")
        try:
            firmware = bytes(range(64))
            fw_crc   = crc32(firmware)

            wait_for_hello_window(port)

            cmd, _ = send_recv(port, encode_frame(CMD_WRITE, struct.pack('<I', 0) + firmware), timeout=5)
            r.check(cmd == RSP_OK, "WRITE -> RSP_OK")

            cmd, _ = send_recv(port, encode_frame(CMD_COMMIT, struct.pack('<II', len(firmware), fw_crc)), timeout=5)
            r.check(cmd == RSP_OK, f"COMMIT -> RSP_OK")

            cmd, _ = send_recv(port, encode_frame(CMD_REBOOT), timeout=5)
            r.check(cmd == RSP_OK, "REBOOT -> RSP_OK")

            print("  INFO  Device rebooting — waiting for next listen window...")
            payload = wait_for_hello_window(port, timeout=15)
            r.check(True, "Device came back after REBOOT")
            if len(payload) == 4:
                print(f"  INFO  Post-reboot: slot_a=0x{payload[0]:02X} slot_b=0x{payload[1]:02X} "
                      f"active={payload[2]}")
            cmd, payload = send_recv(port, encode_frame(CMD_STATUS), timeout=5)
            r.check(cmd == RSP_STATUS, "STATUS after reboot -> RSP_STATUS")

        except Exception as e:
            r.check(False, f"Exception: {e}")

    return r.summary()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--port', default='COM19')
    p.add_argument('--baud', type=int, default=115200)
    args = p.parse_args()
    ok = run_tests(args.port, args.baud)
    sys.exit(0 if ok else 1)

if __name__ == '__main__':
    main()

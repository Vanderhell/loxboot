#!/usr/bin/env python3
"""
loxboot end-to-end test suite.

Spawns loxboot_sim.exe (the device simulator) for each test scenario and
drives the full UART protocol from the host side.  Tests every documented
feature of the loxboot UART protocol.

Usage:
    python tools/test_e2e.py [--sim path/to/loxboot_sim.exe]

Exit code: 0 = all pass, 1 = any failure.
"""

import argparse
import struct
import subprocess
import sys
import zlib
import os
from typing import Optional

# ---------------------------------------------------------------------------
# CRC16-CCITT (poly=0x1021, init=0xFFFF, no final XOR) — for UART frames
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
    """CRC32-CCITT matching loxboot_crc32() — same as zlib/PKZIP."""
    return zlib.crc32(data) & 0xFFFFFFFF

# ---------------------------------------------------------------------------
# Frame encode / decode
# ---------------------------------------------------------------------------
SOF       = 0x7E
CMD_HELLO  = 0x01
CMD_WRITE  = 0x02
CMD_COMMIT = 0x03
CMD_ABORT  = 0x04
CMD_STATUS = 0x05
CMD_REBOOT = 0x06

RSP_OK     = 0x81
RSP_ERROR  = 0x82
RSP_STATUS = 0x83

SLOT_STATE_EMPTY    = 0
SLOT_STATE_PENDING  = 1
SLOT_STATE_VALID    = 2
SLOT_STATE_INVALID  = 3
SLOT_STATE_ACTIVE   = 4
SLOT_STATE_ROLLBACK = 5

def encode_frame(cmd: int, payload: bytes = b'') -> bytes:
    plen = len(payload)
    header = bytes([cmd, plen & 0xFF, (plen >> 8) & 0xFF])
    crc = crc16(header + payload)
    return bytes([SOF]) + header + payload + bytes([crc & 0xFF, (crc >> 8) & 0xFF])

def read_frame(proc) -> tuple[int, bytes]:
    """Read one complete frame from proc.stdout. Returns (cmd, payload)."""
    sof = proc.stdout.read(1)
    if not sof or sof[0] != SOF:
        raise IOError(f"Expected SOF 0x7E, got {sof.hex() if sof else 'EOF'}")
    hdr = proc.stdout.read(3)
    if len(hdr) < 3:
        raise IOError("Truncated frame header")
    cmd  = hdr[0]
    plen = hdr[1] | (hdr[2] << 8)
    body = proc.stdout.read(plen + 2)  # payload + 2 CRC bytes
    if len(body) < plen + 2:
        raise IOError("Truncated frame body")
    payload = body[:plen]
    return cmd, payload

def parse_sim_stderr(text: str) -> dict:
    result = {}
    for line in text.strip().splitlines():
        if '=' in line:
            k, _, v = line.partition('=')
            result[k.strip()] = v.strip()
    return result

# ---------------------------------------------------------------------------
# Test harness
# ---------------------------------------------------------------------------
class Results:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self._current = None

    def begin(self, name: str):
        self._current = name
        print(f"\n{'-'*60}")
        print(f"TEST: {name}")

    def check(self, cond: bool, msg: str):
        if cond:
            self.passed += 1
            print(f"  PASS  {msg}")
        else:
            self.failed += 1
            print(f"  FAIL  {msg}")

    def summary(self):
        total = self.passed + self.failed
        print(f"\n{'='*60}")
        print(f"E2E RESULTS: {self.passed}/{total} passed", end="")
        if self.failed == 0:
            print("  ALL PASS")
        else:
            print(f"  {self.failed} FAILED")
        return self.failed == 0


def spawn_sim(sim_path: str, scenario: str = "default") -> subprocess.Popen:
    return subprocess.Popen(
        [sim_path, "--scenario", scenario],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

def send_recv(proc, frame: bytes) -> tuple[int, bytes]:
    proc.stdin.write(frame)
    proc.stdin.flush()
    return read_frame(proc)

def finish(proc) -> dict:
    proc.stdin.close()
    try:
        _, stderr = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        stderr = b""
    return parse_sim_stderr(stderr.decode(errors='replace'))

# ---------------------------------------------------------------------------
# Individual tests
# ---------------------------------------------------------------------------

def test_hello_returns_status(sim_path: str, r: Results):
    r.begin("HELLO returns RSP_STATUS with slot info")
    proc = spawn_sim(sim_path)
    cmd, payload = send_recv(proc, encode_frame(CMD_HELLO))
    r.check(cmd == RSP_STATUS, f"cmd=0x{cmd:02X} == RSP_STATUS(0x83)")
    r.check(len(payload) == 4, f"payload length={len(payload)} == 4")
    if len(payload) == 4:
        r.check(payload[2] == 0, f"active_slot={payload[2]} == SLOT_A(0)")
    finish(proc)


def test_full_update_flow(sim_path: str, r: Results):
    r.begin("Full update: HELLO -> WRITE -> COMMIT -> REBOOT")
    proc = spawn_sim(sim_path)
    firmware = bytes([0xAA, 0xBB, 0xCC, 0xDD])
    fw_crc = crc32(firmware)

    cmd, _ = send_recv(proc, encode_frame(CMD_HELLO))
    r.check(cmd == RSP_STATUS, f"HELLO -> RSP_STATUS")

    write_payload = struct.pack('<I', 0) + firmware  # offset=0 + data
    cmd, _ = send_recv(proc, encode_frame(CMD_WRITE, write_payload))
    r.check(cmd == RSP_OK, f"WRITE -> RSP_OK")

    commit_payload = struct.pack('<II', len(firmware), fw_crc)
    cmd, _ = send_recv(proc, encode_frame(CMD_COMMIT, commit_payload))
    r.check(cmd == RSP_OK, f"COMMIT -> RSP_OK")

    cmd, _ = send_recv(proc, encode_frame(CMD_REBOOT))
    r.check(cmd == RSP_OK, f"REBOOT -> RSP_OK")

    state = finish(proc)
    r.check(state.get('session_err') == '0',         "session exited cleanly")
    r.check(state.get('bytes_written') == '4',       "bytes_written=4")
    r.check(state.get('slot_erased') == '1',         "slot_erased=1")
    r.check(state.get('slot_b_state') == str(SLOT_STATE_PENDING), "slot_b=PENDING")
    r.check(state.get('slot_b_fw_size') == '4',      "firmware_size=4")
    r.check(state.get('slot_b_fw_crc') == f'0x{fw_crc:08X}', f"fw_crc=0x{fw_crc:08X}")


def test_multi_chunk_write(sim_path: str, r: Results):
    r.begin("Multi-chunk WRITE (3 chunks, 12 bytes total)")
    proc = spawn_sim(sim_path)
    firmware = bytes(range(12))  # 0x00..0x0B
    fw_crc = crc32(firmware)

    send_recv(proc, encode_frame(CMD_HELLO))

    # Write in 3 chunks of 4 bytes
    for i in range(3):
        chunk = firmware[i*4:(i+1)*4]
        payload = struct.pack('<I', i * 4) + chunk
        cmd, _ = send_recv(proc, encode_frame(CMD_WRITE, payload))
        r.check(cmd == RSP_OK, f"WRITE chunk {i} -> RSP_OK")

    commit_payload = struct.pack('<II', len(firmware), fw_crc)
    cmd, _ = send_recv(proc, encode_frame(CMD_COMMIT, commit_payload))
    r.check(cmd == RSP_OK, "COMMIT -> RSP_OK")

    cmd, _ = send_recv(proc, encode_frame(CMD_REBOOT))
    r.check(cmd == RSP_OK, "REBOOT -> RSP_OK")

    state = finish(proc)
    r.check(state.get('bytes_written') == '12',      "bytes_written=12")
    r.check(state.get('slot_b_state') == str(SLOT_STATE_PENDING), "slot_b=PENDING")


def test_write_before_hello_rejected(sim_path: str, r: Results):
    r.begin("WRITE before HELLO -> RSP_ERROR (session gating)")
    proc = spawn_sim(sim_path)
    payload = struct.pack('<I', 0) + b'\xAA\xBB'
    cmd, _ = send_recv(proc, encode_frame(CMD_WRITE, payload))
    r.check(cmd == RSP_ERROR, f"WRITE before HELLO -> RSP_ERROR")
    finish(proc)


def test_commit_before_hello_rejected(sim_path: str, r: Results):
    r.begin("COMMIT before HELLO -> RSP_ERROR (session gating)")
    proc = spawn_sim(sim_path)
    payload = struct.pack('<II', 4, 0x12345678)
    cmd, _ = send_recv(proc, encode_frame(CMD_COMMIT, payload))
    r.check(cmd == RSP_ERROR, "COMMIT before HELLO -> RSP_ERROR")
    finish(proc)


def test_reboot_before_hello_rejected(sim_path: str, r: Results):
    r.begin("REBOOT before HELLO -> RSP_ERROR (session gating)")
    proc = spawn_sim(sim_path)
    cmd, _ = send_recv(proc, encode_frame(CMD_REBOOT))
    r.check(cmd == RSP_ERROR, "REBOOT before HELLO -> RSP_ERROR")
    finish(proc)


def test_commit_size_mismatch(sim_path: str, r: Results):
    r.begin("COMMIT with wrong size -> RSP_ERROR")
    proc = spawn_sim(sim_path)
    send_recv(proc, encode_frame(CMD_HELLO))

    firmware = b'\xDE\xAD\xBE\xEF'
    send_recv(proc, encode_frame(CMD_WRITE, struct.pack('<I', 0) + firmware))

    # Claim size=8 but only wrote 4 bytes
    commit_payload = struct.pack('<II', 8, crc32(firmware))
    cmd, _ = send_recv(proc, encode_frame(CMD_COMMIT, commit_payload))
    r.check(cmd == RSP_ERROR, "COMMIT size mismatch -> RSP_ERROR")
    finish(proc)


def test_abort_invalidates_slot(sim_path: str, r: Results):
    r.begin("ABORT after WRITE -> slot invalidated")
    proc = spawn_sim(sim_path)
    send_recv(proc, encode_frame(CMD_HELLO))
    send_recv(proc, encode_frame(CMD_WRITE, struct.pack('<I', 0) + b'\xAA\xBB'))

    cmd, _ = send_recv(proc, encode_frame(CMD_ABORT))
    r.check(cmd == RSP_OK, "ABORT -> RSP_OK")

    state = finish(proc)
    r.check(state.get('slot_b_state') == str(SLOT_STATE_INVALID), "slot_b=INVALID after ABORT")


def test_status_command(sim_path: str, r: Results):
    r.begin("STATUS command returns slot state at any point")
    proc = spawn_sim(sim_path)
    send_recv(proc, encode_frame(CMD_HELLO))

    cmd, payload = send_recv(proc, encode_frame(CMD_STATUS))
    r.check(cmd == RSP_STATUS, "STATUS -> RSP_STATUS")
    r.check(len(payload) == 4, f"payload={len(payload)} bytes")
    if len(payload) == 4:
        # Fresh device: both slots EMPTY, active_slot=A
        r.check(payload[0] == SLOT_STATE_EMPTY, f"slot_a state={payload[0]} == EMPTY({SLOT_STATE_EMPTY})")
        r.check(payload[2] == 0, f"active_slot={payload[2]} == SLOT_A(0)")
    finish(proc)


def test_write_out_of_bounds(sim_path: str, r: Results):
    r.begin("WRITE beyond slot_size -> RSP_ERROR")
    proc = spawn_sim(sim_path)
    send_recv(proc, encode_frame(CMD_HELLO))

    # slot_size is 0x8000 = 32768 in test layout; write at offset 32767 with 2 bytes -> out of bounds
    offset = 0x8000 - 1  # last byte
    payload = struct.pack('<I', offset) + b'\xAA\xBB'  # 2 bytes starting at last slot byte
    cmd, _ = send_recv(proc, encode_frame(CMD_WRITE, payload))
    r.check(cmd == RSP_ERROR, "WRITE out of bounds -> RSP_ERROR")
    finish(proc)


def test_corrupt_frame_rejected(sim_path: str, r: Results):
    r.begin("Frame with bad CRC -> RSP_ERROR")
    proc = spawn_sim(sim_path)

    # Build a valid HELLO frame then flip the last CRC byte
    frame = bytearray(encode_frame(CMD_HELLO))
    frame[-1] ^= 0xFF  # corrupt CRC
    proc.stdin.write(bytes(frame))
    proc.stdin.flush()
    cmd, _ = read_frame(proc)
    r.check(cmd == RSP_ERROR, "Corrupt frame -> RSP_ERROR")
    finish(proc)


def test_crc32_known_vector(r: Results):
    r.begin("CRC32 host-side: known vector '123456789' -> 0xCBF43926")
    result = crc32(b"123456789")
    r.check(result == 0xCBF43926, f"crc32('123456789')=0x{result:08X} == 0xCBF43926")


def test_crc16_known_vector(r: Results):
    r.begin("CRC16 host-side: known vector '123456789' -> 0x29B1")
    result = crc16(b"123456789")
    r.check(result == 0x29B1, f"crc16('123456789')=0x{result:04X} == 0x29B1")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def find_sim(hint: Optional[str]) -> str:
    if hint:
        return hint
    base = os.path.join(os.path.dirname(__file__), '..', 'build')
    candidates = [
        os.path.join(base, 'Debug', 'loxboot_sim.exe'),
        os.path.join(base, 'Release', 'loxboot_sim.exe'),
        os.path.join(base, 'loxboot_sim'),
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    print("ERROR: loxboot_sim not found. Build with:")
    print("  cmake -B build -DLOXBOOT_BUILD_TESTS=ON -DLOXBOOT_BUILD_UART_PORT=ON")
    print("  cmake --build build --config Debug")
    sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="loxboot E2E test suite")
    parser.add_argument('--sim', help="Path to loxboot_sim executable")
    args = parser.parse_args()

    sim = find_sim(args.sim)
    print(f"Simulator: {sim}")

    r = Results()

    # Pure Python sanity checks (no simulator needed)
    test_crc32_known_vector(r)
    test_crc16_known_vector(r)

    # Protocol tests (each spawns a fresh simulator)
    test_hello_returns_status(sim, r)
    test_full_update_flow(sim, r)
    test_multi_chunk_write(sim, r)
    test_write_before_hello_rejected(sim, r)
    test_commit_before_hello_rejected(sim, r)
    test_reboot_before_hello_rejected(sim, r)
    test_commit_size_mismatch(sim, r)
    test_abort_invalidates_slot(sim, r)
    test_status_command(sim, r)
    test_write_out_of_bounds(sim, r)
    test_corrupt_frame_rejected(sim, r)

    ok = r.summary()
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()

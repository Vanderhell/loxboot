#!/usr/bin/env python3
"""Diagnostic: do one HELLO->WRITE->COMMIT->REBOOT then capture serial log."""
import struct, sys, time, zlib, serial
sys.path.insert(0, ".")
from test_e2e_ota import (encode_frame, read_frame, wait_hello, send_recv,
                          upload_firmware, crc32,
                          CMD_COMMIT, CMD_REBOOT, RSP_OK)

PORT = "COM19"
FW = r"C:\Users\vande\Desktop\loxboot\idf_project\build\loxboot_esp32.bin"

with open(FW, "rb") as f:
    fw = f.read()

with serial.Serial(PORT, 115200, timeout=2) as port:
    print("Waiting for listen window...")
    wait_hello(port)
    print(f"Uploading {len(fw)} bytes...")
    upload_firmware(port, fw)
    cmd, _ = send_recv(port, encode_frame(CMD_COMMIT, struct.pack('<II', len(fw), crc32(fw))), timeout=10)
    print(f"COMMIT -> 0x{cmd:02X}")
    cmd, _ = send_recv(port, encode_frame(CMD_REBOOT), timeout=10)
    print(f"REBOOT -> 0x{cmd:02X}")
    print("=== Capturing serial for 8s ===")
    port.timeout = 0.5
    deadline = time.time() + 8
    buf = b""
    while time.time() < deadline:
        chunk = port.read(512)
        if chunk:
            buf += chunk
    print(buf.decode(errors="replace"))

#!/usr/bin/env python3
"""Run the local loxboot verification profile.

This script only performs local checks:
- configure CMake with tests and UART port enabled
- build the workspace
- run CTest with output on failure

It does not run GitHub Actions, GitHub Release, or hardware tests.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build"


def run(cmd: list[str]) -> None:
    print("+ " + " ".join(cmd))
    subprocess.run(cmd, cwd=ROOT, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run local loxboot verification")
    parser.add_argument("--build-dir", default=str(BUILD_DIR))
    args = parser.parse_args()

    build_dir = Path(args.build_dir)

    try:
        run([
            "cmake",
            "-S", str(ROOT),
            "-B", str(build_dir),
            "-DLOXBOOT_BUILD_TESTS=ON",
            "-DLOXBOOT_BUILD_UART_PORT=ON",
            "-DLOXBOOT_BUILD_EXAMPLES=ON",
        ])
        run(["cmake", "--build", str(build_dir)])
        run(["ctest", "--test-dir", str(build_dir), "-C", "Debug", "--output-on-failure"])
    except subprocess.CalledProcessError as exc:
        print(f"LOCAL VERIFY: FAIL (command exited with {exc.returncode})")
        print("CI: NOT RUN")
        print("HARDWARE: NOT RUN")
        print("RELEASE: NOT RUN")
        return exc.returncode or 1

    print("LOCAL VERIFY: PASS")
    print("CI: NOT RUN")
    print("HARDWARE: NOT RUN")
    print("RELEASE: NOT RUN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

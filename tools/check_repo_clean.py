#!/usr/bin/env python3
"""Fail when generated artifacts are tracked or present in the repo tree."""

from __future__ import annotations

import fnmatch
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

FORBIDDEN_PATTERNS = [
    "__pycache__/",
    "*.pyc",
    "CMakeCache.txt",
    "CMakeFiles/",
    "build/",
    "build_*/",
    "cmake-build-*/",
    "idf_project/build/",
    "idf_project/build_*/",
    "idf_project/.cache/",
    "idf_project/sdkconfig",
    "idf_project/sdkconfig.old",
    "idf_project/dependencies.lock",
    "idf_project/managed_components/",
    "*.elf",
    "*.bin",
    "*.map",
    "*.hex",
    "*.uf2",
]


def git_ls_files() -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=ROOT,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return [item for item in result.stdout.decode("utf-8", errors="replace").split("\0") if item]


def is_forbidden(path: str) -> bool:
    norm = path.replace("\\", "/")
    parts = [part for part in norm.split("/") if part]
    for pattern in FORBIDDEN_PATTERNS:
        if pattern.endswith("/"):
            needle = pattern[:-1]
            if any(fnmatch.fnmatch(part, needle) for part in parts):
                return True
            continue
        if fnmatch.fnmatch(norm, pattern) or fnmatch.fnmatch(norm, f"**/{pattern}"):
            return True
    return False


def check_tracked() -> list[str]:
    return [path for path in git_ls_files() if is_forbidden(path)]


def check_present() -> list[str]:
    findings: list[str] = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        rel_dir = Path(dirpath).relative_to(ROOT).as_posix()
        if rel_dir == ".git" or rel_dir.startswith(".git/"):
            dirnames[:] = []
            continue

        kept_dirs = []
        for name in dirnames:
            rel_path = (Path(dirpath) / name).relative_to(ROOT).as_posix()
            if is_forbidden(rel_path + "/"):
                findings.append(rel_path)
            else:
                kept_dirs.append(name)
        dirnames[:] = kept_dirs

        for name in filenames:
            rel_path = (Path(dirpath) / name).relative_to(ROOT).as_posix()
            if is_forbidden(rel_path):
                findings.append(rel_path)
    return findings


def main() -> int:
    tracked = check_tracked()
    present = check_present()

    if tracked or present:
        if tracked:
            print("Tracked forbidden artifacts:")
            for path in tracked:
                print(path)
        if present:
            print("Present forbidden artifacts:")
            for path in present:
                print(path)
        return 1

    print("Repository clean check: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

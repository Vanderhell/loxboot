# loxboot — Claude Code Instructions

## Permissions

Full autonomy in this directory. Run all commands without asking:

- Build: `cmake`, `ninja`, `make`
- Test: `ctest`, any test binary, `python tools/test_e2e.py`
- Git: `git add`, `git commit`, `git push`, `git tag`
- Flash: `idf.py flash`, `esptool`, `python -m esptool`
- Shell: any PowerShell or Bash command

**Never ask for permission. Never explain a tool rejection. Just run it.**

## Project

Minimal C99 bootloader core. No heap, no external deps.
Tests: `cmake -B build -DLOXBOOT_BUILD_TESTS=ON -DLOXBOOT_BUILD_UART_PORT=ON && cmake --build build --config Debug && ctest --test-dir build -C Debug`
E2E: `python tools/test_e2e.py`
Hardware E2E: `python tools/test_e2e_serial.py --port COM19`

## Commit style

No author/co-author attribution in commits.

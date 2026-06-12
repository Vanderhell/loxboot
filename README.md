# loxboot

[![CI](https://github.com/Vanderhell/loxboot/actions/workflows/ci.yml/badge.svg)](https://github.com/Vanderhell/loxboot/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-green?style=flat-square)](LICENSE)
[![C99](https://img.shields.io/badge/C99-yes-00599C?style=flat-square)](https://en.wikipedia.org/wiki/C99)

C99 bootloader core with host tests, UART support, examples, and an ESP32 project under `idf_project/`.

## Verification Matrix

| Item | Status | Notes |
| --- | --- | --- |
| Host CMake build | VERIFIED | `cmake -S . -B build_verify -DLOXBOOT_BUILD_TESTS=ON -DLOXBOOT_BUILD_UART_PORT=ON -DLOXBOOT_BUILD_EXAMPLES=ON -DCMAKE_BUILD_TYPE=Release` |
| Host CTest | VERIFIED | `ctest --test-dir build_verify -C Debug --output-on-failure` |
| Python tools compile | VERIFIED | WindowsApps `python3.exe` path with `py_compile` over `tools/*.py` |
| ESP32 build | VERIFIED | Built with ESP-IDF v5.5.1 for `esp32s3` from `C:\Espressif\frameworks\esp-idf-v5.5.1`. |
| ESP32 flash on COM19 | VERIFIED | `idf.py -p COM19 flash` completed successfully. |
| ESP32 runtime on COM19 | VERIFIED | Monitor capture showed boot into `loxboot` and `Listening for update via USB...`. |
| STM32 hardware | NOT VERIFIED | Hardware not available. |

## Build

```bash
cmake -S . -B build -DLOXBOOT_BUILD_TESTS=ON -DLOXBOOT_BUILD_UART_PORT=ON -DLOXBOOT_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

The generic example builds when `LOXBOOT_BUILD_EXAMPLES=ON` is set.

## Layout

- `src/` core C sources
- `ports/uart/` UART transport
- `adapters/` platform adapters
- `examples/generic_custom_adapter/` build-checkable example
- `tests/` host unit tests
- `tools/` local verification and E2E scripts
- `idf_project/` ESP32 project

## License

MIT

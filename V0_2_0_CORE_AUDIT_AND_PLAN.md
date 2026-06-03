# v0.2.0-core audit + plan (local)

Status: `v0.2.0-core candidate` (not a complete bootloader).

This file records what was implemented/verified in the current workspace and what remains out of scope until v0.3.0+.

## What was done (v0.2.0-core only)

- Repository layout normalized:
  - Public headers under `include/loxboot/`
  - Sources under `src/`
  - Tests under `tests/`
  - Docs under `docs/`
  - CI workflow under `.github/workflows/ci.yml`
  - Added `.gitignore`
- Spec/API consistency fixes (docs aligned to current public headers):
  - Removed/avoided fields not present in `loxboot_platform_t` (`app_jump_base`, `boot_state_size`)
  - Kept flash adapter types in `include/loxboot/loxboot.h` (no `loxboot_flash.h` split)
  - Normalized CMake option naming (`LOXBOOT_BUILD_STM32_ADAPTER`, `LOXBOOT_BUILD_ESP32_ADAPTER`)
  - Clarified that boot-state copy size in v0.2.0-core is `sizeof(loxboot_state_t)` per copy
  - Clarified that `loxboot_run()` is a v0.2.0-core stub returning `LOXBOOT_ERR_INVALID_STATE`
  - Corrected slot record CRC coverage/size text to match the actual C layout
- Core implementation added:
  - `loxboot_crc32()`
  - boot-state helpers (dual-copy read/validate/restore + write-to-both, erase-before-write)
  - v0.2 slot-control APIs (`loxboot_init`, `loxboot_commit_slot`, `loxboot_invalidate_slot`,
    `loxboot_confirm_boot`, `loxboot_request_slot`, plus getters)
  - `loxboot_run()` stub only
- Deterministic CTest suite added (no external framework):
  - RAM flash model + failure injection + helpers
  - 8 test executables registered in CTest, including boot-state edge cases

## What was verified (executed evidence)

- Local builds + CTest:
  - MSVC (Visual Studio 17 2022 generator): Debug + Release
  - clang-cl (Visual Studio 17 2022 generator, `-T ClangCL`): Debug + Release
- Static checks executed:
  - No heap calls (`malloc/calloc/realloc/free`) in `include/` + `src/`
  - No vendor/RTOS/CMSIS/HAL includes in `include/` + `src/`

## Current limitations (explicit)

- `loxboot_run()` is **not implemented** beyond a stub.
- No rollback, no crash-loop detection, no jump, no UART, no STM32/ESP32 adapters, no OTA.
- Verified compilers are limited to **local MSVC and local clang-cl** in this environment.
- Linux GCC/Clang and GitHub Actions are **not verified** here.

## Plan (next steps)

### Still within v0.2.0-core scope

- Increase test coverage around boot-state/state-write edge cases if needed (more corrupt/partial/failure permutations).
- Make “diff audit” meaningful by introducing a baseline:
  - This repo currently has **no commits**, so `git diff` outputs nothing. Create an initial commit (human-controlled)
    or establish another baseline for comparison.

### Out of scope until v0.3.0+

- Implement `loxboot_run()` full boot sequence.
- Implement crash-loop detection, rollback, and jump mechanism + extensive tests.
- Implement UART port and adapters (STM32/ESP32) and any hardware evidence.


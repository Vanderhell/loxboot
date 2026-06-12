# Power-Loss Test Plan

This file is a plan, not evidence. No power-loss test is verified until a hardware log is added to `docs/HARDWARE_EVIDENCE.md`.

## Goal

Validate that the update path fails safely under flash interruption, disconnect, and restart events.

## Required host-side fault scenarios

- fail before erase
- fail after partial erase
- fail before write
- fail after N bytes written
- corrupt primary boot state
- corrupt backup boot state
- corrupt both boot state copies

## Required update-path scenarios

- primary state corrupted, backup valid -> recovery succeeds
- backup state corrupted, primary valid -> recovery succeeds
- both state copies corrupted -> fresh format or fatal path according to current design
- write failure during firmware update -> target slot not accepted as valid
- COMMIT with bad CRC -> rejected or target invalidated according to implementation

## Required hardware scenarios

- disconnect before HELLO
- disconnect during first WRITE before erase
- disconnect during erase / first WRITE
- disconnect during middle WRITE
- disconnect after all WRITE before COMMIT
- disconnect during COMMIT
- disconnect after COMMIT before REBOOT
- disconnect during reboot
- reconnect and STATUS query
- verify active slot still boots
- verify target slot state is invalid, pending, or valid according to the documented state machine

## Current status

- Host-side fault injection: implemented via `tools/loxboot_sim.c` scenarios and `tools/test_e2e.py`
- Hardware disconnect / power-loss testing: not verified in this task
- Evidence log: missing
- Manual ESP32 helper: `tools/test_e2e_ota_powerloss.py`

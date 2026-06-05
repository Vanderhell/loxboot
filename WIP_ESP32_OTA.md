# ESP32-S3 Full OTA Hardware Test — DONE ✅

**Resolved:** 2026-06-05
**Branch:** master (uncommitted — ready to commit)
**Result:** `tools/test_e2e_ota.py` → **ALL PASS** on real ESP32-S3 (COM19).

---

## Final status

The complete OTA flow passes on hardware:

```
TEST: Initial state ........... active=0 (ota_0)                 PASS
TEST: OTA update .............. HELLO→WRITE→COMMIT→REBOOT→ota_1  PASS
        post-reboot active=1, slot_b=VALID (confirm called)     PASS
TEST: Persistence ............. ota_1 still active after reboot  PASS
TEST: Rollback ................ corrupt update rejected at COMMIT PASS
OTA HARDWARE E2E: ALL PASS
```

Host tests: **15/15 ctest targets pass** (MSVC), including the ESP32 stub
platform test.

---

## Root causes found & fixed (the "no HELLO response" blocker)

The device streamed fine but never answered HELLO. Isolation (liveness marker +
raw-RX echo + a live `loxboot_state_read` report over USB-JTAG) proved:

1. **USB-JTAG was never the problem.** `usb_serial_jtag_driver_install` returned
   ESP_OK and raw RX echo worked. The secondary USB-JTAG console coexists fine.

2. **State corruption (the real bug).** `loxboot_esp32_sync_state_from_ota()`
   operated on `ctx->state`, which `loxboot_init()` memsets to all-zeros
   (magic=0), then wrote it back to flash. That overwrote the valid state
   `loxboot_format_state()` had just written. On read, `magic != STATE_MAGIC`
   → `RECORD_CORRUPT`, so `loxboot_uart_run_session()` bailed at its initial
   `loxboot_state_read()` **before** ever reading a HELLO byte.
   **Fix:** sync now does read-modify-write (seed from `loxboot_state_read`,
   apply OTA mapping, write back, update `ctx->state`) — same pattern as
   `commit_slot`/`invalidate_slot`. `adapters/esp32/loxboot_esp32_platform.c`

3. **Stub test didn't compile.** The address-compare added last session made
   `loxboot_esp32_platform.c` deref an incomplete `esp_partition_t` in the host
   stub build (struct lived only in `tests/esp_ota_stub.h`, not visible to the
   adapter TU). **Fix:** moved the concrete struct into the platform header's
   stub branch. `loxboot_esp32_platform.h`, `tests/esp_ota_stub.h`

4. **Corrupt-image detection.** `loxboot_commit_slot()` only records size+CRC;
   CRC is verified in `loxboot_run()` at boot — but the ESP32 updater hands off
   via `esp_ota_set_boot_partition()` and never calls `loxboot_run()`.
   **Fix:** added `loxboot_verify_slot()` (core) and call it from the UART
   COMMIT handler so a bad CRC is rejected at commit time, on every platform.
   `src/loxboot_core.c`, `include/loxboot/loxboot.h`, `ports/uart/loxboot_uart.c`

Also fixed earlier this effort: the flash procedure must write
`build/ota_data_initial.bin` at `0x10000` — use `idf.py flash` (the manual
esptool line in the old notes omitted it).

---

## Remaining (optional polish, not blocking)

- `sdkconfig.defaults` still sets bootloader log INFO (handy; harmless).
- Rollback-on-corrupt-boot via IDF is not exercised — loxboot rejects the bad
  image at COMMIT first, so the IDF rollback path is never reached in this test.
- GCC/Clang CI + ASAN/UBSAN: needs a push.

## Handy commands

```powershell
& "C:\Espressif\frameworks\esp-idf-v5.5.1\export.ps1"
cd C:\Users\vande\Desktop\loxboot\idf_project
idf.py build
idf.py -p COM19 -b 460800 erase-flash
idf.py -p COM19 -b 460800 flash       # includes ota_data_initial.bin @ 0x10000

cd C:\Users\vande\Desktop\loxboot
python tools\test_e2e_ota.py --port COM19 --firmware idf_project\build\loxboot_esp32.bin

# host tests
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Device
- ESP32-S3 on **COM19** (USB Serial JTAG, VID 303A). Use COM19 (others COM6..30).

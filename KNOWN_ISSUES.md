# Known Issues & Limitations

## Critical Issues (Require Resolution)

### 1. Erase Granularity Mismatch
**Status:** Documented, requires platform-specific handling  
**Impact:** Boot state corruption on platforms with large flash sectors

**Description:**
- Core assumes arbitrary-size erase: `ctx->flash.erase(ctx, base, 52u)` for boot state
- STM32 enforces sector-aligned erase (e.g., 2KB per page)
- ESP32 enforces 4KB-aligned erase (partition-level granularity)

**Current behavior:**
- Core writes boot state (52 bytes) to flash
- Adapter must round up erase size to sector boundary (example: 2KB on STM32, 4KB on ESP32)
- This may corrupt neighboring data if slots aren't properly spaced

**Resolution required:**
1. **Option A (API change):** Add `erase_granularity` to platform configuration
   - Core calls `ctx->flash.erase(ctx, base, ctx->platform.erase_alignment(52u))`
   - Adapters implement granularity rounding
   
2. **Option B (Adapter workaround):** Allocate full sector for boot state in layout
   - Memory layout: Boot state primary (2KB), Backup (2KB), Slot A (256KB), Slot B (256KB)
   - Guarantees no data corruption from erase

**Recommended:** Option B for v0.6.0 (no API change), plan Option A for v1.0

---

## Important Limitations (Hardware/Test Gaps)

### 2. ARM Cortex-M Jump Mechanism
**Status:** Code complete, hardware validation pending  
**Impact:** Device cannot boot application after update

**What works:**
- Jump routine `loxboot_jump_to_app()` implemented in core
- Stack pointer initialization verified in code
- Interrupt disable before jump implemented

**What requires testing:**
- Real STM32/ARM hardware execution
- Interrupt masking actually prevents execution before jump
- Entry point alignment and memory layout assumptions
- Xtensa/RISC-V platforms: no implementation yet

**Test required:** Hardware boot on real STM32 board

---

### 3. Power-Loss Recovery
**Status:** Mock tests exist, real hardware scenarios untested  
**Impact:** Data corruption or unbootable device if power lost during update

**Scenarios covered in code:**
- Dual-copy state recovery on CRC mismatch
- Rollback on firmware CRC fail

**Scenarios NOT tested:**
- Power loss during slot erase (partial erase)
- Power loss during firmware write (mid-chunk)
- Power loss during boot state write
- Interrupt during state write
- Flash corruption patterns from mid-erase
- Recovery from corrupted boot state on boot

**Test required:** Lab equipment with power injection + capture

---

### 4. Flash Adapter Integration
**Status:** Stubbed for Windows build, real hardware untested  
**Impact:** Core UART protocol works, but can't actually write firmware to device

**STM32 Adapter:**
- Code present, cast issue fixed
- Requires real `stm32_hal.h` (user provides)
- Flash page size varies by variant (needs configuration)
- Dual-bank behavior on some STM32s not handled

**ESP32 Adapter:**
- Code present
- Requires `esp_partition.h` from ESP-IDF
- Partition table must be configured by user
- OTA partition layout must match slot definitions

**Test required:** Real hardware with vendor HAL/IDF

---

### 5. UART Frame Loss & Corruption
**Status:** Timeout-based recovery, no retry mechanism  
**Impact:** Long transfers may fail silently

**Current implementation:**
- Per-byte timeout: waits for next byte or aborts
- No frame retry: if frame rejected, update must start over
- No checksum verification beyond CRC16

**Limitations:**
- Single corrupted byte fails entire frame
- Noisy serial lines may cause frequent retries
- Large firmware files (1MB+) on slow UART (9600 baud) risk timeout

**Recommendation:** Implement UART retry mechanism for production

---

## Documentation Gaps

### 6. Memory Layout Requirements
**Missing:** Hardware-specific memory layout templates

**Required per platform:**
- Flash sector size
- Boot state allocation (must fit in one sector)
- Slot A location & size
- Slot B location & size
- Linker script for application partition

**Status:** MEMORY_LAYOUT.md exists but lacks STM32/ESP32 examples

---

### 7. CRC32 Verification Assumptions
**Status:** Assumes 4KB chunks sufficient for all platforms

**Description:**
- Core reads firmware via 4KB chunks to verify CRC32
- Some platforms may have memory-mapped flash (no chunking needed)
- Some platforms may require smaller chunks for cache efficiency

**Impact:** Performance on platforms with non-standard memory models

---

## Future Improvements

### 8. Rollback to N-2 Slots
**Current:** Only A/B (2-copy) rollback  
**Requested:** A/B/C for longer history

**Impact:** Less aggressive about using new firmware if kernel issues suspected

---

### 9. Recovery Mode (UART without Boot)
**Current:** UART only listens during boot sequence  
**Requested:** Always-available recovery mode with hardware button

**Impact:** Can fix bricked devices without JTAG

---

### 10. Signed Firmware Updates
**Current:** CRC32 verification only (detects corruption, not tampering)  
**Missing:** Ed25519 or ECDSA signature validation

**Impact:** Vulnerable to untrusted firmware injection

---

## Test Matrix Status

| Scenario | Mock Test | Hardware Test | Status |
|----------|-----------|---------------|--------|
| Boot from A | ✅ | ⏳ | Ready for HW |
| Boot from B | ✅ | ⏳ | Ready for HW |
| Update A→B | ✅ | ⏳ | Ready for HW |
| Update B→A | ✅ | ⏳ | Ready for HW |
| Rollback on CRC | ✅ | ⏳ | Ready for HW |
| Crash loop auto-rollback | ✅ | ⏳ | Ready for HW |
| Dual-copy recovery | ✅ | ⏳ | Ready for HW |
| Power loss (erase) | ❌ | ⏳ | **MISSING** |
| Power loss (write) | ❌ | ⏳ | **MISSING** |
| Partial frame loss | ✅ | ⏳ | Ready for HW |
| UART timeout | ✅ | ⏳ | Ready for HW |
| NULL adapter callbacks | ✅ | N/A | Complete |

---

## Verification Checklist Before Production

- [ ] Hardware: Flash erase/write/read verified on target
- [ ] Hardware: Jump to app works on real board
- [ ] Hardware: Power loss scenarios tested in lab
- [ ] Hardware: UART update end-to-end on real serial port
- [ ] Hardware: Boot reason tracking validates correctly
- [ ] Security: Firmware signing implemented if needed
- [ ] Documentation: Platform-specific memory layout documented
- [ ] Documentation: Adapter implementation guide completed
- [ ] Testing: All hardware test scenarios passed
- [ ] Review: Code audit by external security reviewer (optional)

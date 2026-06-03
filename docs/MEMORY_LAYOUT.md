# loxboot — Memory Layout Guide

Reference for configuring flash regions on common MCU families.

---

## Concepts

```
┌──────────────────────────────────┐  ← 0x08000000 (STM32) or 0x00000000 (generic)
│                                  │
│   loxboot image                  │  Your compiled bootloader binary
│   (typically 8–32 KB)            │
│                                  │
├──────────────────────────────────┤  ← boot_state_primary_base
│   Boot state — primary           │  sizeof(loxboot_state_t) ≈ 60 bytes
│   (must be sector-aligned)       │  (but occupies a full erase sector)
├──────────────────────────────────┤  ← boot_state_backup_base
│   Boot state — backup            │  Same size, separate sector
│   (must be sector-aligned)       │
├──────────────────────────────────┤  ← slot_a_base
│                                  │
│   Slot A                         │  slot_size bytes
│   (firmware image)               │  Must hold your largest expected firmware
│                                  │
├──────────────────────────────────┤  ← slot_b_base
│                                  │
│   Slot B                         │  slot_size bytes
│   (firmware image)               │
│                                  │
└──────────────────────────────────┘
```

**Rules:**
- All base addresses must be flash-erase-sector aligned
- `boot_state_primary_base` and `boot_state_backup_base` must be in different sectors
- `slot_a_base` and `slot_b_base` must not overlap
- `slot_size` must be identical for both slots
- loxboot image must not overlap any of the above regions

---

## STM32F4xx (1MB flash, 16KB/16KB/16KB/16KB/64KB/128KB... sectors)

```
Sector 0: 0x08000000 – 0x08003FFF  (16KB)  → loxboot image
Sector 1: 0x08004000 – 0x08007FFF  (16KB)  → boot state primary
Sector 2: 0x08008000 – 0x0800BFFF  (16KB)  → boot state backup
Sector 3: 0x0800C000 – 0x0800FFFF  (16KB)  → (reserved / future)
Sector 4: 0x08010000 – 0x0801FFFF  (64KB)  → (reserved / future)
Sector 5: 0x08020000 – 0x0803FFFF  (128KB) → Slot A (start)
Sector 6: 0x08040000 – 0x0805FFFF  (128KB) → Slot A (cont.)
Sector 7: 0x08060000 – 0x0807FFFF  (128KB) → Slot B (start)
Sector 8: 0x08080000 – 0x0809FFFF  (128KB) → Slot B (cont.)
...
```

```c
loxboot_platform_t platform = {
    .boot_state_primary_base = 0x08004000,
    .boot_state_backup_base  = 0x08008000,
    .slot_a_base             = 0x08020000,
    .slot_b_base             = 0x08060000,
    .slot_size               = 0x00040000,  // 256KB per slot
};
```

---

## STM32L4xx (256KB flash, 2KB pages)

```
Pages 0–3:   0x08000000 – 0x08001FFF  (8KB)   → loxboot image
Pages 4–5:   0x08002000 – 0x08002FFF  (4KB)   → boot state primary (2 pages)
Pages 6–7:   0x08003000 – 0x08003FFF  (4KB)   → boot state backup  (2 pages)
Pages 8–63:  0x08004000 – 0x0801FFFF  (112KB) → Slot A
Pages 64–119:0x08020000 – 0x0803BFFF  (112KB) → Slot B
```

```c
loxboot_platform_t platform = {
    .boot_state_primary_base = 0x08002000,
    .boot_state_backup_base  = 0x08003000,
    .slot_a_base             = 0x08004000,
    .slot_b_base             = 0x08020000,
    .slot_size               = 0x0001C000,  // 112KB per slot
};
```

---

## ESP32 (using esp_partition)

ESP32 uses a partition table. Define partitions in `partitions.csv`:

```
# Name,   Type, SubType, Offset,   Size,    Flags
nvs,      data, nvs,     0x9000,   0x6000,
loxboot,  app,  factory, 0x10000,  0x10000,
loxstate, data, 0x40,    0x20000,  0x1000,
loxstbak, data, 0x41,    0x21000,  0x1000,
slot_a,   app,  ota_0,   0x30000,  0xD0000,
slot_b,   app,  ota_1,   0x100000, 0xD0000,
```

```c
// ESP32 addresses come from esp_partition_find() at runtime
// The ESP32 adapter resolves them; loxboot_platform_t is filled by the adapter
```

---

## Generic / simulation (tests)

```c
#define TEST_BOOT_STATE_PRIMARY  0x00000000U
#define TEST_BOOT_STATE_BACKUP   0x00000100U
#define TEST_SLOT_A_BASE         0x00001000U
#define TEST_SLOT_B_BASE         0x00009000U
#define TEST_SLOT_SIZE           0x00008000U  // 32KB

loxboot_platform_t platform = {
    .boot_state_primary_base = TEST_BOOT_STATE_PRIMARY,
    .boot_state_backup_base  = TEST_BOOT_STATE_BACKUP,
    .slot_a_base             = TEST_SLOT_A_BASE,
    .slot_b_base             = TEST_SLOT_B_BASE,
    .slot_size               = TEST_SLOT_SIZE,
};
```

---

## Sizing slot_size

`slot_size` must be large enough for your largest firmware image.

Recommended: `slot_size = round_up(max_firmware_size * 1.1, sector_size)`

Add 10% margin for future growth, then round up to the next erase sector boundary.

---

## loxboot_state_t size reference

```c
sizeof(loxboot_state_t) = 
    4  (magic)
  + 1  (active_slot)
  + 1  (boot_reason)
  + 2  (reserved)
  + 24 (slot_record A)   // see loxboot_slot_record_t
  + 24 (slot_record B)
  + 4  (state_crc32)
= 60 bytes
```

Always reserve at least one full erase sector per copy, regardless of struct size.

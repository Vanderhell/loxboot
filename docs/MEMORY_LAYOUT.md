# loxboot Memory Layout Guide

Reference for configuring flash regions on common MCU families.

## Concepts

- `boot_state_primary_base` and `boot_state_backup_base` must be in different sectors
- `slot_a_base` and `slot_b_base` must not overlap
- `slot_size` must be identical for both slots
- loxboot image must not overlap any of the above regions

## STM32F4xx example

```text
Sector 0: 0x08000000 - 0x08003FFF  (16KB)  -> loxboot image
Sector 1: 0x08004000 - 0x08007FFF  (16KB)  -> boot state primary
Sector 2: 0x08008000 - 0x0800BFFF  (16KB)  -> boot state backup
Sector 3: 0x0800C000 - 0x0800FFFF  (16KB)  -> reserved
Sector 4: 0x08010000 - 0x0801FFFF  (64KB)  -> reserved
Sector 5: 0x08020000 - 0x0803FFFF  (128KB) -> Slot A
Sector 6: 0x08040000 - 0x0805FFFF  (128KB) -> Slot A
Sector 7: 0x08060000 - 0x0807FFFF  (128KB) -> Slot B
Sector 8: 0x08080000 - 0x0809FFFF  (128KB) -> Slot B
```

```c
loxboot_platform_t platform = {
    .boot_state_primary_base = 0x08004000,
    .boot_state_backup_base  = 0x08008000,
    .slot_a_base             = 0x08020000,
    .slot_b_base             = 0x08060000,
    .slot_size               = 0x00040000,
};
```

## Generic test layout

```c
loxboot_platform_t platform = {
    .boot_state_primary_base = 0x00000000,
    .boot_state_backup_base  = 0x00000100,
    .slot_a_base             = 0x00001000,
    .slot_b_base             = 0x00009000,
    .slot_size               = 0x00008000,
};
```

## Sizing note

Add margin for growth, then round up to the next erase sector boundary.

# loxboot Specification

This document describes the protocol and state machine implemented in this repository.
It does not claim hardware verification.

## Overview

loxboot is a C99 bootloader core with:

- dual-copy boot state
- CRC32 integrity checks
- slot management for A/B updates
- crash-loop rollback
- UART update sessions

## Public Concepts

```c
typedef enum {
    LOXBOOT_SLOT_A = 0,
    LOXBOOT_SLOT_B = 1
} loxboot_slot_id_t;

typedef enum {
    LOXBOOT_OK = 0,
    LOXBOOT_ERR_INVALID_ARG,
    LOXBOOT_ERR_INVALID_STATE,
    LOXBOOT_ERR_TIMEOUT,
    LOXBOOT_ERR_TRANSPORT,
    LOXBOOT_ERR_FLASH_READ,
    LOXBOOT_ERR_FLASH_WRITE,
    LOXBOOT_ERR_FLASH_ERASE
} loxboot_err_t;
```

## UART Frame Format

```text
Byte 0:     0x7E (SOF)
Byte 1:     Command
Bytes 2-3:  Payload length, little-endian
Bytes 4..n: Payload
Bytes n+1,n+2: CRC16-CCITT, little-endian
```

CRC16-CCITT settings:

- polynomial: `0x1021`
- initial value: `0xFFFF`
- final XOR: `0x0000`

## Commands

- `CMD_HELLO (0x01)`: starts a UART update session and returns status
- `CMD_WRITE (0x02)`: writes a chunk to the target slot
- `CMD_COMMIT (0x03)`: validates size and marks the target slot pending
- `CMD_ABORT (0x04)`: cancels the session and invalidates the target slot
- `CMD_STATUS (0x05)`: returns slot state and active slot
- `CMD_REBOOT (0x06)`: ends the session and returns control to the caller

## Response Codes

- `RSP_OK (0x81)`: command succeeded
- `RSP_ERROR (0x82)`: command failed
- `RSP_STATUS (0x83)`: state response

## Session Rules

1. `CMD_HELLO` must start the session.
2. `CMD_WRITE`, `CMD_COMMIT`, and `CMD_REBOOT` require an active session.
3. The first valid `CMD_WRITE` erases the target slot.
4. Later writes in the same session do not erase again.
5. `CMD_COMMIT` requires `firmware_size == bytes_written`.
6. Bad frames are ignored by the transport layer.
7. Invalid flash operations invalidate the target slot where the implementation does so.

## Boot State

`RSP_STATUS` returns four bytes:

- slot A state
- slot B state
- active slot
- boot reason

## Boot Flow

1. Load boot state from flash.
2. Listen for UART commands during the configured listen window.
3. Select the active or pending slot.
4. Verify firmware CRC32.
5. Update boot attempt state.
6. Jump to the selected image.

## Constraints

- C99 only
- zero heap
- adapter-based hardware access
- synchronous control flow

## Notes

- CRC32 detects accidental corruption only.
- Firmware authenticity is not implemented in this repository.
- UART retry/resume is not implemented in this repository.

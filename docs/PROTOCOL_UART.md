# UART Update Protocol Specification

## Overview

The UART transport provides in-field firmware updates via a frame-based protocol over serial connection. This document specifies the protocol format, state machine, and error handling.

## Frame Format

```
Byte 0:     0x7E (SOF - Start of Frame)
Byte 1:     Command (8-bit)
Bytes 2-3:  Payload length (16-bit little-endian)
Bytes 4..n: Payload (0 to 1024 bytes)
Bytes n+1, n+2: CRC16-CCITT (16-bit little-endian)
```

### CRC16-CCITT
- Polynomial: 0x1021
- Initial value: 0xFFFF
- Final XOR: 0x0000 (no final XOR)
- Covers: bytes 1 through n (CMD + LEN + PAYLOAD)

## Commands

### CMD_HELLO (0x01)
Query session state. Must be first command in update session.

**Payload:** None (0 bytes)

**Response:** RSP_STATUS with current boot state

**State:** Sets `_session_active = true`

### CMD_WRITE (0x02)
Write firmware chunk to target slot.

**Payload:** (4 + n) bytes
```
Bytes 0-3:  Offset in slot (32-bit little-endian)
Bytes 4..n: Firmware data chunk
```

**Constraints:**
- Requires `_session_active == true` (HELLO sent first)
- `offset + len(chunk) <= slot_size`
- Updates `_bytes_written = offset + len(chunk)`
- Target slot is erased on first WRITE (not at session init)

**Response:** RSP_OK or RSP_ERROR

**Error:** 
- HELLO not sent → RSP_ERROR
- Bounds check failed → RSP_ERROR
- Flash write failed → RSP_ERROR

### CMD_COMMIT (0x03)
Finalize firmware write and mark slot PENDING.

**Payload:** 8 bytes
```
Bytes 0-3:  Firmware size (32-bit little-endian)
Bytes 4-7:  Firmware CRC32 (32-bit little-endian)
```

**Constraints:**
- Requires `_session_active == true`
- `firmware_size == _bytes_written` (must match written bytes)
- CRC32 will be verified at boot time

**Response:** RSP_OK or RSP_ERROR

**Error:**
- HELLO not sent → RSP_ERROR
- Size mismatch → RSP_ERROR
- Slot commit failed → RSP_ERROR

### CMD_ABORT (0x04)
Cancel update session and invalidate target slot.

**Payload:** None (0 bytes)

**Response:** RSP_OK

**State:** Sets `_session_active = false`

### CMD_STATUS (0x05)
Query current boot state (slots, active, boot reason).

**Payload:** None (0 bytes)

**Response:** RSP_STATUS (4 bytes)
```
Byte 0: Slot A state
Byte 1: Slot B state
Byte 2: Active slot (0 = A, 1 = B)
Byte 3: Boot reason (crash loop counter, etc.)
```

### CMD_REBOOT (0x06)
Exit update session and reboot device.

**Payload:** None (0 bytes)

**Response:** RSP_OK

**Action:** Returns from `loxboot_uart_run_session()`

**Constraint:** Requires `_session_active == true`

## Responses

### RSP_OK (0x81)
Command succeeded. No payload.

### RSP_ERROR (0x82)
Command failed.

**Payload:** 1 byte
```
Byte 0: Error code (loxboot_err_t)
```

### RSP_STATUS (0x83)
Boot state response.

**Payload:** 4 bytes
```
Byte 0: Slot A state
Byte 1: Slot B state
Byte 2: Active slot
Byte 3: Boot reason
```

## Session State Machine

```
IDLE
  ↓ (CMD_HELLO)
ACTIVE [_session_active = true]
  ├─ CMD_WRITE → _bytes_written updated → ACTIVE
  ├─ CMD_COMMIT → validate size → call commit_slot() → ACTIVE
  ├─ CMD_STATUS → read state → ACTIVE
  ├─ CMD_ABORT → invalidate slot, _session_active = false → IDLE
  └─ CMD_REBOOT → return OK → exit session
```

## Constraints & Safety

1. **Session Gating:** WRITE/COMMIT/REBOOT require HELLO first
2. **Bounds Checking:** WRITE offset + length must not exceed slot size
3. **Firmware Validation:** COMMIT size must equal written bytes
4. **CRC Verification:** Firmware CRC verified at boot (not during update)
5. **Slot Erase:** Target slot is erased during session init
6. **Error Safety:** Any flash operation failure invalidates slot

## Error Handling

| Condition | Response | Result |
|-----------|----------|--------|
| Frame too short | TRANSPORT | Ignore, wait for next |
| Bad SOF byte | TRANSPORT | Ignore, wait for next |
| Bad CRC | TRANSPORT | Ignore, wait for next |
| Payload > 1024 bytes | TRANSPORT | Reject frame |
| WRITE without HELLO | RSP_ERROR | Session continues |
| WRITE out of bounds | RSP_ERROR | Session continues |
| COMMIT before WRITE | RSP_ERROR | Session continues |
| COMMIT size mismatch | RSP_ERROR | Session continues |
| Flash write fails | RSP_ERROR | Slot invalidated |
| Flash erase fails | Session exit | Return error |

## Example Successful Update

```
Host → Device: CMD_HELLO
Device → Host: RSP_STATUS (current state)

Host → Device: CMD_WRITE (offset=0, chunk=256 bytes)
Device → Host: RSP_OK

Host → Device: CMD_WRITE (offset=256, chunk=256 bytes)
Device → Host: RSP_OK

Host → Device: CMD_WRITE (offset=512, chunk=64 bytes)
Device → Host: RSP_OK

Host → Device: CMD_COMMIT (size=576, crc=0x12345678)
Device → Host: RSP_OK (slot marked PENDING)

Host → Device: CMD_STATUS
Device → Host: RSP_STATUS (new slot shown as PENDING)

Host → Device: CMD_REBOOT
Device → Host: RSP_OK
Device: Exits session, boots into new firmware
```

## Implementation Notes

- Session initializes with target slot erase
- Write payload chunks can be any size ≤ 1024 bytes
- Host may batch writes or send one per command
- CRC32 is computed per-chunk during boot (not session)
- UART timeout: frame timeout 5000ms, listen timeout configurable
- Max frame: 1+1+2+1024+2 = 1030 bytes

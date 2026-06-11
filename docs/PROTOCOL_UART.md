# UART Update Protocol Specification

## Overview

The UART transport provides in-field firmware updates via a frame-based protocol over a serial connection. This document describes the protocol format, session state, and the error behavior that is implemented today.

## Frame Format

```text
Byte 0:     0x7E (SOF - Start of Frame)
Byte 1:     Command (8-bit)
Bytes 2-3:  Payload length (16-bit little-endian)
Bytes 4..n: Payload (0 to 1024 bytes)
Bytes n+1,n+2: CRC16-CCITT (16-bit little-endian)
```

### CRC16-CCITT

- Polynomial: 0x1021
- Initial value: 0xFFFF
- Final XOR: 0x0000
- Covers: bytes 1 through n (CMD + LEN + PAYLOAD)

## Commands

### CMD_HELLO (0x01)

Starts a UART update session.

- Payload: none
- Response: `RSP_STATUS`
- Effect: sets `_session_active = true`
- Does not erase flash

### CMD_WRITE (0x02)

Writes one firmware chunk to the target slot.

- Payload:

```text
Bytes 0-3:  Offset in slot (32-bit little-endian)
Bytes 4..n: Firmware data chunk
```

- Requires `_session_active == true`
- `offset + len(chunk) <= slot_size`
- The first valid `CMD_WRITE` in the session erases the target slot
- Later `CMD_WRITE` commands in the same session do not erase again
- `_bytes_written` is updated to `offset + len(chunk)`

Response:
- `RSP_OK` on success
- `RSP_ERROR` on failure

Failure behavior:
- HELLO not sent -> `RSP_ERROR`
- bounds check failed -> `RSP_ERROR`
- erase failure -> target slot is invalidated
- write failure -> target slot is invalidated

### CMD_COMMIT (0x03)

Finalizes the firmware write and marks the target slot pending.

- Payload:

```text
Bytes 0-3:  Firmware size (32-bit little-endian)
Bytes 4-7:  Firmware CRC32 (32-bit little-endian)
```

- Requires `_session_active == true`
- `firmware_size == _bytes_written`
- CRC32 is verified at boot time
- The implementation also rechecks the written image before accepting the commit

Response:
- `RSP_OK` on success
- `RSP_ERROR` on failure

Failure behavior:
- HELLO not sent -> `RSP_ERROR`
- size mismatch -> target slot is invalidated
- commit failure -> target slot is invalidated
- CRC verification failure -> target slot is invalidated

### CMD_ABORT (0x04)

Cancels the session and invalidates the target slot.

- Payload: none
- Response: `RSP_OK`
- Effect: sets `_session_active = false`

### CMD_STATUS (0x05)

Returns current boot state.

- Payload: none
- Response: `RSP_STATUS`

### CMD_REBOOT (0x06)

Ends the update session and returns control to the caller.

- Payload: none
- Response: `RSP_OK`
- Requires `_session_active == true`

## Responses

### RSP_OK (0x81)

Command succeeded. No payload.

### RSP_ERROR (0x82)

Command failed.

- Payload:

```text
Byte 0: Error code (loxboot_err_t)
```

### RSP_STATUS (0x83)

Boot state response.

- Payload:

```text
Byte 0: Slot A state
Byte 1: Slot B state
Byte 2: Active slot (0 = A, 1 = B)
Byte 3: Boot reason
```

## Session State Machine

```text
IDLE
  -> (CMD_HELLO)
ACTIVE [_session_active = true]
  -> CMD_WRITE  -> erase on first write, then write chunk
  -> CMD_COMMIT -> validate size, commit slot, verify written image
  -> CMD_STATUS -> read state
  -> CMD_ABORT  -> invalidate slot, _session_active = false
  -> CMD_REBOOT  -> return OK, exit session
```

## Constraints and Safety

1. Session gating: WRITE/COMMIT/REBOOT require HELLO first
2. Bounds checking: WRITE offset + length must not exceed slot size
3. Firmware validation: COMMIT size must equal bytes written
4. CRC verification: firmware CRC is checked at boot time, not by this transport layer alone
5. Slot erase: target slot is erased on the first valid WRITE, not at session init
6. Error safety: erase/write/commit failures invalidate the target slot where the implementation does so

## Error Handling

| Condition | Response | Result |
|---|---|---|
| Frame too short | TRANSPORT | Ignore and wait for next frame |
| Bad SOF byte | TRANSPORT | Ignore and wait for next frame |
| Bad CRC | TRANSPORT | Ignore and wait for next frame |
| Payload > 1024 bytes | TRANSPORT | Reject frame |
| WRITE without HELLO | `RSP_ERROR` | Session continues |
| WRITE out of bounds | `RSP_ERROR` | Session continues |
| COMMIT before WRITE | `RSP_ERROR` | Session continues |
| COMMIT size mismatch | `RSP_ERROR` | Target slot invalidated |
| Flash erase fails | `RSP_ERROR` | Target slot invalidated |
| Flash write fails | `RSP_ERROR` | Target slot invalidated |

## Implementation Notes

- `CMD_HELLO` starts the session only
- The first valid `CMD_WRITE` erases the target slot
- Later writes in the same session do not erase again
- There is no production-grade retry/resume support in this protocol today
- The host may batch writes or send one per command
- UART timeout values are configured by the build
- Max frame size is `1 + 1 + 2 + 1024 + 2 = 1030` bytes

#ifndef LOXBOOT_TRANSPORT_H
#define LOXBOOT_TRANSPORT_H

/**
 * loxboot_transport — UART transport session API.
 *
 * Implemented in ports/uart/loxboot_uart.c (v0.4.0+).
 * This header defines the session interface used by loxboot_run()
 * to check for and handle a UART firmware update.
 *
 * See docs/SPEC.md §11 for full UART protocol specification.
 */

#include "loxboot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * UART transport session context (caller-owned)
 * ====================================================================== */

/**
 * loxboot_uart_session_t — Context for one UART update session.
 *
 * Caller fills all fields before passing to loxboot_uart_run_session().
 */
typedef struct {
    loxboot_t                   *boot;          /**< loxboot core context (required) */
    loxboot_transport_adapter_t  transport;     /**< Transport adapter (required) */
    uint32_t                     listen_ms;     /**< How long to wait for CMD_HELLO */

    /* Internal state — zero-init, do not fill */
    uint8_t  _frame_buf[LOXBOOT_UART_MAX_FRAME_PAYLOAD + 8]; /* SOF+CMD+LEN+CRC */
    uint32_t _bytes_written;
    bool     _session_active;
} loxboot_uart_session_t;

/* =========================================================================
 * UART frame commands and responses
 * ====================================================================== */

/** Commands: host → device */
#define LOXBOOT_UART_CMD_HELLO   0x01
#define LOXBOOT_UART_CMD_WRITE   0x02
#define LOXBOOT_UART_CMD_COMMIT  0x03
#define LOXBOOT_UART_CMD_ABORT   0x04
#define LOXBOOT_UART_CMD_STATUS  0x05
#define LOXBOOT_UART_CMD_REBOOT  0x06

/** Responses: device → host */
#define LOXBOOT_UART_RSP_OK      0x81
#define LOXBOOT_UART_RSP_ERROR   0x82
#define LOXBOOT_UART_RSP_STATUS  0x83

/** Frame SOF byte */
#define LOXBOOT_UART_SOF         0x7E

/* =========================================================================
 * UART transport API
 * ====================================================================== */

/**
 * loxboot_uart_run_session — Listen for and handle a UART firmware update.
 *
 * Waits up to session->listen_ms for a CMD_HELLO frame.
 * If no CMD_HELLO received: returns LOXBOOT_ERR_TIMEOUT (caller boots normally).
 * If CMD_HELLO received: handles the full update session until:
 *   - CMD_REBOOT received → returns LOXBOOT_OK (caller should reboot)
 *   - CMD_ABORT received  → returns LOXBOOT_ERR_INVALID_STATE
 *   - Transport error     → returns LOXBOOT_ERR_TRANSPORT
 *   - Session timeout     → returns LOXBOOT_ERR_TIMEOUT
 *
 * On successful receive + commit: target slot is marked PENDING.
 * On any error: target slot is marked INVALID.
 *
 * The caller (loxboot_run) calls this before boot sequence if transport
 * adapter is configured and LOXBOOT_UART_LISTEN_MS > 0.
 */
loxboot_err_t loxboot_uart_run_session(loxboot_uart_session_t *session);

/**
 * loxboot_uart_send_status — Send RSP_STATUS frame to host.
 *
 * Utility used internally and available for testing.
 */
loxboot_err_t loxboot_uart_send_status(loxboot_uart_session_t *session);

/**
 * loxboot_crc16 — CRC16-CCITT (poly=0x1021, init=0xFFFF).
 *
 * Used for UART frame integrity. Exposed for testing.
 */
uint16_t loxboot_crc16(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LOXBOOT_TRANSPORT_H */

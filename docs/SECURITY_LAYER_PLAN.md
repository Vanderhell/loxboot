# Security Layer Plan

This repository currently uses CRC32 for accidental corruption detection only. CRC32 does not prove authenticity.

## Scope

- loxboot core must stay C99
- loxboot core must stay zero-heap
- loxboot core must stay adapter-based
- loxboot core must stay small
- security must be optional through an adapter interface and compile flag
- loxboot core must not directly depend on loxruntime, vh_crypto, IUPD, DELTA, LogsDB, microcrypt, microdh, or other larger LOX components

## Internal LOX security project usage decision

Future optional security providers may be implemented outside the core bootloader, but the core must not directly depend on them.

## microcrypt

`microcrypt` may be used later as an optional backend/provider for:

- SHA-256
- HMAC-SHA256
- AES-128 CBC if encrypted payload support is ever added

Rules:

- SHA-256 may be used to compute a firmware digest
- HMAC-SHA256 may be used as the first practical firmware authentication layer
- AES-128 CBC is not required for initial firmware verification
- AES-128 ECB must not be used directly for firmware payload encryption
- `microcrypt` must not become a hard dependency of the loxboot core
- any use of `microcrypt` must be behind `loxboot_security_ops_t` or a similar adapter
- any use of `microcrypt` must preserve C99, zero-heap, bounded-stack behavior

Recommended first security phase:

```text
CRC32             = accidental corruption detection
SHA-256           = firmware digest
HMAC-SHA256       = firmware authenticity with shared secret
key_id            = key selection / key rotation
monotonic_version = anti-rollback / anti-replay
```

Limitations to document:

- HMAC-SHA256 uses a shared secret
- the same secret must be known by the signer/updater and by the device
- if the updater/signing secret leaks, an attacker can create accepted firmware
- HMAC-SHA256 is acceptable for controlled/internal deployments
- public-key signing is still preferable for public product distribution

## microdh

`microdh` provides minimal X25519 key exchange for embedded systems.

Do not use `microdh` for the first firmware verification layer.

Reason:

- X25519 solves session key agreement
- loxboot's first security need is firmware authenticity, not secure session negotiation
- X25519 does not by itself prove that a firmware image is authorized
- adding key exchange now would increase protocol complexity before power-loss, retry/resume, and hardware evidence are complete

`microdh` may be considered later for:

- secure OTA session establishment
- encrypted update transport
- device provisioning
- host-device pairing
- deriving a temporary session key for encrypted transfer

It must not be added until the basic update path is evidence-backed and stable.

## Recommended future adapter

```c
typedef struct {
    void *ctx;

    loxboot_err_t (*sha256)(
        void *ctx,
        const uint8_t *data,
        size_t len,
        uint8_t out32[32]);

    loxboot_err_t (*hmac_sha256)(
        void *ctx,
        const uint8_t *key,
        size_t key_len,
        const uint8_t *data,
        size_t len,
        uint8_t out32[32]);
} loxboot_security_ops_t;
```

## Recommended first manifest

```c
typedef struct {
    uint32_t magic;
    uint16_t manifest_version;
    uint16_t algorithm;

    uint32_t firmware_size;
    uint32_t firmware_crc32;

    uint32_t monotonic_version;
    uint8_t  key_id[16];

    uint8_t  firmware_sha256[32];
    uint8_t  hmac_sha256[32];
} loxboot_firmware_manifest_t;
```

## Later public-key manifest concept

Do not implement this in the current task unless already available and fully testable.

```c
typedef struct {
    uint32_t magic;
    uint16_t manifest_version;
    uint16_t algorithm;

    uint32_t firmware_size;
    uint32_t firmware_crc32;

    uint32_t monotonic_version;
    uint8_t  key_id[16];

    uint8_t  firmware_sha256[32];
    uint8_t  signature[64];
} loxboot_signed_firmware_manifest_t;
```

## Security rules

- CRC32 remains useful for accidental corruption detection
- SHA-256 provides a strong digest of the firmware image
- HMAC-SHA256 provides shared-secret firmware authentication
- public-key signing remains the stronger long-term model for public distribution
- `monotonic_version` provides anti-rollback / anti-replay protection
- `key_id` allows key rotation
- revoked keys must be represented in caller/platform-owned storage, not hardcoded global mutable state
- `microcrypt` may later provide SHA-256/HMAC-SHA256 implementation behind the adapter
- `microdh` may later provide secure-session key exchange, but not initial firmware verification
- loxboot core must not directly include `microcrypt` or `microdh` headers unless a dedicated optional adapter target is explicitly enabled

## Task boundary

Do not implement firmware signing or HMAC validation in this task unless:

- the implementation is small
- optional
- C99-compatible
- zero-heap
- fully tested
- and all documentation clearly marks it as verified

Otherwise only keep the architecture plan and leave implementation status as `MISSING` or `PLANNED`, not `VERIFIED`.

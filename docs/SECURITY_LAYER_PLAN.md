# Security Status

This repository currently uses CRC32 for accidental corruption detection only.
CRC32 does not prove authenticity.

## Current status

- loxboot core stays C99
- loxboot core stays zero-heap
- loxboot core stays adapter-based
- loxboot core stays small
- loxboot core does not implement firmware signing
- loxboot core does not implement HMAC validation
- loxboot core does not depend on external security libraries

## Consequences

- CRC32 detects accidental corruption
- CRC32 does not authenticate firmware
- firmware authenticity is not implemented in this repository

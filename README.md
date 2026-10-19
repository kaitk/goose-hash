# goose-hash

Cryptographic hashes for [Goose](https://github.com/aardappel/goose): SHA-1, SHA-256,
HMAC, PBKDF2, and a constant-time compare, in Goose with no C and no other package.
Goose's stdlib has `base64` and `json`, and `hash` for dictionaries, but no
cryptographic hash.

Nothing is built yet. The plan is in [tasks](tasks/README.md), and the choices so far
are in [decisions](docs/decisions.md).

## Status

🟢 works · 🟡 partial or next · 🔴 not yet

| | Feature | Notes |
|---|---|---|
| 🟡 | SHA-1 | For WebSocket's handshake (RFC 6455), not for security. |
| 🟡 | SHA-256 | One-shot and streaming. |
| 🟡 | HMAC-SHA-256 | For JWT and SCRAM. |
| 🟡 | PBKDF2-HMAC-SHA-256 | For SCRAM-SHA-256. |
| 🟡 | Constant-time compare | For checking a MAC. |

## Pins

The Goose release in `scripts/goose.sh` (`v2026.2`).

# 001: SHA-1, SHA-256, HMAC, and PBKDF2

Status: todo (2026-10-19)

The first API: what goose-http's WebSocket (its task 052), goose-postgres's SCRAM
login (its task 001), and a JWT verifier in goose-rest need.

## Steps

1. The API in the style of the pinned release's stdlib: one-shot functions that take a
   `u8[:]` and return the digest as a fixed array, and a streaming state for SHA-256
   (`update`, `finish`) for input in pieces.
2. SHA-1 and SHA-256 (FIPS 180-4), HMAC-SHA-256 (RFC 2104), PBKDF2-HMAC-SHA-256
   (RFC 8018), and a compare that takes the same time whatever the input.
3. Tests under `test/`: NIST's examples for both hashes, including the million-`a`
   input; RFC 4231's HMAC-SHA-256 cases; RFC 7914's PBKDF2-HMAC-SHA-256 vectors;
   RFC 6455's handshake example (`dGhlIHNhbXBsZSBub25jZQ==` gives
   `s3pPLMBiTxaQ9kYGzzhZRbK+xOo=`, with stdlib's `base64`); streaming input split at
   every offset; and random inputs against Python's `hashlib` and `hmac`.
   `scripts/test.sh` runs them.
4. A small benchmark: SHA-1 of a 60-byte WebSocket key, SHA-256 at 64 bytes and 1 MB,
   and PBKDF2 at 4,096 iterations (Postgres's default), against OpenSSL's
   `openssl speed`, recorded with how they were measured.
5. `docs/architecture.md` and the API in `README.md`.

## Acceptance criteria

- [ ] The API in `README.md` and `docs/architecture.md`
- [ ] Every published vector above passes, and random inputs match Python's
- [ ] The benchmark recorded with its machine, compiler, and runs
- [ ] `scripts/test.sh` passes on macOS and Linux

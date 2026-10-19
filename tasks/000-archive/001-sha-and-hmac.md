# 001: SHA-1, SHA-256, HMAC, and PBKDF2

Status: done (2026-10-19)

The first API: what goose-http's WebSocket (its task 054), goose-postgres's SCRAM
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

- [x] The API in `README.md` and `docs/architecture.md`
- [x] Every published vector above passes, and random inputs match Python's
- [x] The benchmark recorded with its machine, compiler, and runs
  ([results](../../docs/results.md#four-cases-at-a8a4e37)). OpenSSL has no PBKDF2 in
  `openssl speed`, so PBKDF2 is measured against libcrypto's `PKCS5_PBKDF2_HMAC`, in
  `bench/ref.c`, which times the hashes too and agrees with `openssl speed` within 5
  percent.
- [x] `scripts/test.sh` passes on macOS and Linux: macOS 26.3.1 on an Apple M1 Pro,
  and Linux on x64 in Docker's `python:3.12` image with gcc 14.2.0, emulated on the
  same Mac (2026-10-19). Linux on arm64, where `scripts/goose.sh` builds Goose from
  source, and Windows haven't run.

## Outcome

On an M1 Pro, the package takes 2.0 to 8.2 times as long as OpenSSL using the CPU's
SHA instructions, which Goose can't express, and 1.04 to 1.27 times as long as
OpenSSL's general-purpose code
([findings](../../docs/goose-findings.md#goose-has-no-sha-instructions)).
A WebSocket handshake's SHA-1 takes 0.3 µs, and a SCRAM login's PBKDF2 1.9 ms. The
first tag, which the README's quick start imports, isn't cut.

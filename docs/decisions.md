# Decisions

The record of goose-hash's design decisions, for contributors and agents: each dated,
with its reason and the trade-off that matters.

## Settled

- **A package of its own, in Goose (2026-10-19).** WebSocket's handshake needs SHA-1,
  goose-postgres's SCRAM-SHA-256 login needs SHA-256, HMAC, and PBKDF2, and a JWT
  verifier needs HMAC-SHA-256. Goose's stdlib has none of them (checked at `v2026.2`
  and master `dcc2747`, 2026-10-19: only `hash` and `hash_combine`, for dictionaries).
  Rejected: taking them from OpenSSL through goose-tls, which would make every
  WebSocket server and Postgres client link OpenSSL and need CMake, what goose-tls is a
  package of its own to avoid; and a copy in each package, which would test the same
  security code three times. A hash is bounded, table-driven byte work, the kind Goose
  is measured on, as goose-compress's CRC-32, Adler-32, and XXH64 are.
- **Hashes and MACs only (2026-10-19).** Ciphers, signatures, and key generation stay
  in OpenSSL, through goose-tls, so the package never becomes a cryptography library
  of its own.
- **Benchmarked against the installed OpenSSL (2026-10-19).** `bench/ref.c` links the
  machine's OpenSSL, 3 or later, and `bench/run.sh` runs its `openssl speed`, the
  reference task 001 names; each result records the version. Rejected: building
  OpenSSL at a pinned tag, as goose-compress builds its libraries, which takes minutes
  and Perl on each machine, for code whose speed comes mostly from the CPU's SHA
  instructions. The trade-off: two machines' results may name
  different OpenSSL versions.

## Open

- Whether to offer the package to Goose's stdlib, beside `base64` and `json`, once it
  has users. That's the maintainer's call.

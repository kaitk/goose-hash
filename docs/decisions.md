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
- **The CPU's SHA instructions, in C for now (2026-10-19).** This replaces "Goose, with
  no C" for one purpose: the package may carry C for what Goose can't express yet,
  the CPU's SHA instructions. `shim/sha.c` holds SHA-1's and SHA-256's compression
  functions on ARMv8's SHA instructions and x86-64's SHA-NI, and `hash.build` compiles
  it with the program, as goose-sqlite's and goose-tls's sidecars do. Goose keeps its
  portable rounds as the fallback where the CPU lacks the instructions, and they stay
  tested. Reason: the instructions are most of the gap to OpenSSL, 8 times on SHA-256
  of 1 MB and 4 times on SCRAM's PBKDF2 ([results](results.md)), and goose-http's
  WebSocket and goose-postgres's SCRAM login shouldn't wait for Goose's stdlib to
  offer them (task 003). The C goes away when Goose's stdlib has SHA on the
  instructions, and serves as task 003's prototype meanwhile. Rejected: waiting for
  Goose, which leaves the package 4 to 8 times slower than OpenSSL for an unknown
  time; and taking SHA from OpenSSL through goose-tls, rejected above. The trade-off:
  a program that imports the package now needs a C compiler for its C, which
  `goose build` needs anyway, and the C is the package's to keep correct on each CPU.
- **The OS's secure random source, in C for now (2026-10-19).** This extends the C
  allowance above to a second purpose that Goose can't reach on every platform: the
  OS's cryptographically secure random bytes. `hash::random_bytes(out)` fills a slice
  through `shim/random.c`: `BCryptGenRandom` with `BCRYPT_USE_SYSTEM_PREFERRED_RNG` on
  Windows, linked through `hash.build`'s `c_libs_windows`, and `getentropy` elsewhere,
  256 bytes a call, reading `/dev/urandom` only where the kernel lacks `getrandom`.
  Reason: goose-postgres's SCRAM-SHA-256 login needs a client nonce that can't be
  predicted, and Goose's `random_seed()` is for seeding `rng`, not for secrets. At
  `v2026.2` (`3ee3871`) and on master at `4c58784`, the runtime's `gs_os_random_u64`
  reads 8 bytes from `/dev/urandom` on POSIX, and on Windows, or where that file can't
  be read, returns a mix of the clocks, the process id, and an address
  ([findings](goose-findings.md#goose-has-no-secure-random-bytes)). If the OS's source
  fails, `random_bytes` aborts the program, as `pbkdf2_sha256` does on a count below
  1, rather than return an error: no caller can do anything safe with fewer or weaker
  bytes, and the sources it uses don't fail on a supported OS once it has booted.
  Random bytes are a primitive that nonces and keys are taken from, not the key
  generation that "Hashes and MACs only" leaves to OpenSSL. The C goes away when
  Goose's stdlib has secure random bytes. Rejected: `random_seed()`, which is weak
  on Windows; reading `/dev/urandom` from Goose, which has no Windows equivalent; and
  leaving each caller to bring its own C. The trade-off: one more C file for the
  package to keep correct on each OS, and `bcrypt.lib` linked into every Windows
  program that imports the package.

## Open

- Whether to offer the package to Goose's stdlib, beside `base64` and `json`, once it
  has users. That's the maintainer's call.

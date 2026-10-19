# 005: Secure random bytes from the OS

Status: done (2026-10-19)

goose-postgres's SCRAM-SHA-256 login needs a client nonce that can't be predicted, and
its blocking facade (its task 002) needs somewhere to take it from. Goose's
`random_seed()` reads `/dev/urandom` on POSIX but mixes the clocks on Windows
([findings](../docs/goose-findings.md#goose-has-no-secure-random-bytes)), so nothing in
Goose gives secure random bytes on every platform. The package fills a slice from the
OS's source through C, as a stopgap until Goose's stdlib can
([decisions](../docs/decisions.md)), decided 2026-10-19.

## Steps

1. Record the decision in `docs/decisions.md`, extending the C allowance to the OS's
   secure random source, and the gap in `docs/goose-findings.md` with a minimal
   program.
2. `hash::random_bytes(out: u8[:])` in `random.goose`, over `shim/random.c`:
   `getentropy` in 256-byte calls, `/dev/urandom` only where the kernel lacks
   `getrandom`, and `BCryptGenRandom` with `BCRYPT_USE_SYSTEM_PREFERRED_RNG` on
   Windows, with `bcrypt` linked through `hash.build`. It aborts when the source fails.
3. `test/random/`: the length filled, two draws that differ, every byte value seen
   over many draws and over one draw past 64 KB, and an empty slice.
4. The README's API, status, layout, and quick start at v0.0.3; `AGENTS.md`;
   `docs/architecture.md`; and `docs/development.md`.
5. Run `scripts/test.sh` on macOS and on Linux on arm64 in Docker.

## Acceptance criteria

- [x] The decision and the finding recorded, the finding with a minimal program that
      builds and runs on the pin
- [x] `hash::random_bytes` on `getentropy` and `BCryptGenRandom`, aborting rather
      than return weaker bytes
- [x] `test/random/random_bytes` passes at `-O0` and `-O2`
- [x] `scripts/test.sh` passes on macOS on arm64 and on Linux on arm64, with gcc and
      clang
- [x] The README, `AGENTS.md`, `docs/architecture.md`, and `docs/development.md`
      updated, and what wasn't run, Windows above all, said plainly

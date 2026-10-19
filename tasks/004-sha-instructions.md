# 004: SHA-1 and SHA-256 on the CPU's SHA instructions, through C

Status: in-progress (decided 2026-10-19)

The CPU's SHA instructions make OpenSSL's SHA-256 8 times faster than the package's
on 1 MB, and its PBKDF2 at 4,096 iterations 4 times faster
([results](../docs/results.md)). Goose can't express them
([findings](../docs/goose-findings.md#goose-has-no-sha-instructions)), and task 003's
stdlib proposal will take longer than goose-http's WebSocket and goose-postgres's
SCRAM login can wait. So the package carries a small C shim for them, as a stopgap
([decisions](../docs/decisions.md)): the compression functions of SHA-1 and SHA-256
on ARMv8's SHA instructions and x86-64's SHA-NI, with the package's Goose rounds as the
fallback. The same C is task 003's prototype, and goes away once Goose's stdlib has
SHA on the instructions.

## Steps

1. Record the decision in `docs/decisions.md`, replacing "Goose with no C" for this
   one purpose, and update `AGENTS.md`, the README, and `docs/architecture.md`.
2. `shim/sha.c`, built through `hash.build`: SHA-1 and SHA-256 block functions on
   ARMv8 (`arm_neon.h`) and x86-64 SHA-NI (`immintrin.h`), many blocks a call, each
   with its own target attribute so that no build needs `-march`. They follow a
   public implementation, credited in `docs/credits.md`. The CPU is checked once per
   process: always yes on macOS on arm64, `getauxval(AT_HWCAP)` on Linux on arm64,
   `cpuid` on x86-64, and `IsProcessorFeaturePresent` on Windows on arm64.
3. `sha1_blocks` and `sha256_blocks` call the C through `extern fn`, and run the Goose
   rounds where it reports that the CPU lacks the instructions.
4. `GOOSE_HASH_PORTABLE=1` forces the Goose rounds. The golden tests and the check
   against Python run twice, with the instructions and without, and the check fails
   when the rounds in use aren't the ones the CPU should give.
5. Measure with `bench/run.sh` on a quiet machine, and update `docs/results.md` and
   `docs/results-history.md`.
6. Run `scripts/test.sh` on macOS, and on Linux in Docker on arm64 and on x86-64.

## Acceptance criteria

- [x] The decision recorded, and `AGENTS.md`, the README, `docs/architecture.md`, and
      `tasks/README.md` updated
- [ ] `shim/sha.c` with SHA-1 and SHA-256 on ARMv8 and x86-64 SHA-NI, credited in
      `docs/credits.md`
- [ ] Every test vector and the check against Python pass with the instructions and
      with `GOOSE_HASH_PORTABLE=1`, and the check confirms which rounds ran
- [ ] `scripts/test.sh` passes on macOS on arm64, and on Linux on arm64 and x86-64
- [x] The speed measured, in `docs/results.md` and `docs/results-history.md`
- [ ] What wasn't verified, such as MSVC and SHA-NI on real hardware, said plainly in
      `docs/development.md`

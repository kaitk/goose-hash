# 002: Close the gap to the fastest hashes

Status: todo (asked for 2026-10-19)

Task 001's SHA-256 is 3.3 times slower than OpenSSL on 64 bytes and 8 times slower on
1 MB, and PBKDF2 at 4,096 iterations takes 1.86 ms against 0.47 ms
([results](../docs/results.md)). That is fine to start with, and is why the first tag is
`v0.0.1`, but nothing in the algorithm forces it. This task finds out how the fastest
implementations get their speed, and brings the package as close to them as Goose
allows.

The gap has two parts, which need different fixes:

- **Without the SHA instructions**, OpenSSL takes 1.04 to 1.27 times less time than the
  package. That is a matter of how the rounds are written and what the C compiler makes
  of Goose's output, and should close here.
- **With them**, OpenSSL is 2 to 8 times faster, and Goose has no way to emit
  `SHA256H` or `SHA256RNDS2` ([findings](../docs/goose-findings.md#goose-has-no-sha-instructions)).
  The package and its benchmarks have no C, so that part needs Goose to offer the
  instructions, as an operation or in the stdlib. This task measures how much of the
  gap it is, and makes the case upstream with the numbers.

## Steps

1. Read how each of these hashes, and record what each does that the package doesn't:
   - Aegon (goose-rest's task 048): what its WebSocket handshake and any HMAC use.
   - Rust: RustCrypto's `sha2`, `sha1`, `hmac`, and `pbkdf2` (soft and intrinsic
     back ends, chosen at run time with `cpufeatures`), and `ring`.
   - Zig: `std.crypto.hash.sha2` and `std.crypto.pwhash.pbkdf2` (inline assembly for the
     ARMv8 and x86-64 SHA instructions, and its portable rounds).
   - Bun: BoringSSL underneath, and what its JavaScript layer adds per call.
   - OpenSSL's and BoringSSL's generated assembly for the portable case, and Go's
     `crypto/sha256` as a further reference for unrolled rounds.
2. Add the comparable ones to `bench/` beside libcrypto: at least Rust's `sha2` and
   Zig's std, built in release mode, with and without the SHA instructions, on the same
   four cases.
3. Close the portable gap: the message schedule, unrolling, keeping the state in
   locals, the loads and byte swaps, and the C that `goose build` emits for each. Then
   the per-call costs that dominate small inputs: HMAC's padded keys computed once per
   key, and PBKDF2's inner and outer states reused across its iterations, as OpenSSL
   does.
4. If Goose can't express something a step needs, reduce it to a minimal program in
   `docs/goose-findings.md`, and report the SHA instructions upstream with this task's
   numbers.
5. Measure on a quiet machine: task 001's runs had a load average near 7 from other
   builds. Run on macOS (M1 Pro) and the Linux desktop (x86-64, with SHA-NI).

## Acceptance criteria

- [ ] What each implementation above does, recorded with links to its source
- [ ] Rust's `sha2` and Zig's std in `bench/`, with and without the SHA instructions
- [ ] The package within 10% of the fastest portable implementation on each case, on
      both machines, or the reason it can't be, in `docs/goose-findings.md`
- [ ] The SHA-instruction gap measured and reported upstream
- [ ] `docs/results.md` and `docs/results-history.md` updated, and `scripts/test.sh`
      passes

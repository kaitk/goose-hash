# 003: Propose SHA-1 and SHA-256 to Goose, on the CPU's instructions

Status: todo (asked for 2026-10-19)

A Goose program can't use the CPU's SHA instructions
([findings](../docs/goose-findings.md#goose-has-no-sha-instructions)), and they are
most of the gap to OpenSSL (task 002). Task 004 reaches them through a C shim in this
package as a stopgap ([decisions](../docs/decisions.md)); the lasting fix belongs in
Goose, and the shim goes once the pin has it. This task writes the proposal for
Goose's maintainer, with the prior art and the numbers, and builds a working version
on a branch of `kaitk/goose`, as goose-http's proposals #37, #38, and #39 were. Task
004's `shim/sha.c` is the prototype: its rounds, its CPU checks, and its tests on and
off the instructions.

## What Goose has now

At `v2026.2-28-gdcc2747` (2026-10-19):

- The stdlib has `base64`, `binary`, `csv`, `dictionary`, `exception`, `json`, `math`,
  `os`, `regex`, `std`, and `vec`, and no cryptographic hash. `std`'s `hash` is for
  dictionaries.
- `math` and `os` are C behind `extern fn` (spec §7.10), so a stdlib module with C
  under it has precedent.
- `simd fn` (§7.12) builds a function per instruction set and picks one at each call
  from the running CPU, so the runtime already detects CPU features. It does this on
  x86-64 under clang only, and only widens what the C compiler may vectorize: no SHA
  round comes out of it.
- §17 puts explicit vector types out of scope for v1, which argues against exposing
  the instructions themselves in the language.

## Prior art

Record, for each, where SHA lives, how the instructions are reached, and how the CPU
is detected, with links to the source:

- Standard libraries that include SHA: Go's `crypto/sha256` (assembly, `internal/cpu`),
  Java's `MessageDigest` (HotSpot replaces `implCompress` with an intrinsic), .NET's
  `System.Security.Cryptography.SHA256` (OS crypto underneath), Python's `hashlib`
  (OpenSSL or HACL*), Zig's `std.crypto.hash.sha2` (inline assembly beside portable
  rounds), Odin's `core:crypto`, and Swift's CryptoKit.
- Languages that expose the instructions instead: Rust's `core::arch`
  (`vsha256hq_u32`, `_mm_sha256rnds2_epu32`, with RustCrypto's `sha2` and `cpufeatures`
  on top), .NET's `System.Runtime.Intrinsics.Arm.Sha256` and `X86.Sha`, and C's ACLE
  (`arm_neon.h`) and `immintrin.h`.
- How each detects the CPU: `getauxval(AT_HWCAP)` on Linux arm64, `sysctlbyname` on
  macOS, `IsProcessorFeaturePresent` on Windows, and `cpuid` on x86-64.

## The shapes to weigh

1. **A stdlib module**, such as `sha`, with SHA-1 and SHA-256, one-shot and streaming,
   in C under `extern fn` that uses the instructions where the CPU has them and
   portable rounds where it doesn't. Smallest for Goose's users, and it fits the
   stdlib's precedent. HMAC and PBKDF2 could stay in this package on top of it.
2. **An addition to an existing module**, such as `binary`: the same functions, without
   a module of their own.
3. **The instructions as operations**, as Rust's `core::arch` does. The most general:
   AES, CRC32, and carry-less multiplication would follow. But it means vector types,
   which §17 defers.
4. **A compiler intrinsic** that recognizes a known compression function, as HotSpot
   does. No new API, but the most compiler work, and the most fragile.

The proposal recommends one, and says what the others would cost.

## Steps

1. The prior art above, recorded in this task with links.
2. A draft of the proposal: the problem, with task 001's and 002's numbers; the shape;
   the API in the stdlib's style; CPU detection on each platform Goose supports; and
   the tests, against NIST's vectors and with the instructions on and off.
3. Ask the maintainer before building, as for #37 and #38.
4. If agreed, build it on a branch of `kaitk/goose`, run Goose's full suite on macOS
   arm64, Linux x86-64 and aarch64, and Windows, and open the pull request.
5. Once the pin has it, this package uses it, or forwards to it, and `docs/results.md`
   records the speed.

## Acceptance criteria

- [ ] The prior art recorded, with links
- [ ] The proposal sent to the maintainer, with a recommended shape
- [ ] If agreed: the pull request opened, passing Goose's suite on every platform
- [ ] Once the pin has it: the package on the instructions, within 10% of OpenSSL with
      them on both machines, and the finding deleted

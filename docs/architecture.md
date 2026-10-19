# Architecture

How goose-hash works and why, for its users. The decisions behind it, each with its
reason and what was weighed against it, are in [decisions](decisions.md); measurements
are in [results](results.md).

## A package of its own, in Goose

goose-hash is imported as `hash` and imports nothing but Goose's stdlib, so any Goose
program can use it: goose-http for WebSocket's handshake, goose-postgres for
SCRAM-SHA-256, and goose-rest for verifying a JWT. It holds hashes, MACs, and the OS's
random bytes only. Ciphers, signatures, and key generation stay in OpenSSL, through
goose-tls.

The functions are Goose, not bindings to OpenSSL. Two parts are in C, for what Goose
can't reach yet: SHA-1's and SHA-256's compression function on the CPU's SHA
instructions ([SHA instructions](#the-sha-instructions)), and the OS's secure random
source ([random bytes](#random-bytes)). Bytes go in as `u8[:]` slices and
digests come out as fixed arrays, `u8[20]` or `u8[32]`, which need no allocation and
can be compared with `==` or sliced to a `u8[:]`.

## The API

- `hash::sha1(data) -> u8[20]`: SHA-1 (FIPS 180-4 §6.1), one-shot. It's there for
  WebSocket's `Sec-WebSocket-Accept` (RFC 6455 §4.2.2), which requires it. SHA-1 is
  broken for collision resistance; don't use it for anything that needs security.
- `hash::sha256(data) -> u8[32]`: SHA-256 (FIPS 180-4 §6.2), one-shot.
- Both take a message shorter than 2^61 bytes, as FIPS 180-4 allows. A longer one,
  whether one-shot or a stream whose total reaches 2^61 bytes, aborts the program
  before it is hashed.
- `hash::Sha256`: a SHA-256 computation in pieces. `var s = hash::Sha256 { .. };`
  starts one.
  - `hash::sha256_update(s, data)` adds `data`. How the input is split across calls
    doesn't change the digest.
  - `hash::sha256_finish(s) -> u8[32]` returns the digest of everything added so far
    and leaves `s` as it was, so more input can follow.
  - `hash::sha256_reset(s)` starts `s` over.
  - A `Sha256` is a fixed-size value of 112 bytes with no heap storage, so it fits in a
    slot of a per-connection array, and copying it forks the computation.
- `hash::hmac_sha256(key, data) -> u8[32]`: HMAC-SHA-256 (RFC 2104), one-shot. A key
  longer than 64 bytes is hashed first, as RFC 2104 says.
- `hash::HmacSha256`: an HMAC-SHA-256 computation in pieces, 224 bytes.
  - `hash::hmac_sha256_start(key) -> HmacSha256` hashes the key's two padded blocks
    once. A copy of the result made before any input is the key's state: copy it for
    each message to MAC many messages under one key without hashing the key again.
  - `hash::hmac_sha256_update(m, data)` and `hash::hmac_sha256_finish(m) -> u8[32]`
    work as for `Sha256`; `finish` leaves `m` as it was.
- `hash::pbkdf2_sha256(out, password, salt, iterations)`: PBKDF2-HMAC-SHA-256 (RFC
  8018 §5.2). It fills the `u8[:]` slice `out`, up to (2^32 − 1) × 32 bytes long,
  with the derived key, and `out` may overlap `salt`. `iterations` must be at least
  1. A lower count, or a longer `out`, aborts the program before anything is
  derived. The time it takes grows with `iterations`, which has no upper limit: a
  caller that takes the count from an untrusted source, such as a server's SCRAM
  challenge, must cap it. SCRAM-SHA-256's `SaltedPassword` (RFC 7677) is 32 bytes of
  it.
- `hash::equal(a, b) -> bool`: whether two byte slices hold the same bytes, in a time
  that depends on their lengths only. Compare a received MAC with the expected one
  with it, not with `==`, which returns at the first difference.
- `hash::random_bytes(out)`: fills the `u8[:]` slice `out`, of any length, with bytes
  from the OS's cryptographically secure random source, for a nonce, a salt, or a key.
  An empty slice is no error. If the source fails, the program aborts with a message
  rather than go on with weaker bytes; it returns nothing to check. libpq's SCRAM
  client nonce is 18 such bytes, base64-encoded.
- `hash::sha_instructions() -> bool`: whether SHA-1 and SHA-256 run on the CPU's SHA
  instructions in this process ([below](#the-sha-instructions)).

## SHA-1 and SHA-256

Both run FIPS 180-4's compression function on 64-byte blocks: `sha1_blocks` and
`sha256_blocks` take any whole number of blocks, on the CPU's SHA instructions where it
has them ([below](#the-sha-instructions)), and on the package's Goose rounds where it
doesn't. The Goose rounds keep the message schedule 16 words at a time in a ring
rather than expanded to 64 or 80 words. Every index into the ring and the round
constants is a round number masked to the array's size, so the Goose compiler removes
the bounds checks from the rounds. One function pads the end of a message for both
hashes: the last partial block, a `0x80` byte, and the length in bits, in one block or
two.

The streaming state holds the eight hash words, the byte count, and up to 63 bytes
that don't yet fill a block. `update` fills the held block first, then hashes whole
blocks straight from the caller's slice, and holds the rest. `finish` pads a copy of
the state, which is why the state can go on.

## The SHA instructions

ARMv8 and x86-64 have instructions that run four rounds of SHA-256, or of SHA-1, at
once, and update the message schedule. Goose can't express them
([findings](goose-findings.md#goose-has-no-sha-instructions)), so the package carries
them in C until Goose's stdlib does ([decisions](decisions.md)). `hash.build` has
`goose build` compile `shim/sha.c` with the program. It holds SHA-1's and SHA-256's
compression functions on ARMv8's `SHA1*` and `SHA256*` instructions and on x86-64's
SHA-NI, which `sha1_blocks` and `sha256_blocks` call through `extern fn` with all of a
call's blocks at once. Each C function that uses the instructions is compiled for
them by a target attribute of its own, so the program needs no `-march` flag and runs
on any CPU of its architecture.

Whether the CPU has the instructions is checked once per process, on the first block
hashed:

| Platform | Check |
|---|---|
| macOS on arm64 | None: every Apple Silicon CPU has SHA-1 and SHA-256. |
| Linux on arm64 | `getauxval(AT_HWCAP)` has `HWCAP_SHA1` and `HWCAP_SHA2`. |
| Windows on arm64 | `IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE)`. |
| x86-64 | `cpuid` leaf 7 has SHA (EBX bit 29), and leaf 1 has SSSE3 and SSE4.1. |
| Anything else | None: the Goose rounds run. |

Where the CPU lacks them, or the environment variable `GOOSE_HASH_PORTABLE` is set to
anything but `0` when the process first hashes, the C returns at once and the Goose
rounds run. `hash::sha_instructions()` says which, for the life of the process. The
two give the same digests; the tests run on both ([development](development.md#test)).

The C, like the Goose rounds, has no branch or memory index that depends on the data
or a key.

## Random bytes

Goose's `random_seed()` is entropy for seeding `rng`, and on Windows it's a mix of the
clocks, the process id, and an address
([findings](goose-findings.md#goose-has-no-secure-random-bytes)). So `random_bytes`
calls `shim/random.c`, which asks the OS directly:

| Platform | Source |
|---|---|
| Windows | `BCryptGenRandom` with `BCRYPT_USE_SYSTEM_PREFERRED_RNG`, from `bcrypt.lib`, which `hash.build` links. |
| macOS, Linux, and other POSIX systems | `getentropy`, 256 bytes a call. On Linux it blocks until the kernel's pool is first seeded, early in boot, and never after. |

Any failure makes the C return `false`, and `random_bytes` aborts. Nothing falls back
to `/dev/urandom`, which early in boot can return bytes from a pool not yet seeded, or
to a clock or a PRNG. On Linux, `random_bytes` needs kernel 3.17 or later, for
`getrandom`, and glibc 2.25 or musl 1.1.20 or later, for `getentropy`; on an older
kernel, `getentropy` fails with `ENOSYS`, and the program aborts.

## HMAC and PBKDF2

`hmac_sha256_start` XORs the key, padded to 64 bytes, with `0x36` and `0x5C` and hashes
each into its own `Sha256`: the inner and outer states. A message then costs its own
blocks plus two blocks for the outer hash.

PBKDF2 calls HMAC once per iteration on the previous iteration's 32 bytes. After the
first iteration, each one is exactly one block for the inner hash and one for the
outer, so `pbkdf2_sha256` runs the compression function on copies of the key's two
states directly, with the padding for a 96-byte message written once: two
compression calls an iteration, and no buffering or length counting. The salt is
hashed once, into a copy of the key's inner state, before any of `out` is written;
each 32-byte block of `out` starts from a copy of that state, adds its index, and is
derived on its own. So `out` may overlap `salt`, and a shorter `out` is a prefix of a
longer one.

## No branch on secret data

Nothing in the package branches on, or indexes memory with, the bytes it hashes or a
key. Only lengths steer the code, and a length isn't treated as secret: `equal`
returns `false` at once for slices of different lengths, and HMAC takes a different
path for a key longer than 64 bytes.

`equal` ORs together the XOR of each pair of bytes and tests the result once, after
the loop. Goose emits that loop to C as it is. Compiled with Apple clang 21 at `-O2`
for arm64, it becomes vector XORs and ORs whose only branches count the length
(2026-10-19); other C compilers haven't been checked.

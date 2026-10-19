# Architecture

How goose-hash works and why, for its users. The decisions behind it, each with its
reason and what was weighed against it, are in [decisions](decisions.md); measurements
are in [results](results.md).

## A package of its own, in Goose

goose-hash is imported as `hash` and imports nothing but Goose's stdlib, so any Goose
program can use it: goose-http for WebSocket's handshake, goose-postgres for
SCRAM-SHA-256, and goose-rest for verifying a JWT. It holds hashes and MACs only.
Ciphers, signatures, and key generation stay in OpenSSL, through goose-tls.

The functions are Goose, not bindings to OpenSSL. Bytes go in as `u8[:]` slices and
digests come out as fixed arrays, `u8[20]` or `u8[32]`, which need no allocation and
can be compared with `==` or sliced to a `u8[:]`.

## The API

- `hash::sha1(data) -> u8[20]`: SHA-1 (FIPS 180-4 §6.1), one-shot. It's there for
  WebSocket's `Sec-WebSocket-Accept` (RFC 6455 §4.2.2), which requires it. SHA-1 is
  broken for collision resistance; don't use it for anything that needs security.
- `hash::sha256(data) -> u8[32]`: SHA-256 (FIPS 180-4 §6.2), one-shot.
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
  8018 §5.2). It fills the `u8[:]` slice `out`, of any length, with the derived key.
  `iterations` must be at least 1; a lower count aborts the program. SCRAM-SHA-256's
  `SaltedPassword` (RFC 7677) is 32 bytes of it.
- `hash::equal(a, b) -> bool`: whether two byte slices hold the same bytes, in a time
  that depends on their lengths only. Compare a received MAC with the expected one
  with it, not with `==`, which returns at the first difference.

## SHA-1 and SHA-256

Both run FIPS 180-4's compression function on 64-byte blocks, with the message
schedule kept 16 words at a time in a ring rather than expanded to 64 or 80 words.
Every index into the ring and the round constants is a round number masked to the
array's size, so the Goose compiler removes the bounds checks from the rounds. One
function pads the end of a message for both hashes: the last partial block, a `0x80`
byte, and the length in bits, in one block or two.

The streaming state holds the eight hash words, the byte count, and up to 63 bytes
that don't yet fill a block. `update` fills the held block first, then hashes whole
blocks straight from the caller's slice, and holds the rest. `finish` pads a copy of
the state, which is why the state can go on.

The code runs on the CPU's general-purpose integer instructions. OpenSSL uses the
SHA instructions of ARMv8 and x86-64 where the CPU has them, which Goose can't
express ([findings](goose-findings.md#goose-has-no-sha-instructions)). With them
turned off, OpenSSL's general-purpose code takes 4 to 22 percent less time than the
package's ([results](results.md)).

## HMAC and PBKDF2

`hmac_sha256_start` XORs the key, padded to 64 bytes, with `0x36` and `0x5C` and hashes
each into its own `Sha256`: the inner and outer states. A message then costs its own
blocks plus two blocks for the outer hash.

PBKDF2 calls HMAC once per iteration on the previous iteration's 32 bytes. After the
first iteration, each one is exactly one block for the inner hash and one for the
outer, so `pbkdf2_sha256` runs the compression function on copies of the key's two
states directly, with the padding for a 96-byte message written once: two
compression calls an iteration, and no buffering or length counting. Each 32-byte
block of `out` is derived on its own, so a shorter `out` is a prefix of a longer one.

## No branch on secret data

Nothing in the package branches on, or indexes memory with, the bytes it hashes or a
key. Only lengths steer the code, and a length isn't treated as secret: `equal`
returns `false` at once for slices of different lengths, and HMAC takes a different
path for a key longer than 64 bytes.

`equal` ORs together the XOR of each pair of bytes and tests the result once, after
the loop. Goose emits that loop to C as it is. Compiled with Apple clang 21 at `-O2`
for arm64, it becomes vector XORs and ORs whose only branches count the length
(2026-10-19); other C compilers haven't been checked.

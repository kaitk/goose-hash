# goose-hash

Cryptographic hashes for [Goose](https://github.com/aardappel/goose): SHA-1, SHA-256,
HMAC-SHA-256, PBKDF2-HMAC-SHA-256, a constant-time compare, and secure random bytes
from the OS, bytes in and digests out. Goose, with no other package, and two small C
shims, until Goose's stdlib can do what they do. One runs SHA-1 and SHA-256 on the
CPU's SHA instructions on ARMv8 and x86-64, and elsewhere the package's own Goose
rounds run; the other reads the OS's secure random source
([architecture](docs/architecture.md)). Goose's stdlib has `base64` and `json`, and
`hash` for dictionaries, but no cryptographic hash, and its `random_seed` is for
seeding a PRNG. Its users are goose-http's WebSocket handshake (SHA-1),
goose-postgres's SCRAM-SHA-256 login (PBKDF2, HMAC, and the client nonce), and a JWT
verifier in goose-rest (HMAC-SHA-256).

It holds hashes, MACs, and the OS's random bytes only: ciphers, signatures, and key
generation are OpenSSL's, through goose-tls ([decisions](docs/decisions.md)).

## Status

🟢 works · 🟡 partial or next · 🔴 not yet

| | Feature | Notes |
|---|---|---|
| 🟢 | SHA-1 | One-shot. For WebSocket's handshake (RFC 6455), not for security. |
| 🟢 | SHA-256 | One-shot and streaming. |
| 🟢 | HMAC-SHA-256 | One-shot, and streaming from a keyed state that can be copied. |
| 🟢 | PBKDF2-HMAC-SHA-256 | Any output length. For SCRAM-SHA-256. |
| 🟢 | Constant-time compare | For checking a MAC. |
| 🟢 | Secure random bytes | From the OS's source, through C: `getentropy` on macOS and Linux, `BCryptGenRandom` on Windows. For a nonce, such as SCRAM's. Aborts rather than return weaker bytes. |
| 🟢 | The CPU's SHA instructions | ARMv8's and x86-64's SHA-NI, through C, where the CPU has them; the Goose rounds elsewhere. `hash::sha_instructions()` says which run. |
| 🟡 | The SHA instructions in Goose | The C goes once Goose's stdlib has them ([findings](docs/goose-findings.md#goose-has-no-sha-instructions)). |
| 🟢 | Windows on x86-64, and SHA-NI on real hardware | Tested with MSVC, clang-cl, and clang, and on a Ryzen 7 5800X3D's SHA-NI ([development](docs/development.md#where-it-has-run), [results](docs/results.md#amd-ryzen-7-5800x3d-linux-in-wsl2-and-windows)). Secure random bytes haven't run on Windows yet. |
| 🟡 | Windows on arm64 | Hasn't run ([development](docs/development.md#where-it-has-run)). |
| 🔴 | SHA-1 streaming, SHA-384, SHA-512, HMAC-SHA-1 | No user needs them yet. |

Every function is checked against the published test vectors (NIST's examples, RFC
4231, RFC 7914, and RFC 6455's handshake) and against Python's `hashlib` and `hmac` on
random inputs, on the SHA instructions and on the Goose rounds
([development](docs/development.md#test)).

## Quick start

```goose
import std;
import base64;
import hash from "github.com/kaitk/goose-hash@v0.0.4";

fn main() {
    let d = hash::sha256("abc");                       // u8[32]

    var s = hash::Sha256 { .. };                       // the same, in pieces
    hash::sha256_update(s, "a");
    hash::sha256_update(s, "bc");
    assert(hash::sha256_finish(s) == d);

    let mac = hash::hmac_sha256("key", "message");
    assert(hash::equal(mac, hash::hmac_sha256("key", "message")));

    var salted: u8[32] = [0; 32];                      // SCRAM's SaltedPassword
    hash::pbkdf2_sha256(salted, "pencil", "salt", 4096);

    var accept: u8[>..] = [];                          // Sec-WebSocket-Accept
    base64::encode(accept, hash::sha1("dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
    print(accept);                                     // s3pPLMBiTxaQ9kYGzzhZRbK+xOo=

    var nonce: u8[18] = [0; 18];                       // SCRAM's client nonce
    hash::random_bytes(nonce);
}
```

```sh
goose build hello.goose
```

`goose build` compiles the package's C with the program, with the C compiler it
already uses, and on Windows links `bcrypt`; the package needs no other package and no
`-march` flag. Set
`GOOSE_HASH_PORTABLE=1` in the environment to run the Goose rounds instead of the SHA
instructions.

## API

All in namespace `hash`. [Architecture](docs/architecture.md#the-api) has each
function's contract.

- `hash::sha1(data) -> u8[20]` and `hash::sha256(data) -> u8[32]`: one-shot digests.
- `hash::Sha256 { .. }`, with `sha256_update(s, data)`, `sha256_finish(s) -> u8[32]`,
  and `sha256_reset(s)`: SHA-256 in pieces. `finish` leaves the state to go on. A
  `Sha256` is a fixed-size value of 112 bytes.
- `hash::hmac_sha256(key, data) -> u8[32]`: HMAC-SHA-256, one-shot.
- `hash::hmac_sha256_start(key) -> HmacSha256`, with `hmac_sha256_update(m, data)` and
  `hmac_sha256_finish(m) -> u8[32]`: HMAC-SHA-256 in pieces. A copy of a fresh state
  MACs another message under the same key without hashing the key again.
- `hash::pbkdf2_sha256(out, password, salt, iterations)`: fills `out` with
  PBKDF2-HMAC-SHA-256's key; `iterations` is at least 1. Its time grows with
  `iterations`, which has no upper limit, so cap a count that comes from an untrusted
  source, such as a server's SCRAM challenge, before calling it.
- `hash::equal(a, b) -> bool`: compares two byte slices in a time that depends only on
  their lengths. Check a MAC with it, not with `==`.
- `hash::sha_instructions() -> bool`: whether SHA-1 and SHA-256 run on the CPU's SHA
  instructions in this process.
- `hash::random_bytes(out)`: fills the `u8[:]` slice `out` from the OS's
  cryptographically secure random source. It aborts the program if the source fails,
  rather than return weaker bytes.

## Benchmarks

The package is measured against OpenSSL's libcrypto, through the same loop, and
`openssl speed`. The full tables, setup, and method are in
[docs/results.md](docs/results.md).

On an Apple M1 Pro under macOS 26.3.1, with Goose `v2026.2` and OpenSSL 4.0.3, the
median of 10 runs[^1]:

| Case | goose | OpenSSL | goose without SHA instructions | OpenSSL without them |
|---|---:|---:|---:|---:|
| SHA-1 of 60 bytes, a WebSocket key | 56 ns | 147 ns | 291 ns | 247 ns |
| SHA-256 of 64 bytes | 51 ns | 126 ns | 425 ns | 406 ns |
| SHA-256 of 1 MB | 2,146 MB/s | 2,413 MB/s | 302 MB/s | 382 MB/s |
| PBKDF2-HMAC-SHA-256, 4,096 iterations | 0.36 ms | 0.46 ms | 1.79 ms | 1.57 ms |

[^1]: Both use the CPU's SHA-1 and SHA-256 instructions as built; the last two
    columns turn them off, with `GOOSE_HASH_PORTABLE=1` and `OPENSSL_armcap=0`. On
    small inputs OpenSSL's time is mostly its per-call setup
    ([results](docs/results.md)).

## Credits

The C shim's rounds on the SHA instructions follow Jeffrey Walton's SHA-Intrinsics,
which is in the public domain. The Goose code implements FIPS 180-4, RFC 2104, and RFC
8018 from their text, and is tested with NIST's, RFC 4231's, RFC 7914's, and RFC
6455's vectors; [docs/credits.md](docs/credits.md) names what it takes from each.

## Layout

```text
hash.goose     the package's entry: equal, and what SHA-1 and SHA-256 share
hash.build     the sidecar that has goose build compile shim/
shim/sha.c     SHA-1's and SHA-256's rounds on the CPU's SHA instructions
shim/random.c  the OS's secure random source
sha1.goose     SHA-1
sha256.goose   SHA-256, one-shot and streaming
hmac.goose     HMAC-SHA-256 and PBKDF2-HMAC-SHA-256
random.goose   random_bytes
test/          unit tests, and digest.goose, which scripts/against_python.py drives
scripts/       the Goose pin and test scripts
bench/         the Goose benchmark, the C one for OpenSSL's libcrypto, and scripts
docs/          architecture, decisions, development setup, results, Goose findings,
               credits
tasks/         task tracking (tasks/README.md)
```

## Pins

| What | Pin | Why |
|---|---|---|
| Goose compiler | `aardappel/goose` release `v2026.3` (`8511ae3`), which the scripts fetch ([setup](docs/development.md#set-up-the-goose-compiler)) | An official release builds the same on every platform; a commit is pinned only for a fix not yet released |
| Benchmark library | The OpenSSL installed on the machine, 3 or later | Each result names the version it ran against |

Move the pin in its own commit, with the tests and results that still hold
([development.md](docs/development.md#move-the-pin)).

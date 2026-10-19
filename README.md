# goose-hash

Cryptographic hashes for [Goose](https://github.com/aardappel/goose): SHA-1, SHA-256,
HMAC-SHA-256, PBKDF2-HMAC-SHA-256, and a constant-time compare, bytes in and digests
out. Pure Goose, with no C and no other package
([architecture](docs/architecture.md)). Goose's stdlib has `base64` and `json`, and
`hash` for dictionaries, but no cryptographic hash. Its users are goose-http's
WebSocket handshake (SHA-1), goose-postgres's SCRAM-SHA-256 login (PBKDF2 and HMAC),
and a JWT verifier in goose-rest (HMAC-SHA-256).

It holds hashes and MACs only: ciphers, signatures, and key generation are OpenSSL's,
through goose-tls ([decisions](docs/decisions.md)).

## Status

🟢 works · 🟡 partial or next · 🔴 not yet

| | Feature | Notes |
|---|---|---|
| 🟢 | SHA-1 | One-shot. For WebSocket's handshake (RFC 6455), not for security. |
| 🟢 | SHA-256 | One-shot and streaming. |
| 🟢 | HMAC-SHA-256 | One-shot, and streaming from a keyed state that can be copied. |
| 🟢 | PBKDF2-HMAC-SHA-256 | Any output length. For SCRAM-SHA-256. |
| 🟢 | Constant-time compare | For checking a MAC. |
| 🔴 | SHA-1 streaming, SHA-384, SHA-512, HMAC-SHA-1 | No user needs them yet. |
| 🔴 | The CPU's SHA instructions | Goose can't express them ([findings](docs/goose-findings.md#goose-has-no-sha-instructions)). |

Every function is checked against the published test vectors (NIST's examples, RFC
4231, RFC 7914, and RFC 6455's handshake) and against Python's `hashlib` and `hmac` on
random inputs ([development](docs/development.md#test)).

## Quick start

```goose
import std;
import base64;
import hash from "github.com/kaitk/goose-hash@v0.0.1";

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
}
```

```sh
goose build hello.goose
```

It needs no C compiler of its own and no other package.

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
  PBKDF2-HMAC-SHA-256's key; `iterations` is at least 1.
- `hash::equal(a, b) -> bool`: compares two byte slices in a time that depends only on
  their lengths. Check a MAC with it, not with `==`.

## Benchmarks

The package is measured against OpenSSL's libcrypto, through the same loop, and
`openssl speed`. The full tables, setup, and method are in
[docs/results.md](docs/results.md).

On an Apple M1 Pro under macOS 26.3.1, with Goose `v2026.2` and OpenSSL 4.0.3, the
median of 10 runs[^1]:

| Case | goose | OpenSSL | OpenSSL without SHA instructions |
|---|---:|---:|---:|
| SHA-1 of 60 bytes, a WebSocket key | 295 ns | 150 ns | 253 ns |
| SHA-256 of 64 bytes | 441 ns | 132 ns | 417 ns |
| SHA-256 of 1 MB | 287 MB/s | 2,343 MB/s | 375 MB/s |
| PBKDF2-HMAC-SHA-256, 4,096 iterations | 1.86 ms | 0.47 ms | 1.59 ms |

[^1]: OpenSSL uses the CPU's SHA-1 and SHA-256 instructions, which Goose can't
    express; the last column turns them off with `OPENSSL_armcap=0`. The goose
    column is from the first run; the second gave 296 ns, 432 ns, 294 MB/s, and
    1.82 ms.

## Credits

No code is adopted from another library. The package implements FIPS 180-4, RFC 2104,
and RFC 8018 from their text, and is tested with NIST's, RFC 4231's, RFC 7914's, and
RFC 6455's vectors; [docs/credits.md](docs/credits.md) names what it takes from each.

## Layout

```text
hash.goose     the package's entry: equal, and what SHA-1 and SHA-256 share
sha1.goose     SHA-1
sha256.goose   SHA-256, one-shot and streaming
hmac.goose     HMAC-SHA-256 and PBKDF2-HMAC-SHA-256
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
| Goose compiler | `aardappel/goose` release `v2026.2` (`3ee3871`), which the scripts fetch ([setup](docs/development.md#set-up-the-goose-compiler)) | An official release builds the same on every platform; a commit is pinned only for a fix not yet released |
| Benchmark library | The OpenSSL installed on the machine, 3 or later | Each result names the version it ran against |

Move the pin in its own commit, with the tests and results that still hold
([development.md](docs/development.md#move-the-pin)).

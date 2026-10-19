# Results

The package against OpenSSL's libcrypto and `openssl speed`: each machine's latest run.
Each machine has a section of its own, which starts with how the machine and its tools
were set up; numbers from different machines aren't compared. How to run them is in
[development.md](development.md#benchmark). Earlier runs and the measurements behind
each change are in [results-history.md](results-history.md).

- [Apple M1 Pro, macOS](#apple-m1-pro-macos)

## Apple M1 Pro, macOS

- Apple M1 Pro (8 performance and 2 efficiency cores), macOS 26.3.1 (Darwin 25.3.0).
- Goose `v2026.2` (`3ee3871`), `goose build` defaults (`-O2`), with Apple clang 21.0.0
  (`clang-2100.0.123.102`).
- OpenSSL 4.0.3 from Homebrew (`openssl@4`), the library and the `openssl` command;
  `bench/ref.c` at `-O2`, same compiler.
- Load average was 6.8 before the runs and 6.6 after them; no other benchmark ran.

### Four cases at `a8a4e37`

2026-10-19, package code at `ba4ac33`. `bench/run.sh`: 10 runs per row. A `bench` or
`ref` run times 1,000,000 calls for the 60- and 64-byte inputs, 200 for 1 MB, and 100
for PBKDF2; an `openssl speed -evp` run lasts 1 second. Each cell is the median of the
10 runs, in nanoseconds a call, and MB/s is the input's bytes over that time.
`ref` is libcrypto through the same loop as `bench`: `EVP_Digest` with a digest fetched
once, and `PKCS5_PBKDF2_HMAC`. The cases:

- `sha1-60`: SHA-1 of 60 bytes, a WebSocket key and its GUID.
- `sha256-64`: SHA-256 of 64 bytes.
- `sha256-1m`: SHA-256 of 1,048,576 bytes.
- `pbkdf2-4096`: 32 bytes of PBKDF2-HMAC-SHA-256 from an 8-byte password and a
  16-byte salt at 4,096 iterations, Postgres's default for SCRAM-SHA-256.

As built, OpenSSL uses the CPU's SHA-1 and SHA-256 instructions:

| Case | goose | libcrypto | `openssl speed` | goose MB/s | libcrypto MB/s | goose / libcrypto time |
|---|---:|---:|---:|---:|---:|---:|
| `sha1-60` | 295 | 150 | 149 | 203 | 399 | 1.97 |
| `sha256-64` | 441 | 132 | 138 | 145 | 485 | 3.34 |
| `sha256-1m` | 3,651,105 | 447,590 | 443,262 | 287 | 2,343 | 8.16 |
| `pbkdf2-4096` | 1,855,490 | 466,050 | – | – | – | 3.98 |

With `OPENSSL_armcap=0`, which turns those instructions off, so OpenSSL runs its
general-purpose code, as the package does:

| Case | goose | libcrypto | `openssl speed` | goose MB/s | libcrypto MB/s | goose / libcrypto time |
|---|---:|---:|---:|---:|---:|---:|
| `sha1-60` | 296 | 253 | 252 | 202 | 237 | 1.17 |
| `sha256-64` | 432 | 417 | 412 | 148 | 154 | 1.04 |
| `sha256-1m` | 3,560,845 | 2,793,355 | 2,750,000 | 294 | 375 | 1.27 |
| `pbkdf2-4096` | 1,823,500 | 1,587,100 | – | – | – | 1.15 |

`ref` agrees with `openssl speed` within 5 percent in every case, so the loop measures
what `openssl speed` does. A WebSocket handshake's SHA-1 takes 0.3 µs, and a SCRAM
login's PBKDF2 1.9 ms, against OpenSSL's 0.47 ms. Most of the gap is the SHA
instructions, which Goose has no way to use
([findings](goose-findings.md#goose-has-no-sha-instructions)); without them, the
package takes 4 to 27 percent longer than OpenSSL.

Commands:

```sh
bench/build.sh
bench/run.sh
OPENSSL_armcap=0 bench/run.sh
```

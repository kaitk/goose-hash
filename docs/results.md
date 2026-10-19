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
  (`clang-2100.0.123.102`), which also compiles `shim/sha.c`.
- OpenSSL 4.0.3 from Homebrew (`openssl@4`), the library and the `openssl` command;
  `bench/ref.c` at `-O2`, same compiler.
- Load average was 3.8 before the runs, 4.1 between the two, and 3.4 after them. No
  other benchmark or build ran; Docker's VM was up with idle containers.

### Four cases at `404b23f`, on the SHA instructions

2026-10-19, package code at `404b23f`, the first with the SHA instructions.
`bench/run.sh`: 10 runs per row. A `bench` or `ref` run times 1,000,000 calls for the
60- and 64-byte inputs, 200 for 1 MB, and 100 for PBKDF2; an `openssl speed -evp` run
lasts 1 second. Each cell is the median of the 10 runs, in nanoseconds a call, and
MB/s is the input's bytes over that time. `ref` is libcrypto through the same loop as
`bench`: `EVP_Digest` with a digest fetched once, and `PKCS5_PBKDF2_HMAC`. The cases:

- `sha1-60`: SHA-1 of 60 bytes, a WebSocket key and its GUID.
- `sha256-64`: SHA-256 of 64 bytes.
- `sha256-1m`: SHA-256 of 1,048,576 bytes.
- `pbkdf2-4096`: 32 bytes of PBKDF2-HMAC-SHA-256 from an 8-byte password and a
  16-byte salt at 4,096 iterations, Postgres's default for SCRAM-SHA-256.

Both on the CPU's SHA-1 and SHA-256 instructions, as built (`bench/run.sh`):

| Case | goose | libcrypto | `openssl speed` | goose MB/s | libcrypto MB/s | goose / libcrypto time |
|---|---:|---:|---:|---:|---:|---:|
| `sha1-60` | 56 | 147 | 147 | 1,081 | 407 | 0.38 |
| `sha256-64` | 51 | 126 | 126 | 1,248 | 508 | 0.40 |
| `sha256-1m` | 488,595 | 434,535 | 428,943 | 2,146 | 2,413 | 1.12 |
| `pbkdf2-4096` | 363,600 | 457,210 | – | – | – | 0.80 |

Both without them, the package on its Goose rounds (`GOOSE_HASH_PORTABLE=1
OPENSSL_armcap=0 bench/run.sh`):

| Case | goose | libcrypto | `openssl speed` | goose MB/s | libcrypto MB/s | goose / libcrypto time |
|---|---:|---:|---:|---:|---:|---:|
| `sha1-60` | 291 | 247 | 244 | 206 | 243 | 1.18 |
| `sha256-64` | 425 | 406 | 404 | 151 | 158 | 1.05 |
| `sha256-1m` | 3,469,410 | 2,745,870 | 2,704,918 | 302 | 382 | 1.26 |
| `pbkdf2-4096` | 1,789,460 | 1,573,060 | – | – | – | 1.14 |

`ref` agrees with `openssl speed` within 2 percent in every case, so the loop measures
what `openssl speed` does.

On the instructions, the package takes 12 percent longer than OpenSSL on 1 MB, where
only the rounds count: the shim's C reaches 2,146 MB/s, OpenSSL's assembly 2,413.
On small inputs and PBKDF2 it takes less time than OpenSSL, 0.38 to 0.80 of it,
because OpenSSL spends most of a 60- or 64-byte call in `EVP_Digest`'s setup, which a
call into the package doesn't have, and its `PKCS5_PBKDF2_HMAC` runs each iteration
through its MAC layer's init, update, and final calls, where the package runs two
compression calls on saved states
([architecture](architecture.md#hmac-and-pbkdf2)). A WebSocket handshake's SHA-1 takes
56 ns, and a SCRAM login's PBKDF2 0.36 ms.

Against the package's own rounds, the instructions make SHA-1 of 60 bytes 5.2 times
faster, SHA-256 of 64 bytes 8.3 times, SHA-256 of 1 MB 7.1 times, and PBKDF2 4.9
times. Without the instructions, the package takes 5 to 26 percent longer than
OpenSSL, as before ([history](results-history.md#apple-m1-pro-macos)).

Commands:

```sh
bench/build.sh
bench/run.sh
GOOSE_HASH_PORTABLE=1 OPENSSL_armcap=0 bench/run.sh
```

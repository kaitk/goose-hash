# Results

The package against OpenSSL's libcrypto and `openssl speed`: each machine's latest run.
Each machine has a section of its own, which starts with how the machine and its tools
were set up; numbers from different machines aren't compared. How to run them is in
[development.md](development.md#benchmark). Earlier runs and the measurements behind
each change are in [results-history.md](results-history.md).

- [Apple M1 Pro, macOS](#apple-m1-pro-macos)
- [AMD Ryzen 7 5800X3D, Linux in WSL2 and Windows](#amd-ryzen-7-5800x3d-linux-in-wsl2-and-windows)

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

## AMD Ryzen 7 5800X3D, Linux in WSL2 and Windows

- AMD Ryzen 7 5800X3D (8 cores, 16 threads, Zen 3, with SHA-NI), Windows 11 Pro
  10.0.26300.
- Linux: Ubuntu 26.04.1 in WSL2 3.0.3.0, kernel 6.18.54.1-microsoft-standard-WSL2.
  WSL2 is a virtual machine under Hyper-V; the numbers below are from inside it. The
  checkout was a clone on the VM's own ext4 disk, not under `/mnt/c`.
- Goose `v2026.2` (`3ee3871`), `goose build` defaults (`-O2`), with gcc 15.2.0
  (Ubuntu 15.2.0-16ubuntu1) as `cc`, and again with Ubuntu clang 21.1.8
  (`CC=clang`); the same compiler compiles `shim/sha.c` and `bench/ref.c`, at `-O2`.
- OpenSSL 3.5.5 (27 Jan 2026), Ubuntu's `libssl-dev` and `openssl`, built against with
  `OPENSSL=/usr bench/build.sh`.
- Windows: the package's `bench/bench.goose` alone, built with `goose build --cc msvc`
  (MSVC 19.51) and `--cc clang-cl` (clang-cl 22.1.3), and Git for Windows' `openssl
  speed` (OpenSSL 3.2.1, 30 Jan 2024). There is no `ref` on Windows: no OpenSSL headers
  or `libcrypto` for MSVC were installed, and `bench/ref.c` uses `clock_gettime`.
- Load average inside WSL2 was 0.2 before the first run and under 1.0 throughout; no
  other benchmark or build ran during the Linux runs. During the Windows runs, a gcc
  compile in WSL2 kept one of the 16 threads busy for part of the time.

### Four cases at `3cfe57b`, Linux in WSL2

2026-10-19, package code at `3cfe57b`. `bench/run.sh`: 10 runs per row, with the
cases, loops, and cells as in [the M1 Pro's section](#apple-m1-pro-macos).

Both on the CPU's SHA instructions, as built (`bench/run.sh`), with gcc:

| Case | goose | libcrypto | `openssl speed` | goose MB/s | libcrypto MB/s | goose / libcrypto time |
|---|---:|---:|---:|---:|---:|---:|
| `sha1-60` | 74 | 145 | 147 | 805 | 414 | 0.51 |
| `sha256-64` | 80 | 145 | 143 | 804 | 440 | 0.55 |
| `sha256-1m` | 481,827 | 484,478 | 478,492 | 2,176 | 2,164 | 0.99 |
| `pbkdf2-4096` | 356,273 | 518,412 | – | – | – | 0.69 |

The same with clang (`CC=clang bench/build.sh`, then `bench/run.sh`):

| Case | goose | libcrypto | `openssl speed` | goose MB/s | libcrypto MB/s | goose / libcrypto time |
|---|---:|---:|---:|---:|---:|---:|
| `sha1-60` | 66 | 144 | 143 | 910 | 417 | 0.46 |
| `sha256-64` | 59 | 140 | 142 | 1,090 | 457 | 0.42 |
| `sha256-1m` | 473,669 | 479,363 | 480,307 | 2,214 | 2,187 | 0.99 |
| `pbkdf2-4096` | 330,576 | 512,349 | – | – | – | 0.65 |

The package on its Goose rounds, against OpenSSL with SHA-NI alone turned off, which
leaves it its AVX2 code (`GOOSE_HASH_PORTABLE=1 OPENSSL_ia32cap=':~0x20000000'
bench/run.sh`), with gcc:

| Case | goose | libcrypto | `openssl speed` | goose MB/s | libcrypto MB/s | goose / libcrypto time |
|---|---:|---:|---:|---:|---:|---:|
| `sha1-60` | 288 | 196 | 196 | 208 | 306 | 1.47 |
| `sha256-64` | 426 | 296 | 293 | 150 | 216 | 1.44 |
| `sha256-1m` | 3,317,341 | 1,601,241 | 1,587,302 | 316 | 655 | 2.07 |
| `pbkdf2-4096` | 1,730,482 | 1,186,146 | – | – | – | 1.46 |

The package on its Goose rounds, against OpenSSL's scalar code, with every vector path
turned off (`GOOSE_HASH_PORTABLE=1 OPENSSL_ia32cap='~0x10000A0000000000:~0x20000128'
bench/run.sh`, [the mask](development.md#benchmark)), with gcc, and with clang:

| Case | goose, gcc | goose, clang | libcrypto, gcc build | `openssl speed` | goose MB/s, gcc | libcrypto MB/s | goose / libcrypto time, gcc |
|---|---:|---:|---:|---:|---:|---:|---:|
| `sha1-60` | 293 | 263 | 224 | 222 | 205 | 268 | 1.31 |
| `sha256-64` | 426 | 412 | 370 | 373 | 150 | 173 | 1.15 |
| `sha256-1m` | 3,294,973 | 3,291,972 | 2,248,698 | 2,247,191 | 318 | 466 | 1.47 |
| `pbkdf2-4096` | 1,745,231 | 1,640,314 | 1,523,363 | – | – | – | 1.15 |

The clang build's `libcrypto` and `openssl speed` columns were within 4 percent of the
gcc build's in every case, since both call the same library.

These are the first numbers from SHA-NI on a real CPU; until now only QEMU's emulation
had run the x86-64 code. On 1 MB, where only the rounds count, the shim's C runs as
fast as OpenSSL's assembly: 2,176 MB/s with gcc and 2,214 with clang, against 2,164.
On small inputs and PBKDF2 the package takes 0.42 to 0.69 of OpenSSL's time, for the
reasons the M1 Pro's section gives. clang's build of the shim is 11 to 26 percent
faster than gcc's on 60 and 64 bytes and 7 percent on PBKDF2. The instructions make
the package's SHA-1 of 60 bytes 3.9 times faster than its Goose rounds, SHA-256 of 64
bytes 5.3 times, of 1 MB 6.9 times, and PBKDF2 4.9 times (gcc).

Without the instructions, the Goose rounds take 15 to 47 percent longer than OpenSSL's
scalar code, and 1.44 to 2.07 times as long as OpenSSL's AVX2 code, which the package
has no counterpart of.

Commands, inside WSL2:

```sh
OPENSSL=/usr bench/build.sh
bench/run.sh
GOOSE_HASH_PORTABLE=1 OPENSSL_ia32cap=':~0x20000000' bench/run.sh
GOOSE_HASH_PORTABLE=1 OPENSSL_ia32cap='~0x10000A0000000000:~0x20000128' bench/run.sh
CC=clang OPENSSL=/usr bench/build.sh
bench/run.sh
GOOSE_HASH_PORTABLE=1 OPENSSL_ia32cap='~0x10000A0000000000:~0x20000128' bench/run.sh
```

### The package's side at `3cfe57b`, natively on Windows

2026-10-19, package code at `3cfe57b`, in Git Bash. Each package cell is the median of
10 runs of `build/bench/bench-<cc>.exe CASE 10`, timed as `bench/run.sh` times it, from
`goose build --cc <cc> -o build/bench/bench-<cc> bench/bench.goose`; `openssl speed` is
Git for Windows' 3.2.1, 10 runs of `openssl speed -evp <md> -bytes <n> -seconds 1 -mr`
as in `bench/run.sh`, in nanoseconds a call. Its runs spread more than in WSL2: the best
of the 10 was up to 10 percent under the median.

| Case | Rounds | goose, MSVC | goose, clang-cl | `openssl speed` |
|---|---|---:|---:|---:|
| `sha1-60` | SHA-NI | 73 | 70 | 173 |
| `sha256-64` | SHA-NI | 70 | 63 | 173 |
| `sha256-1m` | SHA-NI | 497,216 | 491,731 | 451,457 |
| `pbkdf2-4096` | SHA-NI | 402,690 | 334,146 | – |
| `sha1-60` | Goose's, and OpenSSL's scalar | 316 | 300 | 269 |
| `sha256-64` | Goose's, and OpenSSL's scalar | 651 | 442 | 402 |
| `sha256-1m` | Goose's, and OpenSSL's scalar | 5,261,838 | 3,554,858 | 2,187,500 |
| `pbkdf2-4096` | Goose's, and OpenSSL's scalar | 2,689,026 | 1,748,417 | – |

The second half ran with `GOOSE_HASH_PORTABLE=1`, and `openssl speed` with
`OPENSSL_ia32cap='~0x10000A0000000000:~0x20000128'`. On SHA-NI, MSVC's build takes 1 to 11
percent longer than clang-cl's on the hashes, and 21 percent longer on PBKDF2. On the
Goose rounds, MSVC's build of the generated C takes 5 percent longer on SHA-1 of 60
bytes, 1.47 to 1.48 times as long on SHA-256, and 1.54 times on PBKDF2.

# Results history

Earlier benchmark runs and the measurements behind each change, moved out of
[results.md](results.md), which keeps each machine's latest run. Each machine's section
starts with how it was set up when these ran; a run names its own package commit and
Goose pin where they differ.

- [Apple M1 Pro, macOS](#apple-m1-pro-macos)

## Apple M1 Pro, macOS

- Apple M1 Pro (8 performance and 2 efficiency cores), macOS 26.3.1 (Darwin 25.3.0).
- Goose `v2026.2` (`3ee3871`), `goose build` defaults (`-O2`), with Apple clang 21.0.0
  (`clang-2100.0.123.102`).
- OpenSSL 4.0.3 from Homebrew (`openssl@4`), the library and the `openssl` command;
  `bench/ref.c` at `-O2`, same compiler.

### Four cases at `a8a4e37`, on the Goose rounds only

2026-10-19, package code at `ba4ac33`, before the package had the SHA instructions.
Load average was 6.8 before the runs and 6.6 after them; no other benchmark ran.
`bench/run.sh`: 10 runs per row, with the cases, loops, and cells as in
[results.md](results.md#apple-m1-pro-macos).

As built, OpenSSL uses the CPU's SHA-1 and SHA-256 instructions:

| Case | goose | libcrypto | `openssl speed` | goose MB/s | libcrypto MB/s | goose / libcrypto time |
|---|---:|---:|---:|---:|---:|---:|
| `sha1-60` | 295 | 150 | 149 | 203 | 399 | 1.97 |
| `sha256-64` | 441 | 132 | 138 | 145 | 485 | 3.34 |
| `sha256-1m` | 3,651,105 | 447,590 | 443,262 | 287 | 2,343 | 8.16 |
| `pbkdf2-4096` | 1,855,490 | 466,050 | – | – | – | 3.98 |

With `OPENSSL_armcap=0`, which turns those instructions off:

| Case | goose | libcrypto | `openssl speed` | goose MB/s | libcrypto MB/s | goose / libcrypto time |
|---|---:|---:|---:|---:|---:|---:|
| `sha1-60` | 296 | 253 | 252 | 202 | 237 | 1.17 |
| `sha256-64` | 432 | 417 | 412 | 148 | 154 | 1.04 |
| `sha256-1m` | 3,560,845 | 2,793,355 | 2,750,000 | 294 | 375 | 1.27 |
| `pbkdf2-4096` | 1,823,500 | 1,587,100 | – | – | – | 1.15 |

Commands:

```sh
bench/build.sh
bench/run.sh
OPENSSL_armcap=0 bench/run.sh
```

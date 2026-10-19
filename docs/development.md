# Development

The package is Goose with no C, so it builds wherever the Goose compiler does. The
tests have run on macOS on arm64, and on Linux on x64 in Docker's `python:3.12` image
with its gcc 14.2.0, emulated on the same Mac (2026-10-19). Linux on arm64 hasn't run:
the release has no archive for it, so `scripts/goose.sh` builds Goose from source
there, which needs CMake. Windows hasn't run either. The benchmark has run on macOS
only.

## Toolchain

Use the Goose pin in [README.md](../README.md#pins). The scripts also need:

| Tool | Requirement | Purpose |
|---|---|---|
| C compiler | what `goose build` finds: clang or gcc | `goose build` and `bench/ref.c` |
| Python | 3, standard library alone | `test/run_tests.py` and `scripts/against_python.py` |
| OpenSSL | 3 or later, headers and `libcrypto`, and its `openssl` command | the benchmark's reference |
| POSIX shell, curl, shasum or sha256sum | on `PATH` | test and benchmark scripts |

macOS's own `/usr/bin/openssl` is LibreSSL, and macOS has no OpenSSL headers:
`bench/build.sh` uses Homebrew's `openssl` when `brew` is on `PATH`. Exact hardware,
OS, compiler, and library versions for measured runs belong in [results](results.md).

## Set up the Goose compiler

The scripts use the Goose release pinned in `scripts/goose.sh` (`v2026.2`, 2026-10-16)
unless told otherwise. The first script that needs the compiler downloads the release's
archive for the platform from
[the releases page](https://github.com/aardappel/goose/releases), checks its SHA-256,
and unpacks it into `build/goose-v2026.2-<platform>/`; later runs reuse it. Run
`scripts/goose.sh` to fetch it and print the compiler's path. The release needs the Xcode
command line tools on macOS and the C library's headers on Linux
([Goose's README](https://github.com/aardappel/goose#install-a-release)). Linux on arm64
has no release archive, so there the script builds the release's tag from source, as
`GOOSE_COMMIT` does below.

Use another compiler only when the release can't do the job, such as a Goose fix not
yet released. Two settings choose one, and the first set wins:

| Setting | Compiler |
|---|---|
| `GOOSE` | The binary it names, such as a checkout's build or a release unpacked elsewhere. |
| `GOOSE_COMMIT` | That commit of `aardappel/goose`, cloned into `build/goose-<commit>/` and built with CMake (and Ninja if installed) on first use. Give the full 40-hex hash; a shallow fetch can't resolve an abbreviated one. |

`GOOSE_ASSET` picks another platform's release archive, such as `linux-x64` for a
container on macOS that shares the checkout.

To build a checkout by hand, clone it anywhere and point `GOOSE` at its build. Don't edit
it (`AGENTS.md`):

```sh
git clone -o upstream https://github.com/aardappel/goose.git ../goose
cmake -S ../goose -B ../goose/build-macos -DCMAKE_BUILD_TYPE=Release
cmake --build ../goose/build-macos -j
export GOOSE=$PWD/../goose/build-macos/goose
```

### Move the pin

The pin is a Goose release. At the start of a session, check for a newer one:

```sh
gh release list -R aardappel/goose --limit 3
```

To move to a new release, update `goose_release` and the four archives' SHA-256 sums in
`scripts/goose.sh`. GitHub lists each asset's digest:

```sh
gh api repos/aardappel/goose/releases/tags/<tag> -q '.assets[] | "\(.name) \(.digest)"'
```

Then run `scripts/test.sh`, and update the pin in `README.md` and this page's release
paths, in its own commit with the tests and results that still hold. Packages imported
by the same program must use a compatible Goose pin, since the program builds with one
compiler.

Pin a commit only when a Goose fix that this repository needs isn't in a release yet:
set `GOOSE_COMMIT` in the scripts' environment, record the commit in the README's Pins
table with the reason, and return to a release once one includes the fix.

## Test

`scripts/test.sh` runs everything, and stops at the first step that fails:

1. `goose check` of each root file under `test/` and `bench/`.
2. `test/run_tests.py`: the tests under `test/<area>/` ([Run the tests](#run-the-tests)).
   `test/sha/` checks SHA-1 and SHA-256 against NIST's examples, `"abc"`, the 448-bit
   message, and a million `a`; SHA-1 against RFC 6455's handshake example, through
   stdlib's `base64`; and streaming SHA-256 against one-shot, with the million `a` fed
   in pieces of 1 to 4,096 bytes, and inputs of 0 to 200 bytes split in two at every
   offset and fed a byte at a time. `test/hmac/` checks RFC 4231's cases 1 to 7, and
   case 7 split at every offset from one keyed state. `test/pbkdf2/` checks RFC 7914's
   two PBKDF2-HMAC-SHA-256 vectors, and that a 40-byte key is a prefix of a 64-byte
   one. `test/compare/` checks `equal` on equal slices, a difference at either end,
   and different lengths.
3. `goose build` of `test/digest.goose` into `build/digest`, a command that answers
   each line of its input: an operation and its arguments in hexadecimal.
4. `scripts/against_python.py`: 300 random cases of each operation through
   `build/digest`, checked against Python's `hashlib` and `hmac`. The cases are SHA-1,
   SHA-256, and HMAC-SHA-256 one-shot; SHA-256 and HMAC-SHA-256 streamed, with the input
   cut at up to 4 random offsets; PBKDF2 at 1 to 99 iterations and 1 to 99 bytes, and
   once at 4,096; and `equal` on slices that are equal, differ in one bit, or differ in
   length. Input lengths cluster around one and two 64-byte blocks, and some run to
   100 KB; key lengths cluster around 32 and 64 bytes, and run to 200. The seed is 0;
   `--cases N` and `--seed S` change them.

```sh
scripts/test.sh
```

`GOOSE`, `GOOSE_COMMIT`, and `CC` pass through to `test/run_tests.py`. It took about
5 s on an M1 Pro (2026-10-19).

### Run the tests

Each test is a program, `test/<area>/<name>.goose`, that prints what it checks and
asserts it; `test/expected/<name>.out` holds what it must print. Each digest a test
checks is written in the test as the published value, and `check::digest` prints it
with what was expected when they differ. The shared helpers are `test/check.goose`,
which a test imports with `import check from "../";`. Names are unique across areas,
since `test/expected/` is one directory. `test/run_tests.py` is Python 3 with its
standard library alone:

```sh
python3 test/run_tests.py                 # every test
python3 test/run_tests.py sha             # one area
python3 test/run_tests.py hmac_sha256     # one test; names and areas can be mixed
python3 test/run_tests.py -O0 pbkdf2      # one level only
python3 test/run_tests.py --bless sha1    # rewrite test/expected/sha1.out from its output
```

It compiles the tests to C in batches, a `goose --multi-test` run per batch, at `-O0`
and `-O2`; builds each with the C compiler `goose toolchain` names (`CC` chooses
another, as for `goose build --cc`), against the Goose runtime compiled once per level,
into `build/test/`; and runs each as a process of its own, in parallel (`-j N`). A run
passes when its stdout is the `.out` file, and it exits 0. Where
`test/expected/<name>.aborts` exists, the run must exit nonzero instead, and each
nonblank line of `test/expected/<name>.stderr` must occur in its stderr. A file with
`// error: <message>` lines is a fixture that must not compile: `goose --check` must
exit 1, with each message in its diagnostics. The runner reports every failure, with a
diff for a changed output, then a count per area, and exits 1 if any test failed; `-v`
lists the passes too.

After a change that alters what a test prints, run it with `--bless`, and read the diff
of `test/expected/` before committing it: blessing writes whatever the program printed
at the first level it ran, provided it exited as expected. A digest in a golden file
must be the published one, never only what the code printed.

To try one case by hand:

```sh
echo "sha256 616263" | build/digest   # SHA-256 of "abc"
```

The release's `stdlib/` under `build/` needs no `exclude`: the repository has no
`.build` sidecar, and the tests pass with the release unpacked there (2026-10-19).

## Benchmark

`bench/build.sh` builds, into `build/bench/`:

- `bench`, from `bench/bench.goose`, which times one case with the package.
- `ref`, from `bench/ref.c`, which times the same case with OpenSSL's libcrypto through
  the same loop: `EVP_Digest` with the digest fetched once, as `openssl speed` does,
  and `PKCS5_PBKDF2_HMAC`. `OPENSSL` names the prefix it builds against.
- `openssl`, a link to that prefix's `openssl` command.

`bench/run.sh` then prints a Markdown table: for each case, the package's, `ref`'s, and
for the hashes `openssl speed -evp`'s nanoseconds a call, median and best of `RUNS`
runs (default 10), and the median as MB/s. The cases are SHA-1 of 60 bytes, SHA-256 of
64 bytes and of 1 MB, and 32 bytes of PBKDF2-HMAC-SHA-256 at 4,096 iterations; `CASES`
picks some.

```sh
bench/build.sh
bench/run.sh
OPENSSL_armcap=0 bench/run.sh      # OpenSSL without the ARMv8 SHA instructions
CASES=pbkdf2-4096 RUNS=20 bench/run.sh
```

`OPENSSL_armcap=0` turns off OpenSSL's use of the CPU's SHA instructions on arm64, so
it runs general-purpose code as the package does; on x86-64, `OPENSSL_ia32cap` does the
same with another mask. `bench/build.sh` ends by naming the Goose compiler, the
OpenSSL, and the C compiler it built with. Record them with the results, with what
`AGENTS.md` asks for, under the machine's own section of [results](results.md). When a
new run replaces a machine's latest, move the old one to
[results-history.md](results-history.md).

## Traps

- OpenSSL 3 and later fetch the digest again on every `EVP_Digest` call given
  `EVP_sha256()` or `EVP_sha1()`: that made `ref` 50 to 70 percent slower than
  `openssl speed` on 60 and 64 bytes. Fetch it once with `EVP_MD_fetch`, as
  `bench/ref.c` does.
- `bench/run.sh` sets `LC_ALL=C`: under a locale with a decimal comma, such as
  `et_EE`, awk prints `10,2` and the table's numbers break.
- `args()`, `read_line`, `clock_ns`, and the other `os` functions need `import os;`
  even though their names are global.
- An `as` the compiler finds redundant, such as `x as f64` where an `i64` converts
  implicitly, is a warning; the scripts don't fail on it, so read `goose build`'s
  output.

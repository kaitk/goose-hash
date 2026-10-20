# Agent rules

This file holds what every session needs. `agents/` holds the rules for particular work:
read the file a task touches before starting it.

## Project context

- What this is, its API, and its pin: `README.md`
- How it works, for users: `docs/architecture.md`; the decisions, each dated with its
  reason and trade-off: `docs/decisions.md`. A new decision goes in `docs/decisions.md`,
  and `docs/architecture.md` changes with it where it describes what changed
- Toolchain, local setup, tests, and benchmarks: `docs/development.md`
- Goose compiler and stdlib gaps found here: `docs/goose-findings.md`
- Benchmark results: `docs/results.md` (each machine's latest run) and
  `docs/results-history.md` (earlier runs and each change's measurements)
- What the package follows, and from where: `docs/credits.md`
- Task tracking: `tasks/` (see `tasks/README.md`)
- The algorithms: FIPS 180-4 (SHA-1, SHA-256), RFC 2104 (HMAC), and RFC 8018
  (PBKDF2), with test vectors from NIST's examples, RFC 4231 (HMAC-SHA-256), RFC 7914
  (PBKDF2-HMAC-SHA-256), and RFC 6455 (the WebSocket handshake). RFC 6070's vectors
  are PBKDF2-HMAC-SHA-1's, which the package doesn't have.
- The C shim's instructions: Arm's ACLE (`arm_neon.h`, `vsha256hq_u32` and kin) and
  Intel's intrinsics (`immintrin.h`, `_mm_sha256rnds2_epu32` and kin); the rounds
  follow the public-domain SHA-Intrinsics (`docs/credits.md`).
- Its users: goose-http's WebSocket handshake (SHA-1, its task 054), goose-postgres's
  SCRAM-SHA-256 login, and a JWT verifier in goose-rest (HMAC-SHA-256).
- The language: the pinned Goose release's `docs/tutorial.md`, `docs/goose_spec.md`,
  `docs/stdlib.md`, and `docs/packages.md` (how a package declares the packages it
  needs). Read the tutorial before writing Goose; check the spec before assuming a
  feature exists.
- Rules for particular work: `agents/goose.md` (writing Goose, and recording what it
  lacks), `agents/writing.md` (docs, tasks, and prose reviews), and
  `agents/benchmarks.md` (measuring, and reporting a number)

Follow the recorded decisions; if a change contradicts one, update the doc in the same
change or ask first. Record setup facts and environment traps you find (paths, shell
quirks, compilers) in `docs/development.md`, not
in agent memory, so the next agent finds them. Docs and commits name no people: date a
decision or a request (`(2026-10-19)`), and describe the setup without anyone's machine,
checkouts, or home directory. What only one machine needs, such as where its Goose
checkout is, goes in `CLAUDE.local.md`, which git ignores. Benchmark results still name
their machine.

## Keep changes small

Agreed 2026-10-20.

- Meet the requirement's intent with the least machinery. Before building around an
  obstacle, ask whether the requirement needs it at all.
- When a fix starts to need a new public API, a change in another repository, or a
  workaround, stop. Say so in a sentence or two, and propose the simpler reading of the
  requirement.
- A feature's cost falls on the programs that use it: no fields in shared structs and no
  code on shared paths for one feature.
- When a design makes other code compensate (a fallback, a rescan, a special case
  elsewhere), change the design rather than add the compensation.
- Add a setting, a check, or a script option only for a failure that isn't visible
  already.

## Working beside other agents

Several agents work in these repositories at once (2026-10-20).

- Work in a worktree of your own, on a branch from `origin/main`:
  `git worktree add ../<repository>-<topic> -b <topic> origin/main`. Remove it once
  the branch is merged.
- Leave alone a branch, a commit, or an uncommitted change that isn't yours, in any
  checkout. Ask if it's in your way.
- Keep logs, exports, and scratch files in a directory of your own.
- Problems with your own sandbox, such as no network or no SSH, go in your report,
  not in the docs.

## Goose

- Build with the official Goose release that `scripts/goose.sh` pins. Check for a newer
  release at the start of a session and move the pin to it. Pin a commit
  (`GOOSE_COMMIT`) only when a fix this repository needs isn't released yet, and return
  to a release once one has it (2026-10-15).
- Don't edit a Goose checkout. Work around a compiler or stdlib gap here, record it in
  `docs/goose-findings.md` as `agents/goose.md` says, and report blockers to the
  maintainer.

## Goose, and C only for what Goose can't reach

- The package is Goose, and imports no other package. Its exceptions are two C files
  built through `hash.build`, for what Goose can't reach yet
  ([decisions](docs/decisions.md), 2026-10-19). Nothing else goes in C.
  - `shim/sha.c`: SHA-1's and SHA-256's compression functions on the CPU's SHA
    instructions. The Goose rounds stay as the fallback, tested with
    `GOOSE_HASH_PORTABLE=1`, and the C goes away once Goose's stdlib has SHA on the
    instructions (task 003).
  - `shim/random.c`: the OS's secure random source, for `random_bytes`. It never
    falls back to anything weaker, and goes away once Goose's stdlib has secure random
    bytes on every platform.
- Other languages belong in `bench/` only, as the language of the library the package
  is measured against: C for OpenSSL's libcrypto.
- Hashes, MACs, and the OS's random bytes only. Ciphers, signatures, and key
  generation are OpenSSL's, through goose-tls ([decisions](docs/decisions.md)).
- Nothing branches or indexes memory on secret data. SHA and HMAC don't; comparing a
  MAC with an expected one goes through the package's constant-time compare.

## Code conventions

- Comment only where the code is hard to follow without it, and say only what a reader
  of that code needs: why, a constraint, or a contract the code can't show. Don't restate
  the next line, and don't describe earlier versions or the task that added the code;
  history belongs in git.
- Edit files with the Edit tool, not with `sed` or a Python or PowerShell script. On
  Windows, line endings differ by file type and a shell adds a layer of escaping, and
  scripted edits break on both.
- Tests follow Goose's own convention: a top-level `test/` by area, each test a small
  program whose printed output a golden file in `test/expected/` checks, `// error:`
  fixtures for code that must not compile, and a runner that batches them with
  `goose --multi-test`. Every function is checked against the published test vectors
  above, and against Python's `hashlib` and `hmac` on random inputs.

## Commits

- One lean sentence in imperative mood, at most 72 characters.
- No body, no `Co-Authored-By` or any other trailers.
- Fetch and rebase before pushing a finished commit to `origin`.

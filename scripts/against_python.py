#!/usr/bin/env python3
"""Checks the package against Python's hashlib and hmac on random inputs.

Writes N random cases of each operation to build/digest (test/digest.goose) in
one run, and fails on the first answer that differs from Python's: SHA-1,
SHA-256, and HMAC-SHA-256 one-shot, SHA-256 and HMAC-SHA-256 streamed with the
input cut at random offsets, PBKDF2-HMAC-SHA-256 at random iteration counts and
lengths, and the compare on equal slices, slices that differ in one byte, and
slices of different lengths. Lengths cluster around the 64-byte block, where
padding takes one block or two and an HMAC key is hashed first; a few inputs run
to 100 KB. Seeded for repeatability.

    scripts/against_python.py [DIGEST] [--cases N] [--seed S]
"""

import argparse
import hashlib
import hmac
import os
import random
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def length(r):
    return r.choice((0, 1, r.randrange(54, 74), r.randrange(118, 138),
                     r.randrange(300), r.randrange(100_000)))


def key_length(r):
    return r.choice((0, 1, r.randrange(30, 34), r.randrange(60, 68), r.randrange(200)))


def h(b):
    return b.hex() or "-"


def cuts(r, n):
    return sorted(r.randrange(n + 1) for _ in range(r.randrange(5)))


def cases(r, n):
    """Yields each case's line for digest and the answer Python gives."""
    for _ in range(n):
        data = r.randbytes(length(r))
        yield f"sha1 {h(data)}", hashlib.sha1(data).hexdigest()
        yield f"sha256 {h(data)}", hashlib.sha256(data).hexdigest()
        c = ",".join(map(str, cuts(r, len(data)))) or "-"
        yield f"sha256-stream {h(data)} {c}", hashlib.sha256(data).hexdigest()

        key = r.randbytes(key_length(r))
        mac = hmac.new(key, data, "sha256").hexdigest()
        yield f"hmac {h(key)} {h(data)}", mac
        yield f"hmac-stream {h(key)} {h(data)} {c}", mac

        password = r.randbytes(key_length(r))
        salt = r.randbytes(r.randrange(40))
        iterations = r.choice((1, 2, r.randrange(1, 100)))
        size = r.choice((1, 31, 32, 33, 64, r.randrange(1, 100)))
        dk = hashlib.pbkdf2_hmac("sha256", password, salt, iterations, size)
        yield f"pbkdf2 {h(password)} {h(salt)} {iterations} {size}", dk.hex()

        a = r.randbytes(r.randrange(65))
        b = bytearray(a)
        kind = r.randrange(3)
        if kind == 1 and b:
            b[r.randrange(len(b))] ^= 1 << r.randrange(8)
        elif kind == 2:
            b = b + b"\x00" if r.random() < 0.5 or not b else b[:-1]
        yield f"equal {h(a)} {h(bytes(b))}", "1" if a == bytes(b) else "0"
    # Postgres's default iteration count, as SCRAM-SHA-256 uses it.
    yield ("pbkdf2 70656e63696c 41a4ca9ce7b39d9ef3b1 4096 32",
           hashlib.pbkdf2_hmac("sha256", b"pencil", bytes.fromhex("41a4ca9ce7b39d9ef3b1"),
                               4096, 32).hex())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("digest", nargs="?", default=os.path.join(ROOT, "build", "digest"))
    ap.add_argument("--cases", type=int, default=300)
    ap.add_argument("--seed", type=int, default=0)
    args = ap.parse_args()
    lines, want = zip(*cases(random.Random(args.seed), args.cases))
    p = subprocess.run([args.digest], input="".join(l + "\n" for l in lines).encode(),
                       capture_output=True)
    if p.returncode != 0:
        sys.exit(f"digest exited {p.returncode}: {p.stderr.decode(errors='replace')}")
    got = p.stdout.decode().splitlines()
    if len(got) != len(want):
        sys.exit(f"digest answered {len(got)} cases of {len(want)}")
    for line, g, w in zip(lines, got, want):
        if g != w:
            shown = line if len(line) < 200 else line[:200] + "..."
            sys.exit(f"FAIL (seed {args.seed}) {shown}\n    got  {g}\n    want {w}")
    print(f"against_python: {len(want)} cases ok")


if __name__ == "__main__":
    main()

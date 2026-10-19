# Goose findings

Gaps, bugs, and surprises in the pinned Goose compiler and stdlib, found while building
this package. Each entry has a minimal program that shows it, the Goose commit it was
found on, the workaround used here, and whether it has been reported upstream.

The entries come in two parts: [bugs](#bugs), where Goose does something wrong or other
than its spec says, then [feature requests](#feature-requests), for what it lacks. Each
part runs from the least work a fix is expected to take to the most, so the cheap fixes
come first. The order is an estimate from reading the compiler. Place a new entry where
its expected work puts it, and delete an entry once the pin has its fix: git keeps it.

## Bugs

None yet.

## Feature requests

### Goose has no secure random bytes

Found on `v2026.2` (`3ee3871`), and still so on master at `4c58784`, 2026-10-19.
Status: open; not reported upstream.

The stdlib's one source of entropy is `random_seed() -> u64`, in `os`, "Entropy, for
seeding an rng". The runtime's `gs_os_random_u64` (`src/runtime/runtime_os.h`) reads 8 bytes
from `/dev/urandom` on POSIX. On Windows, or where `/dev/urandom` can't be opened or
read, it returns a splitmix64 mix of the clocks, the process id, and an address, with
no sign that it did. Its own comment says so:

```c
/* Entropy for seeding a PRNG (not cryptographic): /dev/urandom where there
   is one; on Windows a splitmix64 mix of the clocks, the process id and an
   address, which needs no library beyond the runtime's. */
```

That's right for seeding `rng`, but not for a nonce or a key, and no Goose code can
reach the OS's secure source on Windows. Expected: a stdlib function that fills a slice
from the OS's cryptographically secure source, `BCryptGenRandom` on Windows and
`getentropy` or `getrandom` elsewhere, and fails loudly rather than fall back.

```goose
// A SCRAM client nonce, 18 bytes that must not be predictable. On Windows its
// seed is the clocks, the process id, and an address.
import std;
import os;

fn main() {
    var r = rng { random_seed() };
    var nonce: u8[18] = [0; 18];
    for i in 18 { nonce[i] = (r.rand_u64() >> 56) as! u8; }
    print(nonce[0]);
}
```

Workaround: `hash::random_bytes` fills a slice from the OS's source through
`shim/random.c` ([decisions](decisions.md)). A fix is a small function in the runtime,
which already calls the OS in `runtime_os.h`, but it links `bcrypt` on Windows, which
the runtime avoids today; it's still far less work than the SHA instructions below.

### Goose has no SHA instructions

Found on `v2026.2` (`3ee3871`), task 001. Status: open; not reported upstream.

SHA-256 of 1 MB runs at 287 MB/s on an Apple M1 Pro, on general-purpose integer
instructions. OpenSSL runs it at 2,343 MB/s with the ARMv8 SHA-256 instructions
(`SHA256H`, `SHA256H2`, `SHA256SU0`, `SHA256SU1`), and at 375 MB/s with them turned off
([results](results.md)). x86-64 has the same kind of instructions (`SHA256RNDS2` and
its kin). Goose has no such operation and no way to ask for one: `simd fn` builds wider
versions on x86-64 only (spec §7.12), and C compilers don't turn a SHA round into
those instructions. Expected: the instructions as operations, or SHA-1 and SHA-256 in
the stdlib that use them where the CPU has them.

```goose
// Each of the 64 rounds of a block, as sha256.goose runs them: about 20
// integer operations, where SHA256H and SHA256H2 do 4 rounds each.
let t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + (g ^ (e & (f ^ g))) + k[t & 63] + x;
let t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & bb) | (c & (a | bb)));
```

Workaround: `shim/sha.c` runs the rounds on the instructions in C, through
`hash.build` and `extern fn` ([decisions](decisions.md)), with the package's Goose
rounds as the fallback. On the M1 Pro, SHA-256 of 1 MB then runs at 2,146 MB/s, and a
SCRAM login's PBKDF2 takes 0.36 ms, where the Goose rounds take 1.79 ms
([results](results.md)). The finding stays open until Goose can reach the
instructions itself (task 003), when the C goes.

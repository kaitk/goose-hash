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

Workaround: the package runs FIPS 180-4's rounds on general-purpose instructions. A
WebSocket handshake's SHA-1 then takes 0.3 µs, and a SCRAM login's PBKDF2 1.9 ms
against OpenSSL's 0.47 ms.

// What the tests under test/ share. Each check prints what it checked, for the
// test's golden file in test/expected/, then asserts it, so a failure shows
// in the output as well as in the exit code.
namespace check;

import std;

let DIGITS = "0123456789abcdef";

fn hex(data: const u8[:]) -> u8[>..] {
    var out: u8[>..] = [];
    for b in data {
        out.push(DIGITS[b >> 4]);
        out.push(DIGITS[b & 15]);
    }
    return out;
}

// Prints "label: <hex of got>", and what was expected if it differs from
// `want`, given in hexadecimal.
fn digest(label: const u8[:], got: const u8[:], want: const u8[:]) {
    let h = hex(got);
    let ok = h == want;
    if ok { print(label, ": ", h); }
    else { print(label, ": ", h, ", expected ", want, ": FAILED"); }
    assert(ok);
}

// Prints "label: ok", or "label: FAILED".
fn that(label: const u8[:], ok: bool) {
    print(label, if ok { ": ok" } else { ": FAILED" });
    assert(ok);
}

// n bytes of b.
fn repeat(b: u8, n: i64) -> u8[>..] {
    var out: u8[>..] = [];
    out.push_n(b, n);
    return out;
}

// n bytes that don't repeat with any short period: byte i is the top byte of
// a step of splitmix64, seeded.
fn noise(n: i64, seed: u64) -> u8[>..] {
    var r = rng { seed };
    var out: u8[>..] = [];
    for i in n { out.push((r.rand_u64() >> 56) as! u8); }
    return out;
}

// The OS's random bytes: they fill exactly the slice given, two draws differ,
// every byte value turns up, and an empty slice is no error. A rough check
// that the bytes come from somewhere, not a statistical test of them; each
// check fails by chance with odds below 2^-200.
import std;
import hash from "../../";
import check from "../";

// Whether every byte value occurs in data.
fn every_value(data: const u8[:]) -> bool {
    var seen: bool[256] = [false; 256];
    for b in data { seen[b] = true; }
    for s in seen { if !s { return false; } }
    return true;
}

fn main() {
    var a: u8[32] = [0; 32];
    var b: u8[32] = [0; 32];
    hash::random_bytes(a);
    hash::random_bytes(b);
    let zero: u8[32] = [0; 32];
    check::that("32 random bytes aren't all zero", a != zero);
    check::that("two draws of 32 bytes differ", a != b);

    var framed: u8[48] = [0; 48];
    hash::random_bytes(framed[8..40]);
    var outside: u8 = 0;
    for x in framed[..8] { outside |= x; }
    for x in framed[40..] { outside |= x; }
    check::that("only the slice given is filled", outside == 0 && framed[8..40] != a[..]);

    var many: u8[>..] = [];
    for i in 1000 {
        var d: u8[64] = [0; 64];
        hash::random_bytes(d);
        many.append(d);
    }
    check::that("1,000 draws of 64 bytes hold every byte value", every_value(many));

    // More than getentropy's 256 bytes a call, and than one 64 KB read.
    var big: u8[>..] = [];
    big.push_n(0, 70000);
    hash::random_bytes(big);
    var zero_runs = 0;
    var i = 0;
    while i < big.len {
        let end = if i + 256 < big.len { i + 256 } else { big.len };
        var any: u8 = 0;
        for x in big[i..end] { any |= x; }
        if any == 0 { zero_runs += 1; }
        i = end;
    }
    check::that("one draw of 70,000 bytes fills every 256-byte piece", zero_runs == 0);
    check::that("one draw of 70,000 bytes holds every byte value", every_value(big));

    var none: u8[>..] = [];
    hash::random_bytes(none);
    hash::random_bytes(a[5..5]);
    check::that("an empty slice is filled without error", none.len == 0);
}

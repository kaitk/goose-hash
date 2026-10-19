// The constant-time compare's answers: equal, unequal at each end, and of
// different lengths.
import std;
import hash from "../../";
import check from "../";

fn main() {
    let a = check::noise(32, 7);
    var b = copy(a);
    check::that("the same 32 bytes are equal", hash::equal(a, b));
    check::that("two empty slices are equal", hash::equal(a[..0], b[5..5]));
    b[0] ^= 1;
    check::that("a difference in the first byte is unequal", !hash::equal(a, b));
    b[0] ^= 1;
    b[31] ^= 0x80;
    check::that("a difference in the last byte is unequal", !hash::equal(a, b));
    check::that("a prefix is unequal", !hash::equal(a, a[..31]));
    check::that("an empty slice and a byte are unequal", !hash::equal(a[..0], a[..1]));
}

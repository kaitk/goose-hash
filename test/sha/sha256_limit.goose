// A SHA-256 stream aborts when its total would reach 2^61 bytes, before it
// hashes anything: a state set just below the limit, not real data.
import std;
import hash from "../../";
import check from "../";

fn main() {
    var s = hash::Sha256 { .. };
    s.total = 0x2000000000000000 - 2;
    hash::sha256_update(s, "a");
    check::that("2^61 - 1 bytes", s.total == 0x2000000000000000 - 1 && s.nbuf == 1);
    hash::sha256_update(s, "");
    check::that("and no more", s.total == 0x2000000000000000 - 1);
    hash::sha256_update(s, "b");
    check::that("2^61 bytes: unreachable", false);
}

// SHA-256 and HMAC-SHA-256 states that sit unaligned, after a byte in a
// packed struct, hashed through the member: the C on the SHA instructions
// must not assume its state is aligned. Built with
// -fsanitize=alignment, a misaligned access aborts (docs/development.md).
import std;
import hash from "../../";
import check from "../";

struct Tagged {
    tag: u8,
    s: hash::Sha256,
    m: hash::HmacSha256,
}

fn main() {
    let data = check::noise(1000, 7);
    var t = Tagged { tag: 1, s: hash::Sha256 { .. }, m: hash::hmac_sha256_start("key") };
    hash::sha256_update(t.s, data[..3]);
    hash::sha256_update(t.s, data[3..]);
    check::that("SHA-256 of 1,000 bytes after a byte", hash::sha256_finish(t.s) == hash::sha256(data));
    hash::hmac_sha256_update(t.m, data);
    check::that("HMAC-SHA-256 of 1,000 bytes after a byte",
                hash::hmac_sha256_finish(t.m) == hash::hmac_sha256("key", data));
    var many: Tagged[>..] = [];
    for i in 3 { many.push(Tagged { tag: i as! u8, s: hash::Sha256 { .. }, m: t.m }); }
    for i in many.len { hash::sha256_update(many[i].s, data); }
    var same = true;
    for x in many { same = same && hash::sha256_finish(x.s) == hash::sha256(data); }
    check::that("three in an array", same);
}

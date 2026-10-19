// Streaming SHA-256 against one-shot: input split in two at every offset, fed
// a byte at a time, finished and then continued, and reset.
import std;
import hash from "../../";
import check from "../";

fn main() {
    // Past two blocks, so a split lands before, in, and after each.
    let data = check::noise(200, 1);
    for n in [0, 1, 55, 56, 63, 64, 65, 119, 120, 128, 200] {
        let whole = hash::sha256(data[..n]);
        var bad = 0;
        for cut in 0..n + 1 {
            var s = hash::Sha256 { .. };
            hash::sha256_update(s, data[..cut]);
            hash::sha256_update(s, data[cut..n]);
            if hash::sha256_finish(s) != whole { bad++; }
        }
        var s = hash::Sha256 { .. };
        for b in data[..n] {
            let one: u8[1] = [b];
            hash::sha256_update(s, one);
        }
        check::that(str(n, " bytes split at each of ", n + 1, " offsets, and byte by byte"),
                    bad == 0 && hash::sha256_finish(s) == whole);
    }

    var s = hash::Sha256 { .. };
    hash::sha256_update(s, data[..70]);
    let early = hash::sha256_finish(s);
    hash::sha256_update(s, data[70..]);
    check::that("finish leaves the state to continue",
                early == hash::sha256(data[..70]) && hash::sha256_finish(s) == hash::sha256(data));
    hash::sha256_reset(s);
    hash::sha256_update(s, "abc");
    check::digest("after a reset, \"abc\"", hash::sha256_finish(s),
                  "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

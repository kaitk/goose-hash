// SHA-256 against FIPS 180-2's examples (Appendix B.1, B.2, and B.3, which
// NIST's "Examples with Intermediate Values" repeat for FIPS 180-4), one-shot
// and streamed.
import std;
import hash from "../../";
import check from "../";

fn main() {
    let abc = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    check::digest("sha256 of \"abc\"", hash::sha256("abc"), abc);
    let m448 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    let d448 = "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1";
    check::digest("sha256 of the 448-bit message", hash::sha256(m448), d448);
    let million = check::repeat('a', 1000000);
    let dm = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
    check::digest("sha256 of a million \"a\"", hash::sha256(million), dm);

    for piece in [1, 63, 64, 65, 1000, 4096] {
        var s = hash::Sha256 { .. };
        var at = 0;
        while at < million.len {
            let end = min(at + piece, million.len);
            hash::sha256_update(s, million[at..end]);
            at = end;
        }
        check::digest(str("the same, in pieces of ", piece), hash::sha256_finish(s), dm);
    }
}

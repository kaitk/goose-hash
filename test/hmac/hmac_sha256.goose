// HMAC-SHA-256 against RFC 4231's test cases 1 to 7 (§4.2 to §4.8), one-shot
// and streamed.
import std;
import hash from "../../";
import check from "../";

fn main() {
    check::digest("case 1", hash::hmac_sha256(check::repeat(0x0B, 20), "Hi There"),
                  "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    check::digest("case 2", hash::hmac_sha256("Jefe", "what do ya want for nothing?"),
                  "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    check::digest("case 3", hash::hmac_sha256(check::repeat(0xAA, 20), check::repeat(0xDD, 50)),
                  "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
    var key4: u8[>..] = [];
    for i in 1..26 { key4.push(i as! u8); }
    check::digest("case 4", hash::hmac_sha256(key4, check::repeat(0xCD, 50)),
                  "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b");
    // Truncated to 128 bits.
    let mac5 = hash::hmac_sha256(check::repeat(0x0C, 20), "Test With Truncation");
    check::digest("case 5, the first 16 bytes", mac5[..16], "a3b6167473100ee06e0c796c2955552b");
    let big_key = check::repeat(0xAA, 131);
    check::digest("case 6",
                  hash::hmac_sha256(big_key, "Test Using Larger Than Block-Size Key - Hash Key First"),
                  "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    let text7 = "This is a test using a larger than block-size key and a larger than block-size data. The key needs to be hashed before being used by the HMAC algorithm.";
    let mac7 = "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2";
    check::digest("case 7", hash::hmac_sha256(big_key, text7), mac7);

    let keyed = hash::hmac_sha256_start(big_key);
    var bad = 0;
    for cut in 0..text7.len + 1 {
        var m = keyed;
        hash::hmac_sha256_update(m, text7[..cut]);
        hash::hmac_sha256_update(m, text7[cut..]);
        if check::hex(hash::hmac_sha256_finish(m)) != mac7 { bad++; }
    }
    check::that(str("case 7 split at each of ", text7.len + 1, " offsets, from one keyed state"),
                bad == 0);
}

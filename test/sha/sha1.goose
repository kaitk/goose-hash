// SHA-1 against FIPS 180-2's examples (Appendix A.1, A.2, and A.3, which NIST's
// "Examples with Intermediate Values" repeat for FIPS 180-4), and RFC 6455's
// handshake example (§1.3).
import std;
import base64;
import hash from "../../";
import check from "../";

fn main() {
    check::digest("sha1 of \"abc\"", hash::sha1("abc"),
                  "a9993e364706816aba3e25717850c26c9cd0d89d");
    check::digest("sha1 of the 448-bit message",
                  hash::sha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
                  "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    let million = check::repeat('a', 1000000);
    check::digest("sha1 of a million \"a\"", hash::sha1(million),
                  "34aa973cd4c4daa4f61eeb2bdbad27316534016f");

    var key: u8[>..] = [];
    key.append("dGhlIHNhbXBsZSBub25jZQ==");
    key.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    var accept: u8[>..] = [];
    base64::encode(accept, hash::sha1(key));
    check::that(str("Sec-WebSocket-Accept ", accept), accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

// SHA-1 (FIPS 180-4 §6.1), one-shot: for WebSocket's handshake (RFC 6455 §4.2.2),
// which needs it, not for security, which it no longer gives.
namespace hash;

import std;

fn sha1(data: const u8[:]) -> u8[20] {
    var h: u32[5] = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0];
    let whole = data.len & ~63;
    sha1_blocks(h, data[..whole]);
    let last, n = final_blocks(data[whole..], data.len as! u64);
    sha1_blocks(h, last[..n]);
    var out: u8[20] = [0; 20];
    for v, i in h { store_be32(out, 4 * i, v); }
    return out;
}

// Takes data, a whole number of 64-byte blocks, into the state h, with the
// schedule kept 16 words at a time as in sha256_blocks.
fn sha1_blocks(state: u32[5]&, data: const u8[:]) {
    var w: u32[16] = [0; 16];
    var i = 0;
    while i + 64 <= data.len {
        let b = data[i..i + 64];
        var a = state[0];
        var bb = state[1];
        var c = state[2];
        var d = state[3];
        var e = state[4];
        for t in 80 {
            var x: u32 = 0;
            if t < 16 {
                x = load_be32(b, 4 * (t & 15));
            } else {
                x = rotl(w[(t - 3) & 15] ^ w[(t - 8) & 15] ^ w[(t - 14) & 15] ^ w[t & 15], 1);
            }
            w[t & 15] = x;
            var f: u32 = 0;
            if t < 20 {
                f = (d ^ (bb & (c ^ d))) + 0x5A827999;
            } else if t < 40 {
                f = (bb ^ c ^ d) + 0x6ED9EBA1;
            } else if t < 60 {
                f = ((bb & c) | (d & (bb | c))) + 0x8F1BBCDC;
            } else {
                f = (bb ^ c ^ d) + 0xCA62C1D6;
            }
            let tmp = rotl(a, 5) + f + e + x;
            e = d;
            d = c;
            c = rotl(bb, 30);
            bb = a;
            a = tmp;
        }
        state[0] += a;
        state[1] += bb;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        i += 64;
    }
}

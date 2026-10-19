// SHA-256 (FIPS 180-4 §6.2), one-shot and streaming.
namespace hash;

import std;

let SHA256_K: u32[64] = [
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
];

let SHA256_IV: u32[8] = [
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A, 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19,
];

// A SHA-256 computation in progress: `Sha256 { .. }` starts one. Fixed-size, so
// it can sit in a slot of an array and be copied to fork a computation.
struct Sha256 {
    h: u32[8] = SHA256_IV,
    total: u64 = 0,
    nbuf: i64 = 0,
    buf: u8[64],
}

fn sha256_reset(s: Sha256&) {
    s.h = SHA256_IV;
    s.total = 0;
    s.nbuf = 0;
}

fn sha256_update(s: Sha256&, data: const u8[:]) {
    s.total += data.len as! u64;
    var at = 0;
    if s.nbuf > 0 {
        at = min(64 - s.nbuf, data.len);
        copy_into(s.buf[s.nbuf..s.nbuf + at], data[..at]);
        s.nbuf += at;
        guard s.nbuf == 64 else { return; }
        sha256_blocks(s.h, s.buf);
        s.nbuf = 0;
    }
    let whole = (data.len - at) & ~63;
    sha256_blocks(s.h, data[at..at + whole]);
    at += whole;
    copy_into(s.buf[..data.len - at], data[at..]);
    s.nbuf = data.len - at;
}

// The digest of everything given so far. `s` is left as it was, so more input
// may follow.
fn sha256_finish(s: const Sha256&) -> u8[32] {
    var h = s.h;
    let last, n = final_blocks(s.buf[..s.nbuf], s.total);
    sha256_blocks(h, last[..n]);
    var out: u8[32] = [0; 32];
    for v, i in h { store_be32(out, 4 * i, v); }
    return out;
}

fn sha256(data: const u8[:]) -> u8[32] {
    var s = Sha256 { .. };
    sha256_update(s, data);
    return sha256_finish(s);
}

// Takes data, a whole number of 64-byte blocks, into the state h, on the CPU's
// SHA instructions where it has them (shim/sha.c), else on sha256_rounds.
fn sha256_blocks(state: u32[8]&, data: const u8[:]) {
    if !sha256_blocks_c(state, data) { sha256_rounds(state, data); }
}

extern "gh_sha256_blocks" fn sha256_blocks_c(state: u32[8]&, data: const u8[:]) -> bool;

// sha256_blocks in Goose. The message schedule is kept 16 words at a time;
// every index is a round number masked to the array, so no check is left in
// the loop.
fn sha256_rounds(state: u32[8]&, data: const u8[:]) {
    let k .= SHA256_K;
    var w: u32[16] = [0; 16];
    var i = 0;
    while i + 64 <= data.len {
        let b = data[i..i + 64];
        var a = state[0];
        var bb = state[1];
        var c = state[2];
        var d = state[3];
        var e = state[4];
        var f = state[5];
        var g = state[6];
        var h = state[7];
        for t in 64 {
            var x: u32 = 0;
            if t < 16 {
                x = load_be32(b, 4 * (t & 15));
            } else {
                let w15 = w[(t - 15) & 15];
                let w2 = w[(t - 2) & 15];
                let s0 = rotr(w15, 7) ^ rotr(w15, 18) ^ (w15 >> 3);
                let s1 = rotr(w2, 17) ^ rotr(w2, 19) ^ (w2 >> 10);
                x = w[t & 15] + s0 + w[(t - 7) & 15] + s1;
            }
            w[t & 15] = x;
            let t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + (g ^ (e & (f ^ g))) +
                     k[t & 63] + x;
            let t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & bb) | (c & (a | bb)));
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = bb;
            bb = a;
            a = t1 + t2;
        }
        state[0] += a;
        state[1] += bb;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
        i += 64;
    }
}

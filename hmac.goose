// HMAC-SHA-256 (RFC 2104) and PBKDF2-HMAC-SHA-256 (RFC 8018 §5.2).
namespace hash;

import std;

// An HMAC-SHA-256 computation in progress, from hmac_sha256_start. Like a
// Sha256, it can be copied: a copy made before any input is the key's state,
// and goes on to MAC another message under the same key.
struct HmacSha256 {
    inner: Sha256,
    outer: Sha256,
}

fn hmac_sha256_start(key: const u8[:]) -> HmacSha256 {
    var k: u8[64] = [0; 64];
    if key.len > 64 {
        let d = sha256(key);
        copy_into(k[..32], d);
    } else {
        copy_into(k[..key.len], key);
    }
    var m = HmacSha256 { inner: Sha256 { .. }, outer: Sha256 { .. } };
    var padded: u8[64] = [0; 64];
    for x, i in k { padded[i] = x ^ 0x36; }
    sha256_update(m.inner, padded);
    for x, i in k { padded[i] = x ^ 0x5C; }
    sha256_update(m.outer, padded);
    return m;
}

fn hmac_sha256_update(m: HmacSha256&, data: const u8[:]) {
    sha256_update(m.inner, data);
}

// The MAC of everything given so far. `m` is left as it was.
fn hmac_sha256_finish(m: const HmacSha256&) -> u8[32] {
    let d = sha256_finish(m.inner);
    var o = m.outer;
    sha256_update(o, d);
    return sha256_finish(o);
}

fn hmac_sha256(key: const u8[:], data: const u8[:]) -> u8[32] {
    var m = hmac_sha256_start(key);
    hmac_sha256_update(m, data);
    return hmac_sha256_finish(m);
}

// RFC 8018 §5.2's longest key: 2^32 - 1 blocks of 32 bytes, as the block
// index is 32 bits.
let PBKDF2_SHA256_MAX_BYTES: i64 = 0xFFFFFFFF * 32;

// Aborts if a key of `n` bytes is longer than PBKDF2_SHA256_MAX_BYTES.
fn pbkdf2_sha256_check_length(n: i64) {
    guard n <= PBKDF2_SHA256_MAX_BYTES else {
        abort("hash::pbkdf2_sha256: the key must be at most (2^32 - 1) * 32 bytes");
    }
}

// Fills `out` with the key PBKDF2-HMAC-SHA-256 derives from password and salt
// in `iterations` rounds, at least 1. SCRAM-SHA-256's SaltedPassword (RFC
// 7677) is 32 bytes of it.
//
// Each round after the first MACs the previous round's 32 bytes, one block
// for the inner hash and one for the outer, so it runs the compression
// function on the key's two states directly with the padding already in place.
//
// The salt is taken into a keyed state before any of `out` is written, so
// `out` may overlap `salt`.
fn pbkdf2_sha256(out: u8[:], password: const u8[:], salt: const u8[:], iterations: i64) {
    assert(iterations >= 1);
    pbkdf2_sha256_check_length(out.len);
    let key = hmac_sha256_start(password);
    var salted = key;
    hmac_sha256_update(salted, salt);
    // One 32-byte message after a 64-byte pad: 96 bytes, 768 bits.
    var msg: u8[64] = [0; 64];
    msg[32] = 0x80;
    msg[62] = 0x03;
    var index: u8[4] = [0; 4];
    var n: u32 = 0;
    var at = 0;
    while at < out.len {
        n += 1;
        store_be32(index, 0, n);
        var first = salted;
        hmac_sha256_update(first, index);
        var u = hmac_sha256_finish(first);
        var t = u;
        for r in 1..iterations {
            copy_into(msg[..32], u);
            var h = key.inner.h;
            sha256_blocks(h, msg);
            for v, i in h { store_be32(msg, 4 * i, v); }
            h = key.outer.h;
            sha256_blocks(h, msg);
            for v, i in h { store_be32(u, 4 * i, v); }
            for x, i in u { t[i] ^= x; }
        }
        let take = min(32, out.len - at);
        copy_into(out[at..at + take], t[..take]);
        at += take;
    }
}

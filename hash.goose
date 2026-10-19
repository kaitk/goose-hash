// The package's entry: SHA-1, SHA-256, HMAC-SHA-256, PBKDF2-HMAC-SHA-256, and a
// constant-time compare. Bytes in, digests out as fixed arrays.
//
// Nothing here branches on, or indexes memory with, the bytes it hashes or a
// key: only lengths, which aren't secret, steer the code.
namespace hash;

import std;
import .sha1;
import .sha256;
import .hmac;

// Whether a and b hold the same bytes, taking a time that depends on their
// lengths and not on where they differ: for checking a MAC against the one
// expected. Slices of different lengths are unequal at once.
fn equal(a: const u8[:], b: const u8[:]) -> bool {
    guard a.len == b.len else { return false; }
    var diff: u8 = 0;
    for x, i in a { diff |= x ^ b[i]; }
    return diff == 0;
}

fn rotr(x: u32, n: u32) -> u32 { x >> n | x << (32 - n) }

fn rotl(x: u32, n: u32) -> u32 { x << n | x >> (32 - n) }

fn load_be32(s: const u8[:], i: i64) -> u32 {
    let c = s[i..i + 4];
    return (c[0] as u32) << 24 | (c[1] as u32) << 16 | (c[2] as u32) << 8 | c[3];
}

fn store_be32(s: u8[:], i: i64, v: u32) {
    let c = s[i..i + 4];
    c[0] = (v >> 24) as! u8;
    c[1] = (v >> 16) as! u8;
    c[2] = (v >> 8) as! u8;
    c[3] = v as! u8;
}

// The padded end of a message of `total` bytes whose last `tail.len` bytes,
// fewer than 64, are `tail`, as SHA-1 and SHA-256 both pad it (FIPS 180-4
// §5.1.1): the blocks, and their length, 64 or 128.
fn final_blocks(tail: const u8[:], total: u64) -> u8[128], i64 {
    var out: u8[128] = [0; 128];
    copy_into(out[..tail.len], tail);
    out[tail.len] = 0x80;
    let n = if tail.len < 56 { 64 } else { 128 };
    let bits = total << 3;
    for j in 8 { out[n - 1 - j] = (bits >> (8 * j)) as! u8; }
    return out, n;
}

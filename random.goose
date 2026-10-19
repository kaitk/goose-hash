// Bytes from the OS's cryptographically secure random source: for a nonce,
// such as SCRAM's client nonce, a salt, or a key.
namespace hash;

import std;

// Fills out with bytes from the OS's cryptographically secure random source
// (shim/random.c): BCryptGenRandom on Windows, getentropy elsewhere. If the
// source fails, the program aborts rather than go on with weaker bytes.
fn random_bytes(out: u8[:]) {
    guard random_bytes_c(out) else { abort("hash::random_bytes: the OS's secure random source failed"); }
}

extern "gh_random_bytes" fn random_bytes_c(out: u8[:]) -> bool;

// PBKDF2-HMAC-SHA-256 against RFC 7914's vectors (§11), and lengths that end
// inside a block.
import std;
import hash from "../../";
import check from "../";

fn main() {
    var dk: u8[64] = [0; 64];
    hash::pbkdf2_sha256(dk, "passwd", "salt", 1);
    check::digest("\"passwd\", \"salt\", 1 iteration, 64 bytes", dk,
                  "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
    hash::pbkdf2_sha256(dk, "Password", "NaCl", 80000);
    check::digest("\"Password\", \"NaCl\", 80000 iterations, 64 bytes", dk,
                  "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d");
    // A prefix of a longer key, since each 32-byte block stands alone.
    var short: u8[40] = [0; 40];
    hash::pbkdf2_sha256(short, "Password", "NaCl", 80000);
    check::that("its first 40 bytes alone", short == dk[..40]);

    // The output overlapping the salt: every block comes from the salt as it
    // was before any output was written.
    var same: u8[64] = [0; 64];
    copy_into(same[..4], "salt");
    hash::pbkdf2_sha256(same, "passwd", same[..4], 1);
    check::digest("its own first 4 bytes, \"salt\", as the salt, 1 iteration", same,
                  "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
    var apart: u8[64] = [0; 64];
    hash::pbkdf2_sha256(apart, "Password", "NaCl", 3);
    copy_into(same[..4], "NaCl");
    hash::pbkdf2_sha256(same, "Password", same[..4], 3);
    check::that("the same, 3 iterations, against a salt of its own", same == apart);
}

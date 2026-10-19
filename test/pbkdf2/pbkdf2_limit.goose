// pbkdf2_sha256 aborts on a key longer than (2^32 - 1) * 32 bytes (RFC 8018
// §5.2) before it derives anything; no slice that long can be made, so the
// check is called directly.
import std;
import hash from "../../";
import check from "../";

fn main() {
    hash::pbkdf2_sha256_check_length(0xFFFFFFFF * 32);
    check::that("(2^32 - 1) * 32 bytes", true);
    hash::pbkdf2_sha256_check_length(0xFFFFFFFF * 32 + 1);
    check::that("one byte more: unreachable", false);
}

// sha1 checks a message's length with sha_check_length, which aborts at 2^61
// bytes; no slice that long can be made, so the check is called directly.
import std;
import hash from "../../";
import check from "../";

fn main() {
    hash::sha_check_length(0, 0x2000000000000000 - 1);
    check::that("2^61 - 1 bytes", true);
    hash::sha_check_length(0x2000000000000000 - 64, 63);
    check::that("2^61 - 64 bytes and 63 more", true);
    hash::sha_check_length(0, 0x2000000000000000);
    check::that("2^61 bytes: unreachable", false);
}

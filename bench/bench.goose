// Times one of the benchmark's cases with the package, as bench/ref.c does with
// OpenSSL's libcrypto: RUNS runs, each of a fixed number of calls on the same
// input, timed together.
//
//   bench CASE RUNS
//
// CASE is sha1-60 (a WebSocket key and its GUID), sha256-64, sha256-1m (2^20
// bytes), or pbkdf2-4096 (32 bytes of PBKDF2-HMAC-SHA-256 at 4,096 iterations,
// Postgres's default). Prints the library, the case, the bytes a call takes, and
// the median and best nanoseconds a call took, one line, space-separated.
import std;
import os;
import hash from "../";

fn main() {
    let a = args();
    guard a.len == 3 else {
        write_stderr("usage: bench sha1-60|sha256-64|sha256-1m|pbkdf2-4096 RUNS\n");
        exit(2);
    }
    let name = a[1];
    let runs, rok = parse_int(a[2]);
    guard rok && runs > 0 else { write_stderr("bad RUNS\n"); exit(2); }
    var size = 0;
    var calls = 0;
    if name == "sha1-60" { size = 60; calls = 1000000; }
    else if name == "sha256-64" { size = 64; calls = 1000000; }
    else if name == "sha256-1m" { size = 1 << 20; calls = 200; }
    else if name == "pbkdf2-4096" { size = 8; calls = 100; }
    else { write_stderr("unknown case\n"); exit(2); }
    var data: u8[>..] = [];
    for i in size { data.push((i * 131 + 7) as! u8); }
    let salt = "saltSALTsaltSALT";

    // Each result's first byte goes into sink, which is printed, so no call is
    // left out.
    var sink: u8 = 0;
    var times: f64[>..] = [];
    for r in runs {
        let t = clock_ns();
        if name == "sha1-60" {
            for i in calls { sink ^= hash::sha1(data)[0]; }
        } else if name == "pbkdf2-4096" {
            var dk: u8[32] = [0; 32];
            for i in calls {
                hash::pbkdf2_sha256(dk, data, salt, 4096);
                sink ^= dk[0];
            }
        } else {
            for i in calls { sink ^= hash::sha256(data)[0]; }
        }
        times.push((clock_ns() - t) as f64 / calls);
    }
    sort(times);
    var line: u8[>..] = [];
    format(line, "goose ", name, " ", size, " ");
    format_flt(line, times[runs / 2], 1);
    line.push(' ');
    format_flt(line, times[0], 1);
    print(line);
    if sink == 0x5A { write_stderr(""); }
}

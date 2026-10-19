// Computes what each line of stdin asks for and prints it in hexadecimal, a
// line per case, for scripts/against_python.py to check against Python's
// hashlib and hmac. A line is an operation and its fields, separated by
// spaces, each byte string in hexadecimal and "-" for an empty one:
//
//   sha1 DATA                  sha256 DATA            hmac KEY DATA
//   sha256-stream DATA CUTS    hmac-stream KEY DATA CUTS
//   pbkdf2 PASSWORD SALT ITERATIONS LENGTH            equal A B
//   path
//
// CUTS is a comma-separated list of the offsets where the input is split
// across update calls. equal prints 1 or 0. path prints which rounds SHA-1 and
// SHA-256 run on: "instructions", the CPU's, or "portable", the package's own.
import std;
import os;
import hash from "../";

fn hex_value(c: u8) -> i64 {
    return match c {
        '0'..':' => c - '0',
        'a'..'g' => (c - 'a') as i64 + 10,
        _ => abort("bad hex digit"),
    };
}

fn unhex(s: const u8[:]) -> u8[>..] {
    var out: u8[>..] = [];
    guard s != "-" else { return out; }
    var i = 0;
    while i + 1 < s.len {
        out.push((hex_value(s[i]) * 16 + hex_value(s[i + 1])) as! u8);
        i += 2;
    }
    return out;
}

fn number(s: const u8[:]) -> i64 {
    let n, ok = parse_int(s);
    guard ok else { abort("bad number"); }
    return n;
}

fn cuts(s: const u8[:]) -> i64[>..] {
    var out: i64[>..] = [];
    if s != "-" { each_split(s, ',') { out.push(number(it)); }; }
    return out;
}

fn emit(out: u8[>..]&, d: const u8[:]) {
    let digits = "0123456789abcdef";
    for b in d {
        out.push(digits[b >> 4]);
        out.push(digits[b & 15]);
    }
    out.push('\n');
}

fn main() {
    var line: u8[>..] = [];
    var out: u8[>..] = [];
    while read_line(line) {
        let f = split(line, ' ');
        let op = f[0];
        if op == "sha1" {
            emit(out, hash::sha1(unhex(f[1])));
        } else if op == "sha256" {
            emit(out, hash::sha256(unhex(f[1])));
        } else if op == "sha256-stream" {
            let data = unhex(f[1]);
            var s = hash::Sha256 { .. };
            var at = 0;
            for c in cuts(f[2]) {
                hash::sha256_update(s, data[at..c]);
                at = c;
            }
            hash::sha256_update(s, data[at..]);
            emit(out, hash::sha256_finish(s));
        } else if op == "hmac" {
            emit(out, hash::hmac_sha256(unhex(f[1]), unhex(f[2])));
        } else if op == "hmac-stream" {
            let data = unhex(f[2]);
            var m = hash::hmac_sha256_start(unhex(f[1]));
            var at = 0;
            for c in cuts(f[3]) {
                hash::hmac_sha256_update(m, data[at..c]);
                at = c;
            }
            hash::hmac_sha256_update(m, data[at..]);
            emit(out, hash::hmac_sha256_finish(m));
        } else if op == "pbkdf2" {
            var dk: u8[>..] = [];
            dk.push_n(0, number(f[4]));
            hash::pbkdf2_sha256(dk, unhex(f[1]), unhex(f[2]), number(f[3]));
            emit(out, dk);
        } else if op == "equal" {
            out.append(if hash::equal(unhex(f[1]), unhex(f[2])) { "1\n" } else { "0\n" });
        } else if op == "path" {
            out.append(if hash::sha_instructions() { "instructions\n" } else { "portable\n" });
        } else {
            abort(str("unknown operation ", op));
        }
        line.clear();
    }
    write_stdout(out);
}

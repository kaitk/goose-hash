#!/bin/sh
# Builds what bench/run.sh runs, into build/bench/: bench/bench.goose with goose
# build, and bench/ref.c against the OpenSSL installed under OPENSSL, a prefix
# with include/, lib/, and bin/openssl (default: Homebrew's openssl where brew
# is on PATH, else /usr). Prints the Goose compiler and the OpenSSL it used, for
# the results' record.
#
# GOOSE or GOOSE_COMMIT names the compiler (default: the pinned release,
# scripts/goose.sh), CC the C compiler (default: cc).
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
. "$root/scripts/goose.sh"
goose=$GOOSE
cc=${CC:-cc}
if [ -z "${OPENSSL:-}" ]; then
    OPENSSL=$(brew --prefix openssl 2>/dev/null) || OPENSSL=/usr
fi
out=$root/build/bench
mkdir -p "$out"

"$goose" build ${CC:+--cc "$CC"} -o "$out/bench" "$root/bench/bench.goose"
"$cc" -O2 -o "$out/ref" "$root/bench/ref.c" -I"$OPENSSL/include" -L"$OPENSSL/lib" -lcrypto
ln -sf "$OPENSSL/bin/openssl" "$out/openssl"

echo "built $out with Goose $(goose_version), $("$out/openssl" version), $("$cc" --version | head -n 1)"

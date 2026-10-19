#!/bin/sh
# Runs every test this repository has, and fails on the first step that fails:
# the typecheck of each root file under test/ and bench/, the tests under
# test/<area>/ (test/run_tests.py, which reports all of its failures), then
# test/digest.goose built with goose build and its answers checked against
# Python's hashlib and hmac on random inputs (scripts/against_python.py).
#
# GOOSE or GOOSE_COMMIT names the compiler (default: the pinned release,
# scripts/goose.sh), and CC the C compiler for goose build (default: the one
# goose build finds).
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
. "$root/scripts/goose.sh"
goose=$GOOSE
case $(uname -s) in
MINGW* | MSYS*) exe=.exe ;;
*) exe= ;;
esac

step() {
    echo
    echo "== $*"
}

for f in "$root"/test/*.goose "$root"/bench/*.goose; do
    [ -f "$f" ] || continue
    step "check ${f#"$root"/}"
    "$goose" check "$f"
done

step "test/run_tests.py"
python3 "$root/test/run_tests.py"

step "build test/digest.goose"
mkdir -p "$root/build"
"$goose" build ${CC:+--cc "$CC"} -o "$root/build/digest" "$root/test/digest.goose"

step "against_python.py"
python3 "$root/scripts/against_python.py" "$root/build/digest$exe"

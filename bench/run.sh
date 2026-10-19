#!/bin/sh
# Times each case with the package (bench), OpenSSL's libcrypto through the
# same loop (ref), and, for the hashes, `openssl speed -evp` at the case's size,
# and prints a Markdown table: nanoseconds a call, median and best of RUNS runs,
# and the median as MB/s (10^6 bytes of input a second). bench/build.sh builds
# what it runs.
#
# CASES (default: all four) picks among sha1-60, sha256-64, sha256-1m, and
# pbkdf2-4096. RUNS (default 10). SECONDS_EACH (default 1), the length of each
# `openssl speed` run.
set -eu
# awk prints decimal commas under some locales.
export LC_ALL=C

root=$(cd "$(dirname "$0")/.." && pwd)
out=$root/build/bench
runs=${RUNS:-10}
cases=${CASES:-"sha1-60 sha256-64 sha256-1m pbkdf2-4096"}
seconds=${SECONDS_EACH:-1}

row() { # case bytes lib median best
    awk -v c="$1" -v n="$2" -v l="$3" -v m="$4" -v b="$5" 'BEGIN {
        rate = c ~ /^pbkdf2/ ? "–" : sprintf("%.0f", n / m * 1e3)
        printf "| %s | %s | %.0f | %.0f | %s |\n", c, l, m, b, rate
    }'
}

echo "| Case | Library | ns a call, median | ns a call, best | MB/s, median |"
echo "|---|---|---:|---:|---:|"
for c in $cases; do
    for prog in bench ref; do
        # shellcheck disable=SC2046
        set -- $("$out/$prog" "$c" "$runs")
        row "$c" "$3" "$1" "$4" "$5"
    done
    case $c in
    sha1-*) md=sha1 ;;
    sha256-*) md=sha256 ;;
    *) continue ;;
    esac
    case $c in *-1m) bytes=1048576 ;; *) bytes=${c#*-} ;; esac
    # Each run's rate, in bytes a second, as ns a call; then median and best.
    i=0
    rates=$(while [ $i -lt "$runs" ]; do
        "$out/openssl" speed -evp $md -bytes "$bytes" -seconds "$seconds" -mr 2>/dev/null |
            awk -F: -v n="$bytes" '/^\+F:/ { printf "%.1f\n", n / $4 * 1e9 }'
        i=$((i + 1))
    done | sort -n)
    median=$(echo "$rates" | awk -v r="$runs" 'NR == int(r / 2) + 1')
    best=$(echo "$rates" | head -n 1)
    row "$c" "$bytes" "openssl speed" "$median" "$best"
done

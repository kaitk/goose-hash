# Benchmarks

- Every number comes with how it was measured: machine, OS, compiler, command, OpenSSL
  version, and run count. A result without them doesn't go in a doc.
- OpenSSL runs as installed, with the CPU's SHA instructions, and again with them off
  (`OPENSSL_armcap=0` on arm64), so the gap the instructions make shows apart from the
  rest. Losses are recorded as plainly as wins.

// Times one of the benchmark's cases with OpenSSL's libcrypto, the way
// bench/bench.goose does with the package: RUNS runs, each of a fixed number of
// calls on the same input, timed together. The hashes go through EVP_Digest,
// the one-shot call `openssl speed -evp` times too, and PBKDF2 through
// PKCS5_PBKDF2_HMAC. Needs OpenSSL 3 or later.
//
//   ref CASE RUNS
//
// Prints the same line as bench/bench.goose, under the name openssl.
#include <openssl/evp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

static int by_value(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: ref sha1-60|sha256-64|sha256-1m|pbkdf2-4096 RUNS\n");
        return 2;
    }
    const char *name = argv[1];
    int runs = atoi(argv[2]);
    long size, calls;
    const char *md_name = "SHA256";
    if (!strcmp(name, "sha1-60")) {
        size = 60, calls = 1000000, md_name = "SHA1";
    } else if (!strcmp(name, "sha256-64")) {
        size = 64, calls = 1000000;
    } else if (!strcmp(name, "sha256-1m")) {
        size = 1 << 20, calls = 200;
    } else if (!strcmp(name, "pbkdf2-4096")) {
        size = 8, calls = 100;
    } else {
        fprintf(stderr, "unknown case\n");
        return 2;
    }
    if (runs <= 0) {
        fprintf(stderr, "bad RUNS\n");
        return 2;
    }
    // Fetched once, as `openssl speed` does: EVP_sha256() would be fetched
    // again on each call.
    EVP_MD *md = EVP_MD_fetch(NULL, md_name, NULL);
    unsigned char *data = malloc(size);
    for (long i = 0; i < size; i++) data[i] = (unsigned char)(i * 131 + 7);
    const char *salt = "saltSALTsaltSALT";
    unsigned char out[32];
    unsigned char sink = 0;
    double *times = malloc(runs * sizeof *times);
    for (int r = 0; r < runs; r++) {
        double t = now_ns();
        for (long i = 0; i < calls; i++) {
            if (size == 8) {
                PKCS5_PBKDF2_HMAC((const char *)data, size, (const unsigned char *)salt,
                                  strlen(salt), 4096, md, 32, out);
            } else {
                EVP_Digest(data, size, out, NULL, md, NULL);
            }
            sink ^= out[0];
        }
        times[r] = (now_ns() - t) / calls;
    }
    qsort(times, runs, sizeof *times, by_value);
    printf("openssl %s %ld %.1f %.1f\n", name, size, times[runs / 2], times[0]);
    if (sink == 0x5A) fputs("", stderr);
    return 0;
}

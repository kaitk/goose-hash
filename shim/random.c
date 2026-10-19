/* The C under random.goose: bytes from the OS's cryptographically secure
   random source, which Goose can't reach on every platform yet
   (docs/decisions.md, "The OS's secure random source, in C for now").

   gh_random_bytes fills the slice and returns true, or returns false when the
   OS's source fails; the caller then aborts. It never falls back to anything
   weaker than the OS's source:

   - Windows: BCryptGenRandom with the system-preferred RNG (bcrypt.lib).
   - Elsewhere: getentropy, at most 256 bytes a call, as POSIX 2024 allows.
     Where the C library has it but the kernel lacks getrandom (glibc on Linux
     before 3.17), it fails with ENOSYS, and only then is /dev/urandom read. */
#if defined(__linux__)
#define _DEFAULT_SOURCE 1 /* O_CLOEXEC, getentropy */
#endif

#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/random.h>
#endif

#pragma pack(push, 1)
typedef struct { uint8_t *data; int64_t len; } gh_mut_bytes;
#pragma pack(pop)
_Static_assert(sizeof(gh_mut_bytes) == sizeof(void *) + 8, "a Goose slice is { data, len }, packed");

#if !defined(_WIN32)
static bool from_urandom(uint8_t *p, int64_t n) {
    int fd;
    do { fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC); } while (fd < 0 && errno == EINTR);
    if (fd < 0) return false;
    while (n > 0) {
        ssize_t r = read(fd, p, n > 65536 ? 65536 : (size_t)n);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) {
            close(fd);
            return false;
        }
        p += r;
        n -= r;
    }
    close(fd);
    return true;
}
#endif

bool gh_random_bytes(gh_mut_bytes out) {
    uint8_t *p = out.data;
    int64_t n = out.len;
    if (n < 0) return false;
#if defined(_WIN32)
    while (n > 0) {
        ULONG k = n > 0x40000000 ? 0x40000000 : (ULONG)n;
        if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, p, k, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) return false;
        p += k;
        n -= k;
    }
    return true;
#else
    while (n > 0) {
        size_t k = n > 256 ? 256 : (size_t)n;
        if (getentropy(p, k) != 0) return errno == ENOSYS && from_urandom(p, n);
        p += k;
        n -= k;
    }
    return true;
#endif
}

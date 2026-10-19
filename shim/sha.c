/* The C under sha1.goose and sha256.goose: SHA-1's and SHA-256's compression
   functions on the CPU's SHA instructions, ARMv8's and x86-64's SHA-NI, which
   Goose can't express yet (docs/decisions.md, "The CPU's SHA instructions, in
   C for now"). The rounds follow Jeffrey Walton's SHA-Intrinsics, public
   domain (docs/credits.md).

   gh_sha1_blocks and gh_sha256_blocks take a whole number of 64-byte blocks
   into the state and return true, or return false, having done nothing, where
   the CPU lacks the instructions or GOOSE_HASH_PORTABLE asks for Goose's own
   rounds; the caller then runs those. Each function that uses the
   instructions carries a target attribute, so no build needs -march, and runs
   only once the CPU has been checked.

   The state is copied in and out with memcpy: it sits in a packed Goose
   struct, so it may not be aligned. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS /* getenv */
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__aarch64__) || defined(_M_ARM64)
#define GH_ARM 1
#include <arm_neon.h>
#if defined(_MSC_VER) && !defined(__clang__)
#define GH_TARGET
#elif defined(__clang__)
#define GH_TARGET __attribute__((target("sha2")))
#else
#define GH_TARGET __attribute__((target("+sha2")))
#endif
#if defined(__linux__)
#include <sys/auxv.h>
#ifndef HWCAP_SHA1
#define HWCAP_SHA1 (1 << 5)
#endif
#ifndef HWCAP_SHA2
#define HWCAP_SHA2 (1 << 6)
#endif
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#elif defined(__x86_64__) || defined(_M_X64)
#define GH_X86 1
#include <immintrin.h>
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#define GH_TARGET
#else
#include <cpuid.h>
#define GH_TARGET __attribute__((target("sha,sse4.1")))
#endif
#endif

#pragma pack(push, 1)
typedef struct { const uint8_t *data; int64_t len; } gh_bytes;
#pragma pack(pop)
_Static_assert(sizeof(gh_bytes) == sizeof(void *) + 8, "a Goose slice is { data, len }, packed");

/* The check runs once; threads that race to it compute the same answer. */
#if defined(_MSC_VER) && !defined(__clang__)
static volatile int32_t cached; /* 0: not checked yet, 1: no, 2: yes */
#define LOAD(p) (*(p))
#define STORE(p, v) (*(p) = (v))
#else
static int32_t cached;
#define LOAD(p) __atomic_load_n(p, __ATOMIC_RELAXED)
#define STORE(p, v) __atomic_store_n(p, v, __ATOMIC_RELAXED)
#endif

static bool cpu_has_sha(void) {
#if defined(GH_ARM) && defined(__APPLE__)
    return true; /* every Apple Silicon CPU has SHA-1 and SHA-256 */
#elif defined(GH_ARM) && defined(__linux__)
    unsigned long hw = getauxval(AT_HWCAP);
    return (hw & HWCAP_SHA1) && (hw & HWCAP_SHA2);
#elif defined(GH_ARM) && defined(_WIN32)
    return IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE) != 0;
#elif defined(GH_X86)
    unsigned int a, b, c, d;
#if defined(_MSC_VER) && !defined(__clang__)
    int r[4];
    __cpuid(r, 0);
    if (r[0] < 7) return false;
    __cpuidex(r, 1, 0);
    c = (unsigned int)r[2];
    __cpuidex(r, 7, 0);
    b = (unsigned int)r[1];
    (void)a;
    (void)d;
#else
    if (__get_cpuid_max(0, 0) < 7) return false;
    __cpuid_count(1, 0, a, b, c, d);
    unsigned int c1 = c;
    __cpuid_count(7, 0, a, b, c, d);
    c = c1;
#endif
    bool ssse3 = c & (1u << 9), sse41 = c & (1u << 19), sha = b & (1u << 29);
    return ssse3 && sse41 && sha;
#else
    return false;
#endif
}

bool gh_sha_instructions(void) {
    int32_t v = LOAD(&cached);
    if (v == 0) {
        const char *off = getenv("GOOSE_HASH_PORTABLE");
        bool portable = off && *off && strcmp(off, "0") != 0;
        v = !portable && cpu_has_sha() ? 2 : 1;
        STORE(&cached, v);
    }
    return v == 2;
}

#if defined(GH_ARM)

static const uint32_t K[] = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};

/* Four big-endian words of a block. */
GH_TARGET static inline uint32x4_t load_be(const uint8_t *p) {
    return vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(p)));
}

GH_TARGET static void sha256_arm(uint32_t *s, const uint8_t *data, int64_t blocks) {
    uint32x4_t STATE0, STATE1, ABEF_SAVE, CDGH_SAVE;
    uint32x4_t MSG0, MSG1, MSG2, MSG3;
    uint32x4_t TMP0, TMP1, TMP2;
    uint32_t st[8];
    memcpy(st, s, sizeof st);
    STATE0 = vld1q_u32(&st[0]);
    STATE1 = vld1q_u32(&st[4]);
    for (; blocks > 0; blocks--, data += 64) {
        ABEF_SAVE = STATE0;
        CDGH_SAVE = STATE1;
        MSG0 = load_be(data);
        MSG1 = load_be(data + 16);
        MSG2 = load_be(data + 32);
        MSG3 = load_be(data + 48);
        TMP0 = vaddq_u32(MSG0, vld1q_u32(&K[0x00]));
        /* Rounds 0-3 */
        MSG0 = vsha256su0q_u32(MSG0, MSG1);
        TMP2 = STATE0;
        TMP1 = vaddq_u32(MSG1, vld1q_u32(&K[0x04]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
        MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);
        /* Rounds 4-7 */
        MSG1 = vsha256su0q_u32(MSG1, MSG2);
        TMP2 = STATE0;
        TMP0 = vaddq_u32(MSG2, vld1q_u32(&K[0x08]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
        MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);
        /* Rounds 8-11 */
        MSG2 = vsha256su0q_u32(MSG2, MSG3);
        TMP2 = STATE0;
        TMP1 = vaddq_u32(MSG3, vld1q_u32(&K[0x0c]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
        MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);
        /* Rounds 12-15 */
        MSG3 = vsha256su0q_u32(MSG3, MSG0);
        TMP2 = STATE0;
        TMP0 = vaddq_u32(MSG0, vld1q_u32(&K[0x10]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
        MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);
        /* Rounds 16-19 */
        MSG0 = vsha256su0q_u32(MSG0, MSG1);
        TMP2 = STATE0;
        TMP1 = vaddq_u32(MSG1, vld1q_u32(&K[0x14]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
        MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);
        /* Rounds 20-23 */
        MSG1 = vsha256su0q_u32(MSG1, MSG2);
        TMP2 = STATE0;
        TMP0 = vaddq_u32(MSG2, vld1q_u32(&K[0x18]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
        MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);
        /* Rounds 24-27 */
        MSG2 = vsha256su0q_u32(MSG2, MSG3);
        TMP2 = STATE0;
        TMP1 = vaddq_u32(MSG3, vld1q_u32(&K[0x1c]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
        MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);
        /* Rounds 28-31 */
        MSG3 = vsha256su0q_u32(MSG3, MSG0);
        TMP2 = STATE0;
        TMP0 = vaddq_u32(MSG0, vld1q_u32(&K[0x20]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
        MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);
        /* Rounds 32-35 */
        MSG0 = vsha256su0q_u32(MSG0, MSG1);
        TMP2 = STATE0;
        TMP1 = vaddq_u32(MSG1, vld1q_u32(&K[0x24]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
        MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);
        /* Rounds 36-39 */
        MSG1 = vsha256su0q_u32(MSG1, MSG2);
        TMP2 = STATE0;
        TMP0 = vaddq_u32(MSG2, vld1q_u32(&K[0x28]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
        MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);
        /* Rounds 40-43 */
        MSG2 = vsha256su0q_u32(MSG2, MSG3);
        TMP2 = STATE0;
        TMP1 = vaddq_u32(MSG3, vld1q_u32(&K[0x2c]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
        MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);
        /* Rounds 44-47 */
        MSG3 = vsha256su0q_u32(MSG3, MSG0);
        TMP2 = STATE0;
        TMP0 = vaddq_u32(MSG0, vld1q_u32(&K[0x30]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
        MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);
        /* Rounds 48-51 */
        TMP2 = STATE0;
        TMP1 = vaddq_u32(MSG1, vld1q_u32(&K[0x34]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
        /* Rounds 52-55 */
        TMP2 = STATE0;
        TMP0 = vaddq_u32(MSG2, vld1q_u32(&K[0x38]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
        /* Rounds 56-59 */
        TMP2 = STATE0;
        TMP1 = vaddq_u32(MSG3, vld1q_u32(&K[0x3c]));
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
        /* Rounds 60-63 */
        TMP2 = STATE0;
        STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
        STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
        /* Combine state */
        STATE0 = vaddq_u32(STATE0, ABEF_SAVE);
        STATE1 = vaddq_u32(STATE1, CDGH_SAVE);
    }
    vst1q_u32(&st[0], STATE0);
    vst1q_u32(&st[4], STATE1);
    memcpy(s, st, sizeof st);
}

GH_TARGET static void sha1_arm(uint32_t *s, const uint8_t *data, int64_t blocks) {
    uint32x4_t ABCD, ABCD_SAVED;
    uint32x4_t TMP0, TMP1;
    uint32x4_t MSG0, MSG1, MSG2, MSG3;
    uint32_t E0, E0_SAVED, E1;
    uint32_t st[5];
    memcpy(st, s, sizeof st);
    ABCD = vld1q_u32(&st[0]);
    E0 = st[4];
    for (; blocks > 0; blocks--, data += 64) {
        ABCD_SAVED = ABCD;
        E0_SAVED = E0;
        MSG0 = load_be(data);
        MSG1 = load_be(data + 16);
        MSG2 = load_be(data + 32);
        MSG3 = load_be(data + 48);
        TMP0 = vaddq_u32(MSG0, vdupq_n_u32(0x5A827999));
        TMP1 = vaddq_u32(MSG1, vdupq_n_u32(0x5A827999));
        /* Rounds 0-3 */
        E1 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1cq_u32(ABCD, E0, TMP0);
        TMP0 = vaddq_u32(MSG2, vdupq_n_u32(0x5A827999));
        MSG0 = vsha1su0q_u32(MSG0, MSG1, MSG2);
        /* Rounds 4-7 */
        E0 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1cq_u32(ABCD, E1, TMP1);
        TMP1 = vaddq_u32(MSG3, vdupq_n_u32(0x5A827999));
        MSG0 = vsha1su1q_u32(MSG0, MSG3);
        MSG1 = vsha1su0q_u32(MSG1, MSG2, MSG3);
        /* Rounds 8-11 */
        E1 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1cq_u32(ABCD, E0, TMP0);
        TMP0 = vaddq_u32(MSG0, vdupq_n_u32(0x5A827999));
        MSG1 = vsha1su1q_u32(MSG1, MSG0);
        MSG2 = vsha1su0q_u32(MSG2, MSG3, MSG0);
        /* Rounds 12-15 */
        E0 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1cq_u32(ABCD, E1, TMP1);
        TMP1 = vaddq_u32(MSG1, vdupq_n_u32(0x6ED9EBA1));
        MSG2 = vsha1su1q_u32(MSG2, MSG1);
        MSG3 = vsha1su0q_u32(MSG3, MSG0, MSG1);
        /* Rounds 16-19 */
        E1 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1cq_u32(ABCD, E0, TMP0);
        TMP0 = vaddq_u32(MSG2, vdupq_n_u32(0x6ED9EBA1));
        MSG3 = vsha1su1q_u32(MSG3, MSG2);
        MSG0 = vsha1su0q_u32(MSG0, MSG1, MSG2);
        /* Rounds 20-23 */
        E0 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1pq_u32(ABCD, E1, TMP1);
        TMP1 = vaddq_u32(MSG3, vdupq_n_u32(0x6ED9EBA1));
        MSG0 = vsha1su1q_u32(MSG0, MSG3);
        MSG1 = vsha1su0q_u32(MSG1, MSG2, MSG3);
        /* Rounds 24-27 */
        E1 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1pq_u32(ABCD, E0, TMP0);
        TMP0 = vaddq_u32(MSG0, vdupq_n_u32(0x6ED9EBA1));
        MSG1 = vsha1su1q_u32(MSG1, MSG0);
        MSG2 = vsha1su0q_u32(MSG2, MSG3, MSG0);
        /* Rounds 28-31 */
        E0 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1pq_u32(ABCD, E1, TMP1);
        TMP1 = vaddq_u32(MSG1, vdupq_n_u32(0x6ED9EBA1));
        MSG2 = vsha1su1q_u32(MSG2, MSG1);
        MSG3 = vsha1su0q_u32(MSG3, MSG0, MSG1);
        /* Rounds 32-35 */
        E1 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1pq_u32(ABCD, E0, TMP0);
        TMP0 = vaddq_u32(MSG2, vdupq_n_u32(0x8F1BBCDC));
        MSG3 = vsha1su1q_u32(MSG3, MSG2);
        MSG0 = vsha1su0q_u32(MSG0, MSG1, MSG2);
        /* Rounds 36-39 */
        E0 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1pq_u32(ABCD, E1, TMP1);
        TMP1 = vaddq_u32(MSG3, vdupq_n_u32(0x8F1BBCDC));
        MSG0 = vsha1su1q_u32(MSG0, MSG3);
        MSG1 = vsha1su0q_u32(MSG1, MSG2, MSG3);
        /* Rounds 40-43 */
        E1 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1mq_u32(ABCD, E0, TMP0);
        TMP0 = vaddq_u32(MSG0, vdupq_n_u32(0x8F1BBCDC));
        MSG1 = vsha1su1q_u32(MSG1, MSG0);
        MSG2 = vsha1su0q_u32(MSG2, MSG3, MSG0);
        /* Rounds 44-47 */
        E0 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1mq_u32(ABCD, E1, TMP1);
        TMP1 = vaddq_u32(MSG1, vdupq_n_u32(0x8F1BBCDC));
        MSG2 = vsha1su1q_u32(MSG2, MSG1);
        MSG3 = vsha1su0q_u32(MSG3, MSG0, MSG1);
        /* Rounds 48-51 */
        E1 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1mq_u32(ABCD, E0, TMP0);
        TMP0 = vaddq_u32(MSG2, vdupq_n_u32(0x8F1BBCDC));
        MSG3 = vsha1su1q_u32(MSG3, MSG2);
        MSG0 = vsha1su0q_u32(MSG0, MSG1, MSG2);
        /* Rounds 52-55 */
        E0 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1mq_u32(ABCD, E1, TMP1);
        TMP1 = vaddq_u32(MSG3, vdupq_n_u32(0xCA62C1D6));
        MSG0 = vsha1su1q_u32(MSG0, MSG3);
        MSG1 = vsha1su0q_u32(MSG1, MSG2, MSG3);
        /* Rounds 56-59 */
        E1 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1mq_u32(ABCD, E0, TMP0);
        TMP0 = vaddq_u32(MSG0, vdupq_n_u32(0xCA62C1D6));
        MSG1 = vsha1su1q_u32(MSG1, MSG0);
        MSG2 = vsha1su0q_u32(MSG2, MSG3, MSG0);
        /* Rounds 60-63 */
        E0 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1pq_u32(ABCD, E1, TMP1);
        TMP1 = vaddq_u32(MSG1, vdupq_n_u32(0xCA62C1D6));
        MSG2 = vsha1su1q_u32(MSG2, MSG1);
        MSG3 = vsha1su0q_u32(MSG3, MSG0, MSG1);
        /* Rounds 64-67 */
        E1 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1pq_u32(ABCD, E0, TMP0);
        TMP0 = vaddq_u32(MSG2, vdupq_n_u32(0xCA62C1D6));
        MSG3 = vsha1su1q_u32(MSG3, MSG2);
        MSG0 = vsha1su0q_u32(MSG0, MSG1, MSG2);
        /* Rounds 68-71 */
        E0 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1pq_u32(ABCD, E1, TMP1);
        TMP1 = vaddq_u32(MSG3, vdupq_n_u32(0xCA62C1D6));
        MSG0 = vsha1su1q_u32(MSG0, MSG3);
        /* Rounds 72-75 */
        E1 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1pq_u32(ABCD, E0, TMP0);
        /* Rounds 76-79 */
        E0 = vsha1h_u32(vgetq_lane_u32(ABCD, 0));
        ABCD = vsha1pq_u32(ABCD, E1, TMP1);
        /* Combine state */
        E0 += E0_SAVED;
        ABCD = vaddq_u32(ABCD_SAVED, ABCD);
    }
    vst1q_u32(&st[0], ABCD);
    st[4] = E0;
    memcpy(s, st, sizeof st);
}

#define SHA256_BLOCKS sha256_arm
#define SHA1_BLOCKS sha1_arm

#elif defined(GH_X86)

GH_TARGET static void sha256_x86(uint32_t *s, const uint8_t *data, int64_t blocks) {
    __m128i STATE0, STATE1;
    __m128i MSG, TMP;
    __m128i MSG0, MSG1, MSG2, MSG3;
    __m128i ABEF_SAVE, CDGH_SAVE;
    const __m128i MASK = _mm_set_epi64x(0x0c0d0e0f08090a0bULL, 0x0405060700010203ULL);
    TMP = _mm_loadu_si128((const __m128i *)&s[0]);
    STATE1 = _mm_loadu_si128((const __m128i *)&s[4]);
    TMP = _mm_shuffle_epi32(TMP, 0xB1);          /* CDAB */
    STATE1 = _mm_shuffle_epi32(STATE1, 0x1B);    /* EFGH */
    STATE0 = _mm_alignr_epi8(TMP, STATE1, 8);    /* ABEF */
    STATE1 = _mm_blend_epi16(STATE1, TMP, 0xF0); /* CDGH */
    for (; blocks > 0; blocks--, data += 64) {
        ABEF_SAVE = STATE0;
        CDGH_SAVE = STATE1;
        /* Rounds 0-3 */
        MSG = _mm_loadu_si128((const __m128i*) (data+0));
        MSG0 = _mm_shuffle_epi8(MSG, MASK);
        MSG = _mm_add_epi32(MSG0, _mm_set_epi64x(0xE9B5DBA5B5C0FBCFULL, 0x71374491428A2F98ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        /* Rounds 4-7 */
        MSG1 = _mm_loadu_si128((const __m128i*) (data+16));
        MSG1 = _mm_shuffle_epi8(MSG1, MASK);
        MSG = _mm_add_epi32(MSG1, _mm_set_epi64x(0xAB1C5ED5923F82A4ULL, 0x59F111F13956C25BULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG0 = _mm_sha256msg1_epu32(MSG0, MSG1);
        /* Rounds 8-11 */
        MSG2 = _mm_loadu_si128((const __m128i*) (data+32));
        MSG2 = _mm_shuffle_epi8(MSG2, MASK);
        MSG = _mm_add_epi32(MSG2, _mm_set_epi64x(0x550C7DC3243185BEULL, 0x12835B01D807AA98ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG1 = _mm_sha256msg1_epu32(MSG1, MSG2);
        /* Rounds 12-15 */
        MSG3 = _mm_loadu_si128((const __m128i*) (data+48));
        MSG3 = _mm_shuffle_epi8(MSG3, MASK);
        MSG = _mm_add_epi32(MSG3, _mm_set_epi64x(0xC19BF1749BDC06A7ULL, 0x80DEB1FE72BE5D74ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG3, MSG2, 4);
        MSG0 = _mm_add_epi32(MSG0, TMP);
        MSG0 = _mm_sha256msg2_epu32(MSG0, MSG3);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG2 = _mm_sha256msg1_epu32(MSG2, MSG3);
        /* Rounds 16-19 */
        MSG = _mm_add_epi32(MSG0, _mm_set_epi64x(0x240CA1CC0FC19DC6ULL, 0xEFBE4786E49B69C1ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG0, MSG3, 4);
        MSG1 = _mm_add_epi32(MSG1, TMP);
        MSG1 = _mm_sha256msg2_epu32(MSG1, MSG0);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG3 = _mm_sha256msg1_epu32(MSG3, MSG0);
        /* Rounds 20-23 */
        MSG = _mm_add_epi32(MSG1, _mm_set_epi64x(0x76F988DA5CB0A9DCULL, 0x4A7484AA2DE92C6FULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG1, MSG0, 4);
        MSG2 = _mm_add_epi32(MSG2, TMP);
        MSG2 = _mm_sha256msg2_epu32(MSG2, MSG1);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG0 = _mm_sha256msg1_epu32(MSG0, MSG1);
        /* Rounds 24-27 */
        MSG = _mm_add_epi32(MSG2, _mm_set_epi64x(0xBF597FC7B00327C8ULL, 0xA831C66D983E5152ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG2, MSG1, 4);
        MSG3 = _mm_add_epi32(MSG3, TMP);
        MSG3 = _mm_sha256msg2_epu32(MSG3, MSG2);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG1 = _mm_sha256msg1_epu32(MSG1, MSG2);
        /* Rounds 28-31 */
        MSG = _mm_add_epi32(MSG3, _mm_set_epi64x(0x1429296706CA6351ULL,  0xD5A79147C6E00BF3ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG3, MSG2, 4);
        MSG0 = _mm_add_epi32(MSG0, TMP);
        MSG0 = _mm_sha256msg2_epu32(MSG0, MSG3);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG2 = _mm_sha256msg1_epu32(MSG2, MSG3);
        /* Rounds 32-35 */
        MSG = _mm_add_epi32(MSG0, _mm_set_epi64x(0x53380D134D2C6DFCULL, 0x2E1B213827B70A85ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG0, MSG3, 4);
        MSG1 = _mm_add_epi32(MSG1, TMP);
        MSG1 = _mm_sha256msg2_epu32(MSG1, MSG0);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG3 = _mm_sha256msg1_epu32(MSG3, MSG0);
        /* Rounds 36-39 */
        MSG = _mm_add_epi32(MSG1, _mm_set_epi64x(0x92722C8581C2C92EULL, 0x766A0ABB650A7354ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG1, MSG0, 4);
        MSG2 = _mm_add_epi32(MSG2, TMP);
        MSG2 = _mm_sha256msg2_epu32(MSG2, MSG1);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG0 = _mm_sha256msg1_epu32(MSG0, MSG1);
        /* Rounds 40-43 */
        MSG = _mm_add_epi32(MSG2, _mm_set_epi64x(0xC76C51A3C24B8B70ULL, 0xA81A664BA2BFE8A1ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG2, MSG1, 4);
        MSG3 = _mm_add_epi32(MSG3, TMP);
        MSG3 = _mm_sha256msg2_epu32(MSG3, MSG2);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG1 = _mm_sha256msg1_epu32(MSG1, MSG2);
        /* Rounds 44-47 */
        MSG = _mm_add_epi32(MSG3, _mm_set_epi64x(0x106AA070F40E3585ULL, 0xD6990624D192E819ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG3, MSG2, 4);
        MSG0 = _mm_add_epi32(MSG0, TMP);
        MSG0 = _mm_sha256msg2_epu32(MSG0, MSG3);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG2 = _mm_sha256msg1_epu32(MSG2, MSG3);
        /* Rounds 48-51 */
        MSG = _mm_add_epi32(MSG0, _mm_set_epi64x(0x34B0BCB52748774CULL, 0x1E376C0819A4C116ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG0, MSG3, 4);
        MSG1 = _mm_add_epi32(MSG1, TMP);
        MSG1 = _mm_sha256msg2_epu32(MSG1, MSG0);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        MSG3 = _mm_sha256msg1_epu32(MSG3, MSG0);
        /* Rounds 52-55 */
        MSG = _mm_add_epi32(MSG1, _mm_set_epi64x(0x682E6FF35B9CCA4FULL, 0x4ED8AA4A391C0CB3ULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG1, MSG0, 4);
        MSG2 = _mm_add_epi32(MSG2, TMP);
        MSG2 = _mm_sha256msg2_epu32(MSG2, MSG1);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        /* Rounds 56-59 */
        MSG = _mm_add_epi32(MSG2, _mm_set_epi64x(0x8CC7020884C87814ULL, 0x78A5636F748F82EEULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        TMP = _mm_alignr_epi8(MSG2, MSG1, 4);
        MSG3 = _mm_add_epi32(MSG3, TMP);
        MSG3 = _mm_sha256msg2_epu32(MSG3, MSG2);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        /* Rounds 60-63 */
        MSG = _mm_add_epi32(MSG3, _mm_set_epi64x(0xC67178F2BEF9A3F7ULL, 0xA4506CEB90BEFFFAULL));
        STATE1 = _mm_sha256rnds2_epu32(STATE1, STATE0, MSG);
        MSG = _mm_shuffle_epi32(MSG, 0x0E);
        STATE0 = _mm_sha256rnds2_epu32(STATE0, STATE1, MSG);
        /* Combine state  */
        STATE0 = _mm_add_epi32(STATE0, ABEF_SAVE);
        STATE1 = _mm_add_epi32(STATE1, CDGH_SAVE);
    }
    TMP = _mm_shuffle_epi32(STATE0, 0x1B);       /* FEBA */
    STATE1 = _mm_shuffle_epi32(STATE1, 0xB1);    /* DCHG */
    STATE0 = _mm_blend_epi16(TMP, STATE1, 0xF0); /* DCBA */
    STATE1 = _mm_alignr_epi8(STATE1, TMP, 8);    /* ABEF */
    _mm_storeu_si128((__m128i *)&s[0], STATE0);
    _mm_storeu_si128((__m128i *)&s[4], STATE1);
}

GH_TARGET static void sha1_x86(uint32_t *s, const uint8_t *data, int64_t blocks) {
    __m128i ABCD, ABCD_SAVE, E0, E0_SAVE, E1;
    __m128i MSG0, MSG1, MSG2, MSG3;
    const __m128i MASK = _mm_set_epi64x(0x0001020304050607ULL, 0x08090a0b0c0d0e0fULL);
    uint32_t e;
    memcpy(&e, &s[4], 4);
    ABCD = _mm_loadu_si128((const __m128i *)s);
    E0 = _mm_set_epi32((int)e, 0, 0, 0);
    ABCD = _mm_shuffle_epi32(ABCD, 0x1B);
    for (; blocks > 0; blocks--, data += 64) {
        ABCD_SAVE = ABCD;
        E0_SAVE = E0;
        /* Rounds 0-3 */
        MSG0 = _mm_loadu_si128((const __m128i*)(data + 0));
        MSG0 = _mm_shuffle_epi8(MSG0, MASK);
        E0 = _mm_add_epi32(E0, MSG0);
        E1 = ABCD;
        ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 0);
        /* Rounds 4-7 */
        MSG1 = _mm_loadu_si128((const __m128i*)(data + 16));
        MSG1 = _mm_shuffle_epi8(MSG1, MASK);
        E1 = _mm_sha1nexte_epu32(E1, MSG1);
        E0 = ABCD;
        ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 0);
        MSG0 = _mm_sha1msg1_epu32(MSG0, MSG1);
        /* Rounds 8-11 */
        MSG2 = _mm_loadu_si128((const __m128i*)(data + 32));
        MSG2 = _mm_shuffle_epi8(MSG2, MASK);
        E0 = _mm_sha1nexte_epu32(E0, MSG2);
        E1 = ABCD;
        ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 0);
        MSG1 = _mm_sha1msg1_epu32(MSG1, MSG2);
        MSG0 = _mm_xor_si128(MSG0, MSG2);
        /* Rounds 12-15 */
        MSG3 = _mm_loadu_si128((const __m128i*)(data + 48));
        MSG3 = _mm_shuffle_epi8(MSG3, MASK);
        E1 = _mm_sha1nexte_epu32(E1, MSG3);
        E0 = ABCD;
        MSG0 = _mm_sha1msg2_epu32(MSG0, MSG3);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 0);
        MSG2 = _mm_sha1msg1_epu32(MSG2, MSG3);
        MSG1 = _mm_xor_si128(MSG1, MSG3);
        /* Rounds 16-19 */
        E0 = _mm_sha1nexte_epu32(E0, MSG0);
        E1 = ABCD;
        MSG1 = _mm_sha1msg2_epu32(MSG1, MSG0);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 0);
        MSG3 = _mm_sha1msg1_epu32(MSG3, MSG0);
        MSG2 = _mm_xor_si128(MSG2, MSG0);
        /* Rounds 20-23 */
        E1 = _mm_sha1nexte_epu32(E1, MSG1);
        E0 = ABCD;
        MSG2 = _mm_sha1msg2_epu32(MSG2, MSG1);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 1);
        MSG0 = _mm_sha1msg1_epu32(MSG0, MSG1);
        MSG3 = _mm_xor_si128(MSG3, MSG1);
        /* Rounds 24-27 */
        E0 = _mm_sha1nexte_epu32(E0, MSG2);
        E1 = ABCD;
        MSG3 = _mm_sha1msg2_epu32(MSG3, MSG2);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 1);
        MSG1 = _mm_sha1msg1_epu32(MSG1, MSG2);
        MSG0 = _mm_xor_si128(MSG0, MSG2);
        /* Rounds 28-31 */
        E1 = _mm_sha1nexte_epu32(E1, MSG3);
        E0 = ABCD;
        MSG0 = _mm_sha1msg2_epu32(MSG0, MSG3);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 1);
        MSG2 = _mm_sha1msg1_epu32(MSG2, MSG3);
        MSG1 = _mm_xor_si128(MSG1, MSG3);
        /* Rounds 32-35 */
        E0 = _mm_sha1nexte_epu32(E0, MSG0);
        E1 = ABCD;
        MSG1 = _mm_sha1msg2_epu32(MSG1, MSG0);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 1);
        MSG3 = _mm_sha1msg1_epu32(MSG3, MSG0);
        MSG2 = _mm_xor_si128(MSG2, MSG0);
        /* Rounds 36-39 */
        E1 = _mm_sha1nexte_epu32(E1, MSG1);
        E0 = ABCD;
        MSG2 = _mm_sha1msg2_epu32(MSG2, MSG1);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 1);
        MSG0 = _mm_sha1msg1_epu32(MSG0, MSG1);
        MSG3 = _mm_xor_si128(MSG3, MSG1);
        /* Rounds 40-43 */
        E0 = _mm_sha1nexte_epu32(E0, MSG2);
        E1 = ABCD;
        MSG3 = _mm_sha1msg2_epu32(MSG3, MSG2);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 2);
        MSG1 = _mm_sha1msg1_epu32(MSG1, MSG2);
        MSG0 = _mm_xor_si128(MSG0, MSG2);
        /* Rounds 44-47 */
        E1 = _mm_sha1nexte_epu32(E1, MSG3);
        E0 = ABCD;
        MSG0 = _mm_sha1msg2_epu32(MSG0, MSG3);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 2);
        MSG2 = _mm_sha1msg1_epu32(MSG2, MSG3);
        MSG1 = _mm_xor_si128(MSG1, MSG3);
        /* Rounds 48-51 */
        E0 = _mm_sha1nexte_epu32(E0, MSG0);
        E1 = ABCD;
        MSG1 = _mm_sha1msg2_epu32(MSG1, MSG0);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 2);
        MSG3 = _mm_sha1msg1_epu32(MSG3, MSG0);
        MSG2 = _mm_xor_si128(MSG2, MSG0);
        /* Rounds 52-55 */
        E1 = _mm_sha1nexte_epu32(E1, MSG1);
        E0 = ABCD;
        MSG2 = _mm_sha1msg2_epu32(MSG2, MSG1);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 2);
        MSG0 = _mm_sha1msg1_epu32(MSG0, MSG1);
        MSG3 = _mm_xor_si128(MSG3, MSG1);
        /* Rounds 56-59 */
        E0 = _mm_sha1nexte_epu32(E0, MSG2);
        E1 = ABCD;
        MSG3 = _mm_sha1msg2_epu32(MSG3, MSG2);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 2);
        MSG1 = _mm_sha1msg1_epu32(MSG1, MSG2);
        MSG0 = _mm_xor_si128(MSG0, MSG2);
        /* Rounds 60-63 */
        E1 = _mm_sha1nexte_epu32(E1, MSG3);
        E0 = ABCD;
        MSG0 = _mm_sha1msg2_epu32(MSG0, MSG3);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 3);
        MSG2 = _mm_sha1msg1_epu32(MSG2, MSG3);
        MSG1 = _mm_xor_si128(MSG1, MSG3);
        /* Rounds 64-67 */
        E0 = _mm_sha1nexte_epu32(E0, MSG0);
        E1 = ABCD;
        MSG1 = _mm_sha1msg2_epu32(MSG1, MSG0);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 3);
        MSG3 = _mm_sha1msg1_epu32(MSG3, MSG0);
        MSG2 = _mm_xor_si128(MSG2, MSG0);
        /* Rounds 68-71 */
        E1 = _mm_sha1nexte_epu32(E1, MSG1);
        E0 = ABCD;
        MSG2 = _mm_sha1msg2_epu32(MSG2, MSG1);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 3);
        MSG3 = _mm_xor_si128(MSG3, MSG1);
        /* Rounds 72-75 */
        E0 = _mm_sha1nexte_epu32(E0, MSG2);
        E1 = ABCD;
        MSG3 = _mm_sha1msg2_epu32(MSG3, MSG2);
        ABCD = _mm_sha1rnds4_epu32(ABCD, E0, 3);
        /* Rounds 76-79 */
        E1 = _mm_sha1nexte_epu32(E1, MSG3);
        E0 = ABCD;
        ABCD = _mm_sha1rnds4_epu32(ABCD, E1, 3);
        /* Combine state */
        E0 = _mm_sha1nexte_epu32(E0, E0_SAVE);
        ABCD = _mm_add_epi32(ABCD, ABCD_SAVE);
    }
    ABCD = _mm_shuffle_epi32(ABCD, 0x1B);
    _mm_storeu_si128((__m128i *)s, ABCD);
    e = (uint32_t)_mm_extract_epi32(E0, 3);
    memcpy(&s[4], &e, 4);
}

#define SHA256_BLOCKS sha256_x86
#define SHA1_BLOCKS sha1_x86

#endif

bool gh_sha256_blocks(uint32_t *state, gh_bytes data) {
#if defined(SHA256_BLOCKS)
    if (!gh_sha_instructions()) return false;
    SHA256_BLOCKS(state, data.data, data.len / 64);
    return true;
#else
    (void)state;
    (void)data;
    return false;
#endif
}

bool gh_sha1_blocks(uint32_t *state, gh_bytes data) {
#if defined(SHA1_BLOCKS)
    if (!gh_sha_instructions()) return false;
    SHA1_BLOCKS(state, data.data, data.len / 64);
    return true;
#else
    (void)state;
    (void)data;
    return false;
#endif
}

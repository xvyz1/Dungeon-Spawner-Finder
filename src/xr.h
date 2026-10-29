#pragma once
#include <stdint.h>

typedef struct { uint64_t lo, hi; } Xr;

static inline uint64_t xr_mix(uint64_t z) { z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
static inline void xr_set(Xr* r, int64_t s) { uint64_t l = (uint64_t) s ^ 0x6A09E667F3BCC909ULL, h = l + 0x9E3779B97F4A7C15ULL; r->lo = xr_mix(l); r->hi = xr_mix(h); }
static inline uint64_t xr_raw(Xr* r) {
    uint64_t l = r->lo, m = r->hi, s = l + m, n = ((s << 17) | (s >> 47)) + l;
    m ^= l;
    r->lo = ((l << 49) | (l >> 15)) ^ m ^ (m << 21);
    r->hi = (m << 28) | (m >> 36);
    return n;
}
static inline int xr_next(Xr* r, int bits) { return (int) (xr_raw(r) >> (64 - bits)); }
static inline int xr_int(Xr* r, int b) {
    if ((b & (b - 1)) == 0) return (int) (((int64_t) b * (int64_t) xr_next(r, 31)) >> 31);
    int v, m;
    do { v = xr_next(r, 31); m = v % b; } while ((int32_t) ((uint32_t) v - (uint32_t) m + (uint32_t) (b - 1)) < 0);
    return m;
}
static inline int64_t xr_long(Xr* r) { int a = xr_next(r, 32), c = xr_next(r, 32); return (int64_t) (((uint64_t) (int64_t) a << 32) + (uint64_t) (int64_t) c); }
static inline int64_t xr_deco(Xr* r, int64_t world, int bx, int bz) {
    xr_set(r, world);
    int64_t a = xr_long(r) | 1, b = xr_long(r) | 1;
    int64_t d = (int64_t) (((uint64_t) (int64_t) bx * (uint64_t) a + (uint64_t) (int64_t) bz * (uint64_t) b)) ^ world;
    xr_set(r, d);
    return d;
}

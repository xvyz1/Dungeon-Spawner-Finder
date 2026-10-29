#include "dsf.h"

#define SILVER 0x6A09E667F3BCC909ULL
#define GOLDEN 0x9E3779B97F4A7C15ULL

static uint64_t rotl64(uint64_t v, int k) { return (v << k) | (v >> (64 - k)); }
static uint64_t mix13(uint64_t v) { v = (v ^ (v >> 30)) * 0xBF58476D1CE4E5B9ULL; v = (v ^ (v >> 27)) * 0x94D049BB133111EBULL; return v ^ (v >> 31); }

void xo_make(Xo* r, uint64_t lo, uint64_t hi) {
    r->lo = lo; r->hi = hi;
    if ((lo | hi) == 0) { r->lo = GOLDEN; r->hi = SILVER; }
}
void xo_of_seed(Xo* r, int64_t seed) {
    uint64_t a = (uint64_t) seed ^ SILVER, b = a + GOLDEN;
    xo_make(r, mix13(a), mix13(b));
}
uint64_t xo_long(Xo* r) {
    uint64_t l = r->lo, m = r->hi, n = rotl64(l + m, 17) + l;
    m ^= l; r->lo = rotl64(l, 49) ^ m ^ (m << 21); r->hi = rotl64(m, 28);
    return n;
}
static uint32_t xo_u32(Xo* r) { return (uint32_t) xo_long(r); }
int xo_int(Xo* r, int bound) {
    uint64_t rr = xo_u32(r), m = rr * (uint64_t) bound, low = m & 0xFFFFFFFFULL;
    if (low < (uint64_t) bound) {
        uint32_t t = (uint32_t) (0u - (uint32_t) bound) % (uint32_t) bound;
        while (low < t) { rr = xo_u32(r); m = rr * (uint64_t) bound; low = m & 0xFFFFFFFFULL; }
    }
    return (int) (m >> 32);
}
double xo_double(Xo* r) { return (double) (xo_long(r) >> 11) * 1.1102230246251565E-16; }
Pos xo_fork(Xo* r) { Pos p; p.lo = xo_long(r); p.hi = xo_long(r); return p; }

static int64_t get_seed(int x, int y, int z) {
    int64_t l = (int64_t) (int32_t) ((uint32_t) x * 3129871u) ^ (int64_t) z * 116129781LL ^ (int64_t) y;
    l = (int64_t) ((uint64_t) l * (uint64_t) l * 42317861ULL + (uint64_t) l * 11ULL);
    return l >> 16;
}
void pos_at(const Pos* p, int x, int y, int z, Xo* out) { xo_make(out, (uint64_t) get_seed(x, y, z) ^ p->lo, p->hi); }

typedef struct { uint32_t a, b, c, d; } Md5;
static void md5_digest(const unsigned char* msg, size_t len, unsigned char out[16]) {
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };
    static const int S[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    unsigned char buf[128];
    size_t total = ((len + 8) / 64 + 1) * 64;
    memset(buf, 0, sizeof buf);
    memcpy(buf, msg, len);
    buf[len] = 0x80;
    uint64_t bits = (uint64_t) len * 8;
    for (int i = 0; i < 8; i++) buf[total - 8 + i] = (unsigned char) (bits >> (8 * i));
    Md5 h = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
    for (size_t off = 0; off < total; off += 64) {
        uint32_t M[16];
        for (int i = 0; i < 16; i++) M[i] = (uint32_t) buf[off + i * 4] | ((uint32_t) buf[off + i * 4 + 1] << 8) | ((uint32_t) buf[off + i * 4 + 2] << 16) | ((uint32_t) buf[off + i * 4 + 3] << 24);
        uint32_t A = h.a, B = h.b, C = h.c, D = h.d;
        for (int i = 0; i < 64; i++) {
            uint32_t F; int g;
            if (i < 16) { F = (B & C) | (~B & D); g = i; }
            else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) & 15; }
            else if (i < 48) { F = B ^ C ^ D; g = (3 * i + 5) & 15; }
            else { F = C ^ (B | ~D); g = (7 * i) & 15; }
            F = F + A + K[i] + M[g];
            A = D; D = C; C = B;
            B = B + ((F << S[i]) | (F >> (32 - S[i])));
        }
        h.a += A; h.b += B; h.c += C; h.d += D;
    }
    uint32_t r[4] = {h.a, h.b, h.c, h.d};
    for (int i = 0; i < 16; i++) out[i] = (unsigned char) (r[i / 4] >> (8 * (i % 4)));
}
void pos_hash(const Pos* p, const char* s, Xo* out) {
    unsigned char d[16];
    md5_digest((const unsigned char*) s, strlen(s), d);
    uint64_t a = 0, b = 0;
    for (int i = 0; i < 8; i++) { a = (a << 8) | d[i]; b = (b << 8) | d[8 + i]; }
    xo_make(out, a ^ p->lo, b ^ p->hi);
}

typedef struct { NoiseSet* ns; int nO, nP; } Build;

static int improved(Build* B, Xo* r) {
    Octave* o = &B->ns->octs[B->nO];
    o->xo = xo_double(r) * 256.0; o->yo = xo_double(r) * 256.0; o->zo = xo_double(r) * 256.0;
    for (int i = 0; i < 256; i++) o->p[i] = (uint8_t) i;
    for (int i = 0; i < 256; i++) { int j = xo_int(r, 256 - i); uint8_t t = o->p[i]; o->p[i] = o->p[i + j]; o->p[i + j] = t; }
    return B->nO++;
}
static int perlin(Build* B, Xo* r, int first, const double* amps, int n) {
    PerlinD* p = &B->ns->pers[B->nP];
    memset(p, 0, sizeof *p);
    int b = -first;
    p->n = n;
    Pos f = xo_fork(r);
    for (int i = 0; i < n; i++) {
        p->amp[i] = amps[i];
        if (amps[i] != 0.0) {
            char nm[32]; Xo q;
            snprintf(nm, sizeof nm, "octave_%d", first + i);
            pos_hash(&f, nm, &q);
            p->has[i] = 1; p->oct[i] = improved(B, &q);
        }
    }
    p->lowIn = ldexp(1.0, -b); p->lowVal = ldexp(1.0, n - 1) / (ldexp(1.0, n) - 1.0);
    return B->nP++;
}
static int perlin_legacy(Build* B, Xo* r, int first, int last) {
    PerlinD* p = &B->ns->pers[B->nP];
    memset(p, 0, sizeof *p);
    int n = last - first + 1, b = -first;
    p->n = n;
    for (int i = 0; i < n; i++) p->amp[i] = 1.0;
    int top = improved(B, r);
    if (b >= 0 && b < n) { p->has[b] = 1; p->oct[b] = top; }
    for (int i = b - 1; i >= 0; i--) {
        if (i < n) { p->has[i] = 1; p->oct[i] = improved(B, r); }
        else for (int k = 0; k < 262; k++) xo_long(r);
    }
    p->lowIn = ldexp(1.0, -b); p->lowVal = ldexp(1.0, n - 1) / (ldexp(1.0, n) - 1.0);
    return B->nP++;
}

void noiseset_init(NoiseSet* ns, int64_t seed) {
    memset(ns, 0, sizeof *ns);
    ns->nOcts = 2 * 16 * NOISE_COUNT + 64; ns->nPers = 2 * NOISE_COUNT + 3; ns->nNors = NOISE_COUNT;
    ns->octs = (Octave*) calloc(ns->nOcts, sizeof(Octave));
    ns->pers = (PerlinD*) calloc(ns->nPers, sizeof(PerlinD));
    ns->nors = (NormalD*) calloc(ns->nNors, sizeof(NormalD));
    Build B = {ns, 0, 0};
    Xo base; xo_of_seed(&base, seed);
    Pos random = xo_fork(&base);
    Xo aq; pos_hash(&random, "minecraft:aquifer", &aq);
    ns->aquifer = xo_fork(&aq);
    for (int k = 0; k < NOISE_COUNT; k++) {
        const NoiseDef* d = &NOISE_DEFS[k];
        const double* amps = &NOISE_AMPS[d->ampOff];
        Xo r; pos_hash(&random, d->name, &r);
        int a = perlin(&B, &r, d->first, amps, d->n), b = perlin(&B, &r, d->first, amps, d->n);
        int lo = 0x7fffffff, hi = -0x7fffffff - 1;
        for (int i = 0; i < d->n; i++) if (amps[i] != 0.0) { if (i < lo) lo = i; if (i > hi) hi = i; }
        ns->nors[k].a = a; ns->nors[k].b = b;
        ns->nors[k].vf = 0.16666666666666666 / (0.1 * (1.0 + 1.0 / (double) (hi - lo + 1)));
    }
    Xo t; pos_hash(&random, "minecraft:terrain", &t);
    int mn = perlin_legacy(&B, &t, -15, 0), mx = perlin_legacy(&B, &t, -15, 0), mainP = perlin_legacy(&B, &t, -7, 0);
    ns->bl.mn = mn; ns->bl.mx = mx; ns->bl.mainP = mainP;
    ns->bl.xzMul = 684.412 * TREE_BLEND[0]; ns->bl.yMul = 684.412 * TREE_BLEND[1];
    ns->bl.xzFactor = TREE_BLEND[2]; ns->bl.yFactor = TREE_BLEND[3]; ns->bl.smear = TREE_BLEND[4];
}
void noiseset_free(NoiseSet* ns) { free(ns->octs); free(ns->pers); free(ns->nors); memset(ns, 0, sizeof *ns); }

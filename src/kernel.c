#include "kernel.h"

static const double GX[16] = {1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 1, 0, -1, 0};
static const double GY[16] = {1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1};
static const double GZ[16] = {0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 1, 0, -1};

static inline int ifloor(double d) { int i = (int) d; return d < i ? i - 1 : i; }
static inline long long lfloor(double d) { long long l = (long long) d; return d < (double) l ? l - 1LL : l; }

static inline double wrap(double v) { if (fabs(v) <= 4194304.0) return v; return v - (double) lfloor(v / 3.3554432E7 + 0.5) * 3.3554432E7; }
static inline double lerp(double t, double a, double b) { return a + t * (b - a); }
static inline double lerp2(double a, double b, double c, double d, double e, double f) { return lerp(b, lerp(a, c, d), lerp(a, e, f)); }
static inline double lerp3(double a, double b, double c, double v000, double v100, double v010, double v110, double v001, double v101, double v011, double v111) {
    return lerp(c, lerp2(a, b, v000, v100, v010, v110), lerp2(a, b, v001, v101, v011, v111));
}
static inline double smoothstep(double d) { return d * d * d * (d * (d * 6.0 - 15.0) + 10.0); }
#define PP(i) ((int) o->p[(i) & 0xFF])
static inline double gdot(int idx, double x, double y, double z) { int i = idx & 15; return GX[i] * x + GY[i] * y + GZ[i] * z; }

static double inoise(const Octave* o, double x, double y, double z, double yScale, double yMax) {
    double px = x + o->xo, py = y + o->yo, pz = z + o->zo;
    int ix = ifloor(px), iy = ifloor(py), iz = ifloor(pz);
    double fx = px - ix, fy = py - iy, fz = pz - iz, shift;
    if (yScale != 0.0) { double m = (yMax >= 0.0 && yMax < fy) ? yMax : fy; shift = ifloor(m / yScale + (double) 1.0E-7f) * yScale; } else shift = 0.0;
    double fyr = fy; fy = fy - shift;
    int a = PP(ix), b = PP(ix + 1);
    int aa = PP(a + iy), ab = PP(a + iy + 1), ba = PP(b + iy), bb = PP(b + iy + 1);
    double d000 = gdot(PP(aa + iz), fx, fy, fz);
    double d100 = gdot(PP(ba + iz), fx - 1.0, fy, fz);
    double d010 = gdot(PP(ab + iz), fx, fy - 1.0, fz);
    double d110 = gdot(PP(bb + iz), fx - 1.0, fy - 1.0, fz);
    double d001 = gdot(PP(aa + iz + 1), fx, fy, fz - 1.0);
    double d101 = gdot(PP(ba + iz + 1), fx - 1.0, fy, fz - 1.0);
    double d011 = gdot(PP(ab + iz + 1), fx, fy - 1.0, fz - 1.0);
    double d111 = gdot(PP(bb + iz + 1), fx - 1.0, fy - 1.0, fz - 1.0);
    double sx = smoothstep(fx), sy = smoothstep(fyr), sz = smoothstep(fz);
    return lerp3(sx, sy, sz, d000, d100, d010, d110, d001, d101, d011, d111);
}
#undef PP

static double perlin_get(const World* w, const PerlinD* p, double x, double y, double z) {
    double sum = 0.0, in = p->lowIn, val = p->lowVal;
    for (int i = 0; i < p->n; i++) {
        if (p->has[i]) { const Octave* o = &w->octs[p->oct[i]]; double v = inoise(o, wrap(x * in), wrap(y * in), wrap(z * in), 0.0 * in, 0.0 * in); sum += p->amp[i] * v * val; }
        in *= 2.0; val /= 2.0;
    }
    return sum;
}
double noise_normal(const World* w, int idx, double x, double y, double z) {
    const NormalD* n = &w->nors[idx]; double x2 = x * 1.0181268882175227, y2 = y * 1.0181268882175227, z2 = z * 1.0181268882175227;
    return (perlin_get(w, &w->pers[n->a], x, y, z) + perlin_get(w, &w->pers[n->b], x2, y2, z2)) * n->vf;
}
static double blended0(const World* w, int bx, int by, int bz);
double blended(const World* w, int bx, int by, int bz) {
    World* m = (World*) w; uint64_t h = ((uint64_t) (uint32_t) bx * 0x9E3779B97F4A7C15ULL) ^ ((uint64_t) (uint32_t) by * 0xC2B2AE3D27D4EB4FULL) ^ ((uint64_t) (uint32_t) bz * 0x165667B19E3779F9ULL);
    int s = (int) (h >> 56) & (BLEND_SLOTS - 1);
    if (m->bok[s] && m->bkx[s] == bx && m->bky[s] == by && m->bkz[s] == bz) { return m->bval[s]; }
    double v = blended0(w, bx, by, bz); m->bkx[s] = bx; m->bky[s] = by; m->bkz[s] = bz; m->bok[s] = 1; m->bval[s] = v; return v;
}
static double blended0(const World* w, int bx, int by, int bz) {
    const BlendD* B = &w->bl; const PerlinD* mn = &w->pers[B->mn]; const PerlinD* mx = &w->pers[B->mx]; const PerlinD* mainP = &w->pers[B->mainP];
    double x = bx * B->xzMul, y = by * B->yMul, z = bz * B->xzMul;
    double fx = x / B->xzFactor, fy = y / B->yFactor, fz = z / B->xzFactor;
    double s = B->yMul * B->smear, sf = s / B->yFactor;
    double main_ = 0.0, f = 1.0;
    for (int i = 0; i < 8; i++) {
        int li = mainP->n - 1 - i;
        if (li >= 0 && mainP->has[li]) main_ += inoise(&w->octs[mainP->oct[li]], wrap(fx * f), wrap(fy * f), wrap(fz * f), sf * f, fy * f) / f;
        f /= 2.0;
    }
    double t = (main_ / 10.0 + 1.0) / 2.0; int over = t >= 1.0, under = t <= 0.0;
    double a = 0.0, b = 0.0; f = 1.0;
    for (int i = 0; i < 16; i++) {
        double wx = wrap(x * f), wy = wrap(y * f), wz = wrap(z * f), ss = s * f;
        if (!over) { int li = mn->n - 1 - i; if (li >= 0 && mn->has[li]) a += inoise(&w->octs[mn->oct[li]], wx, wy, wz, ss, y * f) / f; }
        if (!under) { int li = mx->n - 1 - i; if (li >= 0 && mx->has[li]) b += inoise(&w->octs[mx->oct[li]], wx, wy, wz, ss, y * f) / f; }
        f /= 2.0;
    }
    return clamped_lerp(t, a / 512.0, b / 512.0) / 128.0;
}


#include "kernel.h"

#define MIN_Y (-64)
#define DEPTH 384
#define LAVA_Y (-64 + 8)
#define PI_F 3.14159265358979323846f

static float SINT[65536]; static int sin_ready = 0;
void carve_init(void) { if (sin_ready) return; for (int i = 0; i < 65536; i++) SINT[i] = (float) sin(i / 10430.378350470453); sin_ready = 1; }

typedef struct { int64_t seed; } Rand;
static inline void rset(Rand* r, int64_t s) { r->seed = (s ^ 25214903917LL) & 281474976710655LL; }
static inline int rnext(Rand* r, int bits) { r->seed = (int64_t) (((uint64_t) r->seed * 25214903917ULL + 11ULL) & 281474976710655ULL); return (int) (r->seed >> (48 - bits)); }
static inline int rnextInt(Rand* r, int b) {
    if ((b & (b - 1)) == 0) return (int) (((int64_t) b * (int64_t) rnext(r, 31)) >> 31);
    int v, m; do { v = rnext(r, 31); m = v % b; } while ((int32_t) ((uint32_t) v - (uint32_t) m + (uint32_t) (b - 1)) < 0); return m;
}
static inline int64_t rnextLong(Rand* r) { int a = rnext(r, 32); int c = rnext(r, 32); return (int64_t) (((uint64_t) (int64_t) a << 32) + (uint64_t) (int64_t) c); }
static inline float rnextFloat(Rand* r) { return (float) rnext(r, 24) * 5.9604645E-8f; }
static inline void rlarge(Rand* r, int64_t s, int cx, int cz) {
    rset(r, s); int64_t a = rnextLong(r); int64_t b = rnextLong(r);
    rset(r, (int64_t) ((uint64_t) (int64_t) cx * (uint64_t) a ^ (uint64_t) (int64_t) cz * (uint64_t) b ^ (uint64_t) s));
}
static inline float csin(double d) { return SINT[(int) ((long long) (d * 10430.378350470453) & 65535LL)]; }
static inline float ccos(double d) { return SINT[(int) ((long long) (d * 10430.378350470453 + 16384.0) & 65535LL)]; }
static inline int ffloor(double d) { int i = (int) d; return d < i ? i - 1 : i; }
static inline float uniformf(Rand* r, float lo, float hi) { return rnextFloat(r) * (hi - lo) + lo; }
static inline int uniformi(Rand* r, int lo, int hi) { return rnextInt(r, hi - lo + 1) + lo; }

typedef struct { uint8_t* mask; int tcx, tcz; } Ctx;
typedef struct { int kind; double floorLevel; const float* wf; } Skip;

static void ellipsoid(Ctx* c, double x, double y, double z, double hr, double vr, const Skip* skip) {
    double cx = c->tcx * 16 + 8, cz = c->tcz * 16 + 8, reach = 16.0 + hr * 2.0;
    if (fabs(x - cx) > reach || fabs(z - cz) > reach) return;
    int minX = c->tcx * 16, minZ = c->tcz * 16;
    int x0 = ffloor(x - hr) - minX - 1; if (x0 < 0) x0 = 0; int x1 = ffloor(x + hr) - minX; if (x1 > 15) x1 = 15;
    int y0 = ffloor(y - vr) - 1; if (y0 < MIN_Y + 1) y0 = MIN_Y + 1; int y1 = ffloor(y + vr) + 1; if (y1 > MIN_Y + DEPTH - 1 - 7) y1 = MIN_Y + DEPTH - 1 - 7;
    int z0 = ffloor(z - hr) - minZ - 1; if (z0 < 0) z0 = 0; int z1 = ffloor(z + hr) - minZ; if (z1 > 15) z1 = 15;
    if (x0 > x1 || z0 > z1 || y1 <= y0) return;
    double dyv[DEPTH + 2], dyy[DEPTH + 2], dyy6[DEPTH + 2];
    for (int by = y1; by > y0; by--) { double dy = (by - 0.5 - y) / vr; dyv[by - y0] = dy; dyy[by - y0] = dy * dy; dyy6[by - y0] = dy * dy / 6.0; }
    for (int lx = x0; lx <= x1; lx++) {
        int bx = minX + lx; double dx = (bx + 0.5 - x) / hr;
        for (int lz = z0; lz <= z1; lz++) {
            int bz = minZ + lz; double dz = (bz + 0.5 - z) / hr;
            double dxz = dx * dx + dz * dz;
            if (dxz >= 1.0) continue;
            if (skip->kind == 0) {
                for (int by = y1; by > y0; by--) {
                    int k = by - y0;
                    int skipped = dyv[k] <= skip->floorLevel ? 1 : (dx * dx + dyy[k] + dz * dz >= 1.0);
                    if (!skipped) { int i = ((by - MIN_Y) * 16 + lz) * 16 + lx; c->mask[i >> 3] |= (uint8_t) (1u << (i & 7)); }
                }
            } else {
                for (int by = y1; by > y0; by--) {
                    int k = by - y0;
                    if (!(dxz * skip->wf[by - MIN_Y - 1] + dyy6[k] >= 1.0)) { int i = ((by - MIN_Y) * 16 + lz) * 16 + lx; c->mask[i >> 3] |= (uint8_t) (1u << (i & 7)); }
                }
            }
        }
    }
}
static inline int can_reach(const Ctx* c, double x, double z, int step, int steps, float thick) {
    double dx = x - (c->tcx * 16 + 8), dz = z - (c->tcz * 16 + 8), rem = steps - step, lim = thick + 2.0f + 16.0f;
    return dx * dx + dz * dz - rem * rem <= lim * lim;
}

static void create_tunnel(Ctx* c, int64_t seed, double x, double y, double z, double hMul, double vMul, float thick, float yaw, float pitch, int start, int steps, double yScale, const Skip* skip) {
    Rand r; rset(&r, seed);
    int branchAt = rnextInt(&r, steps / 2) + steps / 4;
    int steep = rnextInt(&r, 6) == 0;
    float dYaw = 0.0f, dPitch = 0.0f;
    for (int s = start; s < steps; s++) {
        double hr = 1.5 + csin(PI_F * s / steps) * thick; double vr = hr * yScale;
        float cp = ccos(pitch);
        x += ccos(yaw) * cp; y += csin(pitch); z += csin(yaw) * cp;
        pitch *= steep ? 0.92f : 0.7f;
        pitch += dPitch * 0.1f; yaw += dYaw * 0.1f;
        dPitch *= 0.9f; dYaw *= 0.75f;
        { float a = rnextFloat(&r); float b = rnextFloat(&r); float d = rnextFloat(&r); dPitch += (a - b) * d * 2.0f; }
        { float a = rnextFloat(&r); float b = rnextFloat(&r); float d = rnextFloat(&r); dYaw += (a - b) * d * 4.0f; }
        if (s == branchAt && thick > 1.0f) {
            { int64_t sd = rnextLong(&r); float t = rnextFloat(&r) * 0.5f + 0.5f; create_tunnel(c, sd, x, y, z, hMul, vMul, t, yaw - PI_F / 2.0f, pitch / 3.0f, s, steps, 1.0, skip); }
            { int64_t sd = rnextLong(&r); float t = rnextFloat(&r) * 0.5f + 0.5f; create_tunnel(c, sd, x, y, z, hMul, vMul, t, yaw + PI_F / 2.0f, pitch / 3.0f, s, steps, 1.0, skip); }
            return;
        }
        if (rnextInt(&r, 4) != 0) {
            if (!can_reach(c, x, z, s, steps, thick)) return;
            ellipsoid(c, x, y, z, hr * hMul, vr * vMul, skip);
        }
    }
}

static float cave_thickness(Rand* r) {
    float a = rnextFloat(r); float b = rnextFloat(r); float t = a * 2.0f + b;
    if (rnextInt(r, 10) == 0) { float p = rnextFloat(r); float q = rnextFloat(r); t *= p * q * 3.0f + 1.0f; }
    return t;
}
static void carve_cave(Ctx* c, Rand* r, int scx, int scz, int yMax) {
    int range = 7 * 16;
    int n = rnextInt(r, rnextInt(r, rnextInt(r, 15) + 1) + 1);
    for (int i = 0; i < n; i++) {
        double x = scx * 16 + rnextInt(r, 16);
        double y = uniformi(r, LAVA_Y, yMax);
        double z = scz * 16 + rnextInt(r, 16);
        double hMul = uniformf(r, 0.7f, 1.4f); double vMul = uniformf(r, 0.8f, 1.3f); double floorLevel = uniformf(r, -1.0f, -0.4f);
        Skip skip; skip.kind = 0; skip.floorLevel = floorLevel; skip.wf = 0;
        int tunnels = 1;
        if (rnextInt(r, 4) == 0) {
            double yScale = uniformf(r, 0.1f, 0.9f);
            float size = 1.0f + rnextFloat(r) * 6.0f;
            double hr = 1.5 + csin(PI_F / 2.0f) * size; double vr = hr * yScale;
            ellipsoid(c, x + 1.0, y, z, hr, vr, &skip);
            tunnels += rnextInt(r, 4);
        }
        for (int t = 0; t < tunnels; t++) {
            float yaw = rnextFloat(r) * (PI_F * 2.0f);
            float pitch = (rnextFloat(r) - 0.5f) / 4.0f;
            float thick = cave_thickness(r);
            int steps = range - rnextInt(r, range / 4);
            int64_t sd = rnextLong(r);
            create_tunnel(c, sd, x, y, z, hMul, vMul, thick, yaw, pitch, 0, steps, 1.0, &skip);
        }
    }
}
static void do_canyon(Ctx* c, int64_t seed, double x, double y, double z, float thick, float yaw, float pitch, int start, int steps, double yScale) {
    Rand r; rset(&r, seed);
    float wf[DEPTH]; float f = 1.0f;
    for (int i = 0; i < DEPTH; i++) { if (i == 0 || rnextInt(&r, 3) == 0) { float a = rnextFloat(&r); float b = rnextFloat(&r); f = 1.0f + a * b; } wf[i] = f * f; }
    Skip skip; skip.kind = 1; skip.floorLevel = 0.0; skip.wf = wf;
    float dYaw = 0.0f, dPitch = 0.0f;
    for (int s = start; s < steps; s++) {
        double hr = 1.5 + csin(s * PI_F / steps) * thick; double vr = hr * yScale;
        hr *= uniformf(&r, 0.75f, 1.0f);
        float t = 1.0f - fabsf(0.5f - (float) s / (float) steps) * 2.0f;
        float fac = 1.0f + 0.0f * t;
        vr = fac * vr * uniformf(&r, 0.75f, 1.0f);
        float cp = ccos(pitch), sp = csin(pitch);
        x += ccos(yaw) * cp; y += sp; z += csin(yaw) * cp;
        pitch *= 0.7f; pitch += dPitch * 0.05f; yaw += dYaw * 0.05f;
        dPitch *= 0.8f; dYaw *= 0.5f;
        { float a = rnextFloat(&r); float b = rnextFloat(&r); float d = rnextFloat(&r); dPitch += (a - b) * d * 2.0f; }
        { float a = rnextFloat(&r); float b = rnextFloat(&r); float d = rnextFloat(&r); dYaw += (a - b) * d * 4.0f; }
        if (rnextInt(&r, 4) != 0) {
            if (!can_reach(c, x, z, s, steps, thick)) return;
            ellipsoid(c, x, y, z, hr, vr, &skip);
        }
    }
}
static void carve_canyon(Ctx* c, Rand* r, int scx, int scz) {
    int range = (4 * 2 - 1) * 16;
    double x = scx * 16 + rnextInt(r, 16);
    int y = uniformi(r, 10, 67);
    double z = scz * 16 + rnextInt(r, 16);
    float yaw = rnextFloat(r) * (PI_F * 2.0f);
    float pitch = uniformf(r, -0.125f, 0.125f);
    double yScale = 3.0;
    float thick; { float mn = 0.0f, mx = 6.0f, plateau = 2.0f; float a = mx - mn; float b = (a - plateau) / 2.0f; float cc = a - b; float p = rnextFloat(r); float q = rnextFloat(r); thick = mn + p * cc + q * b; }
    int steps = (int) (range * uniformf(r, 0.75f, 1.0f));
    int64_t sd = rnextLong(r);
    do_canyon(c, sd, x, y, z, thick, yaw, pitch, 0, steps, yScale);
}

void carve_chunk(int64_t worldSeed, int tcx, int tcz, uint8_t* maskOut) {
    memset(maskOut, 0, 16 * 16 * DEPTH / 8);
    Ctx c; c.mask = maskOut; c.tcx = tcx; c.tcz = tcz; Rand r; rset(&r, 0);
    for (int dx = -8; dx <= 8; dx++) for (int dz = -8; dz <= 8; dz++) {
        int scx = tcx + dx, scz = tcz + dz;
        rlarge(&r, worldSeed + 0, scx, scz); if (rnextFloat(&r) <= 0.15f) carve_cave(&c, &r, scx, scz, 180);
        rlarge(&r, worldSeed + 1, scx, scz); if (rnextFloat(&r) <= 0.07f) carve_cave(&c, &r, scx, scz, 47);
        rlarge(&r, worldSeed + 2, scx, scz); if (rnextFloat(&r) <= 0.01f) carve_canyon(&c, &r, scx, scz);
    }
}

#define TILE_T 8
#define MASK_BYTES (16 * 16 * DEPTH / 8)
typedef struct { uint8_t* base; int tx0, tz0; } TileCtx;

static void tile_chunk_ctx(const TileCtx* t, int i, Ctx* c) { c->mask = t->base + (size_t) i * MASK_BYTES; c->tcx = t->tx0 + i / TILE_T; c->tcz = t->tz0 + i % TILE_T; }
static void tile_ellipsoid(const TileCtx* t, int i, double x, double y, double z, double hr, double vr, const Skip* skip) {
    Ctx c; tile_chunk_ctx(t, i, &c);
    ellipsoid(&c, x, y, z, hr, vr, skip);
}
static int tile_step(const TileCtx* t, int* alive, int n, double x, double y, double z, double hr, double vr, const Skip* skip, int s, int steps, float thick) {
    int k = 0;
    for (int j = 0; j < n; j++) {
        int i = alive[j]; Ctx c; tile_chunk_ctx(t, i, &c);
        if (!can_reach(&c, x, z, s, steps, thick)) continue;
        alive[k++] = i;
        ellipsoid(&c, x, y, z, hr, vr, skip);
    }
    return k;
}

static void tile_tunnel(const TileCtx* t, int64_t seed, double x, double y, double z, double hMul, double vMul, float thick, float yaw, float pitch, int start, int steps, double yScale, const Skip* skip, const int* aliveIn, int n) {
    int alive[TILE_T * TILE_T];
    memcpy(alive, aliveIn, sizeof(int) * (size_t) n);
    Rand r; rset(&r, seed);
    int branchAt = rnextInt(&r, steps / 2) + steps / 4;
    int steep = rnextInt(&r, 6) == 0;
    float dYaw = 0.0f, dPitch = 0.0f;
    for (int s = start; s < steps; s++) {
        double hr = 1.5 + csin(PI_F * s / steps) * thick; double vr = hr * yScale;
        float cp = ccos(pitch);
        x += ccos(yaw) * cp; y += csin(pitch); z += csin(yaw) * cp;
        pitch *= steep ? 0.92f : 0.7f;
        pitch += dPitch * 0.1f; yaw += dYaw * 0.1f;
        dPitch *= 0.9f; dYaw *= 0.75f;
        { float a = rnextFloat(&r); float b = rnextFloat(&r); float d = rnextFloat(&r); dPitch += (a - b) * d * 2.0f; }
        { float a = rnextFloat(&r); float b = rnextFloat(&r); float d = rnextFloat(&r); dYaw += (a - b) * d * 4.0f; }
        if (s == branchAt && thick > 1.0f) {
            { int64_t sd = rnextLong(&r); float tk = rnextFloat(&r) * 0.5f + 0.5f; tile_tunnel(t, sd, x, y, z, hMul, vMul, tk, yaw - PI_F / 2.0f, pitch / 3.0f, s, steps, 1.0, skip, alive, n); }
            { int64_t sd = rnextLong(&r); float tk = rnextFloat(&r) * 0.5f + 0.5f; tile_tunnel(t, sd, x, y, z, hMul, vMul, tk, yaw + PI_F / 2.0f, pitch / 3.0f, s, steps, 1.0, skip, alive, n); }
            return;
        }
        if (rnextInt(&r, 4) != 0) {
            n = tile_step(t, alive, n, x, y, z, hr * hMul, vr * vMul, skip, s, steps, thick);
            if (n == 0) return;
        }
    }
}

static void tile_cave(const TileCtx* t, Rand* r, int scx, int scz, int yMax, const int* alive, int n) {
    int range = 7 * 16;
    int count = rnextInt(r, rnextInt(r, rnextInt(r, 15) + 1) + 1);
    for (int i = 0; i < count; i++) {
        double x = scx * 16 + rnextInt(r, 16);
        double y = uniformi(r, LAVA_Y, yMax);
        double z = scz * 16 + rnextInt(r, 16);
        double hMul = uniformf(r, 0.7f, 1.4f); double vMul = uniformf(r, 0.8f, 1.3f); double floorLevel = uniformf(r, -1.0f, -0.4f);
        Skip skip; skip.kind = 0; skip.floorLevel = floorLevel; skip.wf = 0;
        int tunnels = 1;
        if (rnextInt(r, 4) == 0) {
            double yScale = uniformf(r, 0.1f, 0.9f);
            float size = 1.0f + rnextFloat(r) * 6.0f;
            double hr = 1.5 + csin(PI_F / 2.0f) * size; double vr = hr * yScale;
            for (int j = 0; j < n; j++) tile_ellipsoid(t, alive[j], x + 1.0, y, z, hr, vr, &skip);
            tunnels += rnextInt(r, 4);
        }
        for (int k = 0; k < tunnels; k++) {
            float yaw = rnextFloat(r) * (PI_F * 2.0f);
            float pitch = (rnextFloat(r) - 0.5f) / 4.0f;
            float thick = cave_thickness(r);
            int steps = range - rnextInt(r, range / 4);
            int64_t sd = rnextLong(r);
            tile_tunnel(t, sd, x, y, z, hMul, vMul, thick, yaw, pitch, 0, steps, 1.0, &skip, alive, n);
        }
    }
}

static void tile_canyon_run(const TileCtx* t, int64_t seed, double x, double y, double z, float thick, float yaw, float pitch, int start, int steps, double yScale, const int* aliveIn, int n) {
    int alive[TILE_T * TILE_T];
    memcpy(alive, aliveIn, sizeof(int) * (size_t) n);
    Rand r; rset(&r, seed);
    float wf[DEPTH]; float f = 1.0f;
    for (int i = 0; i < DEPTH; i++) { if (i == 0 || rnextInt(&r, 3) == 0) { float a = rnextFloat(&r); float b = rnextFloat(&r); f = 1.0f + a * b; } wf[i] = f * f; }
    Skip skip; skip.kind = 1; skip.floorLevel = 0.0; skip.wf = wf;
    float dYaw = 0.0f, dPitch = 0.0f;
    for (int s = start; s < steps; s++) {
        double hr = 1.5 + csin(s * PI_F / steps) * thick; double vr = hr * yScale;
        hr *= uniformf(&r, 0.75f, 1.0f);
        float tt = 1.0f - fabsf(0.5f - (float) s / (float) steps) * 2.0f;
        float fac = 1.0f + 0.0f * tt;
        vr = fac * vr * uniformf(&r, 0.75f, 1.0f);
        float cp = ccos(pitch), sp = csin(pitch);
        x += ccos(yaw) * cp; y += sp; z += csin(yaw) * cp;
        pitch *= 0.7f; pitch += dPitch * 0.05f; yaw += dYaw * 0.05f;
        dPitch *= 0.8f; dYaw *= 0.5f;
        { float a = rnextFloat(&r); float b = rnextFloat(&r); float d = rnextFloat(&r); dPitch += (a - b) * d * 2.0f; }
        { float a = rnextFloat(&r); float b = rnextFloat(&r); float d = rnextFloat(&r); dYaw += (a - b) * d * 4.0f; }
        if (rnextInt(&r, 4) != 0) {
            n = tile_step(t, alive, n, x, y, z, hr, vr, &skip, s, steps, thick);
            if (n == 0) return;
        }
    }
}
static void tile_canyon(const TileCtx* t, Rand* r, int scx, int scz, const int* alive, int n) {
    int range = (4 * 2 - 1) * 16;
    double x = scx * 16 + rnextInt(r, 16);
    int y = uniformi(r, 10, 67);
    double z = scz * 16 + rnextInt(r, 16);
    float yaw = rnextFloat(r) * (PI_F * 2.0f);
    float pitch = uniformf(r, -0.125f, 0.125f);
    double yScale = 3.0;
    float thick; { float mn = 0.0f, mx = 6.0f, plateau = 2.0f; float a = mx - mn; float b = (a - plateau) / 2.0f; float cc = a - b; float p = rnextFloat(r); float q = rnextFloat(r); thick = mn + p * cc + q * b; }
    int steps = (int) (range * uniformf(r, 0.75f, 1.0f));
    int64_t sd = rnextLong(r);
    tile_canyon_run(t, sd, x, y, z, thick, yaw, pitch, 0, steps, yScale, alive, n);
}

void carve_tile(int64_t worldSeed, int tx0, int tz0, uint8_t* out) {
    memset(out, 0, (size_t) TILE_T * TILE_T * MASK_BYTES);
    TileCtx t; t.base = out; t.tx0 = tx0; t.tz0 = tz0;
    Rand r; rset(&r, 0);
    for (int scx = tx0 - 8; scx < tx0 + TILE_T + 8; scx++) for (int scz = tz0 - 8; scz < tz0 + TILE_T + 8; scz++) {
        int alive[TILE_T * TILE_T], n = 0;
        for (int i = 0; i < TILE_T * TILE_T; i++) {
            int dx = scx - (tx0 + i / TILE_T), dz = scz - (tz0 + i % TILE_T);
            if (dx >= -8 && dx <= 8 && dz >= -8 && dz <= 8) alive[n++] = i;
        }
        rlarge(&r, worldSeed + 0, scx, scz); if (rnextFloat(&r) <= 0.15f) tile_cave(&t, &r, scx, scz, 180, alive, n);
        rlarge(&r, worldSeed + 1, scx, scz); if (rnextFloat(&r) <= 0.07f) tile_cave(&t, &r, scx, scz, 47, alive, n);
        rlarge(&r, worldSeed + 2, scx, scz); if (rnextFloat(&r) <= 0.01f) tile_canyon(&t, &r, scx, scz, alive, n);
    }
}

#include "dsf.h"

#define EMPTY_KEY ((int64_t) 0x8000000000000000ULL)
#define GOLD64 0x9E3779B97F4A7C15ULL

void lmap_init(LMap* m, int bits) {
    size_t n = (size_t) 2 << bits;
    m->kv = (int64_t*) malloc(n * sizeof(int64_t)); m->mask = (1 << bits) - 1; m->size = 0;
    for (size_t i = 0; i < n; i++) m->kv[i] = EMPTY_KEY;
}
void lmap_free(LMap* m) { free(m->kv); m->kv = NULL; }
void lmap_clear(LMap* m) { size_t n = ((size_t) m->mask + 1) * 2; for (size_t i = 0; i < n; i++) m->kv[i] = EMPTY_KEY; m->size = 0; }
static int lslot(const LMap* m, int64_t k) { return (int) (((uint64_t) k * GOLD64) >> 40) & m->mask; }
int lmap_get(const LMap* m, int64_t k, double* out) {
    for (int i = lslot(m, k);; i = (i + 1) & m->mask) {
        int64_t q = m->kv[(size_t) i << 1];
        if (q == k) { int64_t b = m->kv[((size_t) i << 1) + 1]; memcpy(out, &b, 8); return 1; }
        if (q == EMPTY_KEY) return 0;
    }
}
void lmap_put(LMap* m, int64_t k, double v) {
    if (m->size > (m->mask >> 1)) lmap_clear(m);
    int i = lslot(m, k);
    while (m->kv[(size_t) i << 1] != EMPTY_KEY && m->kv[(size_t) i << 1] != k) i = (i + 1) & m->mask;
    if (m->kv[(size_t) i << 1] == EMPTY_KEY) m->size++;
    int64_t b; memcpy(&b, &v, 8);
    m->kv[(size_t) i << 1] = k; m->kv[((size_t) i << 1) + 1] = b;
}

void model_init(Model* m, const NoiseSet* ns, int64_t seed) {
    memset(m, 0, sizeof *m);
    m->ns = ns; m->seed = seed;
    m->w.octs = ns->octs; m->w.pers = ns->pers; m->w.nors = ns->nors; m->w.bl = ns->bl;
    lmap_init(&m->corner, 19); lmap_init(&m->prelim, 17); lmap_init(&m->flat, 16);
    for (int i = 0; i < 256; i++) m->cellKey[i] = EMPTY_KEY;
    size_t n = (size_t) 4 << C2D_BITS;
    m->c2d = (int64_t*) malloc(n * sizeof(int64_t));
    for (size_t i = 0; i < n; i += 4) { m->c2d[i] = EMPTY_KEY; m->c2d[i + 1] = m->c2d[i + 2] = m->c2d[i + 3] = 0; }
}
void model_free(Model* m) { lmap_free(&m->corner); lmap_free(&m->prelim); lmap_free(&m->flat); free(m->c2d); m->c2d = NULL; }
void model_clear_caches(Model* m) {
    lmap_clear(&m->corner); lmap_clear(&m->flat); lmap_clear(&m->prelim);
    for (int i = 0; i < 256; i++) m->cellKey[i] = EMPTY_KEY;
}

void env_init(Env* e, Model* m, int interp, int snap, int chunkX, int chunkZ) {
    memset(e, 0, sizeof *e);
    e->m = m; e->interp = interp; e->snap = snap; e->fx = chunkX * 4; e->fz = chunkZ * 4;
}
void env_free(Env* e) { if (e->noI) { env_free(e->noI); free(e->noI); e->noI = NULL; } }
static Env* env_noi(Env* e) {
    if (!e->interp) return e;
    if (!e->noI) { e->noI = (Env*) malloc(sizeof(Env)); env_init(e->noI, e->m, 0, e->snap, e->fx >> 2, e->fz >> 2); }
    return e->noI;
}

static double lerp(double t, double a, double b) { return a + t * (b - a); }
static double lerp2(double a, double b, double c, double d, double e, double f) { return lerp(b, lerp(a, c, d), lerp(a, e, f)); }
static double lerp3(double a, double b, double c, double v000, double v100, double v010, double v110, double v001, double v101, double v011, double v111) {
    return lerp(c, lerp2(a, b, v000, v100, v010, v110), lerp2(a, b, v001, v101, v011, v111));
}

static float spline_apply(Model* m, int s, int x, int y, int z, Env* e);

static int bsearch_first(int lo, int hi, const float* loc, float xv) {
    int n = hi - lo;
    while (n > 0) { int h = n / 2, mid = lo + h; if (xv < loc[mid]) n = h; else { lo = mid + 1; n -= h + 1; } }
    return lo;
}
static float linear_extend(float xv, const float* loc, float value, const float* der, int i) { float d = der[i]; return d == 0.0f ? value : value + d * (xv - loc[i]); }
static float flerp(float t, float a, float b) { return a + t * (b - a); }

static float spline_apply(Model* m, int s, int x, int y, int z, Env* e) {
    const SplineDef* sd = &TREE_SPLINES[s];
    if (sd->kind == 0) return sd->cv;
    float xv = (float) tree_eval(m, sd->coord, x, y, z, e);
    const float* loc = &TREE_SP_LOC[sd->off]; const float* der = &TREE_SP_DER[sd->off]; const int* ch = &TREE_SP_CHILD[sd->off];
    int n = sd->n, i = bsearch_first(0, n, loc, xv) - 1, last = n - 1;
    if (i < 0) return linear_extend(xv, loc, spline_apply(m, ch[0], x, y, z, e), der, 0);
    if (i == last) return linear_extend(xv, loc, spline_apply(m, ch[last], x, y, z, e), der, last);
    float x0 = loc[i], x1 = loc[i + 1];
    float t = (xv - x0) / (x1 - x0);
    float d0 = der[i], d1 = der[i + 1];
    float y0 = spline_apply(m, ch[i], x, y, z, e), y1 = spline_apply(m, ch[i + 1], x, y, z, e);
    float a = d0 * (x1 - x0) - (y1 - y0);
    float b = -d1 * (x1 - x0) + (y1 - y0);
    return flerp(t, y0, y1) + t * (1.0f - t) * flerp(t, a, b);
}

static void compute_inputs(Model* m, Env* ce, Env* ni) {
    for (int k = 0; k < TREE_INPUT_COUNT; k++)
        for (int rz = 0; rz < 5; rz++)
            for (int rx = 0; rx < 5; rx++) {
                int x = (ni->fx + rx) << 2, z = (ni->fz + rz) << 2;
                ce->in2d[k * 25 + rz * 5 + rx] = tree_eval(m, TREE_INPUTS[k], x, 0, z, ni);
            }
    ce->in2dReady = 1;
}

static int64_t corner_key(int id, int x, int y, int z) {
    return (int64_t) (((uint64_t) id << 54) ^ (((uint64_t) (int64_t) (x >> 2) & 0xFFFFFFULL) << 30) ^ (((uint64_t) (int64_t) (y >> 3) & 0x3FULL) << 24) ^ ((uint64_t) (int64_t) (z >> 2) & 0xFFFFFFULL));
}

static double interp_corner(Model* m, int id, const NodeDef* d, int x, int y, int z, Env* ce, Env* ni) {
    int64_t key = corner_key(id, x, y, z);
    double v;
    if (lmap_get(&m->corner, key, &v)) return v;
    if (d->b >= 0) {
        if (!ce->in2dReady) compute_inputs(m, ce, ni);
        v = eval_root(&m->w, d->b, ce->in2d, ce->fx, ce->fz, x, y, z);
    } else v = tree_eval(m, d->a, x, y, z, ni);
    lmap_put(&m->corner, key, v);
    return v;
}

static double interp_eval(Model* m, int id, const NodeDef* d, int x, int y, int z, Env* e) {
    if (!e->interp) return tree_eval(m, d->a, x, y, z, e);
    Env* ni = env_noi(e);
    int x0 = (x >> 2) * 4, y0 = (y >> 3) * 8, z0 = (z >> 2) * 4, x1 = x0 + 4, y1 = y0 + 8, z1 = z0 + 4;
    int64_t ck = corner_key(id, x0, y0, z0);
    int cs = (int) (((uint64_t) ck * GOLD64) >> 56) << 3;
    double* cv = m->cellVal;
    if (m->cellKey[cs >> 3] != ck) {
        cv[cs] = interp_corner(m, id, d, x0, y0, z0, e, ni); cv[cs + 1] = interp_corner(m, id, d, x1, y0, z0, e, ni);
        cv[cs + 2] = interp_corner(m, id, d, x0, y1, z0, e, ni); cv[cs + 3] = interp_corner(m, id, d, x1, y1, z0, e, ni);
        cv[cs + 4] = interp_corner(m, id, d, x0, y0, z1, e, ni); cv[cs + 5] = interp_corner(m, id, d, x1, y0, z1, e, ni);
        cv[cs + 6] = interp_corner(m, id, d, x0, y1, z1, e, ni); cv[cs + 7] = interp_corner(m, id, d, x1, y1, z1, e, ni);
        m->cellKey[cs >> 3] = ck;
    }
    return lerp3((double) (x - x0) / 4, (double) (y - y0) / 8, (double) (z - z0) / 4, cv[cs], cv[cs + 1], cv[cs + 2], cv[cs + 3], cv[cs + 4], cv[cs + 5], cv[cs + 6], cv[cs + 7]);
}

static double flat_eval(Model* m, int id, const NodeDef* d, int x, int y, int z, Env* e) {
    if (!e->snap) return tree_eval(m, d->a, x, y, z, e);
    int qx = x >> 2, qz = z >> 2, rx = qx - e->fx, rz = qz - e->fz;
    if (rx < 0 || rz < 0 || rx >= 5 || rz >= 5) {
        if (((x | z) & 3) != 0) return tree_eval(m, d->a, x, y, z, e);
    }
    int64_t key = (int64_t) (((uint64_t) id << 54) ^ (((uint64_t) (int64_t) qx & 0x7FFFFFFULL) << 27) ^ ((uint64_t) (int64_t) qz & 0x7FFFFFFULL));
    double v;
    if (lmap_get(&m->flat, key, &v)) return v;
    v = tree_eval(m, d->a, qx << 2, 0, qz << 2, env_noi(e));
    lmap_put(&m->flat, key, v);
    return v;
}

static double c2d_eval(Model* m, int id, const NodeDef* d, int x, int y, int z, Env* e) {
    int64_t key = (int64_t) (((uint64_t) id << 54) ^ (((uint64_t) (int64_t) x & 0x7FFFFFFULL) << 27) ^ ((uint64_t) (int64_t) z & 0x7FFFFFFULL));
    uint64_t h = (uint64_t) key * GOLD64;
    int slot = (int) (h >> (64 - C2D_BITS));
    int64_t ek = ((x | z) & 3) == 0 ? 0 : (int64_t) (((uint64_t) (int64_t) e->fx << 34) ^ (((uint64_t) (int64_t) e->fz & 0xFFFFFFFFULL) << 2) ^ (e->snap ? 2ULL : 0ULL) ^ (e->interp ? 1ULL : 0ULL));
    int64_t* t = m->c2d; size_t si = (size_t) slot << 2;
    if (t[si] == key && t[si + 1] == ek) { double v; memcpy(&v, &t[si + 2], 8); return v; }
    double v = tree_eval(m, d->a, x, y, z, e);
    t[si] = key; t[si + 1] = ek; memcpy(&t[si + 2], &v, 8);
    return v;
}

double tree_eval(Model* m, int n, int x, int y, int z, Env* e) {
    const NodeDef* d = &TREE_NODES[n];
    switch (d->type) {
        case T_CONST: return d->v[0];
        case T_ADD: { double a = tree_eval(m, d->a, x, y, z, e); return a + tree_eval(m, d->b, x, y, z, e); }
        case T_MUL: { double a = tree_eval(m, d->a, x, y, z, e); return a == 0.0 ? 0.0 : a * tree_eval(m, d->b, x, y, z, e); }
        case T_MIN: {
            double a = tree_eval(m, d->a, x, y, z, e);
            const NodeDef* b = &TREE_NODES[d->b];
            if (a <= (b->hasLb ? b->lb : -INFINITY)) return a;
            return jmin(a, tree_eval(m, d->b, x, y, z, e));
        }
        case T_MAX: { double a = tree_eval(m, d->a, x, y, z, e); return jmax(a, tree_eval(m, d->b, x, y, z, e)); }
        case T_MAPPED: {
            double v = tree_eval(m, d->a, x, y, z, e);
            switch (d->b) {
                case 0: return fabs(v);
                case 1: return v * v;
                case 2: return v * v * v;
                case 3: return v > 0.0 ? v : v * 0.5;
                case 4: return v > 0.0 ? v : v * 0.25;
                case 5: return 1.0 / v;
                default: { double c = jclamp(v, -1.0, 1.0); return c / 2.0 - c * c * c / 24.0; }
            }
        }
        case T_CLAMP: return jclamp(tree_eval(m, d->a, x, y, z, e), d->v[0], d->v[1]);
        case T_YCLAMPED: return clamped_map((double) y, (double) d->a, (double) d->b, d->v[0], d->v[1]);
        case T_NOISE: return noise_normal(&m->w, d->a, x * d->v[0], y * d->v[1], z * d->v[0]);
        case T_SHIFTED: {
            double sx = tree_eval(m, d->a, x, y, z, e), sy = tree_eval(m, d->b, x, y, z, e), sz = tree_eval(m, d->c, x, y, z, e);
            double px = x * d->v[0] + sx, py = y * d->v[1] + sy, pz = z * d->v[0] + sz;
            return noise_normal(&m->w, d->d, px, py, pz);
        }
        case T_SHIFT: {
            double a, b, c;
            if (d->b == 0) { a = x; b = 0.0; c = z; } else { a = z; b = x; c = 0.0; }
            return noise_normal(&m->w, d->a, a * 0.25, b * 0.25, c * 0.25) * 4.0;
        }
        case T_WEIRD: {
            double in = tree_eval(m, d->a, x, y, z, e);
            double r = d->c == 1 ? rarity3d(in) : rarity2d(in);
            return r * fabs(noise_normal(&m->w, d->b, x / r, y / r, z / r));
        }
        case T_RANGE: {
            double v = tree_eval(m, d->a, x, y, z, e);
            return v >= d->v[0] && v < d->v[1] ? tree_eval(m, d->b, x, y, z, e) : tree_eval(m, d->c, x, y, z, e);
        }
        case T_OLDBLENDED: return blended(&m->w, x, y, z);
        case T_PASS: return tree_eval(m, d->a, x, y, z, e);
        case T_SPLINE: return (double) spline_apply(m, d->a, x, y, z, e);
        case T_FINDTOP: {
            int top = ifloor(tree_eval(m, d->b, x, y, z, e) / d->d) * d->d;
            if (top <= d->c) return (double) d->c;
            Env* ni = env_noi(e);
            for (int yy = top; yy >= d->c; yy -= d->d) if (tree_eval(m, d->a, x, yy, z, ni) > 0.0) return (double) yy;
            return (double) d->c;
        }
        case T_INTERP: return interp_eval(m, n, d, x, y, z, e);
        case T_FLAT: return flat_eval(m, n, d, x, y, z, e);
        case T_C2D: return c2d_eval(m, n, d, x, y, z, e);
    }
    return 0.0;
}

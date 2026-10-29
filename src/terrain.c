#include "terrain.h"

#define SEA_LEVEL 63
#define GLOBAL_LAVA_BELOW (-54)
#define MIN_Y (-64)
#define HEIGHT 384
#define WAY_BELOW_MIN_Y (MIN_Y * 2)
#define GOLD64 0x9E3779B97F4A7C15ULL

static const Fluid GLOBAL_LAVA = {-54, B_LAVA}, GLOBAL_SEA = {SEA_LEVEL, B_WATER};
static Fluid global_fluid(int y) { return y < GLOBAL_LAVA_BELOW ? GLOBAL_LAVA : GLOBAL_SEA; }
static int fluid_at(Fluid f, int y) { return y < f.level ? f.type : B_AIR; }

static int floor_div(int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) q--; return q; }
static int grid_y(int v) { return floor_div(v, 12); }
static int64_t as_long(int x, int y, int z) { return (int64_t) ((((uint64_t) (int64_t) x & 0x3FFFFFFULL) << 38) | ((uint64_t) (int64_t) y & 0xFFFULL) | (((uint64_t) (int64_t) z & 0x3FFFFFFULL) << 12)); }
static int loc_x(int64_t l) { return (int) (l >> 38); }
static int loc_y(int64_t l) { return (int) ((int64_t) ((uint64_t) l << 52) >> 52); }
static int loc_z(int64_t l) { return (int) ((int64_t) ((uint64_t) l << 26) >> 38); }

static void aquifer_init(Aquifer* a, Model* m, int chunkX, int chunkZ) {
    memset(a, 0, sizeof *a);
    a->m = m; env_init(&a->env, m, 0, 1, chunkX, chunkZ);
    int minBlockX = chunkX * 16, minBlockZ = chunkZ * 16;
    a->minGridX = (minBlockX - 5) >> 4; a->maxGX = ((minBlockX + 15 - 5) >> 4) + 1; a->gridSizeX = a->maxGX - a->minGridX + 1;
    a->minGridY = grid_y(MIN_Y + 1) - 1; int maxGY = grid_y(MIN_Y + HEIGHT + 1) + 1; int sizeY = maxGY - a->minGridY + 1;
    a->minGridZ = (minBlockZ - 5) >> 4; a->maxGZ = ((minBlockZ + 15 - 5) >> 4) + 1; a->gridSizeZ = a->maxGZ - a->minGridZ + 1;
    a->n = a->gridSizeX * sizeY * a->gridSizeZ;
    a->cache = (Fluid*) malloc(sizeof(Fluid) * a->n); a->known = (uint8_t*) calloc(a->n, 1); a->loc = (int64_t*) malloc(sizeof(int64_t) * a->n);
    for (int i = 0; i < a->n; i++) a->loc[i] = INT64_MAX;
}
static void aquifer_free(Aquifer* a) { env_free(&a->env); free(a->cache); free(a->known); free(a->loc); }

static int aq_index(const Aquifer* a, int gx, int gy, int gz) { return ((gy - a->minGridY) * a->gridSizeZ + (gz - a->minGridZ)) * a->gridSizeX + (gx - a->minGridX); }
static int adjust_surface(int v) { return v + 8; }

static int prelim_level(Aquifer* a, int x, int z) {
    int qx = (x >> 2) << 2, qz = (z >> 2) << 2;
    int64_t key = (int64_t) (((((uint64_t) (int64_t) qx << 32) ^ ((uint64_t) (int64_t) qz & 0xffffffffULL))) * GOLD64);
    double v;
    if (lmap_get(&a->m->prelim, key, &v)) return (int) v;
    int r = ifloor(tree_eval(a->m, TREE_PRELIM, qx, 0, qz, &a->env));
    lmap_put(&a->m->prelim, key, (double) r);
    return r;
}
static int max_prelim(Aquifer* a, int x0, int z0, int x1, int z1) {
    int m = INT32_MIN;
    for (int z = z0; z <= z1; z += 4) for (int x = x0; x <= x1; x += 4) { int v = prelim_level(a, x, z); if (v > m) m = v; }
    return m;
}
static int skip_above(Aquifer* a) {
    if (!a->skipKnown) {
        int top = adjust_surface(max_prelim(a, (a->minGridX << 4) + 0, (a->minGridZ << 4) + 0, (a->maxGX << 4) + 9, (a->maxGZ << 4) + 9));
        int g = grid_y(top + 12) + 1;
        a->skipY = g * 12 + 11 - 1; a->skipKnown = 1;
    }
    return a->skipY;
}

static double similarity(int a, int b) { return 1.0 - (b - a) / 25.0; }
static double lerp_(double t, double a, double b) { return a + t * (b - a); }
static double map_(double v, double a, double b, double lo, double hi) { return lerp_((v - a) / (b - a), lo, hi); }

static int compute_surface_level(Aquifer* a, int x, int y, int z, Fluid g, int minSurface, int found) {
    Model* m = a->m; Env* e = &a->env;
    double lo_a, hi_b;
    if (tree_eval(m, TREE_EROSION, x, y, z, e) < (double) -0.225f && tree_eval(m, TREE_DEPTH, x, y, z, e) > (double) 0.9f) { lo_a = -1.0; hi_b = -1.0; }
    else {
        int rel = minSurface + 8 - y;
        double f = found ? clamped_map((double) rel, 0.0, 64.0, 1.0, 0.0) : 0.0;
        double flood = jclamp(tree_eval(m, TREE_FLOODEDNESS, x, y, z, e), -1.0, 1.0);
        double lo = map_(f, 1.0, 0.0, -0.3, 0.8), hi = map_(f, 1.0, 0.0, -0.8, 0.4);
        lo_a = flood - hi; hi_b = flood - lo;
    }
    if (hi_b > 0.0) return g.level;
    if (lo_a > 0.0) {
        int gx = floor_div(x, 16), gy = floor_div(y, 40), gz = floor_div(z, 16), mid = gy * 40 + 20;
        double v = tree_eval(m, TREE_SPREAD, gx, gy, gz, e) * 10.0;
        int q = ifloor(v / 3) * 3;
        int r = mid + q;
        return minSurface < r ? minSurface : r;
    }
    return WAY_BELOW_MIN_Y;
}
static int compute_fluid_type(Aquifer* a, int x, int y, int z, Fluid g, int level) {
    int type = g.type;
    if (level <= -10 && level != WAY_BELOW_MIN_Y && g.type != B_LAVA) {
        double v = tree_eval(a->m, TREE_LAVA, floor_div(x, 64), floor_div(y, 40), floor_div(z, 64), &a->env);
        if (fabs(v) > 0.3) type = B_LAVA;
    }
    return type;
}
static const int OFFS[13][2] = {{0, 0}, {-2, -1}, {-1, -1}, {0, -1}, {1, -1}, {-3, 0}, {-2, 0}, {-1, 0}, {1, 0}, {-2, 1}, {-1, 1}, {0, 1}, {1, 1}};
static Fluid compute_fluid(Aquifer* a, int x, int y, int z) {
    Fluid g = global_fluid(y);
    int minSurface = INT32_MAX, above = y + 12, below = y - 12, found = 0;
    for (int i = 0; i < 13; i++) {
        int sx = x + OFFS[i][0] * 16, sz = z + OFFS[i][1] * 16;
        int prelim = prelim_level(a, sx, sz), surface = adjust_surface(prelim);
        int center = OFFS[i][0] == 0 && OFFS[i][1] == 0;
        if (center && below > surface) return g;
        int aboveSurface = above > surface;
        if (aboveSurface || center) {
            Fluid gf = global_fluid(surface);
            if (fluid_at(gf, surface) != B_AIR) { if (center) found = 1; if (aboveSurface) return gf; }
        }
        if (prelim < minSurface) minSurface = prelim;
    }
    Fluid r;
    r.level = compute_surface_level(a, x, y, z, g, minSurface, found);
    r.type = compute_fluid_type(a, x, y, z, g, r.level);
    return r;
}
static Fluid status(Aquifer* a, int idx) {
    if (a->known[idx]) return a->cache[idx];
    int64_t l = a->loc[idx];
    Fluid f = compute_fluid(a, loc_x(l), loc_y(l), loc_z(l));
    a->cache[idx] = f; a->known[idx] = 1;
    return f;
}
static double pressure(Aquifer* aq, int x, int y, int z, double* cachedBarrier, Fluid a, Fluid b) {
    int ta = fluid_at(a, y), tb = fluid_at(b, y);
    if ((ta == B_LAVA && tb == B_WATER) || (ta == B_WATER && tb == B_LAVA)) return 2.0;
    int diff = abs(a.level - b.level);
    if (diff == 0) return 0.0;
    double mid = 0.5 * (a.level + b.level), rel = y + 0.5 - mid, half = diff / 2.0, edge = half - fabs(rel), scaled;
    if (rel > 0.0) { double t = 0.0 + edge; scaled = t > 0.0 ? t / 1.5 : t / 2.5; }
    else { double t = 3.0 + edge; scaled = t > 0.0 ? t / 3.0 : t / 10.0; }
    double barrierValue;
    if (!(scaled < -2.0) && !(scaled > 2.0)) {
        double cached = *cachedBarrier;
        if (cached != cached) { barrierValue = tree_eval(aq->m, TREE_BARRIER, x, y, z, &aq->env); *cachedBarrier = barrierValue; } else barrierValue = cached;
    } else barrierValue = 0.0;
    return 2.0 * (barrierValue + scaled);
}

static int aquifer_substance(Aquifer* a, int x, int y, int z, double density) {
    if (density > 0.0) return B_SOLID;
    Fluid global = global_fluid(y);
    if (y > skip_above(a)) return fluid_at(global, y);
    if (fluid_at(global, y) == B_LAVA) return B_LAVA;
    int gx0 = (x - 5) >> 4, gy0 = grid_y(y + 1), gz0 = (z - 5) >> 4;
    int d1 = INT32_MAX, d2 = INT32_MAX, d3 = INT32_MAX, d4 = INT32_MAX, i1 = 0, i2 = 0, i3 = 0, i4 = 0;
    for (int dx = 0; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) for (int dz = 0; dz <= 1; dz++) {
        int gx = gx0 + dx, gy = gy0 + dy, gz = gz0 + dz, idx = aq_index(a, gx, gy, gz);
        int64_t loc = a->loc[idx];
        if (loc == INT64_MAX) {
            Xo r; pos_at(&a->m->ns->aquifer, gx, gy, gz, &r);
            int ox = xo_int(&r, 10), oy = xo_int(&r, 9), oz = xo_int(&r, 10);
            loc = as_long((gx << 4) + ox, gy * 12 + oy, (gz << 4) + oz);
            a->loc[idx] = loc;
        }
        int ox = loc_x(loc) - x, oy = loc_y(loc) - y, oz = loc_z(loc) - z, d = ox * ox + oy * oy + oz * oz;
        if (d1 >= d) { i4 = i3; i3 = i2; i2 = i1; i1 = idx; d4 = d3; d3 = d2; d2 = d1; d1 = d; }
        else if (d2 >= d) { i4 = i3; i3 = i2; i2 = idx; d4 = d3; d3 = d2; d2 = d; }
        else if (d3 >= d) { i4 = i3; i3 = idx; d4 = d3; d3 = d; }
        else if (d4 >= d) { i4 = idx; d4 = d; }
    }
    (void) i4; (void) d4;
    Fluid s1 = status(a, i1);
    double sim12 = similarity(d1, d2);
    int block = fluid_at(s1, y);
    if (sim12 <= 0.0) return block;
    if (block == B_WATER && fluid_at(global_fluid(y - 1), y - 1) == B_LAVA) return block;
    double pn = NAN;
    Fluid s2 = status(a, i2);
    double p12 = sim12 * pressure(a, x, y, z, &pn, s1, s2);
    if (density + p12 > 0.0) return B_SOLID;
    Fluid s3 = status(a, i3);
    double sim13 = similarity(d1, d3);
    if (sim13 > 0.0) { double p13 = sim12 * sim13 * pressure(a, x, y, z, &pn, s1, s3); if (density + p13 > 0.0) return B_SOLID; }
    double sim23 = similarity(d2, d3);
    if (sim23 > 0.0) { double p23 = sim12 * sim23 * pressure(a, x, y, z, &pn, s2, s3); if (density + p23 > 0.0) return B_SOLID; }
    return block;
}

#define NB 8192
static int64_t chunk_key(int cx, int cz) { return (int64_t) (((uint64_t) (int64_t) cx << 32) ^ ((uint64_t) (int64_t) cz & 0xffffffffULL)); }
static int bucket_of(int cx, int cz) { return (int) (((uint64_t) chunk_key(cx, cz) * GOLD64) >> 51) & (NB - 1); }

void terrain_init(Terrain* t, const NoiseSet* ns, int64_t seed, int mineshafts) {
    memset(t, 0, sizeof *t);
    t->ns = ns; t->seed = seed; t->carvers = 1;
    model_init(&t->m, ns, seed);
    t->nb = NB; t->buckets = (Chunk**) calloc(NB, sizeof(Chunk*));
    t->ctiles = (CTile**) calloc(NB, sizeof(CTile*));
    t->lastCx = INT32_MIN;
    if (mineshafts) t->overlay = overlay_new(seed, t);
}
static void free_chunks(Terrain* t) {
    for (int i = 0; i < t->nb; i++) {
        Chunk* c = t->buckets[i];
        while (c) { Chunk* n = c->next; aquifer_free(&c->aq); env_free(&c->env); free(c); c = n; }
        t->buckets[i] = NULL;
        CTile* k = t->ctiles[i];
        while (k) { CTile* n = k->next; free(k->data); free(k); k = n; }
        t->ctiles[i] = NULL;
    }
    t->last = NULL;
}
void terrain_free(Terrain* t) {
    free_chunks(t); free(t->buckets); free(t->ctiles); model_free(&t->m);
    if (t->overlay) overlay_free(t->overlay);
}
void terrain_drop_chunks(Terrain* t) {
    free_chunks(t); model_clear_caches(&t->m);
    if (t->overlay) overlay_clear(t->overlay);
}
static Chunk* chunk_get(Terrain* t, int cx, int cz) {
    if (cx == t->lastCx && cz == t->lastCz && t->last) return t->last;
    int b = bucket_of(cx, cz);
    Chunk* c = t->buckets[b];
    while (c && !(c->cx == cx && c->cz == cz)) c = c->next;
    if (!c) {
        c = (Chunk*) calloc(1, sizeof(Chunk));
        c->cx = cx; c->cz = cz;
        aquifer_init(&c->aq, &t->m, cx, cz);
        env_init(&c->env, &t->m, 1, 1, cx, cz);
        c->next = t->buckets[b]; t->buckets[b] = c;
    }
    t->lastCx = cx; t->lastCz = cz; t->last = c;
    return c;
}
#define MASK_BYTES (16 * 16 * 384 / 8)
static const uint8_t* carved_of(Terrain* t, Chunk* c) {
    if (!c->carved) {
        int tx = c->cx >> 3, tz = c->cz >> 3, b = bucket_of(tx, tz);
        CTile* k = t->ctiles[b];
        while (k && !(k->tx == tx && k->tz == tz)) k = k->next;
        if (!k) {
            k = (CTile*) malloc(sizeof(CTile));
            k->tx = tx; k->tz = tz; k->data = (uint8_t*) malloc((size_t) 64 * MASK_BYTES);
            carve_tile(t->seed, tx * 8, tz * 8, k->data);
            k->next = t->ctiles[b]; t->ctiles[b] = k;
        }
        c->carved = k->data + (size_t) (((c->cx - tx * 8) * 8) + (c->cz - tz * 8)) * MASK_BYTES;
    }
    return c->carved;
}

void terrain_clear_rooms(Terrain* t) { t->nRooms = 0; }
void terrain_add_room(Terrain* t, int x, int y, int z, int xr, int zr) {
    if (t->nRooms == 16) return;
    int* r = &t->rooms[t->nRooms++ * 6];
    r[0] = x - xr; r[1] = y; r[2] = z - zr; r[3] = x + xr; r[4] = y + 3; r[5] = z + zr;
}

double terrain_density(Terrain* t, int x, int y, int z) {
    Chunk* ch = chunk_get(t, x >> 4, z >> 4);
    return tree_eval(&t->m, TREE_FINAL, x, y, z, &ch->env);
}

int terrain_base_class(Terrain* t, int x, int y, int z) {
    Chunk* ch = chunk_get(t, x >> 4, z >> 4);
    double d = tree_eval(&t->m, TREE_FINAL, x, y, z, &ch->env);
    int s = aquifer_substance(&ch->aq, x, y, z, d);
    if (t->carvers && (s == B_SOLID || s == B_WATER)) {
        int i = ((y - MIN_Y) * 16 + (z & 15)) * 16 + (x & 15);
        if ((carved_of(t, ch)[i >> 3] >> (i & 7)) & 1) {
            int cs = y <= -56 ? B_LAVA : aquifer_substance(&ch->aq, x, y, z, 0.0);
            if (cs != B_SOLID) s = cs;
        }
    }
    return s;
}

enum { OV_CAVE_AIR = 1, OV_PLANKS = 2, OV_LOG = 3, OV_FENCE = 4, OV_CHAIN = 8, OV_SPAWNER = 9 };
int terrain_block_class(Terrain* t, int x, int y, int z) {
    for (int i = 0, o = 0; i < t->nRooms; i++, o += 6) {
        const int* r = &t->rooms[o];
        if (x >= r[0] && x <= r[3] && y >= r[1] && y <= r[4] && z >= r[2] && z <= r[5]) return B_AIR;
    }
    if (t->overlay) {
        int c = overlay_code(t->overlay, x, y, z);
        if (c != 0) return c == OV_CAVE_AIR ? B_AIR : (c == OV_PLANKS || c == OV_LOG || c == OV_FENCE || c == OV_CHAIN || c == OV_SPAWNER) ? B_SOLID : B_WATER;
    }
    return terrain_base_class(t, x, y, z);
}

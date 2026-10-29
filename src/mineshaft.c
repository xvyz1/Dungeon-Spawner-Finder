#include "terrain.h"
#include "xr.h"
#include "mineshaft.h"

#define MULT 0x5DEECE66DULL
#define MASK ((1ULL << 48) - 1)
enum { N_ = 0, S_ = 1, W_ = 2, E_ = 3 };
enum { AIR = 0, CAVE_AIR = 1, PLANKS = 2, LOG = 3, FENCE = 4, COBWEB = 5, RAIL = 6, TORCH = 7, CHAIN = 8, SPAWNER = 9, ROCK = 10, WATER = 11, LAVA = 12 };

typedef struct { uint64_t s; } Rnd;
static void rnd_set(Rnd* r, int64_t seed) { r->s = ((uint64_t) seed ^ MULT) & MASK; }
static int rnd_next(Rnd* r, int bits) { r->s = (r->s * MULT + 0xBULL) & MASK; return (int) (int64_t) (r->s >> (48 - bits)); }
static int rnd_int(Rnd* r, int bound) {
    if ((bound & -bound) == bound) return (int) (((int64_t) bound * (int64_t) rnd_next(r, 31)) >> 31);
    int bits, val;
    do { bits = rnd_next(r, 31); val = bits % bound; } while ((int32_t) ((uint32_t) bits - (uint32_t) val + (uint32_t) (bound - 1)) < 0);
    return val;
}
static int rnd_bool(Rnd* r) { return rnd_next(r, 1) != 0; }
static int64_t rnd_long(Rnd* r) { int a = rnd_next(r, 32), b = rnd_next(r, 32); return (int64_t) (((uint64_t) (int64_t) a << 32) + (uint64_t) (int64_t) b); }
static double rnd_double(Rnd* r) { int64_t hi = (int64_t) rnd_next(r, 26) << 27; return (double) (hi + rnd_next(r, 27)) * 1.1102230246251565E-16; }
static void rnd_large(Rnd* r, int64_t world, int cx, int cz) {
    rnd_set(r, world);
    int64_t a = rnd_long(r), b = rnd_long(r);
    rnd_set(r, (int64_t) ((uint64_t) (int64_t) cx * (uint64_t) a ^ (uint64_t) (int64_t) cz * (uint64_t) b ^ (uint64_t) world));
}

int mineshaft_starts_here(int64_t seed, int cx, int cz) { Rnd q; rnd_large(&q, seed, cx, cz); return rnd_double(&q) < 0.004; }

static int intersects(const Piece* p, int a0, int b0, int c0, int a1, int b1, int c1) { return p->x1 >= a0 && p->x0 <= a1 && p->z1 >= c0 && p->z0 <= c1 && p->y1 >= b0 && p->y0 <= b1; }
static int xs(const Piece* p) { return p->x1 - p->x0 + 1; }
static int ys_(const Piece* p) { return p->y1 - p->y0 + 1; }
static int zs(const Piece* p) { return p->z1 - p->z0 + 1; }

typedef struct { Rnd r; Tree* t; Piece* start; } Bld;

static Piece* new_piece(char kind, int facing, int depth, int x0, int y0, int z0, int x1, int y1, int z1) {
    Piece* p = (Piece*) calloc(1, sizeof(Piece));
    p->kind = kind; p->facing = facing; p->depth = depth; p->x0 = x0; p->y0 = y0; p->z0 = z0; p->x1 = x1; p->y1 = y1; p->z1 = z1;
    p->twoFloored = kind == 'X' && y1 - y0 + 1 > 3;
    return p;
}
static void tree_add(Tree* t, Piece* p) {
    if (t->n == t->cap) { t->cap = t->cap ? t->cap * 2 : 32; t->p = (Piece**) realloc(t->p, sizeof(Piece*) * t->cap); }
    t->p[t->n++] = p;
}
static int collision(Bld* b, int a0, int b0, int c0, int a1, int b1, int c1) {
    for (int i = 0; i < b->t->n; i++) if (intersects(b->t->p[i], a0, b0, c0, a1, b1, c1)) return 1;
    return 0;
}
static void children(Bld* b, Piece* p);

static Piece* create_random(Bld* b, int x, int y, int z, int dir, int depth) {
    Rnd* r = &b->r;
    int v = rnd_int(r, 100);
    if (v >= 80) {
        int h = rnd_int(r, 4) == 0 ? 6 : 2; int bb[6];
        switch (dir) {
            case S_: { int t[6] = {-1, 0, 0, 3, h, 4}; memcpy(bb, t, sizeof t); break; }
            case W_: { int t[6] = {-4, 0, -1, 0, h, 3}; memcpy(bb, t, sizeof t); break; }
            case E_: { int t[6] = {0, 0, -1, 4, h, 3}; memcpy(bb, t, sizeof t); break; }
            default: { int t[6] = {-1, 0, -4, 3, h, 0}; memcpy(bb, t, sizeof t); }
        }
        if (collision(b, bb[0] + x, bb[1] + y, bb[2] + z, bb[3] + x, bb[4] + y, bb[5] + z)) return NULL;
        return new_piece('X', dir, depth, bb[0] + x, bb[1] + y, bb[2] + z, bb[3] + x, bb[4] + y, bb[5] + z);
    } else if (v >= 70) {
        int bb[6];
        switch (dir) {
            case S_: { int t[6] = {0, -5, 0, 2, 2, 8}; memcpy(bb, t, sizeof t); break; }
            case W_: { int t[6] = {-8, -5, 0, 0, 2, 2}; memcpy(bb, t, sizeof t); break; }
            case E_: { int t[6] = {0, -5, 0, 8, 2, 2}; memcpy(bb, t, sizeof t); break; }
            default: { int t[6] = {0, -5, -8, 2, 2, 0}; memcpy(bb, t, sizeof t); }
        }
        if (collision(b, bb[0] + x, bb[1] + y, bb[2] + z, bb[3] + x, bb[4] + y, bb[5] + z)) return NULL;
        return new_piece('S', dir, depth, bb[0] + x, bb[1] + y, bb[2] + z, bb[3] + x, bb[4] + y, bb[5] + z);
    } else {
        int found[6], have = 0;
        for (int k = rnd_int(r, 3) + 2; k > 0; k--) {
            int len = k * 5, bb[6];
            switch (dir) {
                case S_: { int t[6] = {0, 0, 0, 2, 2, len - 1}; memcpy(bb, t, sizeof t); break; }
                case W_: { int t[6] = {-(len - 1), 0, 0, 0, 2, 2}; memcpy(bb, t, sizeof t); break; }
                case E_: { int t[6] = {0, 0, 0, len - 1, 2, 2}; memcpy(bb, t, sizeof t); break; }
                default: { int t[6] = {0, 0, -(len - 1), 2, 2, 0}; memcpy(bb, t, sizeof t); }
            }
            bb[0] += x; bb[1] += y; bb[2] += z; bb[3] += x; bb[4] += y; bb[5] += z;
            if (!collision(b, bb[0], bb[1], bb[2], bb[3], bb[4], bb[5])) { memcpy(found, bb, sizeof bb); have = 1; break; }
        }
        if (!have) return NULL;
        int rails = rnd_int(r, 3) == 0;
        int spider = !rails && rnd_int(r, 23) == 0;
        Piece* cp = new_piece('C', dir, depth, found[0], found[1], found[2], found[3], found[4], found[5]);
        cp->rails = rails; cp->spider = spider;
        return cp;
    }
}

static Piece* generate_and_add(Bld* b, int x, int y, int z, int dir, int depth) {
    if (depth > 8) return NULL;
    if (abs(x - b->start->x0) <= 80 && abs(z - b->start->z0) <= 80) {
        Piece* p = create_random(b, x, y, z, dir, depth + 1);
        if (p) { tree_add(b->t, p); children(b, p); }
        return p;
    }
    return NULL;
}
static void add_ent(Piece* p, int a, int b, int c, int d, int e, int f) {
    int* q = p->ent[p->nEnt++];
    q[0] = a; q[1] = b; q[2] = c; q[3] = d; q[4] = e; q[5] = f;
}

static void children(Bld* b, Piece* p) {
    Rnd* r = &b->r;
    int d = p->depth;
    switch (p->kind) {
        case 'C': {
            int v = rnd_int(r, 4), q;
            switch (p->facing) {
                case S_:
                    if (v <= 1) { q = rnd_int(r, 3); generate_and_add(b, p->x0, p->y0 - 1 + q, p->z1 + 1, p->facing, d); }
                    else if (v == 2) { q = rnd_int(r, 3); generate_and_add(b, p->x0 - 1, p->y0 - 1 + q, p->z1 - 3, W_, d); }
                    else { q = rnd_int(r, 3); generate_and_add(b, p->x1 + 1, p->y0 - 1 + q, p->z1 - 3, E_, d); }
                    break;
                case W_:
                    if (v <= 1) { q = rnd_int(r, 3); generate_and_add(b, p->x0 - 1, p->y0 - 1 + q, p->z0, p->facing, d); }
                    else if (v == 2) { q = rnd_int(r, 3); generate_and_add(b, p->x0, p->y0 - 1 + q, p->z0 - 1, N_, d); }
                    else { q = rnd_int(r, 3); generate_and_add(b, p->x0, p->y0 - 1 + q, p->z1 + 1, S_, d); }
                    break;
                case E_:
                    if (v <= 1) { q = rnd_int(r, 3); generate_and_add(b, p->x1 + 1, p->y0 - 1 + q, p->z0, p->facing, d); }
                    else if (v == 2) { q = rnd_int(r, 3); generate_and_add(b, p->x1 - 3, p->y0 - 1 + q, p->z0 - 1, N_, d); }
                    else { q = rnd_int(r, 3); generate_and_add(b, p->x1 - 3, p->y0 - 1 + q, p->z1 + 1, S_, d); }
                    break;
                default:
                    if (v <= 1) { q = rnd_int(r, 3); generate_and_add(b, p->x0, p->y0 - 1 + q, p->z0 - 1, p->facing, d); }
                    else if (v == 2) { q = rnd_int(r, 3); generate_and_add(b, p->x0 - 1, p->y0 - 1 + q, p->z0, W_, d); }
                    else { q = rnd_int(r, 3); generate_and_add(b, p->x1 + 1, p->y0 - 1 + q, p->z0, E_, d); }
            }
            if (d < 8) {
                if (p->facing != N_ && p->facing != S_) {
                    for (int x = p->x0 + 3; x + 3 <= p->x1; x += 5) { int qq = rnd_int(r, 5); if (qq == 0) generate_and_add(b, x, p->y0, p->z0 - 1, N_, d + 1); else if (qq == 1) generate_and_add(b, x, p->y0, p->z1 + 1, S_, d + 1); }
                } else {
                    for (int z = p->z0 + 3; z + 3 <= p->z1; z += 5) { int qq = rnd_int(r, 5); if (qq == 0) generate_and_add(b, p->x0 - 1, p->y0, z, W_, d + 1); else if (qq == 1) generate_and_add(b, p->x1 + 1, p->y0, z, E_, d + 1); }
                }
            }
            break;
        }
        case 'X': {
            switch (p->facing) {
                case S_: generate_and_add(b, p->x0 + 1, p->y0, p->z1 + 1, S_, d); generate_and_add(b, p->x0 - 1, p->y0, p->z0 + 1, W_, d); generate_and_add(b, p->x1 + 1, p->y0, p->z0 + 1, E_, d); break;
                case W_: generate_and_add(b, p->x0 + 1, p->y0, p->z0 - 1, N_, d); generate_and_add(b, p->x0 + 1, p->y0, p->z1 + 1, S_, d); generate_and_add(b, p->x0 - 1, p->y0, p->z0 + 1, W_, d); break;
                case E_: generate_and_add(b, p->x0 + 1, p->y0, p->z0 - 1, N_, d); generate_and_add(b, p->x0 + 1, p->y0, p->z1 + 1, S_, d); generate_and_add(b, p->x1 + 1, p->y0, p->z0 + 1, E_, d); break;
                default: generate_and_add(b, p->x0 + 1, p->y0, p->z0 - 1, N_, d); generate_and_add(b, p->x0 - 1, p->y0, p->z0 + 1, W_, d); generate_and_add(b, p->x1 + 1, p->y0, p->z0 + 1, E_, d);
            }
            if (p->twoFloored) {
                if (rnd_bool(r)) generate_and_add(b, p->x0 + 1, p->y0 + 3 + 1, p->z0 - 1, N_, d);
                if (rnd_bool(r)) generate_and_add(b, p->x0 - 1, p->y0 + 3 + 1, p->z0 + 1, W_, d);
                if (rnd_bool(r)) generate_and_add(b, p->x1 + 1, p->y0 + 3 + 1, p->z0 + 1, E_, d);
                if (rnd_bool(r)) generate_and_add(b, p->x0 + 1, p->y0 + 3 + 1, p->z1 + 1, S_, d);
            }
            break;
        }
        case 'S': {
            switch (p->facing) {
                case S_: generate_and_add(b, p->x0, p->y0, p->z1 + 1, S_, d); break;
                case W_: generate_and_add(b, p->x0 - 1, p->y0, p->z0, W_, d); break;
                case E_: generate_and_add(b, p->x1 + 1, p->y0, p->z0, E_, d); break;
                default: generate_and_add(b, p->x0, p->y0, p->z0 - 1, N_, d);
            }
            break;
        }
        default: {
            int ys = ys_(p) - 3 - 1;
            if (ys <= 0) ys = 1;
            int a = 0, q;
            while (a < xs(p)) { a += rnd_int(r, xs(p)); if (a + 3 > xs(p)) break; q = rnd_int(r, ys); Piece* c = generate_and_add(b, p->x0 + a, p->y0 + q + 1, p->z0 - 1, N_, d); if (c) add_ent(p, c->x0, c->y0, p->z0, c->x1, c->y1, p->z0 + 1); a += 4; }
            a = 0;
            while (a < xs(p)) { a += rnd_int(r, xs(p)); if (a + 3 > xs(p)) break; q = rnd_int(r, ys); Piece* c = generate_and_add(b, p->x0 + a, p->y0 + q + 1, p->z1 + 1, S_, d); if (c) add_ent(p, c->x0, c->y0, p->z1 - 1, c->x1, c->y1, p->z1); a += 4; }
            a = 0;
            while (a < zs(p)) { a += rnd_int(r, zs(p)); if (a + 3 > zs(p)) break; q = rnd_int(r, ys); Piece* c = generate_and_add(b, p->x0 - 1, p->y0 + q + 1, p->z0 + a, W_, d); if (c) add_ent(p, p->x0, c->y0, c->z0, p->x0 + 1, c->y1, c->z1); a += 4; }
            a = 0;
            while (a < zs(p)) { a += rnd_int(r, zs(p)); if (a + 3 > zs(p)) break; q = rnd_int(r, ys); Piece* c = generate_and_add(b, p->x1 + 1, p->y0 + q + 1, p->z0 + a, E_, d); if (c) add_ent(p, p->x1 - 1, c->y0, c->z0, p->x1, c->y1, c->z1); a += 4; }
        }
    }
}

Tree* mineshaft_tree(int64_t seed, int cx, int cz) {
    if (!mineshaft_starts_here(seed, cx, cz)) return NULL;
    Bld b; Tree* t = (Tree*) calloc(1, sizeof(Tree)); b.t = t;
    rnd_large(&b.r, seed, cx, cz);
    rnd_double(&b.r);
    int bx = cx * 16 + 2, bz = cz * 16 + 2;
    int x1 = bx + 7 + rnd_int(&b.r, 6);
    int y1 = 54 + rnd_int(&b.r, 6);
    int z1 = bz + 7 + rnd_int(&b.r, 6);
    Piece* room = new_piece('R', -1, 0, bx, 50, bz, x1, y1, z1);
    b.start = room; tree_add(t, room);
    children(&b, room);
    int minY = INT32_MAX, maxY = INT32_MIN;
    for (int i = 0; i < t->n; i++) { if (t->p[i]->y0 < minY) minY = t->p[i]->y0; if (t->p[i]->y1 > maxY) maxY = t->p[i]->y1; }
    int span = maxY - minY + 1, v = span + (-64) + 1;
    if (v < 53) v += rnd_int(&b.r, 53 - v);
    int dy = v - maxY;
    for (int i = 0; i < t->n; i++) {
        Piece* p = t->p[i]; p->y0 += dy; p->y1 += dy;
        for (int k = 0; k < p->nEnt; k++) { p->ent[k][1] += dy; p->ent[k][4] += dy; }
    }
    return t;
}
void mineshaft_tree_free(Tree* t) {
    if (!t) return;
    for (int i = 0; i < t->n; i++) free(t->p[i]);
    free(t->p); free(t);
}

typedef struct { int64_t* keys; void** vals; uint8_t* used; size_t cap, n; } HMap;
static void hm_init(HMap* m) { memset(m, 0, sizeof *m); }
static size_t hm_slot(const HMap* m, int64_t k) { return (size_t) (((uint64_t) k * 0x9E3779B97F4A7C15ULL) >> 20) & (m->cap - 1); }
static int hm_get(const HMap* m, int64_t k, void** out) {
    if (!m->cap) return 0;
    for (size_t i = hm_slot(m, k);; i = (i + 1) & (m->cap - 1)) {
        if (!m->used[i]) return 0;
        if (m->keys[i] == k) { *out = m->vals[i]; return 1; }
    }
}
static void hm_put(HMap* m, int64_t k, void* v);
static void hm_grow(HMap* m) {
    HMap o = *m;
    m->cap = o.cap ? o.cap * 2 : 1024; m->n = 0;
    m->keys = (int64_t*) malloc(sizeof(int64_t) * m->cap); m->vals = (void**) malloc(sizeof(void*) * m->cap); m->used = (uint8_t*) calloc(m->cap, 1);
    for (size_t i = 0; i < o.cap; i++) if (o.used[i]) hm_put(m, o.keys[i], o.vals[i]);
    free(o.keys); free(o.vals); free(o.used);
}
static void hm_put(HMap* m, int64_t k, void* v) {
    if (m->n * 2 >= m->cap) hm_grow(m);
    size_t i = hm_slot(m, k);
    while (m->used[i] && m->keys[i] != k) i = (i + 1) & (m->cap - 1);
    if (!m->used[i]) { m->used[i] = 1; m->n++; }
    m->keys[i] = k; m->vals[i] = v;
}
static void hm_free(HMap* m) { free(m->keys); free(m->vals); free(m->used); hm_init(m); }

typedef struct { Tree** starts; int nStarts; int* rects; int nRects; uint8_t* ov; uint8_t* base; } Info;
typedef struct { int* v; int n; } StartList;

struct Overlay {
    int64_t seed; Terrain* tm; Xr rng;
    HMap trees, infos, startBlocks;
    int lastCx, lastCz; Info* last;
};

static int64_t okey(int x, int z) { return (int64_t) (((uint64_t) (int64_t) x << 32) ^ ((uint64_t) (int64_t) z & 0xffffffffULL)) * (int64_t) 0x9E3779B97F4A7C15ULL; }
static int fdiv(int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) q--; return q; }

Overlay* overlay_new(int64_t seed, Terrain* tm) {
    Overlay* o = (Overlay*) calloc(1, sizeof(Overlay));
    o->seed = seed; o->tm = tm; o->lastCx = INT32_MIN;
    hm_init(&o->trees); hm_init(&o->infos); hm_init(&o->startBlocks);
    return o;
}
static void free_info(Info* in) { free(in->starts); free(in->rects); free(in->ov); free(in->base); free(in); }
void overlay_clear(Overlay* o) {
    for (size_t i = 0; i < o->infos.cap; i++) if (o->infos.used[i]) free_info((Info*) o->infos.vals[i]);
    for (size_t i = 0; i < o->trees.cap; i++) if (o->trees.used[i]) mineshaft_tree_free((Tree*) o->trees.vals[i]);
    hm_free(&o->infos); hm_free(&o->trees);
    o->last = NULL;
}
void overlay_free(Overlay* o) {
    overlay_clear(o);
    for (size_t i = 0; i < o->startBlocks.cap; i++) if (o->startBlocks.used[i]) free(o->startBlocks.vals[i]);
    hm_free(&o->startBlocks);
    free(o);
}

static Tree* otree(Overlay* o, int cx, int cz) {
    int64_t k = okey(cx, cz); void* v;
    if (hm_get(&o->trees, k, &v)) return (Tree*) v;
    Tree* t = mineshaft_tree(o->seed, cx, cz);
    hm_put(&o->trees, k, t);
    return t;
}

int overlay_touches(Overlay* o, int x0, int y0, int z0, int x1, int y1, int z1) {
    for (int cx = fdiv(x0, 16) - 10; cx <= fdiv(x1, 16) + 10; cx++) for (int cz = fdiv(z0, 16) - 10; cz <= fdiv(z1, 16) + 10; cz++) {
        Tree* t = otree(o, cx, cz);
        if (!t) continue;
        for (int i = 0; i < t->n; i++) if (intersects(t->p[i], x0, y0, z0, x1, y1, z1)) return 1;
    }
    return 0;
}

static StartList* starts_of_block(Overlay* o, int bx, int bz) {
    int64_t k = okey(bx, bz); void* v;
    if (hm_get(&o->startBlocks, k, &v)) return (StartList*) v;
    StartList* s = (StartList*) calloc(1, sizeof(StartList));
    int cap = 16; s->v = (int*) malloc(sizeof(int) * cap);
    for (int sx = bx * 64; sx < bx * 64 + 64; sx++) for (int sz = bz * 64; sz < bz * 64 + 64; sz++) if (mineshaft_starts_here(o->seed, sx, sz)) {
        if (s->n + 2 > cap) { cap *= 2; s->v = (int*) realloc(s->v, sizeof(int) * cap); }
        s->v[s->n++] = sx; s->v[s->n++] = sz;
    }
    hm_put(&o->startBlocks, k, s);
    return s;
}
static int cmp_start(const void* a, const void* b) { const int* p = (const int*) a; const int* q = (const int*) b; if (p[0] != q[0]) return p[0] < q[0] ? -1 : 1; return p[1] < q[1] ? -1 : (p[1] > q[1] ? 1 : 0); }

static int touches_chunk(const Piece* p, int cx, int cz) { return p->x1 >= cx * 16 && p->x0 <= cx * 16 + 15 && p->z1 >= cz * 16 && p->z0 <= cz * 16 + 15; }
typedef struct { int* v; int n, cap; } IntList;
static void il_add(IntList* l, int x) { if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 32; l->v = (int*) realloc(l->v, sizeof(int) * l->cap); } l->v[l->n++] = x; }
static void add_rects(const Piece* p, IntList* r) {
    il_add(r, p->x0 - 2); il_add(r, p->z0 - 2); il_add(r, p->x1 + 2); il_add(r, p->z1 + 2);
    for (int i = 0; i < p->nEnt; i++) {
        const int* e = p->ent[i];
        il_add(r, (e[0] < e[3] ? e[0] : e[3]) - 2); il_add(r, (e[2] < e[5] ? e[2] : e[5]) - 2);
        il_add(r, (e[0] > e[3] ? e[0] : e[3]) + 2); il_add(r, (e[2] > e[5] ? e[2] : e[5]) + 2);
    }
}

static Info* info(Overlay* o, int cx, int cz) {
    if (cx == o->lastCx && cz == o->lastCz && o->last) return o->last;
    int64_t k = okey(cx, cz); void* v; Info* in;
    if (hm_get(&o->infos, k, &v)) in = (Info*) v;
    else {
        in = (Info*) calloc(1, sizeof(Info));
        IntList near = {0}, r = {0};
        for (int bx = fdiv(cx - 10, 64); bx <= fdiv(cx + 10, 64); bx++) for (int bz = fdiv(cz - 10, 64); bz <= fdiv(cz + 10, 64); bz++) {
            StartList* a = starts_of_block(o, bx, bz);
            for (int i = 0; i < a->n; i += 2) if (a->v[i] >= cx - 10 && a->v[i] <= cx + 10 && a->v[i + 1] >= cz - 10 && a->v[i + 1] <= cz + 10) { il_add(&near, a->v[i]); il_add(&near, a->v[i + 1]); }
        }
        qsort(near.v, near.n / 2, 2 * sizeof(int), cmp_start);
        in->starts = (Tree**) malloc(sizeof(Tree*) * (near.n / 2 + 1));
        for (int i = 0; i < near.n; i += 2) {
            Tree* t = otree(o, near.v[i], near.v[i + 1]);
            if (!t) continue;
            int any = 0;
            for (int j = 0; j < t->n; j++) if (touches_chunk(t->p[j], cx, cz)) { any = 1; add_rects(t->p[j], &r); }
            if (any) in->starts[in->nStarts++] = t;
        }
        in->rects = r.v; in->nRects = r.n;
        free(near.v);
        hm_put(&o->infos, k, in);
    }
    o->lastCx = cx; o->lastCz = cz; o->last = in;
    return in;
}

static int near_rect(const Info* in, int x, int z) {
    for (int i = 0; i < in->nRects; i += 4) if (x >= in->rects[i] && x <= in->rects[i + 2] && z >= in->rects[i + 1] && z <= in->rects[i + 3]) return 1;
    return 0;
}

typedef struct { Overlay* o; int cx, cz, bx0, bz0, bx1, bz1; uint8_t* ov; uint8_t* base; const Piece* p; } Sim;

static int inside(const Sim* s, int x, int y, int z) { return x >= s->bx0 && x <= s->bx1 && z >= s->bz0 && z <= s->bz1 && y >= -64 && y <= 319; }
static int idx3(int x, int y, int z) { return ((y + 64) << 8) | ((z & 15) << 4) | (x & 15); }
static int base_of(Sim* s, int x, int y, int z) {
    if (x < s->bx0 || x > s->bx1 || z < s->bz0 || z > s->bz1) return terrain_base_class(s->o->tm, x, y, z);
    uint8_t* m = &s->base[idx3(x, y, z)];
    if (*m == 0xFF) *m = (uint8_t) terrain_base_class(s->o->tm, x, y, z);
    return *m;
}
static int sget(Sim* s, int x, int y, int z) {
    if (y < -64 || y > 319) return AIR;
    int c = s->ov[idx3(x, y, z)];
    if (c != 0) return c;
    int b = base_of(s, x, y, z);
    return b == B_SOLID ? ROCK : b == B_AIR ? AIR : b == B_WATER ? WATER : LAVA;
}
static void sset(Sim* s, int x, int y, int z, int c) { if (inside(s, x, y, z)) s->ov[idx3(x, y, z)] = (uint8_t) c; }
static int is_air(int c) { return c == AIR || c == CAVE_AIR; }
static int liquid(int c) { return c == WATER || c == LAVA; }
static int sturdy_up(int c) { return c == ROCK || c == PLANKS || c == LOG; }
static int solid_render(int c) { return c == ROCK || c == PLANKS || c == LOG; }
static int replaceable(int c) { return is_air(c) || liquid(c); }
static int get_box(Sim* s, int x, int y, int z) { return inside(s, x, y, z) ? sget(s, x, y, z) : AIR; }

static int wx(const Sim* s, int lx, int lz) { const Piece* p = s->p; switch (p->facing) { case W_: return p->x1 - lz; case E_: return p->x0 + lz; case N_: case S_: return p->x0 + lx; default: return lx; } }
static int wy(const Sim* s, int ly) { return s->p->facing < 0 ? ly : ly + s->p->y0; }
static int wz(const Sim* s, int lx, int lz) { const Piece* p = s->p; switch (p->facing) { case N_: return p->z1 - lz; case S_: return p->z0 + lz; case W_: case E_: return p->z0 + lx; default: return lz; } }
static int can_be_replaced(Sim* s, int x, int y, int z) { int c = get_box(s, x, y, z); return c != PLANKS && c != LOG && c != FENCE && c != CHAIN; }
static void place(Sim* s, int c, int lx, int ly, int lz) { int x = wx(s, lx, lz), y = wy(s, ly), z = wz(s, lx, lz); if (inside(s, x, y, z) && can_be_replaced(s, x, y, z)) sset(s, x, y, z, c); }
static int block(Sim* s, int lx, int ly, int lz) { return get_box(s, wx(s, lx, lz), wy(s, ly), wz(s, lx, lz)); }
static int solid_at_or_above(Sim* s, int x, int y, int z) {
    for (int yy = y > -64 ? y : -64; yy <= 319; yy++) if (base_of(s, x, yy, z) == B_SOLID) return 1;
    return 0;
}
static int interior(Sim* s, int lx, int ly, int lz) { int x = wx(s, lx, lz), y = wy(s, ly + 1), z = wz(s, lx, lz); return inside(s, x, y, z) && solid_at_or_above(s, x, y, z); }
static void box(Sim* s, int x0, int y0, int z0, int x1, int y1, int z1, int c) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) for (int z = z0; z <= z1; z++) place(s, c, x, y, z); }
static float next_float(Sim* s) { return (float) xr_next(&s->o->rng, 24) / 16777216.0f; }
static void maybe_box(Sim* s, float prob, int x0, int y0, int z0, int x1, int y1, int z1, int edge, int fill, int needNonAir, int needInterior) {
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) for (int z = z0; z <= z1; z++) {
        if (!(next_float(s) > prob) && (!needNonAir || !is_air(block(s, x, y, z))) && (!needInterior || interior(s, x, y, z))) {
            int e = y == y0 || y == y1 || x == x0 || x == x1 || z == z0 || z == z1;
            place(s, e ? edge : fill, x, y, z);
        }
    }
}
static void maybe_block(Sim* s, float prob, int x, int y, int z, int c) { if (next_float(s) < prob) place(s, c, x, y, z); }

static int invalid(Sim* s) {
    const Piece* p = s->p;
    int x0 = p->x0 - 1 > s->bx0 ? p->x0 - 1 : s->bx0, y0 = p->y0 - 1 > -64 ? p->y0 - 1 : -64, z0 = p->z0 - 1 > s->bz0 ? p->z0 - 1 : s->bz0;
    int x1 = p->x1 + 1 < s->bx1 ? p->x1 + 1 : s->bx1, y1 = p->y1 + 1 < 319 ? p->y1 + 1 : 319, z1 = p->z1 + 1 < s->bz1 ? p->z1 + 1 : s->bz1;
    for (int x = x0; x <= x1; x++) for (int z = z0; z <= z1; z++) if (liquid(sget(s, x, y0, z)) || liquid(sget(s, x, y1, z))) return 1;
    for (int x = x0; x <= x1; x++) for (int y = y0; y <= y1; y++) if (liquid(sget(s, x, y, z0)) || liquid(sget(s, x, y, z1))) return 1;
    for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) if (liquid(sget(s, x0, y, z)) || liquid(sget(s, x1, y, z))) return 1;
    return 0;
}
static void set_planks(Sim* s, int lx, int ly, int lz) {
    if (interior(s, lx, ly, lz)) { int x = wx(s, lx, lz), y = wy(s, ly), z = wz(s, lx, lz); if (!sturdy_up(sget(s, x, y, z))) sset(s, x, y, z, PLANKS); }
}
static int sturdy_neighbours(Sim* s, int lx, int ly, int lz, int need) {
    int x = wx(s, lx, lz), y = wy(s, ly), z = wz(s, lx, lz), n = 0;
    static const int D[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};
    for (int i = 0; i < 6; i++) {
        int a = x + D[i][0], b = y + D[i][1], c = z + D[i][2];
        if (inside(s, a, b, c)) { int cc = sget(s, a, b, c); if (cc == ROCK || cc == PLANKS || cc == LOG) if (++n >= need) return 1; }
    }
    return 0;
}
static void cobweb(Sim* s, float prob, int lx, int ly, int lz) {
    if (interior(s, lx, ly, lz) && next_float(s) < prob && sturdy_neighbours(s, lx, ly, lz, 2)) place(s, COBWEB, lx, ly, lz);
}
static void chest(Sim* s, int lx, int ly, int lz) {
    int x = wx(s, lx, lz), y = wy(s, ly), z = wz(s, lx, lz);
    if (inside(s, x, y, z) && is_air(sget(s, x, y, z)) && !is_air(sget(s, x, y - 1, z))) { xr_next(&s->o->rng, 1); place(s, RAIL, lx, ly, lz); xr_long(&s->o->rng); }
}
static void support(Sim* s, int x0, int y0, int z, int h, int x1) {
    int ok = 1;
    for (int x = x0; x <= x1; x++) if (is_air(block(s, x, h + 1, z))) { ok = 0; break; }
    if (!ok) return;
    box(s, x0, y0, z, x0, h - 1, z, FENCE); box(s, x1, y0, z, x1, h - 1, z, FENCE);
    if (xr_int(&s->o->rng, 4) == 0) { box(s, x0, h, z, x0, h, z, PLANKS); box(s, x1, h, z, x1, h, z, PLANKS); }
    else { box(s, x0, h, z, x1, h, z, PLANKS); maybe_block(s, 0.05f, x0 + 1, h, z - 1, TORCH); maybe_block(s, 0.05f, x0 + 1, h, z + 1, TORCH); }
}
static void pillar(Sim* s, int lx, int ly, int lz) {
    int x = wx(s, lx, lz), y0 = wy(s, ly), z = wz(s, lx, lz);
    if (!inside(s, x, y0, z)) return;
    int down = 1, up = 1;
    for (int i = 1; down || up; i++) {
        if (down) {
            int y = y0 - i, c = sget(s, x, y, z), repl = replaceable(c) && c != LAVA;
            if (!repl && sturdy_up(c)) { for (int yy = y0 - i + 1; yy < y0; yy++) sset(s, x, yy, z, LOG); return; }
            down = i <= 20 && repl && y > -64 + 1;
        }
        if (up) {
            int y = y0 + i, c = sget(s, x, y, z), repl = replaceable(c);
            if (!repl && c == ROCK) { sset(s, x, y0 + 1, z, FENCE); for (int yy = y0 + 2; yy < y0 + i; yy++) sset(s, x, yy, z, CHAIN); return; }
            up = i <= 50 && repl && y < 319;
        }
    }
}
static void double_support(Sim* s, int lx, int ly, int lz) {
    if (block(s, lx, ly, lz) == PLANKS) pillar(s, lx, ly, lz);
    if (block(s, lx + 2, ly, lz) == PLANKS) pillar(s, lx + 2, ly, lz);
}
static void corridor(Sim* s) {
    const Piece* p = s->p;
    int sections = ((p->facing == N_ || p->facing == S_) ? zs(p) : xs(p)) / 5, len = sections * 5 - 1;
    int rails = p->rails, spider = p->spider, placedSpider = 0;
    box(s, 0, 0, 0, 2, 1, len, CAVE_AIR);
    maybe_box(s, 0.8f, 0, 2, 0, 2, 2, len, CAVE_AIR, CAVE_AIR, 0, 0);
    if (spider) maybe_box(s, 0.6f, 0, 0, 0, 2, 1, len, COBWEB, CAVE_AIR, 1, 1);
    for (int i = 0; i < sections; i++) {
        int z = 2 + i * 5;
        support(s, 0, 0, z, 2, 2);
        cobweb(s, 0.1f, 0, 2, z - 1); cobweb(s, 0.1f, 2, 2, z - 1); cobweb(s, 0.1f, 0, 2, z + 1); cobweb(s, 0.1f, 2, 2, z + 1);
        cobweb(s, 0.05f, 0, 2, z - 2); cobweb(s, 0.05f, 2, 2, z - 2); cobweb(s, 0.05f, 0, 2, z + 2); cobweb(s, 0.05f, 2, 2, z + 2);
        if (xr_int(&s->o->rng, 100) == 0) chest(s, 2, 0, z - 1);
        if (xr_int(&s->o->rng, 100) == 0) chest(s, 0, 0, z + 1);
        if (spider && !placedSpider) {
            int zz = z - 1 + xr_int(&s->o->rng, 3), x = wx(s, 1, zz), y = wy(s, 0), zw = wz(s, 1, zz);
            if (inside(s, x, y, zw) && interior(s, 1, 0, zz)) { placedSpider = 1; sset(s, x, y, zw, SPAWNER); }
        }
    }
    for (int x = 0; x <= 2; x++) for (int z = 0; z <= len; z++) set_planks(s, x, -1, z);
    double_support(s, 0, -1, 2);
    if (sections > 1) double_support(s, 0, -1, len - 2);
    if (rails) for (int z = 0; z <= len; z++) { int b = block(s, 1, -1, z); if (!is_air(b) && solid_render(b)) maybe_block(s, interior(s, 1, 0, z) ? 0.7f : 0.9f, 1, 0, z, RAIL); }
}
static int wcan_replace(Sim* s, int x, int y, int z) { int c = sget(s, x, y, z); return c != PLANKS && c != LOG && c != FENCE && c != CHAIN; }
static void wbox(Sim* s, int x0, int y0, int z0, int x1, int y1, int z1) {
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) for (int z = z0; z <= z1; z++) if (inside(s, x, y, z) && wcan_replace(s, x, y, z)) sset(s, x, y, z, CAVE_AIR);
}
static void pillar4(Sim* s, int x, int y0, int z, int y1) {
    if (!is_air(get_box(s, x, y1 + 1, z))) for (int y = y0; y <= y1; y++) if (inside(s, x, y, z) && wcan_replace(s, x, y, z)) sset(s, x, y, z, PLANKS);
}
static void crossing(Sim* s) {
    const Piece* p = s->p;
    int two = ys_(p) > 3;
    if (two) {
        wbox(s, p->x0 + 1, p->y0, p->z0, p->x1 - 1, p->y0 + 3 - 1, p->z1); wbox(s, p->x0, p->y0, p->z0 + 1, p->x1, p->y0 + 3 - 1, p->z1 - 1);
        wbox(s, p->x0 + 1, p->y1 - 2, p->z0, p->x1 - 1, p->y1, p->z1); wbox(s, p->x0, p->y1 - 2, p->z0 + 1, p->x1, p->y1, p->z1 - 1);
        wbox(s, p->x0 + 1, p->y0 + 3, p->z0 + 1, p->x1 - 1, p->y0 + 3, p->z1 - 1);
    } else { wbox(s, p->x0 + 1, p->y0, p->z0, p->x1 - 1, p->y1, p->z1); wbox(s, p->x0, p->y0, p->z0 + 1, p->x1, p->y1, p->z1 - 1); }
    pillar4(s, p->x0 + 1, p->y0, p->z0 + 1, p->y1); pillar4(s, p->x0 + 1, p->y0, p->z1 - 1, p->y1); pillar4(s, p->x1 - 1, p->y0, p->z0 + 1, p->y1); pillar4(s, p->x1 - 1, p->y0, p->z1 - 1, p->y1);
    int fy = p->y0 - 1;
    for (int x = p->x0; x <= p->x1; x++) for (int z = p->z0; z <= p->z1; z++) {
        int y1 = fy + 1;
        if (inside(s, x, y1, z) && solid_at_or_above(s, x, y1, z)) { if (inside(s, x, fy, z) && !sturdy_up(sget(s, x, fy, z))) sset(s, x, fy, z, PLANKS); }
    }
}
static void stairs(Sim* s) {
    box(s, 0, 5, 0, 2, 7, 1, CAVE_AIR); box(s, 0, 0, 7, 2, 2, 8, CAVE_AIR);
    for (int i = 0; i < 5; i++) box(s, 0, 5 - i - (i < 4 ? 1 : 0), 2 + i, 2, 7 - i, 2 + i, CAVE_AIR);
}
static void room(Sim* s) {
    const Piece* p = s->p;
    wbox(s, p->x0, p->y0 + 1, p->z0, p->x1, p->y0 + 3 < p->y1 ? p->y0 + 3 : p->y1, p->z1);
    for (int i = 0; i < p->nEnt; i++) { const int* e = p->ent[i]; wbox(s, e[0], e[4] - 2, e[2], e[3], e[4], e[5]); }
    float w = (float) (p->x1 - p->x0 + 1), h = (float) (p->y1 - (p->y0 + 4) + 1), dd = (float) (p->z1 - p->z0 + 1);
    float cx0 = (float) p->x0 + w / 2.0f, cz0 = (float) p->z0 + dd / 2.0f;
    for (int y = p->y0 + 4; y <= p->y1; y++) {
        float fy = (float) (y - (p->y0 + 4)) / h;
        for (int x = p->x0; x <= p->x1; x++) {
            float fx = ((float) x - cx0) / (w * 0.5f);
            for (int z = p->z0; z <= p->z1; z++) {
                float fz = ((float) z - cz0) / (dd * 0.5f);
                float v = fx * fx + fy * fy + fz * fz;
                if (v <= 1.05f && inside(s, x, y, z) && wcan_replace(s, x, y, z)) sset(s, x, y, z, CAVE_AIR);
            }
        }
    }
}
static void post(Sim* s, const Piece* piece) {
    s->p = piece;
    if (invalid(s)) return;
    switch (piece->kind) { case 'C': corridor(s); break; case 'X': crossing(s); break; case 'S': stairs(s); break; default: room(s); }
}

static uint8_t* build(Overlay* o, int cx, int cz, Info* in) {
    if (in->ov) return in->ov;
    in->ov = (uint8_t*) calloc(16 * 16 * 384, 1);
    in->base = (uint8_t*) malloc(16 * 16 * 384); memset(in->base, 0xFF, 16 * 16 * 384);
    int64_t d = xr_deco(&o->rng, o->seed, cx * 16, cz * 16);
    xr_set(&o->rng, d + 1 + 10000LL * 3);
    Sim s; s.o = o; s.cx = cx; s.cz = cz; s.bx0 = cx * 16; s.bz0 = cz * 16; s.bx1 = s.bx0 + 15; s.bz1 = s.bz0 + 15; s.ov = in->ov; s.base = in->base; s.p = NULL;
    for (int i = 0; i < in->nStarts; i++) {
        Tree* t = in->starts[i];
        for (int j = 0; j < t->n; j++) if (touches_chunk(t->p[j], cx, cz)) post(&s, t->p[j]);
    }
    return in->ov;
}

int overlay_code(Overlay* o, int x, int y, int z) {
    if (y < -64 || y > 319) return 0;
    Info* in = info(o, x >> 4, z >> 4);
    if (in->nRects == 0 || !near_rect(in, x, z)) return 0;
    uint8_t* ov = build(o, x >> 4, z >> 4, in);
    return ov[idx3(x, y, z)];
}

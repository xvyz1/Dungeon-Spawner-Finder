#include "search.h"
#include "terrain.h"
#include "xr.h"
#include "threads.h"

const char* const MOB_NAMES[4] = {"skeleton", "zombie", "zombie", "spider"};
int menu_index(const char* mob) { return strcmp(mob, "skeleton") == 0 ? 0 : strcmp(mob, "spider") == 0 ? 1 : 2; }

static const int FEATURES[2][5] = {{2, 3, 10, 0, 320}, {3, 3, 4, -58, 58}};

static int64_t floor_div64(int64_t a, int64_t b) { int64_t q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) q--; return q; }

Area area_make(int64_t cx, int64_t cz, int64_t w, int64_t h) {
    Area ar;
    int64_t x0 = cx - w / 2, x1 = x0 + w - 1, z0 = cz - h / 2, z1 = z0 + h - 1;
    ar.a0 = (int) floor_div64(x0, 16) - 1; ar.a1 = (int) floor_div64(x1, 16) + 1;
    int n = ar.a1 - ar.a0 + 1;
    ar.zlo = (int*) malloc(sizeof(int) * n); ar.zhi = (int*) malloc(sizeof(int) * n); ar.chunks = 0;
    for (int i = 0; i < n; i++) {
        int64_t mid = (int64_t) (ar.a0 + i) * 16 + 8;
        if (mid < x0 || mid > x1) { ar.zlo[i] = 1; ar.zhi[i] = 0; continue; }
        ar.zlo[i] = (int) floor_div64(z0 - 8 + 15, 16); ar.zhi[i] = (int) floor_div64(z1 - 8, 16);
        if (ar.zhi[i] >= ar.zlo[i]) ar.chunks += ar.zhi[i] - ar.zlo[i] + 1;
    }
    return ar;
}
Area area_rect(int x0, int z0, int x1, int z1) {
    Area ar; int n = x1 - x0 + 1;
    ar.a0 = x0; ar.a1 = x1; ar.zlo = (int*) malloc(sizeof(int) * n); ar.zhi = (int*) malloc(sizeof(int) * n);
    for (int i = 0; i < n; i++) { ar.zlo[i] = z0; ar.zhi[i] = z1; }
    ar.chunks = (int64_t) n * (z1 - z0 + 1);
    return ar;
}
void area_free(Area* a) { free(a->zlo); free(a->zhi); a->zlo = a->zhi = NULL; }

static void rows_add(Rows* r, Row v) {
    if (r->n == r->cap) { r->cap = r->cap ? r->cap * 2 : 1024; r->v = (Row*) realloc(r->v, sizeof(Row) * r->cap); }
    r->v[r->n++] = v;
}

static int judge_fast(Terrain* t, int x, int y, int z, int xr, int zr) {
    if (!terrain_is_solid(t, x, y + 4, z) || !terrain_is_solid(t, x, y - 1, z)) return 0;
    int minX = -xr - 1, maxX = xr + 1, minZ = -zr - 1, maxZ = zr + 1, open = 0;
    for (int dx = minX; dx <= maxX; dx++) for (int dz = minZ; dz <= maxZ; dz++) {
        if (!(dx == minX || dx == maxX || dz == minZ || dz == maxZ)) continue;
        if (terrain_is_air(t, x + dx, y, z + dz) && terrain_is_air(t, x + dx, y + 1, z + dz) && ++open > 5) return 0;
    }
    if (open < 1) return 0;
    for (int dx = minX; dx <= maxX; dx++) for (int dz = minZ; dz <= maxZ; dz++) if (!terrain_is_solid(t, x + dx, y - 1, z + dz) || !terrain_is_solid(t, x + dx, y + 4, z + dz)) return 0;
    return 1;
}

static int replay(Terrain* t, Xr* r, int x, int y, int z, int xr, int zr) {
    for (int dx = -xr - 1; dx <= xr + 1; dx++) for (int dz = -zr - 1; dz <= zr + 1; dz++) if (y - 2 < -64 || terrain_is_solid(t, x + dx, y - 2, z + dz)) xr_int(r, 4);
    uint8_t chest[7][7]; memset(chest, 0, sizeof chest);
    for (int it = 0; it < 2; it++) for (int tt = 0; tt < 3; tt++) {
        int ix = xr_int(r, xr * 2 + 1) - xr, iz = xr_int(r, zr * 2 + 1) - zr;
        if (chest[ix + xr][iz + zr]) continue;
        int n = 0;
        for (int d = 0; d < 4; d++) {
            int nx = ix + (d == 0 ? 1 : d == 1 ? -1 : 0), nz = iz + (d == 2 ? 1 : d == 3 ? -1 : 0);
            if (abs(nx) > xr || abs(nz) > zr) { if (terrain_is_solid(t, x + nx, y, z + nz)) n++; }
            else if (chest[nx + xr][nz + zr]) n++;
        }
        if (n == 1) { chest[ix + xr][iz + zr] = 1; xr_long(r); break; }
    }
    return xr_int(r, 4);
}

typedef struct {
    const NoiseSet* ns; int64_t seed; const Area* ar; int threads; int progress;
    int cut[257]; Rows parts[256];
    volatile int64_t done; volatile int64_t running;
} ScanCtx;

static void strip(ScanCtx* sc, int c0, int c1, Rows* out) {
    Terrain tm; terrain_init(&tm, sc->ns, sc->seed, 1);
    Xr r; const Area* ar = sc->ar;
    int cols = 0, maxRows = 1;
    for (int c = c0; c <= c1; c++) { int rows = ar->zhi[c] - ar->zlo[c] + 1; if (rows > maxRows) maxRows = rows; }
    int dropEvery = 2400 / maxRows > 1 ? 2400 / maxRows : 1;
    for (int c = c0; c <= c1; c++) {
        int cx = ar->a0 + c;
        for (int cz = ar->zlo[c]; cz <= ar->zhi[c]; cz++) {
            terrain_clear_rooms(&tm);
            for (int fi = 0; fi < 2; fi++) {
                const int* f = FEATURES[fi];
                int64_t d = xr_deco(&r, sc->seed, cx * 16, cz * 16);
                xr_set(&r, d + f[0] + 10000LL * f[1]);
                for (int i = 0; i < f[2]; i++) {
                    int x = cx * 16 + xr_int(&r, 16);
                    int z = cz * 16 + xr_int(&r, 16);
                    int y = xr_int(&r, f[4]) + f[3];
                    int xrad = xr_int(&r, 2) + 2;
                    int zrad = xr_int(&r, 2) + 2;
                    if (!judge_fast(&tm, x, y, z, xrad, zrad)) continue;
                    int ms = tm.overlay && overlay_touches(tm.overlay, x - xrad - 1, y - 1, z - zrad - 1, x + xrad + 1, y + 4, z + zrad + 1) ? 1 : 0;
                    int mob = replay(&tm, &r, x, y, z, xrad, zrad);
                    terrain_add_room(&tm, x, y, z, xrad, zrad);
                    Row row = {x, y, z, mob, ms};
                    rows_add(out, row);
                }
            }
        }
        ds_atomic_add(&sc->done, ar->zhi[c] - ar->zlo[c] > -1 ? ar->zhi[c] - ar->zlo[c] + 1 : 0);
        if (++cols % dropEvery == 0) terrain_drop_chunks(&tm);
    }
    terrain_free(&tm);
}

void format_duration(double s, char* out) {
    long long v = (long long) s;
    if (v >= 3600) sprintf(out, "%lld h %02lld min", v / 3600, v % 3600 / 60);
    else if (v >= 60) sprintf(out, "%lld min %02lld s", v / 60, v % 60);
    else sprintf(out, "%lld s", v);
}

static void worker(void* arg, int w) {
    ScanCtx* sc = (ScanCtx*) arg;
    const Area* ar = sc->ar;
    if (w == 0) {
        double t0 = ds_time(); int lastPct = 0, shown = 0;
        if (sc->progress) { printf("Initiating...\n"); fflush(stdout); }
        for (;;) {
            int alive = ds_atomic_get(&sc->running) > 0;
            double s = ds_time() - t0;
            if (sc->progress && alive && s >= 10) {
                int64_t d = ds_atomic_get(&sc->done);
                double frac = ar->chunks == 0 ? 1 : (double) d / (double) ar->chunks;
                if (frac > 0.999) frac = 0.999;
                int pct = (int) (frac * 100);
                if (pct < lastPct) pct = lastPct;
                double est = frac > 0.0005 ? s / frac : 1e9;
                int step = est > 300 ? 1 : est > 60 ? 5 : 10;
                if (!shown || pct / step > lastPct / step) {
                    shown = 1; lastPct = pct;
                    if (frac >= 0.02) { char b[48]; format_duration(est - s > 0 ? est - s : 0, b); printf("  %d%% (%.0fs elapsed, about %s left)\n", pct, s, b); }
                    else printf("  %d%% (%.0fs elapsed)\n", pct, s);
                    fflush(stdout);
                }
            }
            if (!alive) break;
            ds_sleep_ms(200);
        }
        return;
    }
    int k = w - 1;
    int c0 = sc->cut[k], c1 = sc->cut[k + 1] - 1;
    if (c1 >= c0) strip(sc, c0, c1, &sc->parts[k]);
    ds_atomic_add(&sc->running, -1);
}

void scan_area(const NoiseSet* ns, int64_t seed, const Area* ar, int threads, int progress, Rows* out) {
    if (threads > 256) threads = 256;
    ScanCtx* sc = (ScanCtx*) calloc(1, sizeof(ScanCtx));
    sc->ns = ns; sc->seed = seed; sc->ar = ar; sc->threads = threads; sc->progress = progress;
    int ncol = ar->a1 - ar->a0 + 1; int64_t acc = 0; int t = 0;
    sc->cut[0] = 0;
    for (int c = 0; c < ncol && t < threads - 1; c++) {
        int rows = ar->zhi[c] - ar->zlo[c] + 1;
        acc += rows > 0 ? rows : 0;
        while (t < threads - 1 && acc * threads >= ar->chunks * (t + 1)) sc->cut[++t] = c + 1;
    }
    while (t < threads) sc->cut[++t] = ncol;
    sc->cut[threads] = ncol;
    sc->running = threads;
    ds_parallel(threads + 1, worker, sc);
    for (int k = 0; k < threads; k++) {
        for (size_t i = 0; i < sc->parts[k].n; i++) rows_add(out, sc->parts[k].v[i]);
        free(sc->parts[k].v);
    }
    free(sc);
}

static double sq(double v) { return v * v; }
static int ball_null(const double* b) { return b[3] < 0; }
static void ball_better(double* best, const double* b) { if (ball_null(b)) return; if (ball_null(best) || b[3] < best[3]) memcpy(best, b, 4 * sizeof(double)); }
static void ball_test(double p[][3], int n, const double* c, double* out) {
    if (c == NULL) { out[3] = -1; return; }
    double r = 0;
    for (int i = 0; i < n; i++) { double v = sqrt(sq(p[i][0] - c[0]) + sq(p[i][1] - c[1]) + sq(p[i][2] - c[2])); if (v > r) r = v; }
    out[0] = c[0]; out[1] = c[1]; out[2] = c[2]; out[3] = r;
}
static int circ3(const double* a, const double* b, const double* c, double* out) {
    double ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, ac[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    double n[3] = {ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2], ab[0] * ac[1] - ab[1] * ac[0]};
    double d = 2 * (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (d < 1e-12) return 0;
    double abab = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2], acac = ac[0] * ac[0] + ac[1] * ac[1] + ac[2] * ac[2];
    double nab[3] = {n[1] * ab[2] - n[2] * ab[1], n[2] * ab[0] - n[0] * ab[2], n[0] * ab[1] - n[1] * ab[0]};
    double acn[3] = {ac[1] * n[2] - ac[2] * n[1], ac[2] * n[0] - ac[0] * n[2], ac[0] * n[1] - ac[1] * n[0]};
    out[0] = a[0] + (acac * nab[0] + abab * acn[0]) / d; out[1] = a[1] + (acac * nab[1] + abab * acn[1]) / d; out[2] = a[2] + (acac * nab[2] + abab * acn[2]) / d;
    return 1;
}
static int circ4(const double* a, const double* b, const double* c, const double* d, double* out) {
    double m[3][4]; const double* q[3] = {b, c, d};
    for (int i = 0; i < 3; i++) { double s = 0; for (int k = 0; k < 3; k++) { m[i][k] = q[i][k] - a[k]; s += m[i][k] * m[i][k]; } m[i][3] = 0.5 * s; }
    for (int col = 0; col < 3; col++) {
        int piv = col;
        for (int r = col + 1; r < 3; r++) if (fabs(m[r][col]) > fabs(m[piv][col])) piv = r;
        if (fabs(m[piv][col]) < 1e-9) return 0;
        double t[4]; memcpy(t, m[col], sizeof t); memcpy(m[col], m[piv], sizeof t); memcpy(m[piv], t, sizeof t);
        for (int r = 0; r < 3; r++) if (r != col) { double f = m[r][col] / m[col][col]; for (int k = col; k < 4; k++) m[r][k] -= f * m[col][k]; }
    }
    out[0] = a[0] + m[0][3] / m[0][0]; out[1] = a[1] + m[1][3] / m[1][1]; out[2] = a[2] + m[2][3] / m[2][2];
    return 1;
}
static void ball_of(double p[][3], int n, double* best) {
    if (n == 1) { best[0] = p[0][0]; best[1] = p[0][1]; best[2] = p[0][2]; best[3] = 0; return; }
    best[3] = -1;
    double c[3], t[4];
    for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) {
        c[0] = (p[a][0] + p[b][0]) / 2; c[1] = (p[a][1] + p[b][1]) / 2; c[2] = (p[a][2] + p[b][2]) / 2;
        ball_test(p, n, c, t); ball_better(best, t);
        for (int cc = b + 1; cc < n; cc++) {
            if (circ3(p[a], p[b], p[cc], c)) ball_test(p, n, c, t); else ball_test(p, n, NULL, t);
            ball_better(best, t);
            for (int d = cc + 1; d < n; d++) {
                if (circ4(p[a], p[b], p[cc], p[d], c)) ball_test(p, n, c, t); else ball_test(p, n, NULL, t);
                ball_better(best, t);
            }
        }
    }
}

typedef struct { int* v; size_t n, cap; } IntVec;
static void iv_add(IntVec* v, int x) { if (v->n == v->cap) { v->cap = v->cap ? v->cap * 2 : 16; v->v = (int*) realloc(v->v, sizeof(int) * v->cap); } v->v[v->n++] = x; }

typedef struct { double (*C)[3]; int min; IntVec* found; size_t nfound, capfound; double (*balls)[4]; size_t* offs; } Dfs;
static void dfs_found(Dfs* d, const int* cur, int len, const double* ball) {
    if (d->nfound == d->capfound) {
        d->capfound = d->capfound ? d->capfound * 2 : 1024;
        d->balls = (double (*)[4]) realloc(d->balls, sizeof(double[4]) * d->capfound);
        d->offs = (size_t*) realloc(d->offs, sizeof(size_t) * (d->capfound + 1));
    }
    d->offs[d->nfound] = d->found->n;
    for (int i = 0; i < len; i++) iv_add(d->found, cur[i]);
    memcpy(d->balls[d->nfound], ball, 4 * sizeof(double));
    d->nfound++;
    d->offs[d->nfound] = d->found->n;
}
static void dfs(Dfs* d, int* cur, int len, const int* cand, int ncand, int from) {
    if (len >= d->min) {
        double p[16][3], b[4];
        for (int i = 0; i < len; i++) memcpy(p[i], d->C[cur[i]], sizeof p[i]);
        ball_of(p, len, b);
        dfs_found(d, cur, len, b);
    }
    for (int k = from; k < ncand; k++) {
        cur[len] = cand[k];
        double p[16][3], b[4];
        for (int i = 0; i <= len; i++) memcpy(p[i], d->C[cur[i]], sizeof p[i]);
        ball_of(p, len + 1, b);
        if (b[3] < 16 - 1e-9 && len + 1 < 16) dfs(d, cur, len + 1, cand, ncand, k + 1);
    }
}

#define P_CLEAN (0.968 * 0.997)
#define P_MS (0.856 * 0.93)

typedef struct { int x, y, z, mob, ms, idx; } Pt;
static int cmp_pt(const void* a, const void* b) {
    const Pt* p = (const Pt*) a; const Pt* q = (const Pt*) b;
    if (p->x != q->x) return p->x < q->x ? -1 : 1;
    if (p->y != q->y) return p->y < q->y ? -1 : 1;
    if (p->z != q->z) return p->z < q->z ? -1 : 1;
    return p->idx < q->idx ? -1 : (p->idx > q->idx ? 1 : 0);
}
static int cmp_int(const void* a, const void* b) { int x = *(const int*) a, y = *(const int*) b; return x < y ? -1 : (x > y ? 1 : 0); }

typedef struct { int64_t key; int head; } GridSlot;
typedef struct { int next; } GridNext;

static Groups* sort_ctx;
static int cmp_group(const void* a, const void* b) {
    const Group* p = (const Group*) a; const Group* q = (const Group*) b;
    if (p->n != q->n) return q->n - p->n;
    if (q->conf != p->conf) return q->conf < p->conf ? -1 : 1;
    if (p->ball[3] != q->ball[3]) return p->ball[3] < q->ball[3] ? -1 : 1;
    int m = p->n < q->n ? p->n : q->n;
    for (int i = 0; i < m; i++) if (p->mem[i] != q->mem[i]) return p->mem[i] < q->mem[i] ? -1 : 1;
    return 0;
}

void groups_build(const Rows* rows, const int count[4], int min, Groups* out) {
    memset(out, 0, sizeof *out);
    size_t nr = rows->n;
    Pt* pts = (Pt*) malloc(sizeof(Pt) * (nr + 1));
    for (size_t i = 0; i < nr; i++) { pts[i].x = rows->v[i].x; pts[i].y = rows->v[i].y; pts[i].z = rows->v[i].z; pts[i].mob = rows->v[i].mob; pts[i].ms = rows->v[i].ms; pts[i].idx = (int) i; }
    qsort(pts, nr, sizeof(Pt), cmp_pt);
    size_t n = 0;
    for (size_t i = 0; i < nr; i++) {
        size_t j = i;
        while (j + 1 < nr && pts[j + 1].x == pts[i].x && pts[j + 1].y == pts[i].y && pts[j + 1].z == pts[i].z) j++;
        if (count[pts[j].mob]) pts[n++] = pts[j];
        i = j;
    }
    out->px = (int*) malloc(sizeof(int) * (n + 1)); out->py = (int*) malloc(sizeof(int) * (n + 1)); out->pz = (int*) malloc(sizeof(int) * (n + 1));
    out->mob = (int*) malloc(sizeof(int) * (n + 1)); out->flag = (int*) malloc(sizeof(int) * (n + 1));
    double (*C)[3] = (double (*)[3]) malloc(sizeof(double[3]) * (n + 1));
    for (size_t i = 0; i < n; i++) {
        out->px[i] = pts[i].x; out->py[i] = pts[i].y; out->pz[i] = pts[i].z; out->mob[i] = pts[i].mob; out->flag[i] = pts[i].ms;
        C[i][0] = pts[i].x + .5; C[i][1] = pts[i].y + .5; C[i][2] = pts[i].z + .5;
    }
    free(pts);

    size_t gcap = 1; while (gcap < n * 2 + 16) gcap <<= 1;
    GridSlot* slots = (GridSlot*) malloc(sizeof(GridSlot) * gcap);
    for (size_t i = 0; i < gcap; i++) slots[i].head = -2;
    int* nextIdx = (int*) malloc(sizeof(int) * (n + 1)); int* tail = (int*) malloc(sizeof(int) * gcap);
    for (size_t i = 0; i < n; i++) {
        int64_t gx = floor_div64(out->px[i], 32), gz = floor_div64(out->pz[i], 32);
        int64_t key = (int64_t) (((uint64_t) gx << 32) ^ ((uint64_t) gz & 0xffffffffULL));
        size_t s = (size_t) (((uint64_t) key * 0x9E3779B97F4A7C15ULL) >> 20) & (gcap - 1);
        while (slots[s].head != -2 && slots[s].key != key) s = (s + 1) & (gcap - 1);
        nextIdx[i] = -1;
        if (slots[s].head == -2) { slots[s].key = key; slots[s].head = (int) i; tail[s] = (int) i; }
        else { nextIdx[tail[s]] = (int) i; tail[s] = (int) i; }
    }
    Dfs d; memset(&d, 0, sizeof d); d.C = C; d.min = min;
    IntVec found = {0}; d.found = &found;
    IntVec nb = {0};
    for (size_t i = 0; i < n; i++) {
        int64_t gx = floor_div64(out->px[i], 32), gz = floor_div64(out->pz[i], 32);
        nb.n = 0;
        for (int dx = -1; dx <= 1; dx++) for (int dz = -1; dz <= 1; dz++) {
            int64_t key = (int64_t) (((uint64_t) (gx + dx) << 32) ^ ((uint64_t) (gz + dz) & 0xffffffffULL));
            size_t s = (size_t) (((uint64_t) key * 0x9E3779B97F4A7C15ULL) >> 20) & (gcap - 1);
            while (slots[s].head != -2 && slots[s].key != key) s = (s + 1) & (gcap - 1);
            if (slots[s].head == -2) continue;
            for (int j = slots[s].head; j >= 0; j = nextIdx[j]) if ((size_t) j > i) {
                double ddx = C[i][0] - C[j][0], ddy = C[i][1] - C[j][1], ddz = C[i][2] - C[j][2];
                if (sqrt(ddx * ddx + ddy * ddy + ddz * ddz) < 32 - 1e-9) iv_add(&nb, j);
            }
        }
        if ((int) nb.n + 1 < min) continue;
        qsort(nb.v, nb.n, sizeof(int), cmp_int);
        int cur[16]; cur[0] = (int) i;
        dfs(&d, cur, 1, nb.v, (int) nb.n, 0);
    }
    free(nb.v); free(slots); free(nextIdx); free(tail);

    size_t nf = d.nfound;
    int* byFirstHead = (int*) malloc(sizeof(int) * (n + 1)); int* byFirstNext = (int*) malloc(sizeof(int) * (found.n + 1)); int* byFirstTail = (int*) malloc(sizeof(int) * (n + 1));
    int* memGroup = (int*) malloc(sizeof(int) * (found.n + 1));
    for (size_t i = 0; i < n; i++) { byFirstHead[i] = -1; byFirstTail[i] = -1; }
    for (size_t g = 0; g < nf; g++) for (size_t k = d.offs[g]; k < d.offs[g + 1]; k++) {
        int m = found.v[k]; memGroup[k] = (int) g; byFirstNext[k] = -1;
        if (byFirstHead[m] < 0) byFirstHead[m] = (int) k; else byFirstNext[byFirstTail[m]] = (int) k;
        byFirstTail[m] = (int) k;
    }
    out->v = (Group*) malloc(sizeof(Group) * (nf + 1));
    for (size_t g = 0; g < nf; g++) {
        const int* a = &found.v[d.offs[g]]; int an = (int) (d.offs[g + 1] - d.offs[g]); int sub = 0;
        for (int k = byFirstHead[a[0]]; k >= 0 && !sub; k = byFirstNext[k]) {
            int h = memGroup[k];
            const int* b = &found.v[d.offs[h]]; int bn = (int) (d.offs[h + 1] - d.offs[h]);
            if (bn > an) {
                int j = 0;
                for (int q = 0; q < bn; q++) if (j < an && b[q] == a[j]) j++;
                if (j == an) sub = 1;
            }
        }
        if (sub) continue;
        Group* gr = &out->v[out->n++];
        gr->n = an; gr->mem = (int*) malloc(sizeof(int) * an); memcpy(gr->mem, a, sizeof(int) * an);
        memcpy(gr->ball, d.balls[g], 4 * sizeof(double));
        gr->conf = 1;
        for (int q = 0; q < an; q++) gr->conf *= out->flag[a[q]] == 1 ? P_MS : P_CLEAN;
    }
    free(byFirstHead); free(byFirstNext); free(byFirstTail); free(memGroup); free(found.v); free(d.balls); free(d.offs); free(C);
    qsort(out->v, out->n, sizeof(Group), cmp_group);
    (void) sort_ctx;
}
void groups_free(Groups* g) {
    for (size_t i = 0; i < g->n; i++) free(g->v[i].mem);
    free(g->v); free(g->px); free(g->py); free(g->pz); free(g->mob); free(g->flag);
    memset(g, 0, sizeof *g);
}

void group_thousands(int64_t v, char* out) {
    char t[32]; sprintf(t, "%lld", (long long) (v < 0 ? -v : v));
    int len = (int) strlen(t), o = 0;
    if (v < 0) out[o++] = '-';
    for (int i = 0; i < len; i++) { if (i > 0 && (len - i) % 3 == 0) out[o++] = ','; out[o++] = t[i]; }
    out[o] = 0;
}

void format_counts(const Groups* gs, const Group* g, char* out, size_t cap) {
    int c[3] = {0, 0, 0}; static const char* const nm[3] = {"skeleton", "spider", "zombie"};
    for (int i = 0; i < g->n; i++) c[menu_index(MOB_NAMES[gs->mob[g->mem[i]]])]++;
    out[0] = 0; size_t len = 0;
    for (int i = 0; i < 3; i++) if (c[i] > 0) len += snprintf(out + len, cap - len, "%s%d %s", len > 0 ? ", " : "", c[i], nm[i]);
}
void format_tp(const Group* g, char* out, size_t cap) {
    snprintf(out, cap, "/tp %lld %lld %lld", (long long) rint(g->ball[0]), (long long) rint(g->ball[1]), (long long) rint(g->ball[2]));
}
void print_group(FILE* f, const Groups* gs, const Group* g, int rank) {
    char cnt[96], tp[96];
    format_counts(gs, g, cnt, sizeof cnt); format_tp(g, tp, sizeof tp);
    fprintf(f, "\n#%d  Found %d spawners (%s) from one spot at %s\n    confidence %.0f%%, ball radius %.2f\n", rank, g->n, cnt, tp, 100 * g->conf, g->ball[3]);
    for (int i = 0; i < g->n; i++) {
        int m = g->mem[i];
        fprintf(f, "    %-8s %d %d %d%s\n", MOB_NAMES[gs->mob[m]], gs->px[m], gs->py[m], gs->pz[m], gs->flag[m] == 1 ? "   (next to a mineshaft: less certain)" : "");
    }
}

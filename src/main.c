#include <inttypes.h>
#include "search.h"
#include "inputs.h"
#include "threads.h"

extern const char* const GOLDEN_ROWS[];

typedef struct { int64_t seed; int count[4]; char mobLabel[64]; int min, results, threads, csv; int64_t w, h, cx, cz; } Opts;

static int get_opts(Opts* o) {
    int64_t v;
    if (!getI64Number("numeric world seed", &o->seed, 0, 0, INT64_MIN, INT64_MAX)) return 0;
    static const char* const mobs[3] = {"SKELETON", "SPIDER", "ZOMBIE"};
    int menu[3];
    if (!getIntSetAll("mobs to include", menu, mobs, 3)) return 0;
    for (int i = 0; i < 4; i++) o->count[i] = menu[menu_index(MOB_NAMES[i])];
    o->mobLabel[0] = 0;
    if (menu[0] && menu[1] && menu[2]) strcpy(o->mobLabel, "ALL");
    else { size_t len = 0; for (int i = 0; i < 3; i++) if (menu[i]) len += snprintf(o->mobLabel + len, sizeof o->mobLabel - len, "%s%s", len > 0 ? "_" : "", mobs[i]); }
    static const char* const areas[2] = {"SQUARE", "RECTANGLE"};
    int area;
    if (!getIntEnum("area type", &area, areas, 2)) return 0;
    if (area == 1) {
        if (!getI64Number("rectangle width (X) in blocks", &v, 0, 0, 1, 60000000)) return 0;
        o->w = v;
        if (!getI64Number("rectangle length (Z) in blocks", &v, 0, 0, 1, 60000000)) return 0;
        o->h = v;
    } else {
        if (!getI64Number("square radius in blocks", &v, 0, 0, 1, 30000000)) return 0;
        o->w = o->h = 2 * v + 1;
    }
    if (!getI64Number("minimum count of spawners to report (2-5)", &v, 1, 2, 2, 5)) return 0;
    o->min = (int) v;
    if (!getI64Number("number of results to show", &v, 1, 10, 1, 1000)) return 0;
    o->results = (int) v;
    if (!getI64Number("search center X", &o->cx, 1, 0, -30000000, 30000000)) return 0;
    if (!getI64Number("search center Z", &o->cz, 1, 0, -30000000, 30000000)) return 0;
    int cpus = ds_cpu_count();
    if (!getI64Number("number of threads", &v, 1, cpus < 1024 ? cpus : 1024, 1, 1024)) return 0;
    o->threads = (int) v;
    static const char* const csv[2] = {"YES", "NO"};
    int pick;
    if (!getIntEnumDefault("CSV output", &pick, csv, 2, 1)) return 0;
    o->csv = pick == 0;
    return 1;
}

static void write_csv(const Opts* o, const Groups* gs, int show) {
    char name[128], base[128];
    snprintf(base, sizeof base, "%" PRId64 "_%s", o->seed, o->mobLabel);
    snprintf(name, sizeof name, "%s.csv", base);
    for (int k = 2; k < 1000; k++) {
        FILE* probe = fopen(name, "r");
        if (!probe) break;
        fclose(probe);
        snprintf(name, sizeof name, "%s_%d.csv", base, k);
    }
    FILE* f = fopen(name, "w");
    if (!f) { printf("(could not write %s)\n", name); return; }
    fprintf(f, "rank,spawners,spawner_types,tp,ball_radius,confidence_percent,members\n");
    for (int i = 0; i < show; i++) {
        const Group* g = &gs->v[i];
        char cnt[96], tp[96], types[96];
        format_counts(gs, g, cnt, sizeof cnt); format_tp(g, tp, sizeof tp);
        size_t o2 = 0;
        for (const char* p = cnt; *p && o2 < sizeof types - 1; p++) { if (p[0] == ',' && p[1] == ' ') { types[o2++] = ' '; p++; } else types[o2++] = *p; }
        types[o2] = 0;
        fprintf(f, "%d,%d,%s,%s,%.2f,%.0f,\"", i + 1, g->n, types, tp, g->ball[3], 100 * g->conf);
        for (int q = 0; q < g->n; q++) { int m = g->mem[q]; fprintf(f, "%s%d %d %d %s", q > 0 ? "; " : "", gs->px[m], gs->py[m], gs->pz[m], MOB_NAMES[gs->mob[m]]); }
        fprintf(f, "\"\n");
    }
    fclose(f);
    printf("Saved %d result%s to %s\n", show, show == 1 ? "" : "s", name);
}

static void run(const Opts* o) {
    Area ar = area_make(o->cx, o->cz, o->w, o->h);
    char a[40], b[40];
    group_thousands(ar.chunks, a); group_thousands(ar.chunks * 14, b);
    printf("\nSearching %s chunks (%s dungeon attempts) with %d threads...\n", a, b, o->threads);
    carve_init();
    NoiseSet ns; noiseset_init(&ns, o->seed);
    double t0 = ds_time();
    Rows rows; memset(&rows, 0, sizeof rows);
    scan_area(&ns, o->seed, &ar, o->threads, 1, &rows);
    double ts = ds_time() - t0;
    Groups gs;
    groups_build(&rows, o->count, o->min, &gs);
    char nrows[40], ngs[40], dur[48], rate[40];
    group_thousands((int64_t) rows.n, nrows); group_thousands((int64_t) gs.n, ngs); format_duration(ts, dur);
    group_thousands((int64_t) llround((double) (ar.chunks * 14) / ts / o->threads), rate);
    printf("\n%s dungeons predicted; %s spots with %d or more counted spawners running together. Scan %s, %s attempts/s/thread.\n", nrows, ngs, o->min, dur, rate);
    int show = (int) (gs.n < (size_t) o->results ? gs.n : (size_t) o->results);
    for (int i = 0; i < show; i++) print_group(stdout, &gs, &gs.v[i], i + 1);
    char file[128];
    snprintf(file, sizeof file, "DungeonSpawnerFinder_results_%" PRId64 ".txt", o->seed);
    FILE* f = fopen(file, "w");
    if (f) {
        fprintf(f, "DungeonSpawnerFinder %s seed %" PRId64 " counted [%s, %s, %s, %s] min %d -> %d spots\n", VERSION_NAME, o->seed,
            o->count[0] ? "true" : "false", o->count[1] ? "true" : "false", o->count[2] ? "true" : "false", o->count[3] ? "true" : "false", o->min, (int) gs.n);
        for (size_t i = 0; i < gs.n; i++) print_group(f, &gs, &gs.v[i], (int) i + 1);
        fclose(f);
    } else printf("(could not write %s)\n", file);
    printf("\nAll %d spots are in %s\n", (int) gs.n, file);
    if (o->csv && show > 0) write_csv(o, &gs, show);
    printf("Note: these are predictions from seed math, not the generated world. Check every spot in game before you build.\n");
    groups_free(&gs); free(rows.v); noiseset_free(&ns); area_free(&ar);
}

static int cmp_str(const void* a, const void* b) { return strcmp(*(char* const*) a, *(char* const*) b); }
static int unique_sorted(char** v, int n) { qsort(v, n, sizeof(char*), cmp_str); int m = 0; for (int i = 0; i < n; i++) if (i == 0 || strcmp(v[i], v[m - 1]) != 0) v[m++] = v[i]; return m; }
static char** rows_to_strings(const Rows* r, int* n) {
    char** v = (char**) malloc(sizeof(char*) * (r->n + 1));
    for (size_t i = 0; i < r->n; i++) { v[i] = (char*) malloc(64); snprintf(v[i], 64, "%d,%d,%d,%s", r->v[i].x, r->v[i].y, r->v[i].z, MOB_NAMES[r->v[i].mob]); }
    *n = unique_sorted(v, (int) r->n);
    return v;
}

static int selftest(void) {
    carve_init();
    int th = ds_cpu_count();
    Area ar = area_make(0, 0, 801, 801);
    double t0 = ds_time();
    NoiseSet ns; noiseset_init(&ns, 12345);
    Rows rows; memset(&rows, 0, sizeof rows);
    scan_area(&ns, 12345, &ar, th, 0, &rows);
    int ngot; char** got = rows_to_strings(&rows, &ngot);
    int nwant = 0; while (GOLDEN_ROWS[nwant]) nwant++;
    char** want = (char**) malloc(sizeof(char*) * (nwant + 1));
    for (int i = 0; i < nwant; i++) want[i] = (char*) GOLDEN_ROWS[i];
    nwant = unique_sorted(want, nwant);
    int miss = 0, extra = 0;
    for (int i = 0; i < nwant; i++) { int f = 0; for (int j = 0; j < ngot && !f; j++) if (!strcmp(want[i], got[j])) f = 1; if (!f) { if (miss < 5) printf("  missing %s\n", want[i]); miss++; } }
    for (int j = 0; j < ngot; j++) { int f = 0; for (int i = 0; i < nwant && !f; i++) if (!strcmp(want[i], got[j])) f = 1; if (!f) { if (extra < 5) printf("  extra   %s\n", got[j]); extra++; } }
    char ch[40]; group_thousands(ar.chunks, ch);
    printf("selftest: seed 12345, square r=400 at 0 0 (%s chunks), %d dungeons predicted, %d in the stored result, %d missing, %d extra, %.1f s\n", ch, ngot, nwant, miss, extra, ds_time() - t0);
    if (miss || extra) { printf("selftest: FAILED\n"); return 1; }
    noiseset_free(&ns); free(rows.v);

    Area a2 = area_rect(2524, 1688, 2526, 1690);
    int64_t seed2 = 7621552094485497633LL;
    NoiseSet ns2; noiseset_init(&ns2, seed2);
    Rows rows2; memset(&rows2, 0, sizeof rows2);
    scan_area(&ns2, seed2, &a2, th, 0, &rows2);
    int ngot2; char** got2 = rows_to_strings(&rows2, &ngot2);
    static const char* const expect[3] = {"40405,17,27035,skeleton", "40408,-7,27025,skeleton", "40414,-9,27042,skeleton"};
    int same = ngot2 == 3;
    for (int i = 0; same && i < 3; i++) if (strcmp(got2[i], expect[i]) != 0) same = 0;
    printf("selftest: overlapping rooms (seed %" PRId64 ", 3 x 3 chunks): %d dungeons predicted, 3 expected\n", seed2, ngot2);
    if (!same) { printf("selftest: FAILED\n"); return 1; }
    printf("selftest: PASSED\n");
    noiseset_free(&ns2); free(rows2.v); area_free(&ar); area_free(&a2);
    return 0;
}

static int fused_multiply_add_in_use(void) {
    volatile double a = 1.0 + ldexp(1.0, -27), b = 1.0 - ldexp(1.0, -27), c = -1.0;
    double r = a * b + c;
    return r != 0.0;
}

int main(int argc, char** argv) {
    if (fused_multiply_add_in_use()) {
        printf("This build uses fused multiply-add, which changes the results. Rebuild with -ffp-contract=off (see compile.txt).\n");
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "selftest") == 0) return selftest();
    Opts o;
    memset(&o, 0, sizeof o);
    int result = 1;
    if (get_opts(&o)) { run(&o); result = 0; }
    printf("Press enter to continue . . .\n");
    getchar();
    return result;
}

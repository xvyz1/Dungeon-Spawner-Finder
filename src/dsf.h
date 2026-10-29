#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "kernel.h"
#include "tree.h"
#include <float.h>

#if FLT_EVAL_METHOD != 0
#error "FLT_EVAL_METHOD must be 0: use a 64-bit build (x86-64 or ARM64) so float and double math is not done in extended precision"
#endif
_Static_assert(sizeof(void*) == 8, "a 64-bit build is required");

typedef struct { uint64_t lo, hi; } Xo;
typedef struct { uint64_t lo, hi; } Pos;
void xo_make(Xo* r, uint64_t lo, uint64_t hi);
void xo_of_seed(Xo* r, int64_t seed);
uint64_t xo_long(Xo* r);
int xo_int(Xo* r, int bound);
double xo_double(Xo* r);
Pos xo_fork(Xo* r);
void pos_at(const Pos* p, int x, int y, int z, Xo* out);
void pos_hash(const Pos* p, const char* s, Xo* out);

typedef struct NoiseSet {
    Octave* octs; PerlinD* pers; NormalD* nors; BlendD bl; Pos aquifer;
    int nOcts, nPers, nNors;
} NoiseSet;
void noiseset_init(NoiseSet* ns, int64_t seed);
void noiseset_free(NoiseSet* ns);
void carve_init(void);
void carve_chunk(int64_t worldSeed, int tcx, int tcz, uint8_t* maskOut);
void carve_tile(int64_t worldSeed, int tx0, int tz0, uint8_t* out);

typedef struct { int64_t* kv; int mask, size; } LMap;
void lmap_init(LMap* m, int bits);
void lmap_free(LMap* m);
void lmap_clear(LMap* m);
int lmap_get(const LMap* m, int64_t k, double* out);
void lmap_put(LMap* m, int64_t k, double v);

#define MAX_INPUTS 8
typedef struct Model Model;
typedef struct Env {
    Model* m; int interp, snap, fx, fz; double in2d[MAX_INPUTS * 25]; int in2dReady; struct Env* noI;
} Env;

#define C2D_BITS 16
struct Model {
    World w; const NoiseSet* ns; int64_t seed;
    LMap corner, prelim, flat; int64_t cellKey[256]; double cellVal[256 * 8]; int64_t* c2d;
};
void model_init(Model* m, const NoiseSet* ns, int64_t seed);
void model_free(Model* m);
void model_clear_caches(Model* m);
void env_init(Env* e, Model* m, int interp, int snap, int chunkX, int chunkZ);
void env_free(Env* e);
double tree_eval(Model* m, int node, int x, int y, int z, Env* e);

static inline int ifloor(double d) { int i = (int) d; return d < i ? i - 1 : i; }

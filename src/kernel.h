#pragma once
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct { uint8_t p[256]; double xo, yo, zo; } Octave;
typedef struct { int n; double lowIn, lowVal; int has[16]; int oct[16]; double amp[16]; } PerlinD;
typedef struct { int a, b; double vf; } NormalD;
typedef struct { int mn, mx, mainP; double xzMul, yMul, xzFactor, yFactor, smear; } BlendD;
#define BLEND_SLOTS 256
typedef struct World {
    const Octave* octs; const PerlinD* pers; const NormalD* nors; BlendD bl;
    int bkx[BLEND_SLOTS], bky[BLEND_SLOTS], bkz[BLEND_SLOTS]; uint8_t bok[BLEND_SLOTS]; double bval[BLEND_SLOTS];
} World;

double noise_normal(const World* w, int idx, double x, double y, double z);
double blended(const World* w, int x, int y, int z);
double eval_root(const World* w, int k, const double* in2d, int qx0, int qz0, int x, int y, int z);

static inline double jmin(double a, double b) { if (a != a) return a; if (a == 0.0 && b == 0.0 && signbit(b)) return b; return a <= b ? a : b; }
static inline double jmax(double a, double b) { if (a != a) return a; if (a == 0.0 && b == 0.0 && signbit(a)) return b; return a >= b ? a : b; }
static inline double jclamp(double v, double lo, double hi) { return v < lo ? lo : jmin(v, hi); }
static inline double clamped_lerp(double t, double lo, double hi) { return t < 0.0 ? lo : (t > 1.0 ? hi : lo + t * (hi - lo)); }
static inline double clamped_map(double v, double a, double b, double lo, double hi) { return clamped_lerp((v - a) / (b - a), lo, hi); }
static inline double rarity3d(double v) { return v < -0.5 ? 0.75 : (v < 0.0 ? 1.0 : (v < 0.5 ? 1.5 : 2.0)); }
static inline double rarity2d(double v) { return v < -0.75 ? 0.5 : (v < -0.5 ? 0.75 : (v < 0.5 ? 1.0 : (v < 0.75 ? 2.0 : 3.0))); }

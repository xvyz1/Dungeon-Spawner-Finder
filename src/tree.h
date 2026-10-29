#pragma once
#include <stdint.h>

enum { T_CONST, T_ADD, T_MUL, T_MIN, T_MAX, T_MAPPED, T_CLAMP, T_YCLAMPED, T_NOISE, T_SHIFTED, T_SHIFT, T_WEIRD, T_RANGE, T_OLDBLENDED, T_PASS, T_SPLINE, T_FINDTOP, T_INTERP, T_FLAT, T_C2D };

typedef struct { int type, a, b, c, d; double v[5]; int hasLb; double lb; } NodeDef;
typedef struct { int kind; float cv; int coord, n, off; } SplineDef;
typedef struct { const char* name; int first, n, ampOff; } NoiseDef;

extern const int TREE_NODE_COUNT, TREE_SPLINE_COUNT, TREE_ROOT_COUNT, TREE_INPUT_COUNT, NOISE_COUNT;
extern const NodeDef TREE_NODES[];
extern const SplineDef TREE_SPLINES[];
extern const float TREE_SP_LOC[], TREE_SP_DER[];
extern const int TREE_SP_CHILD[];
extern const int TREE_FINAL, TREE_EROSION, TREE_DEPTH, TREE_BARRIER, TREE_FLOODEDNESS, TREE_SPREAD, TREE_LAVA, TREE_PRELIM;
extern const int TREE_INPUTS[];
extern const double TREE_BLEND[5];
extern const NoiseDef NOISE_DEFS[];
extern const double NOISE_AMPS[];

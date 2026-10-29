#pragma once
#include <stdint.h>

typedef struct Piece {
    char kind; int facing, depth, x0, y0, z0, x1, y1, z1, twoFloored, rails, spider, nEnt;
    int ent[32][6];
} Piece;
typedef struct { int n, cap; Piece** p; } Tree;

int mineshaft_starts_here(int64_t seed, int cx, int cz);
Tree* mineshaft_tree(int64_t seed, int cx, int cz);
void mineshaft_tree_free(Tree* t);

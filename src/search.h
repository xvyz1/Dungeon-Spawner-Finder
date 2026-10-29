#pragma once
#include <stdint.h>
#include <stdio.h>
#include "dsf.h"

#define VERSION_NAME "1.21.11"

typedef struct { int x, y, z, mob, ms; } Row;
typedef struct { Row* v; size_t n, cap; } Rows;

typedef struct { int a0, a1; int* zlo; int* zhi; int64_t chunks; } Area;
Area area_make(int64_t cx, int64_t cz, int64_t w, int64_t h);
Area area_rect(int x0, int z0, int x1, int z1);
void area_free(Area* a);

void scan_area(const NoiseSet* ns, int64_t seed, const Area* ar, int threads, int progress, Rows* out);

typedef struct { int n; int* mem; double ball[4]; double conf; } Group;
typedef struct { Group* v; size_t n; int* px; int* py; int* pz; int* mob; int* flag; } Groups;
void groups_build(const Rows* rows, const int count[4], int min, Groups* out);
void groups_free(Groups* g);

extern const char* const MOB_NAMES[4];
int menu_index(const char* mob);
void format_counts(const Groups* gs, const Group* g, char* out, size_t cap);
void format_tp(const Group* g, char* out, size_t cap);
void print_group(FILE* f, const Groups* gs, const Group* g, int rank);
void group_thousands(int64_t v, char* out);
void format_duration(double s, char* out);

#pragma once
#include "dsf.h"

enum { B_SOLID = 0, B_AIR = 1, B_WATER = 2, B_LAVA = 3 };

typedef struct { int level, type; } Fluid;

typedef struct Aquifer {
    Model* m; Env env;
    int minGridX, minGridY, minGridZ, gridSizeX, gridSizeZ, maxGX, maxGZ;
    int skipY, skipKnown, n;
    Fluid* cache; uint8_t* known; int64_t* loc;
} Aquifer;

typedef struct Terrain Terrain;
typedef struct Overlay Overlay;
Overlay* overlay_new(int64_t seed, Terrain* t);
void overlay_free(Overlay* o);
void overlay_clear(Overlay* o);
int overlay_code(Overlay* o, int x, int y, int z);
int overlay_touches(Overlay* o, int x0, int y0, int z0, int x1, int y1, int z1);

typedef struct CTile { int tx, tz; uint8_t* data; struct CTile* next; } CTile;

typedef struct Chunk {
    int cx, cz; Env env; Aquifer aq; uint8_t* carved; struct Chunk* next;
} Chunk;

struct Terrain {
    const NoiseSet* ns; Model m; int64_t seed;
    Chunk** buckets; int nb;
    int lastCx, lastCz; Chunk* last;
    int rooms[6 * 16], nRooms;
    CTile** ctiles; Overlay* overlay; int carvers;
};

void terrain_init(Terrain* t, const NoiseSet* ns, int64_t seed, int mineshafts);
void terrain_free(Terrain* t);
void terrain_drop_chunks(Terrain* t);
void terrain_clear_rooms(Terrain* t);
void terrain_add_room(Terrain* t, int x, int y, int z, int xr, int zr);
int terrain_block_class(Terrain* t, int x, int y, int z);
int terrain_base_class(Terrain* t, int x, int y, int z);
double terrain_density(Terrain* t, int x, int y, int z);
static inline int terrain_is_solid(Terrain* t, int x, int y, int z) { return y >= -64 && y <= 319 && terrain_block_class(t, x, y, z) == B_SOLID; }
static inline int terrain_is_air(Terrain* t, int x, int y, int z) { return y >= -64 && y <= 319 && terrain_block_class(t, x, y, z) == B_AIR; }

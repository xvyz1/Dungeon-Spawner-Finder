# DungeonSpawnerFinder

Finds dungeon spawner clusters (skeleton, spider, zombie) in an area on Minecraft Java, notably triple skeleton spawners. Vibecoded.

[VIDEO](https://www.youtube.com/watch?v=XXXXXXXXXXX)

- Groups of 2 and 3 of the same type are common: a 20,000 x 20,000 block area holds on average 1-2 groups of 3 skeleton spawners.
- Groups of 4 and 5 are much rarer, none found, but likely exist on every seed; finding them is limited by search speed.
- In my opinion 3 is a good compromise: you can always build two triples if you'd really like.
- A 20,000 x 20,000 block area takes about 4 minutes and a 100,000 x 100,000 square about 8 hours (16 threads), because every spawner needs the terrain around it. Doubling the radius takes about 4 times as long.
- Use: mostly early game, to skip building and or repairing a moss farm, maybe to bypass the mob cap on a server. Traditional whiske or general mob farms are usually better. A triple skeleton spawner gives about 20 stacks of bone blocks an hour.

- RECOMMENDED PARAMETERS: 10000 radius ; minimum 3 skeleton spawners. REMEMBER TO CHECK IF IT EXISTS BEFOREHAND, not guaranteed.

Any ideas/issues, DM me xvyz#4060

# Run
Run `DungeonSpawnerFinder.exe` (Windows), or build it from the source for Linux and macOS (see Build). One file, nothing to install.

# Version
1.21.11 (dungeons and mineshafts of that version). Other versions are not checked.

# Input parameters
**Numeric world seed** - the world seed in numeric form  
**Mobs to include** - `1` SKELETON, `2` SPIDER, `3` ZOMBIE; numbers separated by commas without spaces, for example `1` to count
only skeletons or `1,2` for skeletons and spiders; leave empty for all three. Mobs that are not included are not counted and not
shown. Dungeon mobs only: mineshaft cave spiders and trial spawners are not part of this  
**Area type** - `1` SQUARE or `2` RECTANGLE, centered on the search center  
**Square radius in blocks** - 1 to 30,000,000 (the square is 2r+1 blocks wide); for a rectangle instead **Rectangle width (X)** and
**Rectangle length (Z)** in blocks, 1 to 60,000,000 each. Every chunk whose middle is inside the area is searched. Memory does not
grow with the size; doubling the sides takes about 4 times as long  
**Minimum count of spawners to report** - 2 to 5; leave empty for 2  
**Number of results to show** - leave empty for 10  
**Search center X** and **Search center Z** - leave empty for 0  
**Number of threads** - leave empty to use every thread the computer has; the results are the same for any number  
**CSV output** - `1` YES or `2` NO (leave empty for NO). The file is named after the seed and the mobs counted, for example
`7621552094485497633_SKELETON.csv` (a number is added if that file already exists) and holds the shown results: rank, spawners,
spawner types (`3 skeleton`), the `/tp` (one cell, `/tp x y z`), the ball radius, the confidence and every spawner position

An invalid value is simply asked for again. Inputs can also be piped in, one per line, in the order above (the size follows the area type).
`DungeonSpawnerFinder.exe selftest` re-computes fixed areas and compares them with the stored results.

While a search runs it first says `Initiating...`; after the first 10 seconds it prints the percentage done with the time elapsed
and an estimate of the time left: every 10% for a search that takes under a minute in total, every 5% up to 5 minutes, every 1%
above that.

# Output
```
#1  Found 3 spawners (3 skeleton) from one spot at /tp 40409 3 27036
    confidence 74%, ball radius 14.55
    skeleton 40405 17 27035   (next to a mineshaft: less certain)
    skeleton 40408 -7 27025
    skeleton 40414 -9 27042
```
Every result is also written to `DungeonSpawnerFinder_results_<seed>.txt` (all spots, not only the shown ones). A spot is listed
once (maximal group). **Confidence** is the model's measured reliability per spawner (a real dungeon with the right mob:
96.5% away from mineshafts, 79.6% when its room touches a mineshaft piece, which is marked on the line) multiplied over the
spawners: it is a prediction of how likely the whole group exists, not a guarantee. Check in game before building.

# How it works
**Attempts.** Every chunk makes 14 dungeon attempts (10 anywhere from y 0 up, 4 at y -58 to -1). Their positions and room sizes
come from the world seed alone.  
**Terrain.** An attempt only becomes a dungeon when the real terrain allows it (solid floor and ceiling, one to five two-high
openings in the wall ring), and since 1.18 the caves come from 3D noise. DungeonSpawnerFinder contains its own model of the overworld
terrain (noise terrain, the underground water system and the cave, extra cave and canyon carvers) and of the mineshafts, which
change the terrain before the dungeons are placed.  
**Dungeons.** A successful attempt consumes more random numbers (floor blocks, chests, the mob), which moves the later attempts
of the same chunk; this is replayed exactly, so later dungeons and the mob of every spawner are found. The rooms of dungeons
built earlier in the same chunk count as air for the later ones.  
**Groups.** A spot is the centre of the smallest ball that holds the counted spawners; it must have a radius below 16. Only the
largest group of each set of spawners is listed.

# Accuracy
Measured on real generated worlds. On a real 5,000 x 5,000 world (seed 12345) 95.9% of predicted dungeons are real and 94.6% of real ones are predicted (mob right
for 99.2%). Where the room touches a mineshaft piece the precision is 85.6% (mob 93%), elsewhere 96.8% (mob 99.7%). Two more
worlds (seed 7621552094485497633, 1,800 x 1,800 at 40409 27036; seed 0, 1,600 x 1,600 at 120000 -80000) gave 95.4% / 95.1% and
96.3% / 97.1% (precision / recall), so the figures hold on other seeds and far from the origin.  
Where it goes wrong: structures other than mineshafts that change the terrain (trial chambers and ancient cities: about a third of
the wrong predictions sit inside their boxes), amethyst geodes, a few blocks of the underground water system that the model places
differently, caves whose wall differs by one block (which changes the opening count), and the mesa mineshaft variant. A predicted
spawner can therefore be missing in the game: check a spot before building.

# Speed
About 50,000 dungeon attempts per second on a 16-thread processor (on processors with fast and slow cores the extra threads add
less than the first ones). A square of radius 10,000 (1.5M chunks, 22M attempts) takes about 7-10 minutes; the progress lines show an estimate.
Doubling the radius takes about 4 times as long.

# Build
Windows: run `build.bat` from a Visual Studio Developer Command Prompt (or download `DungeonSpawnerFinder.exe` from Releases).

Linux and macOS (gcc or clang, 64-bit, including Apple Silicon):
```
gcc -o DungeonSpawnerFinder -std=gnu11 -O2 -ffp-contract=off ./src/*.c -lm -lpthread
```
`compile.txt` has this and a MinGW command. Keep `-ffp-contract=off` and use a 64-bit build: the terrain noise must not use fused
multiply-add or x87 arithmetic, or the results stop matching the game. Run `DungeonSpawnerFinder selftest` once on a new system; it
prints `selftest: PASSED`. The program also checks itself at start and refuses to run if it was built with fused multiply-add, and a
32-bit build does not compile. Windows (MSVC and clang) is what has been run so far; the Linux and macOS builds compile without
warnings but have not been run on those systems. Sources are in `src`.

Memory: about 170 MB per thread (about 1 GB with 6 threads), and it does not grow with the search size. Use fewer threads on a
computer with little free memory.

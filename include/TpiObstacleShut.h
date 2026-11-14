// TpiObstacleShut.h: the TPI Worlds and Rides mod's obstacles that stay until their land is bought (TPW_MOD_HOOK
// tpi_worlds). Theme Park World has no obstacles, so the converter (assets/tpi_terrain.h) makes Theme Park Inc's part of
// the terrain. A cleared tunnel (Arabian Nights', Polar Zone's) is drawn open, its floor footbridge cells, with what
// closed it before it was cleared (its rubble, its wreckage) drawn too, each such node named "tpishut X Y W H": the
// tunnel's footprint (its shape's first map cell and its size in cells). An obstacle that clears once its land is bought
// (Arabian Nights' rock and cliff, Polar Zone's swamp) is drawn as it was before clearing, its cells closed, each of its
// nodes named "tpishut X Y W H M" ("tpishut X Y W H" when it closed none, the swamp):
// its footprint and the cells it closed (M, hex digits, four cells to a digit with the first the lowest bit, row by row
// from (X, Y)). In the mod's worlds the game (src/game/TPEngine.cpp EngineTick2) shows those nodes while the park owns
// none of the land the footprint and the cells next to it hold that a new park does not start with, the Invention gate's
// rule (include/TpiObstacleGate.h TpiGateOpen), and folds them away once it owns some; the closed cells then
// become land, their quads shown again, as the level's own (gHeightfieldCopy). So the obstacle's state is the park's
// land, and a saved park needs nothing more. (A tunnel's ends are walked whatever it shows: no path is laid on land the
// park does not own.)
// Header only and pure, so the host test sees what the game does.
#pragma once


// TpiObstacleGate.h: the TPI Worlds and Rides mod's opening gate (TPW_MOD_HOOK tpi_worlds): Land of Invention's gate,
// a Theme Park Inc obstacle that opened by itself once the land up to it was bought (its .sam's
// Obstacle.OpenedAutomaticly 1). Theme Park World has no obstacles, so the converter (assets/tpi_terrain.h) makes it part
// of the terrain, drawn closed, with its posts' cells closed and the passage between them ground, and names each of its
// leaves "tpigate X Y W H cx cy cz cw ox oy oz ow": the gate's footprint (its shape's first map cell and its size in
// cells) and the leaf's closed and open orientations (quaternions, x y z w, as its first animation's first and last
// rotation keys hold them). In the mod's worlds the game swings each leaf (src/game/TPEngine.cpp EngineTick1) open
// while the park owns any cell of the footprint or any cell next to it (sides and corners) that a new park does not
// start with, and closed while it owns none, so the gate's state is the park's land and a saved park needs nothing
// more. (Land of Invention's starting hoarding runs through the middle of its gate, so a new park owns half of it.)
// Header only and pure, so the host test sees what the game does.
#pragma once


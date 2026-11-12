// tpi_island_ring.h: the island screen's second ring for the TPI Worlds and Rides mod (tpw-mods tpi-worlds; TPW_MOD_HOOK
// tpi_islands, include/TpiWorlds.h). The lobby's description (lobby.txt) has ten camera places, and the original four islands
// stand at the first four, a square of side 200 round (500, 500). With the mod on, the three worlds of platform/tpi_content.h
// (Arabian Nights, Polar Zone, Land of Invention) stand in a row 200 apart at z 930, north of the first square: inside the
// lobby's sea (lobby.wad's terrain/Base.MD2, a disk at least 523 in radius round (550.5, 553.1); each island's centre is 96 or
// more inside its edge) and out of the first ring's fog (the lobby's fog ends 250 away, and the nearest ring-two island is
// 263 from the farthest place the browsing camera reaches in ring one), as list entries 4, 5 and 6. Tab (and a right click on
// the view) flies the camera between the rings; the left and right arrows and the panel's buttons browse the ring the camera
// is in.
// The functions here are the pure part (which ring a world is in, which island comes next, where the camera lands, the way it
// flies there), so a
// host test (tests/host_tpi_island_ring.cpp) can run them without the game; src/game/online/lobby_island.cpp and
// lobby_parser.cpp apply them to the screen's list. Only with the port's fixes (the original has one ring and no Tab).
#pragma once


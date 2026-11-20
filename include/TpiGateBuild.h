// TpiGateBuild.h: the ground beside the park gate in the TPI Worlds and Rides mod's worlds (TPW_MOD_HOOK tpi_worlds).
// Theme Park World's starting hoarding runs from the two gate posts (gHoardingLeft, gHoardingRight on row gHoardingBottom)
// up both sides of the entrance road, and its posts, two cells apart, each hold a 2 x 2 block of cells that nothing can be
// built on. In the mod's worlds that fence is hidden (LLCCM_CreateHoardingMesh) but still there, and Theme Park Inc has no
// fence at all: its land is its own hoardings.map, which in all three worlds gives a new park, beside the gate, the two
// entrance rows from left - 2 to right + 3 and, on the four rows before them, the three columns left - 2 to left and
// right + 1 to right + 3 (the road between stays outside). Theme Park World's fence owns only some of those cells and
// blocks the rest, so the strips right beside the gate and along the road could not be built on. Header only and pure,
// apart from the theme flag, so the host test sees what the game does.
#pragma once


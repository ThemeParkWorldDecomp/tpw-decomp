// TpiBumperKind.h: the TPI Worlds and Rides mod's bumper ride kind (TPW_MOD_HOOK tpi_worlds, LLCR_AddRideToDB).
// A bumper ride's .sam gives its kind (Bumper.BumperType), and BMP_CreateRide copies the ride's record from the
// template of that kind: Theme Park World has 14 of them, kinds -1 to -14 (gRideTemplates). Theme Park Inc's The Scorpions
// gives kind -15, a kind of its own that Theme Park World does not have, so the copy read the 208 bytes past the end of
// the table and the first car launched found no car mesh: the game crashed as soon as the ride took a rider. In the mod's
// worlds a kind outside Theme Park World's range is built as Theme Park World's bumper cars (kind -6): the same record
// as its other bumper cars (-11, -14), the largest arena ring, steered like every arena bumper ride, and the kind Theme
// Park Inc's own Science zone bumper cars give. A park saved before 0.5.2 keeps The Scorpions' id with the stray word's kind;
// loading it, the ride gets the stand-in's kind too (TpiBumperLoadId). Header only and pure, so the host tests see what the
// game does.
#pragma once


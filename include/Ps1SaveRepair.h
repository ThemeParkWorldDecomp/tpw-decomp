// Ps1SaveRepair.h: the ps1-rides mod's repair of parks saved with its 0.4.0 rides (TPW_MOD_HOOK ps1_save_repair). The
// mod's converter of 0.4.1 turns a PlayStation ride's footprint half a turn where 0.4.0 mirrored it, and gives the
// entrance and the exit their right roles, so a ride re-imported with 0.4.1 has its doors on other cells than a park
// saved before keeps for it: the park keeps the entry and exit cells (CObject's mF32 and mF34) and the map's door,
// queue and path cells as they were placed. On loading such a park (CSaveLoad::PostLoadOperations) each ride of the
// mod whose saved doors are not the ones its record now gives has them moved: the old door cells become the ride's
// body, the new ones doors, the old queue a path, the new exit joined to a path and the new entrance given a queue
// joined to one. Where no path can be reached that way, the door is left unjoined: the game's own state of a ride
// that needs its queue or its exit path built. A ride whose doors match is left alone, so a second load changes
// nothing. Only with the port's fixes: off (the default) or in a build without them, a park loads as it was saved.
//
// The planning works on a grid behind the small interface of RepairPs1RideDoors (the game's map cells in
// src/game/save/SaveLoad.cpp, a stand-in in tests/host_ps1_save_repair.cpp).
#pragma once


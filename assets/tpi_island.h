// tpi_island.h: the lobby islands of the three Theme Park Inc worlds (the TPI Worlds and Rides mod, tpi_convert.h).
// The game's island screen draws each world as two meshes (src/game/online/lobby_island.cpp: the isle and the gate,
// placed by a descriptor data/lobby/<world>.txt). Theme Park Inc has no islands, so the converter composes one for
// each of its worlds from the world's own pieces: a generated slab with cliff sides and a fence (the only geometry of
// ours), the world's entrance gate (features/Gates.wad, scaled down, with its sword animation and sign), one giant
// ride as the landmark and a few scenery features, all textured with the world's own textures (tpi_fsh.h). The
// originals' islands (lobby.wad) are the reference for the format and the proportions: about 50 by 65 units, the top
// near y 9, the gate (which carries the sign) a few units tall at the front (-z, where the camera settles), the
// landmark rising about 25 units above the top.
//
// Files written under the content root (outRoot = <data>/assets/tpi) for a world <w> with prefix <p> (arabian: ara,
// water: pol, science: inv):
//   data/lobby/<w>.txt                      the descriptor: ISLAND(slot,"data\lobby\<w>","<p>_isle","<p>_gate",title,0.0,y)
//                                           then RAINY, LIGHTNING and SKYCOLOUR lines
//   data/lobby/<w>/<p>_isle.md2             the island mesh (slab, fence, landmark, scenery)
//   data/lobby/<w>/<p>_gate.md2             the gate mesh; <p>_gateM1.md2 and <p>_gateM2.md2 its animations,
//   data/lobby/<w>/<p>_gate.sgn             its sign description, as the world's gate has them
//   data/lobby/<w>/textures/<stem>.tga      every texture both meshes name (lower-case stems, 32-bit TGA)
//   data/lobby/<w>/stexture/<stem>.tga      the same files for the low-detail setting
//   data/lobby/<w>/qickload.txt             the texture list (also in textures/ and stexture/), one name per line
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "tpi_disc.h"

namespace tpw::assets {

struct TpiIslandFiles {
    // Path relative to the content root ('/' separated) to the file's bytes.
    std::map<std::string, std::vector<uint8_t>> files;
    // One line per piece placed, texture missing or piece skipped, for the converter's log.
    std::vector<std::string> log;
};

// The three worlds' island files, in memory. False with a reason when a world's gate or terrain cannot be read.
bool BuildTpiIslandFiles(const TpiDisc& disc, TpiIslandFiles& out, std::string& reason);

// The same, written under outRoot (folders are created). False with a reason when a file cannot be written.
bool BuildTpiIslands(const TpiDisc& disc, const std::string& outRoot, std::string& reason);

}  // namespace tpw::assets

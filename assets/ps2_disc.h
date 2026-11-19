// ps2_disc.h: the PS2 Guest Models mod's disc reader (tpw-mods ps2-characters). It recognises the player's own copy of
// the PlayStation 2 version of the game (Theme Park Roller Coaster, USA, SLUS_200.99) by its content: SYSTEM.CNF boots
// SLUS_200.99, DATA/DATA.WAD is an 'FKNL' linkfile whose Chars folder holds the 24 characters, and each character's model
// has the triangle count the table in ps2_disc.cpp names. No disc data is built in: only those names and counts.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "source.h"

namespace tpw::assets {

enum class Ps2Status { NotPs2, Rejected, Supported };

struct Ps2Disc {
    std::string boot;
    // The image file or folder the disc was read from, for the launcher to remember (as Ps1Disc's path).
    std::string path;
    // DATA.WAD's Chars folder, unpacked (RefPack entries decoded): the path inside the WAD with '/' separators
    // ("Chars/Boy1a/boy1a.mps", "Chars/Textures/boybody1.tga") to its bytes.
    std::map<std::string, std::vector<uint8_t>> chars;
};

// One character of the PS2 disc: its folder in Chars and its model's triangle count (the recognition table).
struct Ps2Character {
    const char* folder;
    int triangles;
};
// The 24 characters of the disc's Chars folder, in the WAD's order.
const std::vector<Ps2Character>& Ps2Characters();

// The files of an 'FKNL' linkfile (the PS2 .WAD) whose path starts with `prefix` (case-insensitive), unpacked. Every
// offset and size is checked; false with a reason on a damaged or foreign file.
bool ReadPs2Wad(const std::vector<uint8_t>& wad, const std::string& prefix,
                std::map<std::string, std::vector<uint8_t>>& files, std::string& reason);

// The file in `chars` whose path matches `path` ignoring case, or nullptr.
const std::vector<uint8_t>* FindPs2File(const std::map<std::string, std::vector<uint8_t>>& chars, const std::string& path);

// NotPs2 when the source is not a PS2 disc of this game at all (with one line in `reason` when it is another PS2 game);
// Rejected with one line saying why when it is a PS2 disc of this game that cannot be used; Supported with `disc` filled.
Ps2Status RecognisePs2(Source& source, Ps2Disc& disc, std::string& reason);

// The same for a path: a folder holding the disc's files, an ISO 9660 image (.iso, or a raw .bin of 2352-byte
// sectors), or a .cue sheet naming its .bin.
Ps2Status RecognisePs2Path(const std::string& path, Ps2Disc& disc, std::string& reason);

}  // namespace tpw::assets

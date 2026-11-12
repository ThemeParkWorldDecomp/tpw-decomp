// tpi_wad.h: writes Bullfrog DWFB wads (version 2, stored members) for the Theme Park Inc converter (tpi_convert.h).
// The reader is wad.cpp. The layout is the originals': a 0x58-byte header ("DWFB", version 2, zeros, then at 0x48 the
// member count, the name block's offset and length), 40-byte entries from 0x58 (name hash, name offset, name length,
// data offset, stored size, flags 0, unpacked size 0), the members' bytes, and the name block last. A member's name is
// its path inside the wad with '\' between the folder and the file; the game's name tree (src/bullfrog/WadNameTree.cpp)
// puts an entry whose name has no folder part into the folder of the entry before it, so, as in the originals, only the
// first member of each run of one folder carries the folder prefix in the name block. The hash is the game's lookup
// hash of the whole path (TreeIterator::init): the characters upper-cased, summed four at a time as byte << (0..3), the
// last one to three bytes added plainly, 32 bits.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tpw::assets {

struct TpiWadMember {
    std::string name;            // "file" or "folder\file" (one folder level, as the game's wads have)
    std::vector<uint8_t> data;   // the member's bytes, stored as they are
};

// The game's hash of a member path (case does not matter).
uint32_t TpiWadNameHash(const std::string& path);

// The wad's bytes. Members keep the order given; members of one folder should be adjacent (BuildTpiWad does not sort,
// so a caller that mixes folders gets a correct wad with more prefixed names).
std::vector<uint8_t> BuildTpiWad(const std::vector<TpiWadMember>& members);
// Reads a wad (DWFB) into its members, unpacking RefPack members, with every name the full path the game gives it ('/'
// separated): the folder of a name without one is the folder of the entry before it (bullfrog/WadNameTree.cpp).
bool ReadTpiWad(const std::vector<uint8_t>& wad, std::vector<TpiWadMember>& members, std::string& err);
// True for the DWFB signature.
bool IsTpiWad(const std::vector<uint8_t>& bytes);

}  // namespace tpw::assets

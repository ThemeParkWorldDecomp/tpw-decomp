// ps2_convert.h: the PS2 Guest Models mod's converter (tpw-mods ps2-characters). From the player's recognised PS2 disc
// (ps2_disc.h) it writes, into the data folder's assets/ps2:
//   <character>.p2m   one model pack per character of the disc's Chars folder (include/Ps2Pack.h): the triangles, the
//                     bind pose and every clip baked to vertex positions at 30 frames a second (skinned here, so the
//                     game only blends two frames);
//   tex/<name>.tga    the textures the models use, from the disc's own TGAs, tiled where the models' texture
//                     coordinates run past one copy (the game's texture pages do not repeat);
//   converter.txt     the stamp, written last: the content is ready when it names kPs2ConverterVersion.
// No disc data is built into the converter.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "Ps2Pack.h"
#include "ps2_model.h"

namespace tpw::assets {
struct Ps2Disc;

constexpr int kPs2ConverterVersion = 1;
std::filesystem::path Ps2ContentPath(const std::filesystem::path& dataRoot);   // <dataRoot>/assets/ps2
std::filesystem::path Ps2ConverterStampPath(const std::filesystem::path& dataRoot);
// The version the content was converted by, 0 with no stamp or one that cannot be read.
int ReadPs2ConverterStamp(const std::filesystem::path& dataRoot);

// An image as RGBA rows from the top, from a 24- or 32-bit uncompressed TGA (type 2, either row order).
struct Ps2Image {
    int width = 0, height = 0;
    bool alpha = false;   // the TGA had 8 alpha bits
    std::vector<uint8_t> rgba;
};
bool DecodePs2Tga(const std::vector<uint8_t>& tga, Ps2Image& out, std::string& reason);
// Type 2, rows from the bottom, 32-bit when the image has alpha, else 24-bit.
std::vector<uint8_t> EncodePs2Tga(const Ps2Image& image);
// `image` repeated `across` x `down` times, then halved until it fits the game's 256 x 256 texture pages.
Ps2Image TilePs2Image(const Ps2Image& image, int across, int down);

// How many copies of each texture the triangles need across and down (1, 2 or 4) once each triangle is moved by whole
// copies so that its smallest u and v are in [0, 1). Merged over several models with MergePs2Tiling.
using Ps2Tiling = std::map<std::string, std::pair<int, int>>;
void MergePs2Tiling(const Ps2Mps& model, Ps2Tiling& tiling);

// Another character whose keys a shared clip may use.
struct Ps2Donor {
    std::string name;
    const Ps2Mps* model = nullptr;
    const Ps2Aps* anims = nullptr;
};
// One character's pack. `donors` are searched in order for the keys of a clip this one shares (Ps2Clip::shared): the
// first with the same group and clip, of the same length, with keys of its own, on a skeleton of as many bones.
// `notes` gets one line per clip that used a donor or moved bones rigidly.
bool BuildPs2Pack(const Ps2Mps& model, const Ps2Aps& anims, const std::vector<Ps2Donor>& donors, const Ps2Tiling& tiling,
                  Ps2Pack& out, std::vector<std::string>& notes, std::string& reason);

// The whole conversion into a staging folder next to assets/ps2, then swapped in; the stamp is written last. `log`
// gets one line per character.
bool ConvertPs2(const Ps2Disc& disc, const std::filesystem::path& dataRoot, std::vector<std::string>& log, std::string& reason);

}  // namespace tpw::assets

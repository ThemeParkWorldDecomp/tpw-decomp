// ps1_convert.h: convert the player's recognised PlayStation disc using headers and scripts from their PC files.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tpw::assets {
struct Ps1Disc;
// Each completed ride is installed by a directory rename. No disc or donor data is built into the converter.
bool ConvertPs1(const Ps1Disc &disc, const std::filesystem::path &dataRoot, std::string &reason);
// Publish a complete staging directory. On a failed replacement the previous complete ride is restored.
bool InstallPs1Ride(const std::filesystem::path &staged, const std::filesystem::path &destination, std::string &reason);
// A ride's own ground keeps its water, shore, rocks and paths but lets the park's grass show through its own: in a w x h
// tile of RGBA texels its grass gets alpha 0, except next to the rest, which it fades from over two texels.
void KeyPs1Grass(std::vector<std::array<uint8_t, 4>> &tile, int w, int h);
// Halloween's and Space's rides (theme "hallow" or "space") key their world's plain ground instead, over the whole
// patch of their cells at once: the plain ground the patch's edge reaches through plain texels, so enclosed art stays.
void KeyPs1Patch(std::vector<std::array<uint8_t, 4>> &tile, int w, int h, const std::string &theme);
// Whether a texel is the plain ground of a theme's world (none for other themes).
bool Ps1PlainGround(const std::string &theme, const std::array<uint8_t, 4> &p);
// The keying itself: the texels `ground` marks get alpha 0, faded from over two texels next to the rest.
void KeyPs1Ground(std::vector<std::array<uint8_t, 4>> &tile, int w, int h, const std::vector<bool> &ground);
} // namespace tpw::assets

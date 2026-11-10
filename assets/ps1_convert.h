// ps1_convert.h: convert the player's recognised PlayStation disc using headers and scripts from their PC files.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tpw::assets {
struct Ps1Disc;
// The converter's version: bumped with every change to what ConvertPs1 writes, so the launcher knows rides converted
// by an older converter and converts the disc again (launcher/ps1_refresh.h). 0.4.0 wrote no stamp (its output reads
// as 1); 0.4.1 (rides turned, not mirrored; doors; the ride's own ground) is 2.
constexpr int kPs1ConverterVersion = 2;
// The stamp ConvertPs1 leaves with its rides: <dataRoot>/assets/ps1/converter.txt, the version number on one line.
std::filesystem::path Ps1ConverterStampPath(const std::filesystem::path &dataRoot);
// The version the rides in <dataRoot>/assets/ps1 were converted by: 1 when there is no stamp (0.4.0's output, or a
// stamp that cannot be read), else the number in it.
int ReadPs1ConverterStamp(const std::filesystem::path &dataRoot);
// Writes the stamp for kPs1ConverterVersion (whole, through a temporary file). False with a reason.
bool WritePs1ConverterStamp(const std::filesystem::path &dataRoot, std::string &reason);
// Each completed ride is installed by a directory rename, and the stamp above is written after the last. No disc or
// donor data is built into the converter.
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

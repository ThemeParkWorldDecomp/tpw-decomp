// ps1_convert.h: convert the player's recognised PlayStation disc using headers and scripts from their PC files.
#pragma once

#include <filesystem>
#include <string>

namespace tpw::assets {
struct Ps1Disc;
// Each completed ride is installed by a directory rename. No disc or donor data is built into the converter.
bool ConvertPs1(const Ps1Disc &disc, const std::filesystem::path &dataRoot, std::string &reason);
// Publish a complete staging directory. On a failed replacement the previous complete ride is restored.
bool InstallPs1Ride(const std::filesystem::path &staged, const std::filesystem::path &destination, std::string &reason);
} // namespace tpw::assets

// tpi_convert.h: convert the player's recognised Theme Park Inc copy into three Theme Park World worlds, the content of
// the TPI Worlds and Rides mod (tpw-mods tpi-worlds; platform/tpi_content.h). No Theme Park Inc data is built into the
// converter: everything it writes comes from the player's copy, with formats and donors from their Theme Park World files.
#pragma once

#include <filesystem>
#include <string>

namespace tpw::assets {
struct TpiDisc;
// The converter's version: bumped with every change to what ConvertTpi writes, so the launcher knows worlds converted
// by an older converter and converts the copy again (launcher/tpi_refresh.h).
constexpr int kTpiConverterVersion = 16;
// The stamp ConvertTpi leaves with its worlds: <dataRoot>/assets/tpi/converter.txt, the version number on one line,
// written last, so content without it is not complete (the mod stays locked: platform/tpi_content.h).
std::filesystem::path TpiConverterStampPath(const std::filesystem::path &dataRoot);
// The version the worlds in <dataRoot>/assets/tpi were converted by: 0 when there is no stamp or it cannot be read.
int ReadTpiConverterStamp(const std::filesystem::path &dataRoot);
// Writes the stamp for kTpiConverterVersion (whole, through a temporary file). False with a reason.
bool WriteTpiConverterStamp(const std::filesystem::path &dataRoot, std::string &reason);
// Converts the copy into <dataRoot>/assets/tpi (built in a staging folder beside it and published by a directory
// rename, the stamp last). False with a reason, the previous complete content kept. The player's Theme Park World
// data under <dataRoot>/assets/<edition>/Theme Park Data must be there: each world takes its donor world's mystery
// item and ground items from it (tpi_convert.cpp, kDonorItems).
bool ConvertTpi(const TpiDisc &disc, const std::filesystem::path &dataRoot, std::string &reason);
// The launcher's way to convert a remembered copy again: opens `path` (a disc image, an archive or a folder),
// recognises it and converts it. False with a reason ("not a supported Theme Park Inc copy any more", ...).
bool ConvertTpiFromPath(const std::string &path, const std::filesystem::path &dataRoot, std::string &reason);
} // namespace tpw::assets

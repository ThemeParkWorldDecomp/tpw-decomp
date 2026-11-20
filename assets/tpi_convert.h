// tpi_convert.h: convert the player's recognised Theme Park Inc copy into three Theme Park World worlds, the content of
// the TPI Worlds and Rides mod (tpw-mods tpi-worlds; platform/tpi_content.h). No Theme Park Inc data is built into the
// converter: everything it writes comes from the player's copy, with formats and donors from their Theme Park World files.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace tpw::assets {
struct TpiDisc;
// The converter's version: bumped with every change to what ConvertTpi writes, so the launcher knows worlds converted
// by an older converter and converts the copy again (launcher/tpi_refresh.h).
constexpr int kTpiConverterVersion = 29;
// The stamp ConvertTpi leaves with its worlds: <dataRoot>/assets/tpi/converter.txt, the version number on one line,
// written last, so content without it is not complete (the mod stays locked: platform/tpi_content.h).
std::filesystem::path TpiConverterStampPath(const std::filesystem::path &dataRoot);
// The version the worlds in <dataRoot>/assets/tpi were converted by: 0 when there is no stamp or it cannot be read.
int ReadTpiConverterStamp(const std::filesystem::path &dataRoot);
// Writes the stamp for kTpiConverterVersion (whole, through a temporary file). False with a reason.
bool WriteTpiConverterStamp(const std::filesystem::path &dataRoot, std::string &reason);
// What a conversion did with the add-on kept in TpiAddOnFolder: each item it added, as the name the build menu shows and
// its world ("Depth Charge (Polar Zone)"), each one it left out with why ("lostsub: already in the copy"), and the
// ready-made coasters kept there (Theme Park World has no ready-made coaster to build them with, so they are only kept).
struct TpiAddOnResult {
    std::vector<std::string> added;
    std::vector<std::string> leftOut;
    int coasters = 0;
};
// Converts the copy into <dataRoot>/assets/tpi (built in a staging folder beside it and published by a directory
// rename, the stamp last). False with a reason, the previous complete content kept. The player's Theme Park World
// data under <dataRoot>/assets/<edition>/Theme Park Data must be there: each world takes its donor world's mystery
// item and ground items from it (tpi_convert.cpp, kDonorItems), and its golden ticket thresholds from the world of the
// same key cost (TicketThresholds). The items of the add-on kept in TpiAddOnFolder join their worlds after the copy's own
// items, so they take the next ids and every id the copy's own items had stays as it was (parks keep their rides); an
// item the copy already has (an installed game with the Bonus Disk on it) is left to the copy's own. `addOn`, when given,
// says what was added.
bool ConvertTpi(const TpiDisc &disc, const std::filesystem::path &dataRoot, std::string &reason,
                TpiAddOnResult *addOn = nullptr);
// The launcher's way to convert a remembered copy again: opens `path` (a disc image, an archive or a folder),
// recognises it and converts it. False with a reason ("not a supported Theme Park Inc copy any more", ...).
bool ConvertTpiFromPath(const std::string &path, const std::filesystem::path &dataRoot, std::string &reason,
                        TpiAddOnResult *addOn = nullptr);

// The SimCoaster Bonus Disk (tpi_disc.h TpiStatus::AddOn) is kept in the data folder, as its installer lays it out below
// an installed game's Data folder, so every later conversion (a new copy, a newer converter) adds it again:
// <dataRoot>/assets/tpi/bonus/Data/levels/<world>/<item folder>/<item>.wad and .../Data/My Coasters/<coaster>.cos.
std::filesystem::path TpiAddOnFolder(const std::filesystem::path &dataRoot);
// What StoreTpiAddOn did: the files written and those already there with the same bytes (paths below Data), and the
// add-on's files not kept, with why ("levels/water/rides/x.wad: not a ride's wad").
struct TpiAddOnStore {
    std::vector<std::string> written;
    std::vector<std::string> alreadyThere;
    std::vector<std::string> leftOut;
};
// Keeps the add-on's files (each read whole and checked: an item's wad must open and hold its .sam with an Info.Id),
// each written through a temporary file and a rename. False with a reason when a file cannot be written; a file that
// fails its check is only left out. The .cdk registration files are not kept (the installer's own bookkeeping).
bool StoreTpiAddOn(const TpiDisc &addOn, const std::filesystem::path &dataRoot, TpiAddOnStore &store,
                   std::string &reason);
// Whether the worlds in <dataRoot>/assets/tpi were converted with every file kept in TpiAddOnFolder (converted.txt lists
// the add-on files a conversion took, by path and size). True when nothing is kept.
bool TpiAddOnConverted(const std::filesystem::path &dataRoot);
} // namespace tpw::assets

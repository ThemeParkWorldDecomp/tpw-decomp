// The Theme Park Inc import's content checks. Theme Park Inc (SimCoaster in North America, Theme Park Manager in
// Germany) is Theme Park World's sequel, on the same engine: the TPI Worlds and Rides mod (tpw-mods tpi-worlds) makes
// three more worlds from the player's own copy of it (tpi_convert.h). A supported copy is identified by its own files,
// wherever they are: a disc image, the disc's folder, an installed game's folder, or an archive of one of those. Nothing
// goes by a file name alone or by a checksum of the whole image: the game's data folder holds its three worlds
// (levels/arabian, levels/water, levels/science, each with its Standard.sam and terrain.wad), as plain files in an
// installed game or inside the disc's InstallShield 5 or 6 cabinets (data1.hdr, data1.cab, data2.cab; installshield.h).
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sniff.h"
#include "source.h"

namespace tpw::assets {

enum class TpiStatus { NotThemeParkInc, Rejected, Supported };

struct TpiDisc {
    std::string edition;   // which release it is, for the report's line: "Theme Park Inc", "SimCoaster", ...
    // The file (a disc image or an archive) or folder the copy was read from, and for a file its SHA-256 (lower-case
    // hex): the importer remembers them (launcher.ini tpi_disc_path, tpi_disc_sha256) so the launcher can convert the
    // copy again when its converter changes (tpi_convert.h kTpiConverterVersion). The hash is empty for a folder, and
    // both are empty for a copy inside another container.
    std::string path;
    std::string sha256;
    // The copy's files in installed form: a Source whose root holds the game's Data folder as "Data/..." (always, so
    // root is ""), with the files below Data under the names the copy gives them (their case varies between copies:
    // FindTpiEntry). For a disc it reads the cabinets' "Data" group through installshield.h, so the converter never
    // knows about cabinets; for an installed game it is a view of the dropped source's Data folder.
    std::shared_ptr<Source> source;
    std::string root;
    // Where the copy sits inside the source it was recognised in: the prefix ('/' separated, trailing '/') of the
    // folder holding the game (the Data folder's parent or the cabinets), "" when the whole source is the copy. The
    // importer imports nothing under it as Theme Park World files. Set for a Rejected copy too.
    std::string claimed;
    // The InstallShield major version of the cabinets, 0 for an installed game's plain files.
    int cabinetVersion = 0;
    // The worlds found, in the order arabian, water, science ("" for one that is missing: Rejected).
    std::vector<std::string> worlds;
};

// NotThemeParkInc leaves the ordinary importer in charge. Rejected gives one line explaining why a Theme Park Inc copy
// must not be converted (a demo, a copy with files missing). Reads only the source's listing and small files; the
// overload with options opens cabinet volumes inside zip and cab sources through a spill file (OpenEntryBlobSpilled).
TpiStatus RecogniseTpi(const std::shared_ptr<Source>& source, TpiDisc& disc, std::string& reason);
TpiStatus RecogniseTpi(const std::shared_ptr<Source>& source, const SniffOptions& options, TpiDisc& disc,
                       std::string& reason);
// A path the player dropped (or the launcher remembered): a folder, a disc image or an archive, the copy at its root
// or inside a disc image or archive nested in it (up to kMaxNesting). NotThemeParkInc when nothing in it is this game
// (reason says why it could not be opened when it could not be).
TpiStatus RecogniseTpiPath(const std::string& path, const SniffOptions& options, TpiDisc& disc, std::string& reason);
// The entry of a copy's source at a path, compared without regard to case ("Data/levels/arabian/Standard.sam" finds
// "Data/Levels/Arabian/STANDARD.SAM"); null when there is none. Data entries only.
const Entry* FindTpiEntry(const Source& source, const std::string& path);

}  // namespace tpw::assets

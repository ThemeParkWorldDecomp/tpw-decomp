// tpi_convert.cpp: the Theme Park Inc converter (tpi_convert.h). Theme Park Inc keeps each of its worlds under
// Data/levels/<world>/ in the same shape as Theme Park World's (the item folders hold one .wad per ride, shop,
// sideshow, feature or upgrade; the terrain, shared textures, queues, hoardings and advisor are wads too), so the
// converter walks that tree and writes a Theme Park World world from it:
//  - textures: Theme Park Inc's .fsh become TGAs with the same stem (tpi_fsh.h; 24-bit when every texel is opaque,
//    32-bit otherwise), inside wads and items alike, and the qickload.txt lists are rewritten to name them and only
//    what their own folder holds; 8-bit grey TGAs become 24-bit ones too;
//  - items: each .wad is written as an unpacked folder (the game reads both) whose .sam keeps only the statements
//    Theme Park World's ride table has (tpi_sam.h); its Info.Id is moved into the world's own block of ids so it never
//    collides with a Theme Park World item, and every reference to it follows; rides lose their price per use, as
//    Theme Park World's rides have none (the park charges at the gate); its Info.Name becomes the name Theme Park Inc
//    shows, from the copy's own object names bank (tpi_names.h), the port's own name for a fixed item the bank does not
//    name (the helipad, the ticket booth...), or its working name in title case; shops keep their prices and get the
//    RipOffOK that gives them Theme Park World's room between price and worth (ShopRipOffOK), and the photo booth, a
//    shop its upgrades folder marks unchoosable, is made one visitors use (MakeShopChoosable);
//  - the mystery item and the two ground items every Theme Park World world has (and the Buy Ride panel needs), and
//    the advisor, are copied from the donor world in the player's own Theme Park World data under <dataRoot>/assets
//    (kDonorItems); nothing of Theme Park Inc's advisor is used;
//  - the world's own .sam files (Standard, global, Hoardings, EndHoardings) are filtered the same way, with the
//    few renamed statements and the lobby settings the mod needs (key cost, tickets), and the local golden ticket
//    thresholds of the Theme Park World world of the same key cost, from the player's own data (TicketThresholds);
//  - the world wads (terrain, sharetex, ssharete, dynamic, queue, miscmesh, hoarding) are rebuilt stored
//    (tpi_wad.h), the terrain's .tct named after the world; the world's obstacles (obstacles/*.wad: the lake, the
//    bridges, the tunnels, the rocks, which a Theme Park Inc player paid to clear) become fixed scenery of the terrain,
//    drawn as a player who cleared them sees them where their files tell that state, with the cells they block made
//    cells nothing is built on (tpi_terrain.h), and
//    the quads Theme Park Inc textured fully transparent under them are hidden;
//  - sky, 2D map, scape, music and sounds are copied as they are, but for the few sounds tuned for the PC's 3D sound
//    whose volume is lowered (kSfxPatches, tpi_sfx.h);
//  - the island screen's music (Sound/LobbyMusicHD.sdt and its cat_locallobbymusic maps), which Theme Park Inc does
//    not have, is made from three segments of the world's own park music (tpi_lobby_music.h);
//  - the worlds' entertainers, costumes and costume heads come out of Theme Park Inc's esprites.wad as loose files
//    under data/esprites/<world>/ (kSpriteGroups), which the game finds before its own esprites.wad; a world with two
//    entertainers gets the first again under a new name, as the game takes three.
//  - each world's lobby island (data/lobby/<world>.txt and data/lobby/<world>/) is composed from the world's own gate,
//    terrain and textures (tpi_island.h), since Theme Park Inc has no island screen.
// What Theme Park World has no use for is left out and listed in <dataRoot>/assets/tpi/converted.txt: the facilities
// it does not know, the online files, the speech, the advisor. Everything is written into a staging folder first and
// swapped into place at the end, the stamp (converter.txt) last of all.
#include "tpi_convert.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

#include "../platform/path_text.h"
#include "sniff.h"
#include "source.h"
#include "tpi_disc.h"
#include "tpi_fsh.h"
#include "tpi_island.h"
#include "tpi_island_md2.h"
#include "tpi_lobby_music.h"
#include "tpi_names.h"
#include "tpi_sam.h"
#include "tpi_sfx.h"
#include "tpi_terrain.h"
#include "tpi_wad.h"

namespace fs = std::filesystem;

namespace tpw::assets {

fs::path TpiConverterStampPath(const fs::path &dataRoot) { return dataRoot / "assets" / "tpi" / "converter.txt"; }

int ReadTpiConverterStamp(const fs::path &dataRoot) {
    std::ifstream in(TpiConverterStampPath(dataRoot));
    std::string line;
    if (!in || !std::getline(in, line))
        return 0;
    size_t end = line.find_first_not_of("0123456789");
    if (end == 0 || (end != std::string::npos && line.find_first_not_of(" \t\r", end) != std::string::npos))
        return 0;
    if (end == std::string::npos)
        end = line.size();
    if (end > 9)
        return 0;
    return std::stoi(line.substr(0, end));
}

bool WriteTpiConverterStamp(const fs::path &dataRoot, std::string &reason) {
    try {
        fs::path path = TpiConverterStampPath(dataRoot);
        if (fs::is_symlink(fs::symlink_status(path))) {
            reason = "SimCoaster AKA Theme Park Inc converter stamp is a symbolic link";
            return false;
        }
        fs::create_directories(path.parent_path());
        fs::path part = path;
        part += ".part";
        {
            std::ofstream out(part, std::ios::binary | std::ios::trunc);
            out << kTpiConverterVersion << "\n";
            if (!out) {
                reason = "cannot write the SimCoaster AKA Theme Park Inc converter stamp";
                return false;
            }
        }
        fs::rename(part, path);
        return true;
    } catch (const std::exception &e) {
        reason = std::string("cannot write the SimCoaster AKA Theme Park Inc converter stamp: ") + e.what();
        return false;
    }
}

namespace {


using Bytes = std::vector<uint8_t>;

// The three worlds: Theme Park Inc's folder, the id block each one's items move into (WORLD-RECIPE: the ids Theme Park
// World's own tables use run from 1100 to 4605).
struct WorldSpec {
    const char *folder;
    int32_t idBase;
    const char *donor;     // the Theme Park World world whose files answer for a missing one (tpi_content.h)
    const char *display;   // the world's name, written on the park's entrance gate (the gate item's Info.Name)
    int keys;              // global.sam Keys.CostToEnter: the lobby draws the digit frame cost - 1, so 1 to 5 (no
                           // Theme Park World world costs 0: jungle 1, hallow 1, fantasy 3, space 5)
};
// The key costs follow Theme Park Inc's own order of the worlds, from its missions (Data/missions): play starts in Land
// of Invention (miss00.sam and miss01.sam, theme 1), miss02.sam opens Polar Zone (Completion.ThemeNumber 2) and
// miss04.sam opens Arabian Nights (Completion.ThemeNumber 0). So 1, 3 and 5 keys, as Theme Park World's worlds go.
const WorldSpec kWorlds[] = {{"arabian", 5000, "jungle", "Arabian Nights", 5},
                             {"water", 6000, "hallow", "Polar Zone", 3},
                             {"science", 7000, "space", "Land of Invention", 1}};
constexpr int32_t kIdStep = 10;
// Fixed items whose Theme Park World ids are kept (the bus stop and the end hoarding mean the same in both games).
constexpr int32_t kKeptIds[] = {1600, 1605};
constexpr size_t kMaxItems = 330;   // the game's ride list
constexpr uint64_t kMaxEntry = 256ull << 20;

std::string Lower(std::string s) {
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::string Upper(std::string s) {
    for (char &c : s)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    return s;
}

bool EndsWith(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string Stem(const std::string &name) {
    const size_t dot = name.find_last_of('.');
    const size_t slash = name.find_last_of('/');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return name;
    return name.substr(0, dot);
}

std::string Extension(const std::string &name) {   // lower case, with the dot
    const std::string stem = Stem(name);
    return Lower(name.substr(stem.size()));
}

std::string Base(const std::string &path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string Dir(const std::string &path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

[[noreturn]] void Fail(const std::string &what) { throw std::runtime_error(what); }

// The conversion's record for converted.txt.
struct Log {
    std::ostringstream text;
    int itemsConverted = 0;
    int itemsLeftOut = 0;
    int texturesConverted = 0;
    int filesWritten = 0;
    uint64_t bytesWritten = 0;
    std::set<std::string> droppedStatements;   // "<file kind>: Group.Field"
    TpiAddOnResult addOn;                      // what the add-on kept in TpiAddOnFolder gave (tpi_convert.h)
};

struct Writer {
    fs::path root;   // the staging folder for this world
    Log *log;
    void file(const std::string &relative, const Bytes &bytes) {
        fs::path path = root / fs::u8path(relative);
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
            Fail("cannot create " + relative);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        out.close();
        if (!out)
            Fail("cannot write " + relative);
        log->filesWritten++;
        log->bytesWritten += bytes.size();
    }
    void text(const std::string &relative, const std::string &s) { file(relative, Bytes(s.begin(), s.end())); }
};

// The source's entries under the game's Data folder, by lower-case path relative to Data/levels/.
struct Levels {
    const Source *source;
    std::vector<std::pair<std::string, const Entry *>> entries;   // (lower relative path, entry), sorted
    Bytes read(const Entry &entry) const {
        std::string err;
        auto stream = const_cast<Source *>(source)->Open(entry, err);
        if (!stream)
            Fail("cannot open " + entry.path + ": " + err);
        Bytes out;
        if (!ReadAll(*stream, kMaxEntry, out, err))
            Fail("cannot read " + entry.path + ": " + err);
        return out;
    }
    const Entry *find(const std::string &lowerRelative) const {
        auto it = std::lower_bound(entries.begin(), entries.end(), lowerRelative,
                                   [](const auto &a, const std::string &b) { return a.first < b; });
        return it != entries.end() && it->first == lowerRelative ? it->second : nullptr;
    }
    // The entries under a folder (lower case, with its trailing '/'), as (path after the folder, entry).
    std::vector<std::pair<std::string, const Entry *>> under(const std::string &folder) const {
        std::vector<std::pair<std::string, const Entry *>> out;
        auto it = std::lower_bound(entries.begin(), entries.end(), folder,
                                   [](const auto &a, const std::string &b) { return a.first < b; });
        for (; it != entries.end() && it->first.compare(0, folder.size(), folder) == 0; ++it)
            out.push_back({it->first.substr(folder.size()), it->second});
        return out;
    }
};

// ---- members (the files of an item or of a world wad)

struct Converted {
    std::vector<TpiWadMember> members;
};

// Textures and texture lists: .fsh to .tga (24-bit when opaque, OpaqueTgaTo24), a .tga beside a .fsh of the same stem
// dropped, qickload.txt rewritten.
// An uncompressed 8-bit grey TGA (image type 3; TPI's heli has nine) as the 32-bit true colour TGA the game's texture
// loader takes (TextureLoadManager.cpp: type 2 only): each grey level becomes B = G = R, alpha 255, the origin kept.
// False when `in` is not such a TGA.
bool GreyTgaToTrueColour(const Bytes &in, Bytes &out) {
    if (in.size() < 18 || in[1] != 0 || in[2] != 3 || in[16] != 8)
        return false;
    const size_t width = in[12] | in[13] << 8, height = in[14] | in[15] << 8, start = 18 + size_t(in[0]);
    if (in.size() < start + width * height)
        return false;
    out.assign(in.begin(), in.begin() + 18);
    out[0] = 0;
    out[2] = 2;
    out[16] = 32;
    out[17] = static_cast<uint8_t>((in[17] & 0x30) | 8);
    out.reserve(18 + width * height * 4);
    for (size_t i = 0; i < width * height; ++i) {
        const uint8_t grey = in[start + i];
        out.insert(out.end(), {grey, grey, grey, 255});
    }
    return true;
}

// A 32-bit true colour TGA whose every alpha is 255 as a 24-bit one (the alpha bits dropped, the origin kept), so it
// loads as Theme Park World's own opaque textures do, with no alpha (ps1_convert.cpp writes its opaque ones the same
// way). The game's loader marks every 4-byte texel
// texture as having alpha (TextureLoadManager.cpp UncompressedConvert: load flag 1, entry flag 0x100), and
// TextureEntry.cpp GetBlendingMode then draws it alpha tested and blended (0x2756) rather than opaque (0x1256). Science's
// road outside the gate (terrain.wad textures/cobbles03, every alpha 255) drawn that way left the guests walking on it
// with a cyan edge and no shadow (0.5.1). Unchanged when `tga` is not such a TGA.
void OpaqueTgaTo24(Bytes &tga) {
    if (tga.size() < 18 || tga[1] != 0 || tga[2] != 2 || tga[16] != 32)
        return;
    const size_t width = tga[12] | tga[13] << 8, height = tga[14] | tga[15] << 8, start = 18 + size_t(tga[0]);
    const size_t end = start + width * height * 4;
    if (tga.size() < end)
        return;
    for (size_t i = start + 3; i < end; i += 4)
        if (tga[i] != 255)
            return;
    Bytes out(tga.begin(), tga.begin() + static_cast<ptrdiff_t>(start));
    out[16] = 24;
    out[17] = static_cast<uint8_t>(tga[17] & 0x30);
    out.reserve(tga.size() - width * height);
    for (size_t i = start; i < end; i += 4)
        out.insert(out.end(), {tga[i], tga[i + 1], tga[i + 2]});
    out.insert(out.end(), tga.begin() + static_cast<ptrdiff_t>(end), tga.end());
    tga = std::move(out);
}

// Meshes: a boat's wake. The Great Lake's boat (science's rides/vict_hs, vict_bt.MD2) has a flat "wake" node at its
// waterline, a soft cyan patch of water (wake.tga) that Theme Park Inc draws on its lake. The converted ride stands on
// land (the lake obstacle is drained, tpi_terrain.h), so the patch lay on the grass at the ground's own height and came
// and went as the camera moved, the two fighting over the depth test (0.5.3). A "wake" geometry node with no nodes
// below it has its vertices drawn together into its first one, so its faces cover no pixel. The file is patched in
// place, not rewritten (WriteMd2 writes no embedded records): a saved park keeps each ride's node and embedded counts
// and a word per node and per embedded record (SaveRides, LoadRides in src/game/rides/TPRide.cpp), and the boat's
// embedded records are its heads (Head01, the guests' seats), so the node tables, their order, the textures and the
// embedded records stay exactly as Theme Park Inc's. False, `out` untouched, when the mesh has no such node.
bool HideWake(const Bytes &in, Bytes &out) {
    tpi_island::Md2Model model;
    std::string err;
    if (!tpi_island::ReadMd2(in, model, err))   // also checks that every table below lies inside the file
        return false;
    auto u16 = [&](size_t at) { return static_cast<uint32_t>(in[at] | in[at + 1] << 8); };
    auto u32 = [&](size_t at) { return u16(at) | u16(at + 2) << 16; };
    constexpr size_t kNode = 0xa0, kMini = 0x58;
    const uint32_t allNodes = u16(0x42), nodeCount = u16(0x44), nodeOff = u32(0x70), miniOff = u32(0x74);
    std::vector<size_t> records;   // every node record, full then mini: the parents are their offsets
    for (uint32_t i = 0; i < allNodes; ++i)
        records.push_back(i < nodeCount ? nodeOff + kNode * i : miniOff + kMini * (i - nodeCount));
    Bytes patched = in;
    bool hidden = false;
    for (uint32_t i = 0; i < nodeCount; ++i) {
        const size_t at = records[i];
        const uint32_t nameOff = u32(at + 0x54), vertexCount = u16(at + 0x58), sets = u32(at + 0x60);
        std::string name;
        for (size_t k = nameOff; nameOff != 0 && k < in.size() && in[k] != 0; ++k)
            name += static_cast<char>(in[k]);
        bool parent = false;
        for (size_t r : records)
            parent = parent || u32(r + 4) == at;
        if (Lower(name) != "wake" || parent || vertexCount < 2)
            continue;
        // Positions come in sets of four: x0..x3, y0..y3, z0..z3.
        for (uint32_t v = 1; v < vertexCount; ++v)
            for (size_t axis = 0; axis < 3; ++axis)
                std::memcpy(&patched[sets + (v / 4) * 0x30 + axis * 0x10 + (v % 4) * 4], &in[sets + axis * 0x10], 4);
        hidden = true;
    }
    if (!hidden || patched == in)
        return false;
    out = std::move(patched);
    return true;
}

// The two statements of a coaster's track file that cap its trains' speed, at the lowest and the highest speed setting
// (CoasterSetSpeed blends them by the ride's setting). Theme Park Inc's files set them far below the game's own limits
// (SetCoasterTypeDefaults), and no Theme Park World coaster sets them. The game carries the capped speed on along the
// track (CalculateTrackGForcesSpeedAndExcitement, CoasterCarRideTrackTick), so with Theme Park Inc's caps a train stops
// gaining speed part way down a drop, the next climb or loop runs out of speed and its brace gets a chain lift. They are
// left out, so the game's own limits apply and the coaster rides by Theme Park World's physics.
const char *const kCoasterSpeedLimits[] = {"fMaxSpeedAtMinSetting", "fMaxSpeedAtMaxSetting"};

// Adds the speed-limit statements `text` has to `options.drop`; returns them with their values for the record ("" when
// there are none).
std::string DropCoasterSpeedLimits(const std::string &text, TpiSamOptions &options) {
    std::string note;
    for (const char *field : kCoasterSpeedLimits) {
        float value = 0.0f;
        if (!TpiSamFloat(text, "sCoasterType", field, value))
            continue;
        options.drop.push_back(std::string("sCoasterType.") + field);
        std::ostringstream one;
        one << field << " " << value;
        note += (note.empty() ? "" : ", ") + one.str();
    }
    return note;
}

// A trough (a flume's track, bSpecialIsTrough) shows its lift belt on climbs and its water everywhere else: one select
// with only a pitch range, a climb (the game's pitch is negative going up), answers the belt's cross section, and one
// with no range at all answers the water's. Every Theme Park World and Theme Park Inc trough has them so, and names no
// cross section for its lift braces (uiWinchCrossSection, which the game draws over the whole of a chain lift brace,
// the part past the top going down too). The Bonus Disk's Depth Charge flume (lostsub) has its two cross sections the
// other way round (the water first, then the belt) but the same selects, and names the belt for its lift braces: the
// belt runs along every flat and every drop and over the tops of its lift hills, and the water shows only on climbs.
// When a trough names its lift's cross section and only the select without a range answers it, the two selects' cross
// sections are swapped (in `options.append`) and the lift's cross section is left out (in `options.drop`), so the
// trough draws by pitch like every other.
// Returns what was changed for the record ("" when nothing was).
std::string FixTroughBelt(const std::string &text, TpiSamOptions &options) {
    int32_t trough = 0, lift = 0;
    if (!TpiSamInteger(text, "sCoasterType", "bSpecialIsTrough", trough) || trough != 1 ||
        !TpiSamInteger(text, "sCoasterType", "uiWinchCrossSection", lift) || lift == 0)
        return "";
    static const char *const kRanges[] = {"fSpeedRangeMin", "fSpeedRangeMax", "fDirectionRangeMin", "fDirectionRangeMax",
                                          "fPitchRangeMin", "fPitchRangeMax", "fBankRangeMin",      "fBankRangeMax"};
    int climb = -1, rest = -1;
    int32_t climbSection = 0, restSection = 0;
    for (int i = 0; i < 20; ++i) {
        const std::string group = "asCrossSectionSelects[" + std::to_string(i) + "]";
        int32_t section = 0;
        if (!TpiSamInteger(text, group, "usCrossSectionIndex", section))
            continue;
        int ranges = 0;
        for (const char *field : kRanges) {
            float value = 0.0f;
            if (TpiSamFloat(text, group, field, value))
                ++ranges;
        }
        float low = 0.0f, high = 0.0f;
        const bool pitch = TpiSamFloat(text, group, "fPitchRangeMin", low) && TpiSamFloat(text, group, "fPitchRangeMax", high);
        if (ranges == 0) {
            rest = i;
            restSection = section;
        } else if (ranges == 2 && pitch && low < 0.0f && high <= 0.0f) {
            climb = i;
            climbSection = section;
        } else {
            return "";
        }
    }
    if (climb < 0 || rest < 0 || climbSection == lift || restSection != lift)
        return "";
    const std::string head = "asCrossSectionSelects[";
    options.append.push_back(head + std::to_string(climb) + "].usCrossSectionIndex " + std::to_string(lift));
    options.append.push_back(head + std::to_string(rest) + "].usCrossSectionIndex " + std::to_string(climbSection));
    options.drop.push_back("sCoasterType.uiWinchCrossSection");
    return "climbs showed cross section " + std::to_string(climbSection) + " and the rest and the lift " +
           std::to_string(lift);
}

// Sam files: filtered through `samTable` with `samOptions` when the member's extension is .sam (nullptr: copied).
// Online_*.sam members are left out, and so are meshes kept among the textures (TPI's gokarts has copies there).
// A boat's wake node is drawn as a point (HideWake), but with `keepWake` (the lake ride's boats, which the game sails on
// its lake: include/TpiGreatLake.h), whose meshes are copied as they are.
// An animated texture list (.ifl; TPI's penguins and seals blink with one) becomes a .tga of its most used frame, as
// Theme Park World has no animated textures and the mesh asks for "<stem>.tga".
Converted ConvertMembers(const std::vector<TpiWadMember> &in, const std::vector<SamRow> *samTable,
                         const TpiSamOptions *samOptions, const std::vector<SamRow> *coasterTable, const std::string &what,
                         Log &log, bool keepWake = false) {
    Converted out;
    std::vector<const TpiWadMember *> ifls;
    std::set<std::string> fshStems;   // lower "folder/stem"
    for (const auto &m : in)
        if (Extension(m.name) == ".fsh")
            fshStems.insert(Lower(Stem(m.name)));
    for (const auto &m : in) {
        const std::string ext = Extension(m.name);
        const std::string base = Base(m.name);
        if (ext == ".fsh") {
            TpiWadMember t;
            t.name = Stem(m.name) + ".tga";
            std::string err;
            if (!TpiFshToTga(m.data, t.data, err)) {
                log.text << "    texture not converted: " << what << "/" << m.name << ": " << err << "\n";
                continue;
            }
            OpaqueTgaTo24(t.data);
            log.texturesConverted++;
            out.members.push_back(std::move(t));
        } else if (ext == ".tga" && fshStems.count(Lower(Stem(m.name)))) {
            log.text << "    texture left out: " << what << "/" << m.name << " (a .fsh of the same name wins)\n";
        } else if (TpiWadMember t{m.name, {}}; ext == ".tga" && GreyTgaToTrueColour(m.data, t.data)) {
            OpaqueTgaTo24(t.data);
            log.texturesConverted++;
            out.members.push_back(std::move(t));
        } else if (Lower(base) == "qickload.txt") {
            std::string text(m.data.begin(), m.data.end()), rewritten;
            std::string line;
            for (size_t i = 0; i <= text.size(); ++i) {
                if (i < text.size() && text[i] != '\n') {
                    line.push_back(text[i]);
                    continue;
                }
                std::string trimmed = line;
                while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == ' ' || trimmed.back() == '\t'))
                    trimmed.pop_back();
                if (Extension(trimmed) == ".fsh")
                    trimmed = Stem(trimmed) + ".tga";
                if (i < text.size() || !line.empty())
                    rewritten += trimmed + "\n";
                line.clear();
            }
            out.members.push_back({m.name, Bytes(rewritten.begin(), rewritten.end())});
        } else if (ext == ".ifl") {
            ifls.push_back(&m);
        } else if (ext == ".md2" && (Lower(Base(Dir(m.name))) == "textures" || Lower(Base(Dir(m.name))) == "stexture")) {
            log.text << "    left out: " << what << "/" << m.name << " (a mesh among the textures)\n";
        } else if (TpiWadMember t{m.name, {}}; ext == ".md2" && !keepWake && HideWake(m.data, t.data)) {
            log.text << "    wake hidden: " << what << "/" << m.name << " (Theme Park Inc draws it on its lake)\n";
            out.members.push_back(std::move(t));
        } else if (ext == ".sam" && Lower(base).compare(0, 7, "online_") == 0) {
            log.text << "    left out: " << what << "/" << m.name << " (online)\n";
        } else if (ext == ".sam" && samTable != nullptr) {
            // A coaster's track file (coaster.sam, read with the coaster table) or the item's own file.
            const bool coaster = Lower(base) == "coaster.sam" && coasterTable != nullptr;
            TpiSamOptions options = coaster ? TpiSamOptions{} : *samOptions;
            std::string text(m.data.begin(), m.data.end()), filtered, err;
            const std::string limits = coaster ? DropCoasterSpeedLimits(text, options) : std::string();
            const std::string belt = coaster ? FixTroughBelt(text, options) : std::string();
            TpiSamReport report;
            if (!FilterTpiSam(coaster ? *coasterTable : *samTable, text, options, filtered, report, err))
                Fail(what + "/" + m.name + ": " + err);
            // The statements left out or rewritten on purpose are in the record lines below, not among the dropped.
            std::vector<std::string> meant = coaster ? options.drop : std::vector<std::string>();
            if (coaster)
                for (const auto &a : options.append)
                    meant.push_back(a.substr(0, a.find(' ')));
            for (const auto &d : report.dropped)
                if (std::find(meant.begin(), meant.end(), d) == meant.end())
                    log.droppedStatements.insert(what + ": " + d);
            if (!limits.empty())
                log.text << "    coaster speed: " << what << "/" << m.name << ": " << limits
                         << " left out, the game's own speed limits apply\n";
            if (!belt.empty())
                log.text << "    trough belt: " << what << "/" << m.name << ": " << belt
                         << "; swapped and the lift's left out, so the belt shows on climbs and the water on the rest\n";
            for (const auto &u : report.unmappedIds)
                log.text << "    id not remapped: " << what << "/" << m.name << ": " << u << "\n";
            out.members.push_back({m.name, Bytes(filtered.begin(), filtered.end())});
        } else {
            out.members.push_back(m);
        }
    }
    for (const TpiWadMember *ifl : ifls) {
        const std::string folder = Dir(ifl->name).empty() ? std::string() : Dir(ifl->name) + "/";
        std::map<std::string, int> uses;
        std::vector<std::string> order;
        std::istringstream in(std::string(ifl->data.begin(), ifl->data.end()));
        std::string frame;
        while (in >> frame)
            if (uses[Lower(frame)]++ == 0)
                order.push_back(Lower(frame));
        std::string best;
        for (const auto &f : order)
            if (best.empty() || uses[f] > uses[best])
                best = f;
        if (Extension(best) == ".fsh")
            best = Stem(best) + ".tga";
        const std::string name = Stem(ifl->name) + ".tga";
        const TpiWadMember *source = nullptr;
        bool taken = false;
        for (const auto &m : out.members)
            taken = taken || Lower(m.name) == Lower(name);
        if (taken) {
            log.text << "    left out: " << what << "/" << ifl->name << " (" << Base(name) << " is there already)\n";
            continue;
        }
        for (const auto &m : out.members)
            if (Lower(m.name) == Lower(folder + best))
                source = &m;
        if (source == nullptr) {
            log.text << "    left out: " << what << "/" << ifl->name << " (its frame " << best << " is not there)\n";
            continue;
        }
        TpiWadMember copy{name, source->data};
        out.members.push_back(std::move(copy));
        // The mesh loader looks a texture up in its folder's qickload.txt first (MeshLoad.cpp), so the list names it.
        for (auto &m : out.members)
            if (Lower(m.name) == Lower(folder + "qickload.txt")) {
                const std::string add = Base(name) + "\n";
                if (!m.data.empty() && m.data.back() != '\n')
                    m.data.push_back('\n');
                m.data.insert(m.data.end(), add.begin(), add.end());
            }
        log.text << "    animated texture " << what << "/" << ifl->name << " written as " << Base(name) << ", its most used frame ("
                 << best << ")\n";
    }
    // A qickload.txt keeps only the names its own folder holds, as Theme Park World's lists do. A name listed there
    // sends the mesh loader to the item's own folder (MeshLoad.cpp), so a texture TPI lists but keeps in the world's
    // sharetex (smrdsgn's sign1.tga) would not be found; left out, it is looked up in the shared folder.
    std::set<std::string> held;   // lower "folder/name"
    for (const auto &m : out.members)
        held.insert(Lower(m.name));
    for (auto &m : out.members) {
        if (Lower(Base(m.name)) != "qickload.txt")
            continue;
        const std::string folder = Dir(m.name).empty() ? std::string() : Dir(m.name) + "/";
        std::string text(m.data.begin(), m.data.end()), kept, line;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] != '\n') {
                line.push_back(text[i]);
                continue;
            }
            std::string path = folder + line;   // the terrain's list names its subfolders' files as "pathtex\\p.tga"
            std::replace(path.begin(), path.end(), '\\', '/');
            if (line.empty() || held.count(Lower(path)))
                kept += line + "\n";
            else
                log.text << "    qickload name left out: " << what << "/" << m.name << ": " << line << " (not in this wad)\n";
            line.clear();
        }
        m.data.assign(kept.begin(), kept.end());
    }
    return out;
}

// ---- feature scripts

// A placed item's build animation (its "<item>c" mesh, kind C) is started and held at its first frame when the instance
// is made (TPRide.cpp), until the item's ride script, "<folder>:<item>.rse" (Object.cpp CreateRideScript), plays it.
// Theme Park World's plants each carry the smallest such script: play C once, then idle. Their C meshes grow the plant
// from the ground, so the first frame draws it flat. Theme Park Inc's plants and flower beds have a C mesh and no
// script, so placed in a converted world they stayed flat and were not seen (0.5.1: Bush (small) and Palm Tree (small)
// in Arabian Nights). A feature with a C mesh and no .rse is given that script, built from the format below.
//
// The file as RSSE_InitScript (rsse.cpp) reads it, little-endian words: "RSSE", the interpreter's version (the sum of
// its op table's texts, RSSE_Init), the variable, stack, instructions-per-turn, limbo, bounce and walk counts, four words
// it drops ("Pad "), the program's word count and words, then the text block's size and bytes. The program, as Theme
// Park World's own plant scripts have it: op 37 with text 0 (the script's name, the text block's first text), op 17
// with kind 0 and index 0 (play the C animation and wait for its end), op 6 (a countdown of 0) and op 31 to word 5
// (back to the countdown: idle). After C ends the ride holds its last frame (RideAnim.cpp DoAnimateRideSet), the
// grown plant.
Bytes FeatureScript(const std::string &label) {
    Bytes out;
    const auto put = [&out](uint32_t v) {
        for (int i = 0; i < 4; ++i)
            out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    };
    put(0x45535352u);   // "RSSE"
    put(0x00010f51u);   // the version
    put(0);             // variables
    put(0);             // stack words
    put(50);            // instructions per turn
    put(0);             // limbo
    put(0);             // bounce
    put(0);             // walk
    for (int i = 0; i < 4; ++i)
        put(0x20646150u);   // "Pad "
    const uint32_t program[] = {0x80000025u, 0x10000000u, 0x80000011u, 0, 0, 0x80000006u, 0x8000001fu, 0x20000005u};
    put(static_cast<uint32_t>(std::size(program)));
    for (const uint32_t word : program)
        put(word);
    put(static_cast<uint32_t>(label.size() + 1));
    out.insert(out.end(), label.begin(), label.end());
    out.push_back(0);
    return out;
}

// True when `members` (an item's converted files) hold the item's C mesh and no script of its own.
bool NeedsFeatureScript(const std::vector<TpiWadMember> &members, const std::string &item) {
    bool build = false;
    for (const auto &m : members) {
        if (Lower(m.name) == Lower(item) + "c.md2")
            build = true;
        if (Dir(m.name).empty() && Extension(m.name) == ".rse")
            return false;
    }
    return build;
}

// ---- items

struct Item {
    std::string tpiFolder;   // "rides", ..., "facilities"
    std::string outFolder;   // "rides", ..., "features"
    std::string name;        // the wad's stem: the item's name and folder
    std::vector<TpiWadMember> members;
    int32_t oldId = -1;
    int32_t newId = -1;
    bool addOn = false;      // from the add-on kept in TpiAddOnFolder, not the copy
};

struct ItemFolder {
    const char *tpi;
    const char *out;
};
const ItemFolder kItemFolders[] = {{"rides", "rides"},       {"shops", "shops"},       {"sideshow", "sideshow"},
                                   {"features", "features"}, {"upgrades", "upgrades"}, {"facilities", "features"}};
// The facilities Theme Park World knows (its features): both toilets (everyone's there; in the mod's worlds the game
// keeps men and women apart while a park has both, by their folders, include/TpiToilets.h), the staff room (two names:
// the science world's is "nstaff", a "Staff Room" that rests staff as the others do), the security camera, the litter
// bin (two spellings). The information desk, the research lab and the training room have no feature, so they are left
// out.
const char *const kFacilitiesTaken[] = {"ftoilet", "mtoilet", "staffrm", "nstaff", "seccam", "littbin", "litbin"};

const TpiWadMember *ItemSam(const Item &item) {
    const std::string want = Lower(item.name) + ".sam";
    for (const auto &m : item.members)
        if (Lower(m.name) == want)
            return &m;
    return nullptr;
}

// The name an item's own language file gives it (english.txt, else american.txt: UTF-16 lines, a tag line "NAME" and
// the name on the next line, RideDB.cpp gRideStringTags), or empty. The game shows that name over Info.Name
// (CRideDatabase::GetTypeName), so the Bonus Disk's lostsub is "Depth Charge" in the build menu. ASCII only.
std::string ItemOwnName(const Item &item) {
    for (const char *file : {"english.txt", "american.txt"}) {
        for (const auto &m : item.members) {
            if (Lower(m.name) != file)
                continue;
            std::vector<std::string> lines(1);
            bool ascii = true;
            size_t i = m.data.size() >= 2 && m.data[0] == 0xff && m.data[1] == 0xfe ? 2 : 0;
            for (; i + 1 < m.data.size(); i += 2) {
                const unsigned c = m.data[i] | (m.data[i + 1] << 8);
                if (c == 10 || c == 13)
                    lines.emplace_back();
                else if (c >= 0x20 && c < 0x7f)
                    lines.back() += static_cast<char>(c);
                else if (c != 9)
                    ascii = false;
            }
            for (size_t l = 0; ascii && l < lines.size(); ++l) {
                if (lines[l].compare(0, 4, "NAME") != 0 || lines[l].find_first_not_of(" \t", 4) != std::string::npos)
                    continue;
                for (size_t n = l + 1; n < lines.size(); ++n) {
                    const size_t a = lines[n].find_first_not_of(" \t");
                    if (a == std::string::npos)
                        continue;
                    const size_t b = lines[n].find_last_not_of(" \t");
                    return lines[n].substr(a, b - a + 1);
                }
            }
        }
    }
    return std::string();
}

void CollectItems(const Levels &levels, const std::string &world, std::vector<Item> &items,
                  std::map<std::string, Bytes> &folderSams, Log &log) {
    for (const ItemFolder &folder : kItemFolders) {
        const std::string prefix = world + "/" + folder.tpi + "/";
        std::string currentDir;
        Item *current = nullptr;
        for (const auto &[rel, entry] : levels.under(prefix)) {
            const size_t slash = rel.find('/');
            if (slash == std::string::npos) {
                // A file in the item folder: a wad item, the folder's own .sam, or something else.
                current = nullptr;
                const std::string lowerName = rel;
                if (EndsWith(lowerName, ".wad")) {
                    Item item;
                    item.tpiFolder = folder.tpi;
                    item.outFolder = folder.out;
                    item.name = Base(entry->path).substr(0, Base(entry->path).size() - 4);
                    std::string err;
                    if (!ReadTpiWad(levels.read(*entry), item.members, err))
                        Fail(entry->path + ": " + err);
                    items.push_back(std::move(item));
                } else if (EndsWith(lowerName, ".sam")) {
                    if (lowerName.compare(0, 7, "online_") == 0)
                        log.text << "    left out: " << folder.tpi << "/" << Base(entry->path) << " (online)\n";
                    else if (std::string(folder.tpi) == "facilities")
                        // Theme Park World's features read Features.sam only; the facilities' table-wide
                        // settings (Info.WhichUIType 5, Research.Category 6) are outside its ranges anyway.
                        log.text << "    left out: " << folder.tpi << "/" << Base(entry->path)
                                 << " (the features' own Features.sam applies)\n";
                    else
                        folderSams[std::string(folder.out) + "/" + Base(entry->path)] = levels.read(*entry);
                } else {
                    log.text << "    left out: " << folder.tpi << "/" << Base(entry->path) << " (not an item)\n";
                }
                continue;
            }
            // A file inside an unpacked item folder.
            const std::string dir = rel.substr(0, slash);
            if (current == nullptr || dir != currentDir) {
                Item item;
                item.tpiFolder = folder.tpi;
                item.outFolder = folder.out;
                item.name = Base(entry->path.substr(0, entry->path.size() - (rel.size() - slash)));
                items.push_back(std::move(item));
                current = &items.back();
                currentDir = dir;
            }
            current->members.push_back({entry->path.substr(entry->path.size() - (rel.size() - slash - 1)), levels.read(*entry)});
        }
    }
    // The facilities Theme Park World has no feature for.
    std::vector<Item> kept;
    for (auto &item : items) {
        if (item.tpiFolder == "facilities") {
            bool taken = false;
            for (const char *name : kFacilitiesTaken)
                if (Lower(item.name) == name)
                    taken = true;
            if (!taken) {
                log.text << "    left out: facilities/" << item.name << " (no such feature in Theme Park World)\n";
                log.itemsLeftOut++;
                continue;
            }
        }
        kept.push_back(std::move(item));
    }
    items = std::move(kept);
}

// The add-on's items of a world (TpiAddOnFolder), after the copy's own: each wad directly in an item folder. One the copy
// already has (the same item folder and name: an installed game with the Bonus Disk on it) stays the copy's.
void CollectAddOnItems(const Levels &addOn, const std::string &world, std::vector<Item> &items, Log &log) {
    const size_t copyItems = items.size();
    for (const ItemFolder &folder : kItemFolders) {
        for (const auto &[rel, entry] : addOn.under(world + "/" + folder.tpi + "/")) {
            if (rel.find('/') != std::string::npos || !EndsWith(rel, ".wad"))
                continue;
            Item item;
            item.tpiFolder = folder.tpi;
            item.outFolder = folder.out;
            item.name = Base(entry->path).substr(0, Base(entry->path).size() - 4);
            item.addOn = true;
            bool inCopy = false;
            for (size_t i = 0; i < copyItems; ++i)
                inCopy = inCopy || (items[i].tpiFolder == item.tpiFolder && Lower(items[i].name) == Lower(item.name));
            std::string err;
            if (inCopy)
                err = "already in the copy";
            else if (!ReadTpiWad(addOn.read(*entry), item.members, err))
                err = "not a wad the converter reads: " + err;
            if (!err.empty()) {
                log.text << "    left out: add-on " << folder.tpi << "/" << item.name << " (" << err << ")\n";
                log.addOn.leftOut.push_back(item.name + ": " + err);
                if (!inCopy)
                    log.itemsLeftOut++;
                continue;
            }
            items.push_back(std::move(item));
        }
    }
}

void AssignIds(std::vector<Item> &items, int32_t base, std::map<int32_t, int32_t> &ids, Log &log) {
    std::vector<Item> kept;
    int32_t next = base;
    for (auto &item : items) {
        const TpiWadMember *sam = ItemSam(item);
        int32_t id = -1;
        if (sam == nullptr || !TpiSamInteger(std::string(sam->data.begin(), sam->data.end()), "Info", "Id", id)) {
            log.text << "    left out: " << item.tpiFolder << "/" << item.name << " (no " << item.name << ".sam with an Info.Id)\n";
            log.itemsLeftOut++;
            if (item.addOn)
                log.addOn.leftOut.push_back(item.name + ": no " + item.name + ".sam with an Info.Id");
            continue;
        }
        item.oldId = id;
        bool keep = false;
        for (const int32_t k : kKeptIds)
            if (id == k)
                keep = true;
        if (ids.count(id)) {
            log.text << "    left out: " << item.tpiFolder << "/" << item.name << " (Info.Id " << id << " used twice)\n";
            log.itemsLeftOut++;
            if (item.addOn)
                log.addOn.leftOut.push_back(item.name + ": its Info.Id " + std::to_string(id) + " is the copy's");
            continue;
        }
        if (keep) {
            item.newId = id;
        } else {
            item.newId = next;
            next += kIdStep;
        }
        ids[id] = item.newId;
        kept.push_back(std::move(item));
    }
    items = std::move(kept);
    if (items.size() > kMaxItems)
        Fail("more items than the game holds (" + std::to_string(items.size()) + ")");
}

// ---- the world's own .sam files

const std::vector<SamRow> &TableFor(const char *fileName, std::map<std::string, std::vector<SamRow>> &cache) {
    auto it = cache.find(fileName);
    if (it == cache.end()) {
        std::vector<SamRow> table = SamTableFor(fileName);
        if (table.empty())
            Fail(std::string("no settings table is registered for ") + fileName);
        it = cache.emplace(fileName, std::move(table)).first;
    }
    return it->second;
}

std::string FilterWorldSam(const Levels &levels, const std::string &world, const char *name, const std::vector<SamRow> &table,
                           const TpiSamOptions &options, Log &log) {
    const Entry *entry = levels.find(world + "/" + Lower(name));
    if (entry == nullptr)
        Fail(world + "/" + name + " is missing");
    const Bytes bytes = levels.read(*entry);
    std::string filtered, err;
    TpiSamReport report;
    if (!FilterTpiSam(table, std::string(bytes.begin(), bytes.end()), options, filtered, report, err))
        Fail(std::string(name) + ": " + err);
    for (const auto &d : report.dropped)
        log.droppedStatements.insert(std::string(name) + ": " + d);
    for (const auto &u : report.unmappedIds)
        log.text << "    id not remapped: " << name << ": " << u << "\n";
    return filtered;
}

// The lines of Theme Park Inc's .sam text `text` that start with one of `prefixes`, each as it is (without its line end),
// one to a line.
std::string SamLinesStarting(const std::string &text, const std::vector<std::string> &prefixes, int &count) {
    std::string out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos)
            continue;
        for (const auto &prefix : prefixes)
            if (line.compare(start, prefix.size(), prefix) == 0) {
                out += line.substr(start) + "\n";
                ++count;
                break;
            }
    }
    return out;
}

// The world's TpiPeeps.sam (include/TpiGenerations.h, read by the game in the mod's worlds only, never as a settings
// table): Theme Park Inc's visitor types, starting cash spread and per-generation weather effects from its
// levels/Standard.sam, and the world's heatwave quality from the world's Standard.sam. Copied as they are: the game
// reads what it knows and leaves the visitors as Theme Park World's when the types are not complete.
std::string TpiPeepsSam(const Levels &levels, const std::string &world, Log &log) {
    std::string out = "# TPI Worlds and Rides: Theme Park Inc's visitor types and weather effects, from the player's copy\n";
    int count = 0;
    if (const Entry *global = levels.find("standard.sam")) {
        const Bytes bytes = levels.read(*global);
        out += SamLinesStarting(std::string(bytes.begin(), bytes.end()),
                                {"PeepInfo.StartingCashVarPc", "PeepInfo.WeatherModifierFrequency", "PeepTypes[",
                                 "PeepHappinessWeatherEffects[", "PeepThirstWeatherEffects[", "PeepHungerWeatherEffects["},
                                count);
    }
    if (const Entry *own = levels.find(world + "/standard.sam")) {
        const Bytes bytes = levels.read(*own);
        out += SamLinesStarting(std::string(bytes.begin(), bytes.end()), {"WeatherEffects.QualityForHeatWaveLow"}, count);
    }
    log.text << "    TpiPeeps.sam: " << count << " lines\n";
    return out;
}

// An outside feature of the world (Info.IsOutsideFeature 1): its converted Info.Id and "features/<name>".
struct OutsideFeature {
    int32_t id = -1;
    std::string name;
};

// The world's TpiOutside.sam (include/TpiOutside.h, read by the game in the mod's worlds only): one line for each outside
// feature, "Outside <id>", its folder after it for the reader.
std::string TpiOutsideSam(const std::vector<OutsideFeature> &outside, Log &log) {
    std::string out = "# TPI Worlds and Rides: Theme Park Inc's outside features, placed when a park starts\n";
    log.text << "    TpiOutside.sam: " << outside.size() << " outside features (";
    for (size_t i = 0; i < outside.size(); ++i) {
        out += "Outside\t" + std::to_string(outside[i].id) + "\t\t# " + outside[i].name + "\n";
        log.text << (i == 0 ? "" : ", ") << outside[i].name << " " << outside[i].id;
    }
    log.text << ")\n";
    return out;
}

// The lake ride: a bumper ride of Theme Park Inc's boat kind (Bumper.BumperType -11, The Great Lake) whose own
// bumper.sam gives the circles and buoys its boats sail between (ColObjects, Buoys; the ride table has neither, so
// FilterTpiSam leaves them out of the converted ride).
struct LakeRide {
    int32_t id = -1;            // its converted Info.Id
    std::string name;           // "rides/vict_hs", for the report
    std::string bumper;         // its bumper.sam as Theme Park Inc has it
    std::string folder;         // where it is written ("rides/vict_hs")
    std::vector<TpiWadMember> meshes;   // its meshes, written with their wakes (hidden after all when there is no lake)
};

bool IsLakeRide(const Item &item, const std::string &samText, LakeRide &lake) {
    int32_t kind = 0;
    if (!TpiSamInteger(samText, "Bumper", "BumperType", kind) || kind != -11)
        return false;
    for (const auto &m : item.members) {
        if (!Dir(m.name).empty() || Lower(Base(m.name)) != "bumper.sam")
            continue;
        std::string text(m.data.begin(), m.data.end());
        if (text.find("ColObjects[") == std::string::npos)
            continue;
        lake.id = item.newId;
        lake.name = item.tpiFolder + "/" + item.name;
        lake.bumper = std::move(text);
        lake.folder = item.outFolder + "/" + item.name;
        for (const auto &mesh : item.members) {
            const std::string folder = Lower(Base(Dir(mesh.name)));
            if (Extension(mesh.name) == ".md2" && folder != "textures" && folder != "stexture")
                lake.meshes.push_back(mesh);
        }
        return true;
    }
    return false;
}

// "Group[i].Field value" lines of a .sam (the value an integer), by index: Theme Park Inc's ColObjects and Buoys lists.
std::map<int, std::map<std::string, int32_t>> SamIndexed(const std::string &text, const std::string &group) {
    std::map<int, std::map<std::string, int32_t>> out;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        size_t i = line.find_first_not_of(" \t");
        if (i == std::string::npos || line.compare(i, group.size() + 1, group + "[") != 0)
            continue;
        i += group.size() + 1;
        const size_t close = line.find(']', i);
        if (close == std::string::npos || close + 1 >= line.size() || line[close + 1] != '.')
            continue;
        const std::string index = line.substr(i, close - i);
        const size_t fieldAt = close + 2, fieldEnd = line.find_first_of(" \t\r", fieldAt);
        if (index.empty() || index.find_first_not_of("0123456789") != std::string::npos || fieldEnd == std::string::npos)
            continue;
        const std::string field = line.substr(fieldAt, fieldEnd - fieldAt);
        const size_t valueAt = line.find_first_not_of(" \t", fieldEnd);
        if (valueAt == std::string::npos)
            continue;
        char *stop = nullptr;
        const long value = std::strtol(line.c_str() + valueAt, &stop, 10);
        if (stop == line.c_str() + valueAt)
            continue;
        out[std::stoi(index)][field] = static_cast<int32_t>(value);
    }
    return out;
}

// The world's TpiLake.sam (include/TpiGreatLake.h, read by the game in the mod's worlds only): the lake ride's id, the
// water the lake obstacle leaves once it is drained (its cleared shape's closed cells, row runs "y x0 x1") and its water
// level, and the ride's circles (duplicates once) and buoys in Theme Park Inc's units (10 to a cell), each list up to its
// end marker (x 512). Empty when no obstacle's water holds every circle's centre.
std::string TpiLakeSam(const LakeRide &lake, const TpiObstacleReport &report, Log &log) {
    struct Circle {
        int32_t x, y, r;
    };
    std::vector<Circle> circles;
    for (const auto &[index, f] : SamIndexed(lake.bumper, "ColObjects")) {
        auto x = f.find("X"), y = f.find("Y"), r = f.find("Rad");
        if (x == f.end() || x->second >= 512)
            break;
        if (y == f.end() || r == f.end() || r->second <= 0)
            continue;
        bool seen = false;
        for (const Circle &c : circles) seen = seen || (c.x == x->second && c.y == y->second && c.r == r->second);
        if (!seen)
            circles.push_back({x->second, y->second, r->second});
    }
    std::vector<std::pair<int32_t, int32_t>> buoys;
    for (const auto &[index, f] : SamIndexed(lake.bumper, "Buoys")) {
        auto x = f.find("X"), y = f.find("Y");
        if (x == f.end() || y == f.end() || x->second >= 512 || y->second >= 512)
            break;
        buoys.emplace_back(x->second, y->second);
    }
    const TpiObstacleResult *water = nullptr;
    for (const auto &o : report.obstacles) {
        if (!o.skipped.empty() || !o.hasSurface || o.closedCells.empty() || circles.empty())
            continue;
        const std::set<std::pair<int, int>> cells(o.closedCells.begin(), o.closedCells.end());
        bool all = true;
        for (const Circle &c : circles) all = all && cells.count({c.x / 10, c.y / 10}) != 0;
        if (all) {
            water = &o;
            break;
        }
    }
    if (water == nullptr) {
        log.text << "    lake ride " << lake.name << ": no obstacle's water holds its " << circles.size()
                 << " circles; its boats stay on its plot\n";
        return std::string();
    }
    std::string out = "# TPI Worlds and Rides: The Great Lake's water and its boats' circles and buoys, from the player's copy\n";
    out += "Lake.RideId " + std::to_string(lake.id) + "\n";
    char level[32];
    std::snprintf(level, sizeof level, "%.3f", static_cast<double>(water->surface));
    out += std::string("Lake.WaterLevel ") + level + "\n";
    std::set<std::pair<int, int>> byRow;   // (y, x)
    for (const auto &[x, y] : water->closedCells) byRow.insert({y, x});
    int runs = 0;
    for (auto it = byRow.begin(); it != byRow.end();) {
        const int y = it->first, x0 = it->second;
        int x1 = x0;
        ++it;
        while (it != byRow.end() && it->first == y && it->second == x1 + 1) {
            x1 = it->second;
            ++it;
        }
        out += "Lake.Water " + std::to_string(y) + " " + std::to_string(x0) + " " + std::to_string(x1) + "\n";
        ++runs;
    }
    for (const Circle &c : circles)
        out += "Lake.Circle " + std::to_string(c.x) + " " + std::to_string(c.y) + " " + std::to_string(c.r) + "\n";
    for (const auto &[x, y] : buoys) out += "Lake.Buoy " + std::to_string(x) + " " + std::to_string(y) + "\n";
    log.text << "    TpiLake.sam: ride " << lake.name << " (" << lake.id << ") on obstacle " << water->name << "'s water, "
             << byRow.size() << " cells in " << runs << " runs, level " << level << ", " << circles.size() << " circles, "
             << buoys.size() << " buoys; its boats' wake kept\n";
    return out;
}

// ---- one world

struct WorldWad {
    const char *name;   // Theme Park World's spelling
};
const WorldWad kWorldWads[] = {{"terrain.wad"},  {"sharetex.wad"}, {"ssharete.wad"}, {"dynamic.wad"},
                               {"queue.wad"},    {"miscmesh.wad"}, {"hoarding.wad"}};
const char *const kCopiedFiles[] = {"2dmap.tga", "scape.omp", "hoardings.map"};
struct CopiedFolder {
    const char *tpi;
    const char *out;
};
const CopiedFolder kCopiedFolders[] = {{"sky", "sky"}, {"ssky", "ssky"}, {"music", "Music"}, {"sound", "Sound"}};
// Sounds whose volume Theme Park Inc tuned for the PC's 3D sound (tpi_sfx.h). Land of Invention's gate crackle (event
// 224 of Sound/cat_ambientSFX.map: a sentence of the samples gatel000 to gatel008 chained without end, started once by
// the gate's script, Gates.RSE, and never stopped) is a loop at volume 100 of 127 that the PC's DirectSound3D faded
// from its full level at 2 units to about 6% at the distance the park opens at (rolloff 63 * 2 / (100 - 2), about 28
// units out); the port has no distance fade, so it played at 100 anywhere within its 100 units of the gate. Volume 6 is
// the PC's level at that distance and the level of Theme Park Inc's other endless ambient loops (1 to 10).
struct SfxPatch {
    const char *world;
    const char *file;   // in Sound/, any case
    TpiSfxVolumePatch patch;
};
const SfxPatch kSfxPatches[] = {{"science", "cat_ambientsfx.map", {224, 0x64, 0x06}}};
const char *const kLeftOutFolders[] = {"speech"};
const char *const kLeftOutFiles[] = {"online_standard.sam", "cem_hoardings.sam", "easymode.tpwi", "advisor.wad"};

// The three items every Theme Park World world has and no Theme Park Inc world does, taken from the donor world in
// the player's own Theme Park World data (never from a repository): the mystery item (Info.Id 100), which the Buy
// Ride panel previews in place of a golden ticket ride that is not bought yet (ui_ridepanel.cpp: a ride with
// UsageInfo.GoldenTicketCost keeps that cost here and is bought with tickets as in Theme Park World), and the ground
// raising and lowering items (101 and 102), which the panel previews for the two rows it adds to every features list.
// Their folders are the world's own (tpi_content.h: no donor fallback for rides/ and features/), so the converter
// copies the wads whole, bytes as they are. The advisor is the Theme Park World ant of the same donor world (the maintainer,
// 2026-11-11: models, textures and animations all Theme Park World's, nothing of Theme Park Inc's advisor): the game
// asks for the advisor's models by the donor world's three letters (the mod's TPLevel.cpp hook, "Advisor_JUN.MD2"
// for arabian), which are the names the donor's own advisor.wad holds.
const char *const kDonorItems[] = {"rides/mystery.wad", "features/ground.wad", "features/groundc.wad", "advisor.wad"};
// Where the launcher keeps each edition's game (assets/importer.cpp, GameFolder), under <dataRoot>/assets.
const char *const kGameFolders[] = {"eur/Theme Park Data", "us/Theme Park Data", "eur-pc/Theme Park Data",
                                    "us-pc/Theme Park Data"};

// The path of `relative` (a '/' path) under `base` with each part matched regardless of case; empty when absent.
fs::path FindNoCase(const fs::path &base, const std::string &relative) {
    fs::path path = base;
    size_t start = 0;
    while (start <= relative.size()) {
        const size_t slash = relative.find('/', start);
        const std::string part = Lower(relative.substr(start, slash == std::string::npos ? std::string::npos : slash - start));
        std::error_code ec;
        fs::path found;
        for (const auto &e : fs::directory_iterator(path, ec))
            if (Lower(tpw::PathText(e.path().filename())) == part) {
                found = e.path();
                break;
            }
        if (found.empty())
            return {};
        path = found;
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return path;
}

// The UsageInfo.GoldenTicketCost of an item's own .sam among its converted members, 0 when it has none.
int32_t GoldenTicketCost(const std::vector<TpiWadMember> &members, const std::string &name) {
    for (const auto &m : members) {
        if (Lower(m.name) != Lower(name) + ".sam")
            continue;
        std::istringstream in(std::string(m.data.begin(), m.data.end()));
        std::string key;
        int32_t value = 0;
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream row(line);
            if (row >> key && key == "UsageInfo.GoldenTicketCost" && row >> value)
                return value;
        }
    }
    return 0;
}

// Theme Park World's rides charge nothing per use: the park charges at the gate. A visitor at the head of a ride's queue
// values its use at its cost of goods (CPerson::TooPricey), which rides have none of, so any price per use sends every
// visitor away and the ride never runs. Theme Park Inc prices each ride's use, so UsageInfo.InitPricePerUse is left out
// of every ride's .sam and of the rides' folder .sam. A ride is what the game shows as one: Info.WhichUIType 0, the
// item's own, else its folder .sam's, else the game's default 0. Shops and sideshows keep their prices; they sell.
const char *const kPricePerUse = "UsageInfo.InitPricePerUse";
// A ride's upgrade levels (Upgrades[0] to Upgrades[2]).
constexpr int kUpgradeLevels = 3;

int32_t UiType(const std::string &samText, int32_t fallback) {
    int32_t type = 0;
    return TpiSamInteger(samText, "Info", "WhichUIType", type) ? type : fallback;
}

// A visitor at the head of a shop's queue buys only when its price is at most what the shop is worth to them
// (CPerson::TooPricey): its cost of goods times 1.15, raised by the visitor's hunger, thirst and mood, then by RipOffOK
// percent. Theme Park World's shops start at one and a half times their cost of goods, with RipOffOK 100 in every
// world's Shops.sam. Theme Park Inc's start at up to six times it, and Polar Zone comments RipOffOK out (so the game's
// default 0 applies) and Land of Invention gives 0, so nearly every visitor walks away and the shop serves no one. The converter keeps Theme Park Inc's prices and gives the shops Theme Park World's room between price and worth:
// a shops folder .sam gets RipOffOK 100 where it has none or less, and a shop whose price is more than one and a half
// times its cost of goods gets RipOffOK (100 + 100) x (price / cost) / 1.5 - 100 in its own .sam, so a visitor values it
// against its price as a Theme Park World visitor values a Theme Park World shop. A higher RipOffOK is never lowered.
const char *const kRipOffOK = "UsageInfo.RipOffOK";
constexpr int32_t kShopRipOffOK = 100;

// The RipOffOK a shop with this price and cost of goods needs (at least Theme Park World's 100).
int32_t ShopRipOffOK(int32_t price, int32_t cost) {
    if (price <= 0 || cost <= 0)
        return kShopRipOffOK;
    const int64_t need = (int64_t{400} * price + 3 * cost - 1) / (3 * cost) - 100;   // rounded up
    return need > kShopRipOffOK ? static_cast<int32_t>(need) : kShopRipOffOK;
}

// Raises a shops folder .sam's RipOffOK to Theme Park World's 100 through `options`. The value it had, -1 for none, or
// kShopRipOffOK when it is left.
int32_t RaiseFolderRipOffOK(const std::string &samText, TpiSamOptions &options) {
    int32_t ripOff = -1;
    if (!TpiSamInteger(samText, "UsageInfo", "RipOffOK", ripOff))
        ripOff = -1;
    if (ripOff >= kShopRipOffOK)
        return kShopRipOffOK;
    options.append.push_back(std::string(kRipOffOK) + "\t\t\t" + std::to_string(kShopRipOffOK));
    return ripOff;
}

// Raises a shop's own RipOffOK through `options` when its price needs more than it has (its own, else its folder's
// after RaiseFolderRipOffOK); a shop without a price or cost of goods needs Theme Park World's 100. `folderText`: its
// folder .sam, for what the shop's own leaves out. Returns the value it had, `raised` set to the new one; -1 when it is
// left.
int32_t RaiseShopRipOffOK(const std::string &samText, const std::string &folderText, TpiSamOptions &options,
                          int32_t &raised) {
    const auto value = [&](const char *field, int32_t &v) {
        return TpiSamInteger(samText, "UsageInfo", field, v) || TpiSamInteger(folderText, "UsageInfo", field, v);
    };
    int32_t price = 0, cost = 0, had = 0;
    raised = value("InitPricePerUse", price) && value("InitCostOfGoods", cost) ? ShopRipOffOK(price, cost) : kShopRipOffOK;
    if (!TpiSamInteger(samText, "UsageInfo", "RipOffOK", had) &&
        (!TpiSamInteger(folderText, "UsageInfo", "RipOffOK", had) || had < kShopRipOffOK))
        had = kShopRipOffOK;
    if (raised <= had)
        return -1;
    options.append.push_back(std::string(kRipOffOK) + "\t\t\t" + std::to_string(raised));
    return had;
}

// A visitor picks only an object its record marks Info.IsChoosable (the object's flag 4, CObject::IsAvailableToChoose).
// Theme Park Inc's shops folders say 1, but its photo booth (fotobth, a shop by its own Info.WhichUIType 1) sits in the
// upgrades folder, whose Upgrades.sam says 0 for the track add-ons beside it, and names none of its own, so no visitor
// ever uses it. A shop that takes Info.IsChoosable 0 from its folder .sam gets Info.IsChoosable 1 in its own. Like Theme
// Park World's shops it has no queue (Info.HasQueue 0): a visitor walks to the shop itself. Returns whether it did.
bool MakeShopChoosable(const std::string &samText, const std::string &folderText, TpiSamOptions &options) {
    int32_t choosable = 0;
    if (TpiSamInteger(samText, "Info", "IsChoosable", choosable) ||
        !TpiSamInteger(folderText, "Info", "IsChoosable", choosable) || choosable != 0)
        return false;
    options.append.push_back("Info.IsChoosable\t\t\t1");
    return true;
}

// The text of the first folder .sam under `outFolder`, empty when it has none.
std::string FolderSamText(const std::map<std::string, Bytes> &folderSams, const std::string &outFolder) {
    for (const auto &[name, bytes] : folderSams)
        if (name.compare(0, outFolder.size() + 1, outFolder + "/") == 0)
            return std::string(bytes.begin(), bytes.end());
    return std::string();
}

// The UI type of the folder .sam files under `outFolder` (the first that names one), 0 when none does.
int32_t FolderUiType(const std::map<std::string, Bytes> &folderSams, const std::string &outFolder) {
    for (const auto &[name, bytes] : folderSams) {
        int32_t type = 0;
        if (name.compare(0, outFolder.size() + 1, outFolder + "/") == 0 &&
            TpiSamInteger(std::string(bytes.begin(), bytes.end()), "Info", "WhichUIType", type))
            return type;
    }
    return 0;
}

// The Theme Park Inc id of the world's gate item (global.sam's ParkName.GateObjectId), -1 when it names none.
int32_t GateObjectId(const Levels &levels, const std::string &world) {
    const Entry *entry = levels.find(world + "/global.sam");
    if (entry == nullptr)
        return -1;
    const Bytes data = levels.read(*entry);
    std::istringstream in(std::string(data.begin(), data.end()));
    std::string line, key;
    int32_t value = -1;
    while (std::getline(in, line)) {
        std::istringstream row(line);
        if (row >> key && key == "ParkName.GateObjectId" && row >> value)
            return value;
    }
    return -1;
}

// Copies kDonorItems from the donor world of the first edition under <dataRoot>/assets that has the first of them.
void CopyDonorItems(const fs::path &assets, const WorldSpec &spec, Writer &w, Log &log) {
    fs::path levels;
    for (const char *game : kGameFolders) {
        const fs::path candidate = FindNoCase(assets / fs::u8path(game), std::string("data/levels/") + spec.donor);
        if (!candidate.empty() && !FindNoCase(candidate, kDonorItems[0]).empty()) {
            levels = candidate;
            break;
        }
    }
    if (levels.empty())
        Fail(std::string("no Theme Park World data to take the ") + spec.donor +
             " world's mystery item from (" + kDonorItems[0] + " under <data folder>/assets/<edition>/Theme Park Data/data/levels/" +
             spec.donor + "): import Theme Park World first");
    for (const char *item : kDonorItems) {
        const fs::path path = FindNoCase(levels, item);
        if (path.empty())
            Fail(std::string("the ") + spec.donor + " world of the player's Theme Park World data has no " + item);
        std::ifstream in(path, std::ios::binary);
        Bytes bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (!in.good() && !in.eof())
            Fail("cannot read " + tpw::PathText(path));
        if (!IsTpiWad(bytes))
            Fail(tpw::PathText(path) + " is not a wad");
        w.file(item, bytes);
        log.text << "    " << item << ": copied from the " << spec.donor << " world of " << tpw::PathText(path) << "\n";
    }
}

// ---- golden tickets: the world's local thresholds
//
// The game gives a park's six local golden tickets (CGoldenTicketControl::CheckForLocal, every 100th turn) when the
// park's measures pass the GoldenTicketLocal.* values, which Theme Park World sets in each world's own Standard.sam
// (data:levels:Standard.sam, loaded fresh before it, has none). Theme Park Inc's world files have these lines commented
// out and its game never read them (its tickets came from its missions), so a converted world kept them all at 0 and
// gave its tickets, and with them keys for every island, at the first checks. Each world takes the thresholds of the
// Theme Park World world that costs as many keys to enter (jungle 1, fantasy 3, space 5), from the player's own data.
const char *const kTicketFields[] = {"Visitors",   "PeopleInPark",   "Happiness",          "AtLeastThisManyHappyPeople",
                                     "ProfitYear", "RecentVisitors", "RecentVisitorMonths"};
const char *const kTpwWorlds[] = {"jungle", "hallow", "fantasy", "space"};

std::string ReadText(const fs::path &path) {
    if (path.empty())
        return {};
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// The GoldenTicketLocal.* statements for `spec`'s Standard.sam, from the first edition under <dataRoot>/assets whose
// world of the same key cost has all seven.
std::vector<std::string> TicketThresholds(const fs::path &assets, const WorldSpec &spec, Log &log) {
    for (const char *game : kGameFolders)
        for (const char *tpwWorld : kTpwWorlds) {
            const fs::path levels = FindNoCase(assets / fs::u8path(game), std::string("data/levels/") + tpwWorld);
            if (levels.empty())
                continue;
            int32_t cost = 0;
            if (!TpiSamInteger(ReadText(FindNoCase(levels, "global.sam")), "Keys", "CostToEnter", cost) || cost != spec.keys)
                continue;
            const fs::path standard = FindNoCase(levels, "Standard.sam");
            const std::string text = ReadText(standard);
            std::vector<std::string> lines;
            std::string values;
            for (const char *field : kTicketFields) {
                int32_t value = 0;
                if (!TpiSamInteger(text, "GoldenTicketLocal", field, value))
                    break;
                lines.push_back(std::string("GoldenTicketLocal.") + field + "\t\t" + std::to_string(value));
                values += std::string(values.empty() ? "" : ", ") + field + " " + std::to_string(value);
            }
            if (lines.size() != std::size(kTicketFields))
                continue;
            log.text << "    Standard.sam: golden ticket thresholds of the " << tpwWorld << " world (" << spec.keys
                     << " keys to enter, as this world) from " << tpw::PathText(standard) << ": " << values << "\n";
            return lines;
        }
    Fail(std::string("no Theme Park World world costing ") + std::to_string(spec.keys) +
         " keys to enter with its golden ticket thresholds (GoldenTicketLocal.* in <data folder>/assets/<edition>/Theme Park "
         "Data/data/levels/<world>/Standard.sam) for " + spec.display + ": import Theme Park World first");
}

// ---- sprites: the entertainers and their costumes
//
// The game takes a world's entertainers, costumes and costume heads from "<theme>\<group>\*.ESP", the theme being the
// world's level folder (HLSprite.cpp ESP_FindESPFiles, Game.cpp GetCurrentThemeName), searched first in the disc
// storage "data:esprites" and then in data:esprites.wad (Game.cpp). Theme Park World's esprites.wad holds only its own
// four themes, so a Theme Park Inc world had no entertainer at all, and hiring one crashed the game (CEntertainer::
// SetState picks an act modulo the sprite's number of acts less one, as the original does). Theme Park Inc's
// esprites.wad holds the same three groups for its worlds, in the same format (an "ESP_FILE2.00" description with its
// .TPC and, for a first person view, .FPC beside it), so they are written unpacked under data/esprites/<world>/<group>/.
// The content folder is listed after the game folder (tpi_content.h) and the game has no esprites folder of its own,
// so the disc storage finds them there, while the generic groups and Theme Park World's four themes still come from
// the player's esprites.wad, which is neither read nor touched for them.
// A group the copy lacks, or whose files are not whole, comes from the donor world's in the player's own Theme Park
// World esprites.wad (the advisor's donors). Entertainers and costumes must come from one or the other; costume heads
// may be missing, as Theme Park World's own Fantasy and Halloween worlds have none.
// Each of Theme Park World's worlds has three entertainers, and the game counts on it: the entertainer the cheat keys and
// an online park's opening make (CEntertainer::CEntertainer(MapId)) takes the costumes 0, 1 and 2 in turn, whatever the
// group holds. Theme Park Inc's worlds have two, so the third read the sprite after the group's, a handyman's, with no
// act, and the act picked from it was past the script table (signal 11 in CDelayedScript::ProcessSpriteScripts). So a
// group with fewer sprites than `least` has its sprites written again under new names until it has that many
// (RepeatSprites).
struct SpriteGroup {
    const char *name;   // Theme Park World's spelling
    bool required;
    int least;          // the sprites the game takes by number
};
const SpriteGroup kSpriteGroups[] = {{"Entertainers", true, 3}, {"Costumes", true, 1}, {"Costumeheads", false, 1}};
constexpr char kEspSignature[] = "ESP_FILE2.00";
constexpr size_t kEspFirstPerson = 12 + 0x100;   // after the signature and the data file's name (ESP_CSprite::LoadFromESPFile)

// The members of `wad` directly in <theme>/<group>/ (case ignored), each named by its file name alone; empty, with the
// reason, when the group has no sprite or one is not whole.
std::vector<TpiWadMember> SpriteGroupFiles(const std::vector<TpiWadMember> &wad, const std::string &theme,
                                           const std::string &group, std::string &why) {
    const std::string prefix = Lower(theme + "/" + group + "/");
    std::vector<TpiWadMember> files;
    std::set<std::string> names;   // lower case
    for (const auto &m : wad) {
        const std::string lower = Lower(m.name);
        if (lower.compare(0, prefix.size(), prefix) != 0 || lower.find('/', prefix.size()) != std::string::npos)
            continue;
        files.push_back({m.name.substr(prefix.size()), m.data});
        names.insert(lower.substr(prefix.size()));
    }
    int sprites = 0;
    for (const auto &f : files) {
        if (Extension(f.name) != ".esp")
            continue;
        sprites++;
        if (f.data.size() <= kEspFirstPerson || std::memcmp(f.data.data(), kEspSignature, std::strlen(kEspSignature)) != 0) {
            why = f.name + " is not a sprite description Theme Park World reads";
            return {};
        }
        const std::string stem = Lower(Stem(f.name));
        if (!names.count(stem + ".tpc") || (f.data[kEspFirstPerson] != 0 && !names.count(stem + ".fpc"))) {
            why = f.name + " has its picture files missing";
            return {};
        }
    }
    if (sprites == 0) {
        why = "no sprite";
        return {};
    }
    return files;
}

// Fills the group `files` up to `least` sprites by writing its sprites again in name order, each with its picture files,
// under its own name with the first number from 2 that no file of the group has (SPR_BU.ESP, .TPC and .FPC as
// SPR_BU2.*): the game reads the data files' names from the description's own (ESP_CSprite::LoadFromESPFile). Each
// copy is named in `again` for the record.
void RepeatSprites(std::vector<TpiWadMember> &files, int least, std::string &again) {
    std::vector<std::string> stems;   // the sprites
    std::set<std::string> taken;      // the group's names without extension, lower case
    for (const auto &f : files) {
        taken.insert(Lower(Stem(f.name)));
        if (Extension(f.name) == ".esp")
            stems.push_back(Stem(f.name));
    }
    std::sort(stems.begin(), stems.end(), [](const std::string &a, const std::string &b) { return Lower(a) < Lower(b); });
    const std::vector<TpiWadMember> originals = files;
    for (size_t i = 0; !stems.empty() && stems.size() + i < static_cast<size_t>(least); i++) {
        const std::string &stem = stems[i % stems.size()];
        std::string name;
        for (int n = 2; !taken.insert(Lower(name = stem + std::to_string(n))).second; n++) {
        }
        for (const auto &f : originals)
            if (Lower(Stem(f.name)) == Lower(stem))
                files.push_back({name + f.name.substr(stem.size()), f.data});
        again += (again.empty() ? "" : ", ") + stem + " as " + name;
    }
}

// The members of the first edition's esprites.wad under <dataRoot>/assets, and its path; none when there is none.
std::vector<TpiWadMember> TpwSprites(const fs::path &assets, std::string &where) {
    for (const char *game : kGameFolders) {
        const fs::path path = FindNoCase(assets / fs::u8path(game), "data/esprites.wad");
        if (path.empty())
            continue;
        std::ifstream in(path, std::ios::binary);
        const Bytes bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::vector<TpiWadMember> members;
        std::string err;
        if (!ReadTpiWad(bytes, members, err))
            Fail("cannot read " + tpw::PathText(path) + ": " + err);
        where = tpw::PathText(path);
        return members;
    }
    return {};
}

// The members of the first edition's lobby.wad under <dataRoot>/assets (the island screen's files: the islands' waves and
// the sea, tpi_island.h), and its path; none when there is none or it cannot be read (the islands are then built
// without waves and the sea stays the original's).
std::vector<TpiWadMember> TpwLobby(const fs::path &assets, std::string &where) {
    for (const char *game : kGameFolders) {
        const fs::path path = FindNoCase(assets / fs::u8path(game), "data/lobby.wad");
        if (path.empty())
            continue;
        std::ifstream in(path, std::ios::binary);
        const Bytes bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::vector<TpiWadMember> members;
        std::string err;
        if (!ReadTpiWad(bytes, members, err)) {
            where = tpw::PathText(path) + " (cannot be read: " + err + ")";
            return {};
        }
        where = tpw::PathText(path);
        return members;
    }
    return {};
}

// ---- extra visitors: Theme Park Inc's kids, parents and grandparents among Theme Park World's kids
//
// Theme Park Inc's esprites.wad has generic visitors Theme Park World has not, in the kids' format: four kids of its own
// (Generic\Kids, SPR_BD, SPR_GB, SPR_GK and SPR_KB), parents (Generic\Parents, SPR_FA and SPR_MO) and grandparents
// (Generic\Grandparents, SPR_GM and SPR_GR), each with its heads for the rides (the groups' "heads" twins). Each of the
// three worlds gets them all as its own "Kids" and "Kidsheads" groups, which no Theme Park World world has: in the mod's
// worlds the game loads them after Theme Park World's kids, outside the group's count, and visitors wear them by Theme
// Park Inc's spread of generations (TPW_MOD_HOOK tpi_worlds, include/TpiGuests.h), a look only. All six groups whole or
// none: heads that are not the bodies' would not match on the rides, and a spread without a generation is not Theme
// Park Inc's. File names in upper case, as Theme Park World's own (one of Theme Park Inc's heads is "Spr_bd.esp").
// Taken only from the player's own Theme Park Inc copy; without it a world has none.
// A visitor's scripts play actions these do not all have (2, running, which the parents and grandparents lack; 11 and
// 14, cross, which all lack). The scripts give the frame numbers, so a missing action changes nothing a visitor does,
// but shows the wrong picture; each such action of the bodies' descriptions takes the place of one they have with as
// many frames as the scripts play of it (kGuestStandIns): running is walking, 11 and 14 their action 15 (four frames, a
// gesture in place).
struct GuestGroup {
    const char *tpi;   // Theme Park Inc's generic group
    const char *as;    // the world group it is written as (Theme Park World's spelling)
};
const GuestGroup kGuestGroups[] = {{"Kids", "Kids"},         {"Kidsheads", "Kidsheads"},
                                   {"Parents", "Kids"},      {"Parentsheads", "Kidsheads"},
                                   {"Grandparents", "Kids"}, {"Grandparentsheads", "Kidsheads"}};
struct GuestStandIn {
    int action;   // an action the visitors' scripts play
    int from;     // the action that stands in for it when the sprite has none
};
const GuestStandIn kGuestStandIns[] = {{2, 1}, {11, 15}, {14, 15}};
constexpr size_t kEspActions = kEspFirstPerson + 2;   // 16 actions: first frame (16 bits), frames, views (a byte each)

// Fills each action of the description `esp` that kGuestStandIns names and that has no frames with its stand-in, when
// that has frames; the filled ones are named in `filled` ("2 from 1").
void FillGuestActions(Bytes &esp, std::string &filled) {
    if (esp.size() < kEspActions + 16 * 4)
        return;
    for (const GuestStandIn &s : kGuestStandIns) {
        uint8_t *to = esp.data() + kEspActions + 4 * s.action;
        const uint8_t *from = esp.data() + kEspActions + 4 * s.from;
        if (to[2] != 0 || from[2] == 0)
            continue;
        std::memcpy(to, from, 4);
        filled += (filled.empty() ? "" : ", ") + std::to_string(s.action) + " from " + std::to_string(s.from);
    }
}

void ConvertGuests(const std::vector<TpiWadMember> &tpi, const std::string &tpiWhy, Writer &w, Log &log) {
    std::string why = tpiWhy;
    std::vector<std::vector<TpiWadMember>> groups;
    for (const GuestGroup &group : kGuestGroups) {
        if (!why.empty())
            break;
        groups.push_back(SpriteGroupFiles(tpi, "Generic", group.tpi, why));
        if (!why.empty())
            why = std::string("Generic/") + group.tpi + ": " + why;
    }
    if (!why.empty()) {
        log.text << "    extra visitors (Theme Park Inc's kids, parents and grandparents): none (" << why << ")\n";
        return;
    }
    std::vector<std::string> filled(groups.size());
    for (size_t g = 0; g < groups.size(); g++)
        for (auto &f : groups[g]) {
            f.name = Upper(f.name);
            if (std::strcmp(kGuestGroups[g].as, "Kids") != 0 || Extension(f.name) != ".esp")
                continue;
            std::string these;
            FillGuestActions(f.data, these);
            if (!these.empty())
                filled[g] += (filled[g].empty() ? "" : "; ") + Stem(f.name) + " " + these;
        }
    for (const WorldSpec &spec : kWorlds)
        for (size_t g = 0; g < groups.size(); g++) {
            const std::string out = std::string(spec.folder) + "/" + kGuestGroups[g].as;
            int sprites = 0;
            for (const auto &f : groups[g]) {
                w.file(out + "/" + f.name, f.data);
                sprites += Extension(f.name) == ".esp";
            }
            log.text << "    " << out << ": " << sprites << (sprites == 1 ? " sprite, " : " sprites, ") << groups[g].size()
                     << " files, Theme Park Inc's Generic/" << kGuestGroups[g].tpi << ", extra visitors in the mod's worlds"
                     << (!filled[g].empty() ? "; actions filled: " + filled[g] : std::string()) << "\n";
        }
}

void ConvertSprites(const TpiDisc &disc, const Levels &levels, const fs::path &staging, const fs::path &assets, Log &log) {
    log.text << "sprites (data/esprites):\n";
    std::vector<TpiWadMember> tpi;
    std::string tpiWhy = "Theme Park Inc's copy has no Data/esprites.wad";
    const std::string wanted = Lower(disc.root) + "data/esprites.wad";
    for (const Entry &e : disc.source->Entries()) {
        if (e.kind != EntryKind::Data || e.unreadable || Lower(e.path) != wanted)
            continue;
        std::string err;
        if (ReadTpiWad(levels.read(e), tpi, err))
            tpiWhy.clear();
        else
            tpiWhy = "Theme Park Inc's esprites.wad cannot be read: " + err;
        break;
    }
    std::vector<TpiWadMember> tpw;
    std::string tpwWhere;
    bool tpwRead = false;
    Writer w{staging / "data" / "esprites", &log};
    for (const WorldSpec &spec : kWorlds) {
        for (const SpriteGroup &group : kSpriteGroups) {
            const std::string out = std::string(spec.folder) + "/" + group.name;
            std::string why = tpiWhy;
            std::vector<TpiWadMember> files;
            if (why.empty())
                files = SpriteGroupFiles(tpi, spec.folder, group.name, why);
            std::string from = "Theme Park Inc's esprites.wad";
            if (files.empty()) {
                if (!tpwRead) {
                    tpw = TpwSprites(assets, tpwWhere);
                    tpwRead = true;
                }
                std::string donorWhy;
                files = SpriteGroupFiles(tpw, spec.donor, group.name, donorWhy);
                if (files.empty() && group.required)
                    Fail(std::string("no ") + group.name + " sprites for the " + spec.folder + " world: " + why + ", and " +
                         (tpwWhere.empty() ? std::string("no Theme Park World esprites.wad to take the ") + spec.donor +
                                                 " world's from: import Theme Park World first"
                                           : std::string("the ") + spec.donor + " world's in " + tpwWhere + ": " + donorWhy));
                if (files.empty()) {
                    log.text << "    " << out << ": none (" << why << "; the " << spec.donor
                             << " world has none either, as Theme Park World's Fantasy and Halloween worlds)\n";
                    continue;
                }
                from = std::string("the ") + spec.donor + " world's in " + tpwWhere + " (Theme Park Inc's: " + why + ")";
            }
            std::string again;
            RepeatSprites(files, group.least, again);
            if (!again.empty())
                from += "; written again: " + again + ", as the game takes " + std::to_string(group.least) + " " +
                        Lower(group.name);
            int sprites = 0;
            for (const auto &f : files) {
                w.file(out + "/" + f.name, f.data);
                sprites += Extension(f.name) == ".esp";
            }
            log.text << "    " << out << ": " << sprites << (sprites == 1 ? " sprite, " : " sprites, ") << files.size()
                     << " files, from " << from << "\n";
        }
    }
    ConvertGuests(tpi, tpiWhy, w, log);
    log.text << "    the generic groups and Theme Park World's own themes: the player's esprites.wad, untouched\n";
}

// The object names bank of the copy's English strings, or of its American ones (SimCoaster), with its character map;
// false, with the reason in the record, when neither language has both files, they cannot be decoded, or the bank is
// not the one the binding is for (tpi_names.h): every item then keeps its working name in title case.
bool ReadObjectNames(const TpiDisc &disc, const Levels &levels, TpiStrings &names, Log &log) {
    std::string why = "no Data/Language/English or American with OBJECT_NAMES.str and MBToUni.dat";
    for (const char *language : {"english", "american"}) {
        const std::string folder = Lower(disc.root) + "data/language/" + language + "/";
        const Entry *bank = nullptr, *charMap = nullptr;
        for (const Entry &e : disc.source->Entries()) {
            if (e.kind != EntryKind::Data || e.unreadable)
                continue;
            const std::string lower = Lower(e.path);
            if (lower == folder + "object_names.str")
                bank = &e;
            else if (lower == folder + "mbtouni.dat")
                charMap = &e;
        }
        if (bank == nullptr || charMap == nullptr)
            continue;
        std::string err;
        if (!DecodeTpiStrings(levels.read(*bank), levels.read(*charMap), names, err)) {
            why = bank->path + ": " + err;
            continue;
        }
        if (!IsTpiObjectNames(names, err)) {
            why = bank->path + " is not the object names bank the converter knows: " + err;
            continue;
        }
        log.text << "names: " << bank->path << " (" << names.text.size() << " strings)\n";
        return true;
    }
    log.text << "names: the items' own working names in title case (" << why << ")\n";
    return false;
}

void ConvertWorld(const Levels &levels, const Levels *addOn, const WorldSpec &spec, const fs::path &staging,
                  const fs::path &assets, std::map<std::string, std::vector<SamRow>> &tables, const TpiStrings *names,
                  Log &log) {
    const std::string world = spec.folder;
    if (levels.under(world + "/").empty())
        Fail("the world " + world + " is missing from Data/levels");
    log.text << "world " << world << ":\n";
    Writer w{staging / "data" / "levels" / world, &log};

    // Items: collect, give ids, write unpacked.
    std::vector<Item> items;
    std::map<std::string, Bytes> folderSams;
    CollectItems(levels, world, items, folderSams, log);
    // the add-on's items take the ids after the copy's, so the copy's own keep theirs whether it is there or not
    if (addOn != nullptr)
        CollectAddOnItems(*addOn, world, items, log);
    std::map<int32_t, int32_t> ids;
    AssignIds(items, spec.idBase, ids, log);
    const std::vector<SamRow> &rideTable = TableFor("rides.sam", tables);
    const std::vector<SamRow> &coasterTable = TableFor("coaster.sam", tables);
    TpiSamOptions itemOptions;
    itemOptions.idFields = {"Info.Id", "AddOn.UpgradesId"};
    itemOptions.ids = &ids;
    // The entrance gate's board shows the gate item's name. Both games' data call it "Gates"; Theme Park World's own
    // gates take their name from the object names table first (RideDB.cpp GetSignNameA), which has no converted id,
    // so the converted gate's Info.Name is the world's name.
    const int32_t gateId = GateObjectId(levels, world);
    TpiSamOptions gateOptions = itemOptions;
    gateOptions.append = {std::string("Info.Name\t\t\t\"") + spec.display + "\""};
    bool gateNamed = false;
    LakeRide lake;
    std::vector<OutsideFeature> outside;
    for (const auto &item : items) {
        const std::string what = item.tpiFolder + "/" + item.name;
        const bool gate = item.oldId == gateId;
        TpiSamOptions options = gate ? gateOptions : itemOptions;
        const TpiWadMember *sam = ItemSam(item);
        const std::string samText = sam != nullptr ? std::string(sam->data.begin(), sam->data.end()) : std::string();
        const int32_t uiType = UiType(samText, FolderUiType(folderSams, item.outFolder));
        const bool ride = uiType == 0;
        int32_t price = 0;
        const bool freeRide = ride && TpiSamInteger(samText, "UsageInfo", "InitPricePerUse", price);
        if (ride)
            options.drop.push_back(kPricePerUse);
        // Theme Park Inc's special rides never wear out (Info.IsSpecial: "they can't be rebuilt"); Theme Park World has no
        // such key, and a ride whose wear rate is 0 at its level takes no wear (CObject::AddWearAndTear).
        int32_t special = 0;
        const bool neverWears = ride && TpiSamInteger(samText, "Info", "IsSpecial", special) && special != 0;
        if (neverWears)
            for (int level = 0; level < kUpgradeLevels; ++level)
                options.append.push_back("Upgrades[" + std::to_string(level) + "].WearRate\t\t\t0");
        // An outside feature (Info.IsOutsideFeature: the scenery round the park, modelled where it stands) is placed when
        // a park starts, which Theme Park World does only for its gates and lights: TpiOutside.sam names them.
        int32_t outsideFlag = 0;
        const bool outsideFeature = TpiSamInteger(samText, "Info", "IsOutsideFeature", outsideFlag) && outsideFlag != 0;
        if (outsideFeature)
            outside.push_back({item.newId, item.outFolder + "/" + item.name});
        int32_t ripOff = 0;
        const int32_t ripOffWas =
            uiType == 1 ? RaiseShopRipOffOK(samText, FolderSamText(folderSams, item.outFolder), options, ripOff) : -1;
        const bool madeChoosable =
            uiType == 1 && MakeShopChoosable(samText, FolderSamText(folderSams, item.outFolder), options);
        // The name Theme Park Inc shows (tpi_names.h): the build menu and the ride's panels read Info.Name, as the
        // game's own name table has no converted id (RideDB.cpp GetTypeName).
        std::string infoName, shownName;
        bool fromBank = false;
        if (!gate && sam != nullptr) {
            TpiSamString(samText, "Info", "Name", infoName);
            shownName = TpiShownName(names, world, item.oldId, item.name, infoName, fromBank);
            if (!shownName.empty() && shownName != infoName)
                options.append.push_back("Info.Name\t\t\t\"" + shownName + "\"");
        }
        if (item.addOn) {
            const std::string ownName = ItemOwnName(item);
            log.addOn.added.push_back((!ownName.empty() ? ownName : !shownName.empty() ? shownName : item.name) + " (" +
                                      spec.display + ")");
        }
        const bool lakeRide = lake.id < 0 && ride && IsLakeRide(item, samText, lake);
        Converted c = ConvertMembers(item.members, &rideTable, &options, &coasterTable, what, log, lakeRide);
        const bool script = item.outFolder == "features" && NeedsFeatureScript(c.members, item.name);
        if (script)
            c.members.push_back({item.name + ".RSE", FeatureScript(item.name)});
        if (gate) {
            log.text << "    gate " << item.outFolder << "/" << item.name << " named \"" << spec.display << "\"\n";
            gateNamed = true;
        }
        for (const auto &m : c.members)
            w.file(item.outFolder + "/" + item.name + "/" + m.name, m.data);
        log.text << "    item " << item.outFolder << "/" << item.name << " (Info.Id " << item.oldId << " -> " << item.newId
                 << ", " << c.members.size() << " files";
        if (const int32_t tickets = GoldenTicketCost(c.members, item.name); tickets > 0)
            log.text << ", costs " << tickets << " golden ticket" << (tickets == 1 ? "" : "s");
        if (freeRide)
            log.text << ", no price per use (was " << price << ")";
        if (script)
            log.text << ", given a script that plays its build animation once";
        if (ripOffWas >= 0)
            log.text << ", RipOffOK " << ripOff << " (was " << ripOffWas << ")";
        if (madeChoosable)
            log.text << ", visitors can use it (its folder's Info.IsChoosable was 0)";
        if (neverWears)
            log.text << ", never wears out (Info.IsSpecial)";
        if (outsideFeature)
            log.text << ", placed when a park starts (Info.IsOutsideFeature)";
        if (!shownName.empty() && shownName != infoName)
            log.text << ", named \"" << shownName << "\" (was \"" << infoName << "\", "
                     << (fromBank                                ? "from the object names"
                         : TpiFixtureName(item.name) != nullptr ? "the port's name for a fixed item"
                                                                : "in title case")
                     << ")";
        if (item.addOn)
            log.text << ", from the add-on";
        log.text << ")\n";
        log.itemsConverted++;
    }
    if (!gateNamed)
        log.text << "    no gate item (global.sam's ParkName.GateObjectId is " << gateId << ")\n";
    CopyDonorItems(assets, spec, w, log);
    for (const auto &[name, bytes] : folderSams) {
        std::string filtered, err;
        TpiSamReport report;
        TpiSamOptions folderOptions;
        const std::string text(bytes.begin(), bytes.end());
        int32_t price = 0;
        if (UiType(text, 0) == 0) {
            folderOptions.drop.push_back(kPricePerUse);
            if (TpiSamInteger(text, "UsageInfo", "InitPricePerUse", price))
                log.text << "    " << name << ": no price per use (was " << price << ")\n";
        } else if (UiType(text, 0) == 1) {
            const int32_t ripOff = RaiseFolderRipOffOK(text, folderOptions);
            if (ripOff < kShopRipOffOK)
                log.text << "    " << name << ": RipOffOK " << kShopRipOffOK << " (was "
                         << (ripOff < 0 ? std::string("none") : std::to_string(ripOff)) << ")\n";
        }
        if (!FilterTpiSam(rideTable, text, folderOptions, filtered, report, err))
            Fail(name + ": " + err);
        for (const auto &d : report.dropped)
            log.droppedStatements.insert(Base(name) + ": " + d);
        w.text(name, filtered);
    }

    // The world wads, rebuilt with TGA textures, the terrain's height field repaired with the textures of all of them and
    // the world's obstacles merged into it (tpi_terrain.h).
    std::vector<std::pair<std::string, Converted>> wads;
    for (const WorldWad &wad : kWorldWads) {
        const Entry *entry = levels.find(world + "/" + wad.name);
        if (entry == nullptr) {
            log.text << "    missing: " << wad.name << " (the donor world's is used)\n";
            continue;
        }
        std::vector<TpiWadMember> members;
        std::string err;
        if (!ReadTpiWad(levels.read(*entry), members, err))
            Fail(entry->path + ": " + err);
        Converted c = ConvertMembers(members, nullptr, nullptr, nullptr, wad.name, log);
        if (std::string(wad.name) == "terrain.wad") {
            for (auto &m : c.members)
                if (Extension(m.name) == ".tct" && Dir(m.name).empty() && Lower(Stem(m.name)) != world)
                    m.name = world + ".tct";
        }
        wads.emplace_back(wad.name, std::move(c));
    }
    std::map<std::string, const Bytes *> textures;   // lower-case file name: the first world wad's member of that name
    std::map<std::string, const Bytes *> shared;     // "textures/<name>" from sharetex.wad, "stexture/<name>" from ssharete.wad
    for (const auto &wad : wads)
        for (const auto &m : wad.second.members) {
            if (Extension(m.name) != ".tga")
                continue;
            textures.emplace(Lower(Base(m.name)), &m.data);
            if (wad.first == "sharetex.wad" || wad.first == "ssharete.wad")
                shared.emplace(std::string(wad.first == "sharetex.wad" ? "textures/" : "stexture/") + Lower(Base(m.name)),
                               &m.data);
        }
    std::vector<TpiObstacle> obstacles;
    std::string lakeSam;   // TpiLake.sam, once the obstacles are merged
    for (const auto &[rel, entry] : levels.under(world + "/obstacles/")) {
        if (rel.find('/') != std::string::npos || Extension(rel) != ".wad")
            continue;
        std::vector<TpiWadMember> members;
        std::string err;
        if (!ReadTpiWad(levels.read(*entry), members, err))
            Fail(entry->path + ": " + err);
        TpiObstacle obstacle;
        obstacle.name = Stem(Base(entry->path));
        obstacle.members = ConvertMembers(members, nullptr, nullptr, nullptr, "obstacles/" + Base(entry->path), log).members;
        obstacles.push_back(std::move(obstacle));
    }
    for (auto &wad : wads) {
        if (wad.first != "terrain.wad")
            continue;
        bool hasBase = false;
        for (auto &m : wad.second.members) {
            if (!Dir(m.name).empty() || Lower(m.name) != "base.md2")
                continue;
            hasBase = true;
            TpiTerrainReport report;
            std::string err;
            const bool repaired = RepairTpiTerrain(
                m.data,
                [&](const std::string &name) {
                    auto it = textures.find(Lower(Base(name)));
                    return it == textures.end() ? nullptr : it->second;
                },
                report, err);
            if (!repaired)
                Fail(world + "/terrain.wad/" + m.name + ": " + err);
            log.text << "    terrain " << m.name << ": " << report.madeHidden << " quads with a clear texture hidden";
            for (const auto &t : report.clearTextures)
                log.text << " (" << t << ")";
            log.text << "\n";
        }
        if (!hasBase) {
            if (!obstacles.empty())
                log.text << "    left out: obstacles/ (terrain.wad has no base.MD2)\n";
            continue;
        }
        TpiObstacleReport report;
        std::string err;
        // The shared wads are not changed by the merge, so their members stay where `shared` points.
        const bool merged = MergeTpiObstacles(
            wad.second.members, obstacles,
            [&](const std::string &path) {
                auto it = shared.find(Lower(path));
                return it == shared.end() ? nullptr : it->second;
            },
            report, err);
        if (!merged)
            Fail(world + "/obstacles: " + err);
        if (lake.id >= 0)
            lakeSam = TpiLakeSam(lake, report, log);
        for (const auto &o : report.obstacles) {
            log.text << "    obstacle " << o.name << ": ";
            if (!o.skipped.empty()) {
                log.text << "left out (" << o.skipped << ")\n";
                continue;
            }
            log.text << o.state << ", " << o.nodes << " nodes, " << o.faces << " faces in the terrain; blocked cells: "
                     << o.blocked << " made hidden, " << o.alreadyHidden << " hidden already, " << o.open << " left open";
            if (o.walkway != 0)
                log.text << "; walkway cells shown: " << o.walkway;
            if (o.leaves != 0)
                log.text << "; gate leaves: " << o.leaves;
            if (o.shut != 0)
                log.text << "; shown until its land is bought: " << o.shut << " nodes";
            if (!o.leftOut.empty()) {
                log.text << "; left out:";
                for (const auto &n : o.leftOut)
                    log.text << " \"" << n << "\"";
            }
            log.text << "\n";
        }
        for (const auto &t : report.renamedTextures)
            log.text << "    obstacle texture renamed: " << t << "\n";
        for (const auto &t : report.missingTextures)
            log.text << "    obstacle texture missing: " << t << "\n";
        log.text << "    terrain: " << report.addedTextures.size() << " obstacle textures added, " << report.stillClear.size()
                 << " hidden quads nothing covers\n";
    }
    // A lake ride whose lake was not found keeps its boats on its plot, as before: their wakes are hidden after all.
    if (lake.id >= 0 && lakeSam.empty())
        for (const auto &m : lake.meshes)
            if (TpiWadMember t{m.name, {}}; HideWake(m.data, t.data)) {
                w.file(lake.folder + "/" + m.name, t.data);
                log.text << "    wake hidden: " << lake.name << "/" << m.name << " (no lake for its boats)\n";
            }
    for (const auto &wad : wads) {
        w.file(wad.first, BuildTpiWad(wad.second.members));
        log.text << "    wad " << wad.first << " (" << wad.second.members.size() << " files)\n";
    }

    // Files copied as they are.
    for (const char *name : kCopiedFiles) {
        const Entry *entry = levels.find(world + "/" + name);
        if (entry == nullptr) {
            log.text << "    missing: " << name << "\n";
            continue;
        }
        w.file(name, levels.read(*entry));
    }
    for (const CopiedFolder &folder : kCopiedFolders) {
        int count = 0;
        for (const auto &[rel, entry] : levels.under(world + "/" + folder.tpi + "/")) {
            const std::string out = std::string(folder.out) + "/" + entry->path.substr(entry->path.size() - rel.size());
            Bytes bytes = levels.read(*entry);
            for (const SfxPatch &p : kSfxPatches)
                if (world == p.world && Lower(out) == std::string("sound/") + p.file) {
                    std::string reason;
                    if (PatchTpiSfxVolume(bytes, p.patch, reason))
                        log.text << "    " << out << ": event " << p.patch.event << " volume " << int(p.patch.fromVolume)
                                 << " to " << int(p.patch.toVolume) << "\n";
                    else
                        log.text << "    " << out << ": event " << p.patch.event << " left as it is: " << reason << "\n";
                }
            w.file(out, bytes);
            ++count;
        }
        if (count)
            log.text << "    copied " << folder.out << "/ (" << count << " files)\n";
    }
    // The island screen's music, three segments of the world's own park music (tpi_lobby_music.h), in the place of the
    // donor's; without them the donor's plays.
    {
        const Entry *map = levels.find(world + "/music/cat_musicsfx.map");
        const Entry *bank = levels.find(world + "/music/musichd.sdt");
        TpiLobbyMusic lobby;
        std::string reason = "the world has no Music/cat_musicSFX.map and Music/MusicHD.sdt";
        if (map != nullptr && bank != nullptr && BuildTpiLobbyMusic(levels.read(*map), levels.read(*bank), lobby, reason)) {
            w.file("Sound/LobbyMusicHD.sdt", lobby.bank);
            w.file("Sound/cat_locallobbymusicBANK.map", lobby.bankMap);
            w.file("Sound/cat_locallobbymusicSFX.map", lobby.sfxMap);
            log.text << "    lobby music: park music group " << lobby.picks.front().group << ", samples";
            for (const TpiLobbyPick &p : lobby.picks)
                log.text << " " << p.sample << " (" << p.name << ", " << p.length << " ms)";
            log.text << "\n";
            if (lobby.unusable != 0)
                log.text << "    lobby music: " << lobby.unusable << " samples of the park music map have no whole bank entry\n";
        } else
            log.text << "    lobby music: the donor's (" << reason << ")\n";
    }

    // The world's own .sam files.
    TpiSamOptions standard;
    standard.rename = {{"TicketBooth[0].PosX", "FixedItemInfo.TicketBoothAPosX"},
                       {"TicketBooth[0].PosY", "FixedItemInfo.TicketBoothAPosY"},
                       {"TicketBooth[1].PosX", "FixedItemInfo.TicketBoothBPosX"},
                       {"TicketBooth[1].PosY", "FixedItemInfo.TicketBoothBPosY"}};
    standard.append = TicketThresholds(assets, spec, log);
    w.text("Standard.sam", FilterWorldSam(levels, world, "Standard.sam", TableFor("standard.sam", tables), standard, log));
    w.text("TpiPeeps.sam", TpiPeepsSam(levels, world, log));
    if (!lakeSam.empty())
        w.text("TpiLake.sam", lakeSam);
    if (!outside.empty())
        w.text("TpiOutside.sam", TpiOutsideSam(outside, log));
    TpiSamOptions global;
    global.drop = {"StaffPoolInfo.*", "ParkInfo.AvailableFromStart"};
    global.idFields = {"ParkName.GateObjectId"};
    global.ids = &ids;
    global.append = {"Keys.CostToEnter\t\t\t\t\t" + std::to_string(spec.keys), "Tickets.CanEarnTicketsInTheme\t\t1",
                     "Tickets.CanSpendTicketsInTheme\t\t1"};
    w.text("global.sam", FilterWorldSam(levels, world, "global.sam", TableFor("global.sam", tables), global, log));
    TpiSamOptions plain;
    w.text("Hoardings.sam", FilterWorldSam(levels, world, "Hoardings.sam", TableFor("hoardings.sam", tables), plain, log));
    w.text("EndHoardings.sam",
           FilterWorldSam(levels, world, "EndHoardings.sam", TableFor("endhoardings.sam", tables), plain, log));

    for (const char *folder : kLeftOutFolders)
        if (!levels.under(world + "/" + folder + "/").empty())
            log.text << "    left out: " << folder << "/\n";
    for (const char *name : kLeftOutFiles)
        if (levels.find(world + "/" + name) != nullptr)
            log.text << "    left out: " << name << "\n";
}

// ---- the add-on (TpiAddOnFolder)

// The add-on folder's files, as (path below the folder, '/' separated, entry): none when there is no folder.
struct AddOnFiles {
    std::unique_ptr<Source> source;
    std::vector<std::pair<std::string, const Entry *>> files;
};
AddOnFiles ListAddOn(const fs::path &dataRoot) {
    AddOnFiles out;
    std::error_code ec;
    const fs::path folder = TpiAddOnFolder(dataRoot);
    if (!fs::is_directory(fs::symlink_status(folder, ec)))
        return out;
    std::string err;
    out.source = OpenFolder(tpw::PathText(folder), err);
    if (!out.source)
        Fail("cannot read the add-on folder: " + err);
    for (const Entry &e : out.source->Entries())
        if (e.kind == EntryKind::Data && !e.unreadable && !EndsWith(e.path, ".part"))
            out.files.push_back({e.path, &e});
    std::sort(out.files.begin(), out.files.end());
    return out;
}

// converted.txt's line for an add-on file the conversion took (TpiAddOnConverted reads them back).
std::string AddOnFileLine(const std::string &path, uint64_t size) {
    return "add-on file: " + path + " (" + std::to_string(size) + " bytes)";
}

// A file name of the add-on that is safe to write as it is on every system the launcher runs on.
bool PlainFileName(const std::string &name) {
    if (name.empty() || name[0] == '.' || name.size() > 128)
        return false;
    for (unsigned char c : name)
        if (c < 0x20 || c == 0x7f || std::strchr("\\/:*?\"<>|", c) != nullptr)
            return false;
    return name.back() != ' ' && name.back() != '.';
}

void ClearTree(const fs::path &path) {
    std::error_code ec;
    auto status = fs::symlink_status(path, ec);
    if (ec)
        return;
    if (fs::is_directory(status))
        for (auto &e : fs::directory_iterator(path))
            ClearTree(e.path());
    fs::remove(path, ec);
}

// Puts the staged data folder in the place of the current one (the old one is kept beside it until the new one is in
// place, as ps1_convert.cpp does).
void Install(const fs::path &staged, const fs::path &destination) {
    fs::create_directories(destination.parent_path());
    fs::path backup = destination.parent_path() / ".tpi-previous-data";
    if (fs::exists(fs::symlink_status(backup))) {
        if (fs::exists(fs::symlink_status(destination)))
            ClearTree(backup);
        else
            fs::rename(backup, destination);
        if (fs::exists(fs::symlink_status(backup)))
            Fail("the previous converted worlds need recovery");
    }
    const bool existing = fs::exists(fs::symlink_status(destination));
    if (existing)
        fs::rename(destination, backup);
    try {
        fs::rename(staged, destination);
    } catch (...) {
        if (existing)
            fs::rename(backup, destination);
        throw;
    }
    if (existing)
        ClearTree(backup);
}

}   // namespace

bool ConvertTpi(const TpiDisc &disc, const fs::path &dataRoot, std::string &reason, TpiAddOnResult *addOnResult) {
    const auto started = std::chrono::steady_clock::now();
    fs::path staging;
    try {
        if (!disc.source)
            Fail("the SimCoaster AKA Theme Park Inc copy has no source");
        Levels levels;
        levels.source = disc.source.get();
        const std::string prefix = Lower(disc.root) + "data/levels/";
        for (const Entry &e : disc.source->Entries()) {
            if (e.kind != EntryKind::Data || e.unreadable)
                continue;
            const std::string lower = Lower(e.path);
            if (lower.compare(0, prefix.size(), prefix) == 0)
                levels.entries.push_back({lower.substr(prefix.size()), &e});
        }
        std::sort(levels.entries.begin(), levels.entries.end(),
                  [](const auto &a, const auto &b) { return a.first < b.first; });
        if (levels.entries.empty())
            Fail("no Data/levels folder in the SimCoaster AKA Theme Park Inc copy");
        // The add-on kept in the data folder (StoreTpiAddOn): its items join their worlds after the copy's own.
        const AddOnFiles addOnFiles = ListAddOn(dataRoot);
        Levels addOn;
        addOn.source = addOnFiles.source.get();
        int coasters = 0;
        for (const auto &[path, entry] : addOnFiles.files) {
            const std::string lower = Lower(path);
            if (lower.compare(0, 12, "data/levels/") == 0)
                addOn.entries.push_back({lower.substr(12), entry});
            else if (lower.compare(0, 17, "data/my coasters/") == 0 && EndsWith(lower, ".cos"))
                coasters++;
        }
        std::sort(addOn.entries.begin(), addOn.entries.end());

        const fs::path assets = dataRoot / "assets";
        const fs::path top = assets / ".staging";
        if (fs::is_symlink(fs::symlink_status(top)))
            Fail("the staging folder is a symbolic link");
        fs::create_directories(top);
        // A staging folder left by an interrupted run goes first: only one conversion runs at a time.
        {
            std::error_code ec;
            for (auto &e : fs::directory_iterator(top, ec))
                if (tpw::PathText(e.path().filename()).compare(0, 4, "tpi-") == 0)
                    ClearTree(e.path());
        }
        for (unsigned n = 0;; n++) {
            staging = top / ("tpi-" + std::to_string(n));
            std::error_code ec;
            if (fs::create_directory(staging, ec))
                break;
            if (ec)
                Fail("cannot create the staging folder");
        }

        Log log;
        log.addOn.coasters = coasters;
        std::map<std::string, std::vector<SamRow>> tables;
        TpiStrings names;
        const bool named = ReadObjectNames(disc, levels, names, log);
        for (const WorldSpec &spec : kWorlds)
            ConvertWorld(levels, addOn.entries.empty() ? nullptr : &addOn, spec, staging, assets, tables,
                         named ? &names : nullptr, log);
        ConvertSprites(disc, levels, staging, assets, log);
        // The islands go into the staging folder too (its data/lobby/), so they reach assets/tpi/data in the same swap as the
        // worlds: the game never sees a world without its island or an island without its world.
        std::string lobbyWhere;
        const std::vector<TpiWadMember> lobby = TpwLobby(assets, lobbyWhere);
        log.text << "island screen: " << (lobby.empty() ? "no Theme Park World lobby.wad" + (lobbyWhere.empty() ? std::string() : " " + lobbyWhere) + ", no waves and the original sea"
                                                         : "waves and sea from " + lobbyWhere)
                 << "\n";
        if (!BuildTpiIslands(disc, tpw::PathText(staging), reason, lobby.empty() ? nullptr : &lobby))
            Fail(reason);

        const fs::path destination = assets / "tpi" / "data";
        if (fs::is_symlink(fs::symlink_status(destination)))
            Fail("the converted worlds folder is a symbolic link");
        Install(staging / "data", destination);
        ClearTree(staging);
        staging.clear();

        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        std::ostringstream record;
        record << "Theme Park Inc worlds converted by the TPI Worlds and Rides mod, converter version " << kTpiConverterVersion
               << "\n"
               << "edition: " << disc.edition << "\n"
               << "from: " << disc.path << "\n"
               << "items converted: " << log.itemsConverted << ", left out: " << log.itemsLeftOut
               << ", textures converted: " << log.texturesConverted << ", files written: " << log.filesWritten << " ("
               << (log.bytesWritten >> 20) << " MB), " << static_cast<int>(seconds + 0.5) << " s\n\n"
               << log.text.str() << "\nstatements Theme Park World has no setting for (dropped):\n";
        for (const auto &d : log.droppedStatements)
            record << "    " << d << "\n";
        if (!addOnFiles.files.empty()) {
            record << "\nthe add-on kept in assets/tpi/bonus (" << log.addOn.added.size() << " items added, "
                   << coasters << " ready-made coasters kept, not built: Theme Park World has no ready-made coaster):\n";
            for (const auto &[path, entry] : addOnFiles.files)
                record << "    " << AddOnFileLine(path, entry->size) << "\n";
        }
        {
            fs::path path = assets / "tpi" / "converted.txt";
            fs::path part = path;
            part += ".part";
            std::ofstream out(part, std::ios::binary | std::ios::trunc);
            out << record.str();
            out.close();
            if (!out)
                Fail("cannot write converted.txt");
            fs::rename(part, path);
        }
        if (!WriteTpiConverterStamp(dataRoot, reason))
            return false;
        if (addOnResult != nullptr)
            *addOnResult = log.addOn;
        return true;
    } catch (const std::exception &e) {
        if (!staging.empty())
            ClearTree(staging);
        reason = std::string("SimCoaster AKA Theme Park Inc conversion failed: ") + e.what();
        return false;
    }
}

bool ConvertTpiFromPath(const std::string &path, const fs::path &dataRoot, std::string &reason, TpiAddOnResult *addOn) {
    std::string err;
    std::shared_ptr<Source> source;
    std::error_code ec;
    if (fs::is_directory(fs::u8path(path), ec)) {
        source = OpenFolder(path, err);
    } else {
        auto blob = OpenFileBlob(path, err);
        if (blob) {
            SniffOptions options;
            options.spillDir = tpw::PathText(dataRoot / "assets" / ".staging");
            source = OpenSniffed(blob, options, 0, err);
        }
    }
    if (!source) {
        reason = "cannot open " + path + ": " + err;
        return false;
    }
    TpiDisc disc;
    const TpiStatus status = RecogniseTpi(source, disc, reason);
    if (status == TpiStatus::NotThemeParkInc) {
        reason = path + " is not a copy of SimCoaster AKA Theme Park Inc";
        return false;
    }
    if (status == TpiStatus::Rejected)
        return false;
    if (status == TpiStatus::AddOn) {
        reason = path + " is the SimCoaster Bonus Disk, not the game";
        return false;
    }
    disc.path = path;
    return ConvertTpi(disc, dataRoot, reason, addOn);
}

fs::path TpiAddOnFolder(const fs::path &dataRoot) { return dataRoot / "assets" / "tpi" / "bonus"; }

bool StoreTpiAddOn(const TpiDisc &addOn, const fs::path &dataRoot, TpiAddOnStore &store, std::string &reason) {
    store = TpiAddOnStore();
    try {
        if (!addOn.source)
            Fail("the SimCoaster Bonus Disk has no source");
        const fs::path folder = TpiAddOnFolder(dataRoot);
        std::error_code ec;
        for (fs::path p = folder; p != dataRoot && !p.empty(); p = p.parent_path())
            if (fs::is_symlink(fs::symlink_status(p, ec)))
                Fail("the add-on folder is a symbolic link");
        Levels reader;
        reader.source = addOn.source.get();
        for (const Entry &e : addOn.source->Entries()) {
            if (e.kind != EntryKind::Data || e.unreadable)
                continue;
            if (e.path.compare(0, 5, "Data/") != 0)
                continue;
            const std::string rel = e.path.substr(5), lower = Lower(rel);
            const std::string name = Base(rel);
            const bool coaster = lower.compare(0, 12, "my coasters/") == 0 && EndsWith(lower, ".cos");
            const bool wad = lower.compare(0, 7, "levels/") == 0 && EndsWith(lower, ".wad");
            if (!coaster && !wad)
                continue;
            std::string why;
            Bytes bytes;
            if (!PlainFileName(name))
                why = "a file name the launcher does not write";
            else if (e.size == 0 || e.size > kMaxEntry)
                why = e.size == 0 ? "an empty file" : "too big";
            else
                bytes = reader.read(e);
            if (why.empty() && wad) {
                Item item;
                item.name = name.substr(0, name.size() - 4);
                int32_t id = -1;
                std::string err;
                if (!ReadTpiWad(bytes, item.members, err))
                    why = "not a wad the converter reads: " + err;
                else if (const TpiWadMember *sam = ItemSam(item);
                         sam == nullptr ||
                         !TpiSamInteger(std::string(sam->data.begin(), sam->data.end()), "Info", "Id", id))
                    why = "no " + item.name + ".sam with an Info.Id";
            }
            if (!why.empty()) {
                store.leftOut.push_back(rel + ": " + why);
                continue;
            }
            const fs::path destination = folder / "Data" / fs::u8path(rel);
            if (fs::is_regular_file(fs::symlink_status(destination, ec)) && fs::file_size(destination, ec) == bytes.size()) {
                std::ifstream in(destination, std::ios::binary);
                const Bytes have((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                if (have == bytes) {
                    store.alreadyThere.push_back(rel);
                    continue;
                }
            }
            fs::create_directories(destination.parent_path());
            fs::path part = destination;
            part += ".part";
            {
                std::ofstream out(part, std::ios::binary | std::ios::trunc);
                out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                out.close();
                if (!out)
                    Fail("cannot write " + rel);
            }
            fs::rename(part, destination);
            store.written.push_back(rel);
        }
        return true;
    } catch (const std::exception &e) {
        reason = std::string("the SimCoaster Bonus Disk could not be kept: ") + e.what();
        return false;
    }
}

bool TpiAddOnConverted(const fs::path &dataRoot) {
    try {
        const AddOnFiles files = ListAddOn(dataRoot);
        if (files.files.empty())
            return true;
        std::ifstream in(dataRoot / "assets" / "tpi" / "converted.txt", std::ios::binary);
        if (!in)
            return false;
        std::set<std::string> lines;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const size_t start = line.find_first_not_of(' ');
            if (start != std::string::npos)
                lines.insert(line.substr(start));
        }
        for (const auto &[path, entry] : files.files)
            if (!lines.count(AddOnFileLine(path, entry->size)))
                return false;
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

} // namespace tpw::assets

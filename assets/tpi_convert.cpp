// tpi_convert.cpp: the Theme Park Inc converter (tpi_convert.h). Theme Park Inc keeps each of its worlds under
// Data/levels/<world>/ in the same shape as Theme Park World's (the item folders hold one .wad per ride, shop,
// sideshow, feature or upgrade; the terrain, shared textures, queues, hoardings and advisor are wads too), so the
// converter walks that tree and writes a Theme Park World world from it:
//  - textures: Theme Park Inc's .fsh become 32-bit TGAs with the same stem (tpi_fsh.h), inside wads and items alike,
//    and the qickload.txt lists are rewritten to name them and only what their own folder holds; 8-bit grey TGAs
//    become 32-bit ones too;
//  - items: each .wad is written as an unpacked folder (the game reads both) whose .sam keeps only the statements
//    Theme Park World's ride table has (tpi_sam.h); its Info.Id is moved into the world's own block of ids so it never
//    collides with a Theme Park World item, and every reference to it follows; rides lose their price per use, as
//    Theme Park World's rides have none (the park charges at the gate); its Info.Name becomes the name Theme Park Inc
//    shows, from the copy's own object names bank (tpi_names.h), or its working name in title case;
//  - the mystery item and the two ground items every Theme Park World world has (and the Buy Ride panel needs), and
//    the advisor, are copied from the donor world in the player's own Theme Park World data under <dataRoot>/assets
//    (kDonorItems); nothing of Theme Park Inc's advisor is used;
//  - the world's own .sam files (Standard, global, Hoardings, EndHoardings) are filtered the same way, with the
//    few renamed statements and the lobby settings the mod needs (free entry, tickets);
//  - the world wads (terrain, sharetex, ssharete, dynamic, queue, miscmesh, hoarding) are rebuilt stored
//    (tpi_wad.h), the terrain's .tct named after the world;
//  - sky, 2D map, scape, music and sounds are copied as they are;
//  - the worlds' entertainers, costumes and costume heads come out of Theme Park Inc's esprites.wad as loose files
//    under data/esprites/<world>/ (kSpriteGroups), which the game finds before its own esprites.wad; a world with two
//    entertainers gets the first again under a new name, as the game takes three.
//  - each world's lobby island (data/lobby/<world>.txt and data/lobby/<world>/) is composed from the world's own gate,
//    terrain and textures (tpi_island.h), since Theme Park Inc has no island screen.
// What Theme Park World has no use for is left out and listed in <dataRoot>/assets/tpi/converted.txt: the obstacles,
// the facilities it does not know, the online files, the speech, the advisor. Everything is written into a staging folder first
// and swapped into place at the end, the stamp (converter.txt) last of all.
#include "tpi_convert.h"

#include <algorithm>
#include <cctype>
#include <chrono>
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
#include "tpi_names.h"
#include "tpi_sam.h"
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
            reason = "Theme Park Inc converter stamp is a symbolic link";
            return false;
        }
        fs::create_directories(path.parent_path());
        fs::path part = path;
        part += ".part";
        {
            std::ofstream out(part, std::ios::binary | std::ios::trunc);
            out << kTpiConverterVersion << "\n";
            if (!out) {
                reason = "cannot write the Theme Park Inc converter stamp";
                return false;
            }
        }
        fs::rename(part, path);
        return true;
    } catch (const std::exception &e) {
        reason = std::string("cannot write the Theme Park Inc converter stamp: ") + e.what();
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

// Textures and texture lists: .fsh to .tga, a .tga beside a .fsh of the same stem dropped, qickload.txt rewritten.
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

// Sam files: filtered through `samTable` with `samOptions` when the member's extension is .sam (nullptr: copied).
// Online_*.sam members are left out, and so are meshes kept among the textures (TPI's gokarts has copies there).
// An animated texture list (.ifl; TPI's penguins and seals blink with one) becomes a .tga of its most used frame, as
// Theme Park World has no animated textures and the mesh asks for "<stem>.tga".
Converted ConvertMembers(const std::vector<TpiWadMember> &in, const std::vector<SamRow> *samTable,
                         const TpiSamOptions *samOptions, const std::vector<SamRow> *coasterTable, const std::string &what,
                         Log &log) {
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
            log.texturesConverted++;
            out.members.push_back(std::move(t));
        } else if (ext == ".tga" && fshStems.count(Lower(Stem(m.name)))) {
            log.text << "    texture left out: " << what << "/" << m.name << " (a .fsh of the same name wins)\n";
        } else if (TpiWadMember t{m.name, {}}; ext == ".tga" && GreyTgaToTrueColour(m.data, t.data)) {
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
        } else if (ext == ".sam" && Lower(base).compare(0, 7, "online_") == 0) {
            log.text << "    left out: " << what << "/" << m.name << " (online)\n";
        } else if (ext == ".sam" && samTable != nullptr) {
            // A coaster's track file (coaster.sam, read with the coaster table) or the item's own file.
            const bool coaster = Lower(base) == "coaster.sam" && coasterTable != nullptr;
            const TpiSamOptions options = coaster ? TpiSamOptions{} : *samOptions;
            std::string text(m.data.begin(), m.data.end()), filtered, err;
            TpiSamReport report;
            if (!FilterTpiSam(coaster ? *coasterTable : *samTable, text, options, filtered, report, err))
                Fail(what + "/" + m.name + ": " + err);
            for (const auto &d : report.dropped)
                log.droppedStatements.insert(what + ": " + d);
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

// ---- items

struct Item {
    std::string tpiFolder;   // "rides", ..., "facilities"
    std::string outFolder;   // "rides", ..., "features"
    std::string name;        // the wad's stem: the item's name and folder
    std::vector<TpiWadMember> members;
    int32_t oldId = -1;
    int32_t newId = -1;
};

struct ItemFolder {
    const char *tpi;
    const char *out;
};
const ItemFolder kItemFolders[] = {{"rides", "rides"},       {"shops", "shops"},       {"sideshow", "sideshow"},
                                   {"features", "features"}, {"upgrades", "upgrades"}, {"facilities", "features"}};
// The facilities Theme Park World knows (its features): both toilets (unisex ones there), the staff room, the
// security camera, the litter bin (two spellings). The information desk, the research lab, the training room and the
// science world's "nstaff" have no feature, so they are left out.
const char *const kFacilitiesTaken[] = {"ftoilet", "mtoilet", "staffrm", "seccam", "littbin", "litbin"};

const TpiWadMember *ItemSam(const Item &item) {
    const std::string want = Lower(item.name) + ".sam";
    for (const auto &m : item.members)
        if (Lower(m.name) == want)
            return &m;
    return nullptr;
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

void AssignIds(std::vector<Item> &items, int32_t base, std::map<int32_t, int32_t> &ids, Log &log) {
    std::vector<Item> kept;
    int32_t next = base;
    for (auto &item : items) {
        const TpiWadMember *sam = ItemSam(item);
        int32_t id = -1;
        if (sam == nullptr || !TpiSamInteger(std::string(sam->data.begin(), sam->data.end()), "Info", "Id", id)) {
            log.text << "    left out: " << item.tpiFolder << "/" << item.name << " (no " << item.name << ".sam with an Info.Id)\n";
            log.itemsLeftOut++;
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
const char *const kLeftOutFolders[] = {"obstacles", "speech"};
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

int32_t UiType(const std::string &samText, int32_t fallback) {
    int32_t type = 0;
    return TpiSamInteger(samText, "Info", "WhichUIType", type) ? type : fallback;
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

void ConvertWorld(const Levels &levels, const WorldSpec &spec, const fs::path &staging, const fs::path &assets,
                  std::map<std::string, std::vector<SamRow>> &tables, const TpiStrings *names, Log &log) {
    const std::string world = spec.folder;
    if (levels.under(world + "/").empty())
        Fail("the world " + world + " is missing from Data/levels");
    log.text << "world " << world << ":\n";
    Writer w{staging / "data" / "levels" / world, &log};

    // Items: collect, give ids, write unpacked.
    std::vector<Item> items;
    std::map<std::string, Bytes> folderSams;
    CollectItems(levels, world, items, folderSams, log);
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
    for (const auto &item : items) {
        const std::string what = item.tpiFolder + "/" + item.name;
        const bool gate = item.oldId == gateId;
        TpiSamOptions options = gate ? gateOptions : itemOptions;
        const TpiWadMember *sam = ItemSam(item);
        const std::string samText = sam != nullptr ? std::string(sam->data.begin(), sam->data.end()) : std::string();
        const bool ride = UiType(samText, FolderUiType(folderSams, item.outFolder)) == 0;
        int32_t price = 0;
        const bool freeRide = ride && TpiSamInteger(samText, "UsageInfo", "InitPricePerUse", price);
        if (ride)
            options.drop.push_back(kPricePerUse);
        // The name Theme Park Inc shows (tpi_names.h): the build menu and the ride's panels read Info.Name, as the
        // game's own name table has no converted id (RideDB.cpp GetTypeName).
        std::string infoName, shownName;
        bool fromBank = false;
        if (!gate && sam != nullptr) {
            TpiSamString(samText, "Info", "Name", infoName);
            shownName = TpiShownName(names, world, item.oldId, infoName, fromBank);
            if (!shownName.empty() && shownName != infoName)
                options.append.push_back("Info.Name\t\t\t\"" + shownName + "\"");
        }
        Converted c = ConvertMembers(item.members, &rideTable, &options, &coasterTable, what, log);
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
        if (!shownName.empty() && shownName != infoName)
            log.text << ", named \"" << shownName << "\" (was \"" << infoName << "\", "
                     << (fromBank ? "from the object names" : "in title case") << ")";
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
        }
        if (!FilterTpiSam(rideTable, text, folderOptions, filtered, report, err))
            Fail(name + ": " + err);
        for (const auto &d : report.dropped)
            log.droppedStatements.insert(Base(name) + ": " + d);
        w.text(name, filtered);
    }

    // The world wads, rebuilt with TGA textures.
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
        w.file(wad.name, BuildTpiWad(c.members));
        log.text << "    wad " << wad.name << " (" << c.members.size() << " files)\n";
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
            w.file(std::string(folder.out) + "/" + entry->path.substr(entry->path.size() - rel.size()), levels.read(*entry));
            ++count;
        }
        if (count)
            log.text << "    copied " << folder.out << "/ (" << count << " files)\n";
    }

    // The world's own .sam files.
    TpiSamOptions standard;
    standard.rename = {{"TicketBooth[0].PosX", "FixedItemInfo.TicketBoothAPosX"},
                       {"TicketBooth[0].PosY", "FixedItemInfo.TicketBoothAPosY"},
                       {"TicketBooth[1].PosX", "FixedItemInfo.TicketBoothBPosX"},
                       {"TicketBooth[1].PosY", "FixedItemInfo.TicketBoothBPosY"}};
    w.text("Standard.sam", FilterWorldSam(levels, world, "Standard.sam", TableFor("standard.sam", tables), standard, log));
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

bool ConvertTpi(const TpiDisc &disc, const fs::path &dataRoot, std::string &reason) {
    const auto started = std::chrono::steady_clock::now();
    fs::path staging;
    try {
        if (!disc.source)
            Fail("the Theme Park Inc copy has no source");
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
            Fail("no Data/levels folder in the Theme Park Inc copy");

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
        std::map<std::string, std::vector<SamRow>> tables;
        TpiStrings names;
        const bool named = ReadObjectNames(disc, levels, names, log);
        for (const WorldSpec &spec : kWorlds)
            ConvertWorld(levels, spec, staging, assets, tables, named ? &names : nullptr, log);
        ConvertSprites(disc, levels, staging, assets, log);
        // The islands go into the staging folder too (its data/lobby/), so they reach assets/tpi/data in the same swap as the
        // worlds: the game never sees a world without its island or an island without its world.
        if (!BuildTpiIslands(disc, tpw::PathText(staging), reason))
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
        return true;
    } catch (const std::exception &e) {
        if (!staging.empty())
            ClearTree(staging);
        reason = std::string("Theme Park Inc conversion failed: ") + e.what();
        return false;
    }
}

bool ConvertTpiFromPath(const std::string &path, const fs::path &dataRoot, std::string &reason) {
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
        reason = path + " is not a copy of Theme Park Inc";
        return false;
    }
    if (status == TpiStatus::Rejected)
        return false;
    disc.path = path;
    return ConvertTpi(disc, dataRoot, reason);
}

} // namespace tpw::assets

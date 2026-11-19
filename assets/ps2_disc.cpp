// ps2_disc.cpp: see ps2_disc.h.
#include "ps2_disc.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "ps2_model.h"

namespace tpw::assets {
namespace {

using Bytes = std::vector<uint8_t>;

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string Upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

void Require(bool ok, const std::string& why) {
    if (!ok) throw std::runtime_error(why);
}

uint32_t U32(const Bytes& b, size_t at) {
    Require(at + 4 <= b.size() && at + 4 > at, "DATA.WAD: a field is past the end of the file");
    return static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 | static_cast<uint32_t>(b[at + 2]) << 16 |
           static_cast<uint32_t>(b[at + 3]) << 24;
}

std::string Name(const Bytes& b, uint32_t at) {
    Require(at < b.size(), "DATA.WAD: a name is past the end of the file");
    const void* end = std::memchr(b.data() + at, 0, b.size() - at);
    Require(end != nullptr, "DATA.WAD: a name has no end");
    return std::string(reinterpret_cast<const char*>(b.data() + at), static_cast<const uint8_t*>(end) - (b.data() + at));
}

const Entry* FindEntry(Source& source, const std::string& path) {
    for (const Entry& e : source.Entries())
        if (e.kind == EntryKind::Data && Lower(e.path) == Lower(path)) return &e;
    return nullptr;
}

Bytes ReadEntry(Source& source, const Entry& entry, uint64_t limit) {
    std::string err;
    std::unique_ptr<Stream> stream = source.Open(entry, err);
    Require(stream != nullptr, entry.path + ": " + err);
    Bytes out;
    Require(ReadAll(*stream, limit, out, err), entry.path + ": " + err);
    return out;
}

}  // namespace

const std::vector<Ps2Character>& Ps2Characters() {
    static const std::vector<Ps2Character> table = {
        {"alien", 369},      {"Boy1a", 260},      {"Boy2a", 276},     {"Boy3a", 250},       {"Boy4a", 250},
        {"Dino", 477},       {"Fantasykid", 242}, {"FatMechanic", 424}, {"Flower", 452},    {"Franky", 500},
        {"Girl1a", 274},     {"Girl2a", 254},     {"Girl3a", 274},    {"Girl4a", 256},      {"Gnome", 534},
        {"Guard", 328},      {"hallowkid", 224},  {"Handyman", 363},  {"Hunter", 400},      {"junglekid", 241},
        {"Researcher", 360}, {"spacekid", 232},   {"Spaceman", 384},  {"Vampire", 552},
    };
    return table;
}

bool ReadPs2Wad(const Bytes& wad, const std::string& prefix, std::map<std::string, Bytes>& files, std::string& reason) {
    files.clear();
    reason.clear();
    try {
        Require(wad.size() >= 24 && std::memcmp(wad.data(), "FKNL", 4) == 0, "DATA.WAD is not an FKNL linkfile");
        const std::string want = Lower(prefix);
        // Node: files, subfolders, file count, subfolder count. File: name, offset, stored size, unpacked size.
        // Subfolder: name, node. Names are offsets from the start of the file.
        std::vector<std::pair<uint32_t, std::string>> pending = {{U32(wad, 20), ""}};
        size_t visited = 0;
        while (!pending.empty()) {
            const auto [node, folder] = pending.back();
            pending.pop_back();
            Require(++visited < 100000, "DATA.WAD: its folders loop");
            const uint32_t fp = U32(wad, node), sp = U32(wad, node + 4), nf = U32(wad, node + 8), ns = U32(wad, node + 12);
            Require(nf < 100000 && ns < 100000, "DATA.WAD: a folder has too many entries");
            for (uint32_t i = 0; i < nf; ++i) {
                const size_t at = static_cast<size_t>(fp) + 16 * static_cast<size_t>(i);
                const std::string path = folder + Name(wad, U32(wad, at));
                const uint32_t off = U32(wad, at + 4), size = U32(wad, at + 8), unpacked = U32(wad, at + 12);
                if (Lower(path).compare(0, want.size(), want) != 0) continue;
                Require(static_cast<uint64_t>(off) + size <= wad.size(), "DATA.WAD: " + path + " is past the end of the file");
                Bytes data;
                if (size != unpacked) {
                    std::string err;
                    Require(RefPackDecode(wad.data() + off, size, unpacked, data, err), "DATA.WAD: " + path + ": " + err);
                    Require(data.size() == unpacked, "DATA.WAD: " + path + " unpacks to another size than its entry gives");
                } else {
                    data.assign(wad.begin() + off, wad.begin() + off + size);
                }
                files[path] = std::move(data);
            }
            for (uint32_t i = 0; i < ns; ++i) {
                const size_t at = static_cast<size_t>(sp) + 8 * static_cast<size_t>(i);
                pending.push_back({U32(wad, at + 4), folder + Name(wad, U32(wad, at)) + "/"});
            }
        }
        return true;
    } catch (const std::exception& e) {
        files.clear();
        reason = e.what();
        return false;
    }
}

const Bytes* FindPs2File(const std::map<std::string, Bytes>& chars, const std::string& path) {
    const std::string want = Lower(path);
    for (const auto& [name, bytes] : chars)
        if (Lower(name) == want) return &bytes;
    return nullptr;
}

Ps2Status RecognisePs2(Source& source, Ps2Disc& disc, std::string& reason) {
    disc = Ps2Disc{};
    reason.clear();
    const Entry* cnfEntry = FindEntry(source, "SYSTEM.CNF");
    if (cnfEntry == nullptr) return Ps2Status::NotPs2;
    bool ps2 = false;
    try {
        const Bytes cnf = ReadEntry(source, *cnfEntry, 65536);
        std::istringstream lines(std::string(cnf.begin(), cnf.end()));
        std::string line, boot;
        while (std::getline(lines, line)) {
            const size_t eq = line.find('=');
            if (eq == std::string::npos || Upper(Trim(line.substr(0, eq))) != "BOOT2") continue;
            boot = Upper(Trim(line.substr(eq + 1)));
            const size_t slash = boot.find_last_of("\\/:");
            if (slash != std::string::npos) boot.erase(0, slash + 1);
            if (boot.find(';') != std::string::npos) boot.resize(boot.find(';'));
        }
        if (boot.empty()) return Ps2Status::NotPs2;   // a PlayStation 1 disc names BOOT, not BOOT2
        const Entry* wadEntry = FindEntry(source, "DATA/DATA.WAD");
        if (boot != "SLUS_200.99" && wadEntry == nullptr) {   // another PS2 game
            reason = "it is another PlayStation 2 game (SYSTEM.CNF boots " + boot +
                     "), not Theme Park Roller Coaster (SLUS_200.99)";
            return Ps2Status::NotPs2;
        }
        ps2 = true;
        Require(boot == "SLUS_200.99", "SYSTEM.CNF boots " + boot + ", not the supported US disc (SLUS_200.99)");
        const Entry* exe = FindEntry(source, boot);
        Require(exe != nullptr, boot + " is missing");
        const Bytes head = ReadEntry(source, *exe, 64 * 1024 * 1024);
        Require(head.size() >= 4 && std::memcmp(head.data(), "\x7f" "ELF", 4) == 0, boot + " is not a PS2 executable");
        Require(wadEntry != nullptr, "DATA/DATA.WAD is missing");
        const Bytes wad = ReadEntry(source, *wadEntry, 256 * 1024 * 1024);
        Ps2Disc found;
        found.boot = boot;
        Require(ReadPs2Wad(wad, "Chars/", found.chars, reason), reason);
        for (const Ps2Character& ch : Ps2Characters()) {
            Ps2Mps mps;
            std::string err;
            const std::string folder = std::string("Chars/") + ch.folder + "/";
            const Bytes* model = nullptr;
            for (const auto& [name, bytes] : found.chars) {
                const std::string low = Lower(name);
                if (low.compare(0, folder.size(), Lower(folder)) == 0 && low.size() > 4 && low.compare(low.size() - 4, 4, ".mps") == 0)
                    model = &bytes;
            }
            Require(model != nullptr, folder + " has no model (.mps)");
            Require(ParsePs2Mps(*model, mps, err), folder + ": " + err);
            Require(static_cast<int>(mps.triangles) == ch.triangles,
                    folder + "'s model has " + std::to_string(mps.triangles) + " triangles, not " + std::to_string(ch.triangles));
        }
        disc = std::move(found);
        return Ps2Status::Supported;
    } catch (const std::exception& e) {
        reason = e.what();
        for (char& c : reason)
            if (c == '\r' || c == '\n') c = ' ';
        return ps2 ? Ps2Status::Rejected : Ps2Status::NotPs2;
    }
}

Ps2Status RecognisePs2Path(const std::string& path, Ps2Disc& disc, std::string& reason) {
    disc = Ps2Disc{};
    reason.clear();
    std::error_code ec;
    const std::filesystem::path p = std::filesystem::u8path(path);
    std::unique_ptr<Source> source;
    if (std::filesystem::is_directory(p, ec)) {
        source = OpenFolder(path, reason);
    } else {
        std::filesystem::path image = p;
        if (Lower(p.extension().u8string()) == ".cue") {
            // FILE "<name>.bin" BINARY: the image next to the sheet.
            std::ifstream cue(p);
            std::string line;
            image.clear();
            while (std::getline(cue, line)) {
                const size_t a = line.find('"'), b = line.rfind('"');
                if (Upper(Trim(line)).compare(0, 5, "FILE ") == 0 && a != std::string::npos && b > a) {
                    image = p.parent_path() / std::filesystem::u8path(line.substr(a + 1, b - a - 1));
                    break;
                }
            }
            if (image.empty()) {
                reason = "the .cue sheet names no image file";
                return Ps2Status::NotPs2;
            }
        }
        std::shared_ptr<Blob> blob = OpenFileBlob(image.u8string(), reason);
        if (blob) source = OpenIso9660(blob, reason);
    }
    if (!source) return Ps2Status::NotPs2;
    const Ps2Status status = RecognisePs2(*source, disc, reason);
    if (status == Ps2Status::Supported) disc.path = path;
    return status;
}

}  // namespace tpw::assets

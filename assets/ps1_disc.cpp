// Recognition of the player's PlayStation disc, before any converted content is written. The few MIPS encodings
// below describe constant table construction only: this scans them as data, never executes code from the image.
#include "ps1_disc.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace tpw::assets {
namespace {

using Bytes = std::vector<uint8_t>;
constexpr uint32_t kBase = 0x80010000;
constexpr uint32_t kCopy = 0x8daf0000;
const uint8_t kSync[12] = {0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0};

uint16_t U16(const Bytes& b, size_t o) {
    if (o > b.size() || b.size() - o < 2) throw std::runtime_error("table runs past its entry");
    return uint16_t(b[o] | uint16_t(b[o + 1]) << 8);
}
uint32_t U32(const Bytes& b, size_t o) { return uint32_t(U16(b, o)) | uint32_t(U16(b, o + 2)) << 16; }
int32_t S16(uint32_t x) { return (x & 0x8000) ? int32_t(x & 65535) - 65536 : int32_t(x & 65535); }
std::string Upper(std::string s) { for (char& c : s) c = char(std::toupper(static_cast<unsigned char>(c))); return s; }
std::string Trim(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n\0", 0, 6), z = s.find_last_not_of(" \t\r\n\0", std::string::npos, 6);
    return a == std::string::npos ? "" : s.substr(a, z - a + 1);
}
void Require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }

struct IsoFile { uint32_t sector = 0, size = 0; };

class DiscReader {
public:
    explicit DiscReader(std::shared_ptr<Blob> blob) : blob_(std::move(blob)) {}
    bool Init() {
        for (auto layout : {std::pair<uint32_t, uint32_t>{2048, 0}, {2352, 24}, {2352, 16}, {2448, 24}, {2448, 16}}) {
            stride = layout.first;
            offset = layout.second;
            Bytes p(2048);
            std::string err;
            if (blob_->ReadAt(uint64_t(16) * stride + offset, p.data(), p.size(), err) == 2048 &&
                p[0] == 1 && std::memcmp(p.data() + 1, "CD001", 5) == 0) {
                primary = std::move(p);
                return true;
            }
        }
        return false;
    }
    uint64_t Sectors() const { return blob_->Size() / stride; }
    Bytes Read(uint32_t first, uint32_t size) {
        Require(uint64_t(first) + (uint64_t(size) + 2047) / 2048 <= Sectors(), "file runs past the end of the image");
        Bytes out(size);
        std::string err;
        if (stride == 2048) {
            Require(blob_->ReadAt(uint64_t(first) * stride, out.data(), out.size(), err) == int64_t(out.size()),
                    err.empty() ? "image is truncated" : err);
        } else {
            for (uint32_t i = 0; uint64_t(i) * 2048 < size; ++i) {
                size_t take = std::min<size_t>(2048, size - size_t(i) * 2048);
                Require(blob_->ReadAt(uint64_t(first + i) * stride + offset, out.data() + size_t(i) * 2048, take, err) == int64_t(take),
                        err.empty() ? "image is truncated" : err);
            }
        }
        return out;
    }
    void Walk(uint32_t sector, uint32_t size, const std::string& prefix, size_t depth = 0) {
        Require(depth <= 32 && size <= 16 * 1024 * 1024, "ISO directory is too large or deeply nested");
        Require(seen_.insert(sector).second, "ISO directory contains a loop");
        Bytes d = Read(sector, size);
        for (size_t o = 0; o < d.size();) {
            size_t n = d[o];
            if (!n) { o = (o / 2048 + 1) * 2048; continue; }
            Require(n >= 34 && n <= d.size() - o && size_t(d[o + 32]) + 33 <= n, "ISO directory record is damaged");
            size_t len = d[o + 32];
            if (!(len == 1 && d[o + 33] <= 1)) {
                std::string name(reinterpret_cast<const char*>(d.data() + o + 33), len);
                name = Upper(name.substr(0, name.find(';')));
                uint32_t at = U32(d, o + 2), bytes = U32(d, o + 10);
                Require(uint64_t(at) + (uint64_t(bytes) + 2047) / 2048 <= Sectors(), name + " runs past the end of the image");
                Require(d[o + 1] == 0 && !(d[o + 25] & 0x80), "unsupported ISO file extent");
                if (d[o + 25] & 2) Walk(at, bytes, prefix + name + "/", depth + 1);
                else Require(files.emplace(prefix + name, IsoFile{at, bytes}).second, "ISO directory has duplicate file names");
            }
            o += n;
        }
    }
    Bytes File(const std::string& name, uint32_t limit = 256 * 1024 * 1024) {
        auto it = files.find(name);
        Require(it != files.end(), name + " is missing");
        Require(it->second.size <= limit, name + " is too large");
        return Read(it->second.sector, it->second.size);
    }
    void CheckSectors(const std::string& name) {
        if (stride == 2048) return;
        IsoFile f = files.at(name);
        Bytes r(stride);
        std::string err;
        static const auto table = [] {
            std::array<uint32_t, 256> t{};
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t v = i;
                for (int k = 0; k < 8; ++k) v = (v >> 1) ^ ((v & 1) ? 0xd8018001u : 0);
                t[i] = v;
            }
            return t;
        }();
        for (uint32_t i = 0; uint64_t(i) * 2048 < f.size; ++i) {
            uint32_t sector = f.sector + i;
            std::string where = name + " has a damaged sector " + std::to_string(sector) + ": ";
            Require(blob_->ReadAt(uint64_t(sector) * stride, r.data(), r.size(), err) == int64_t(r.size()), where + "truncated");
            Require(std::memcmp(r.data(), kSync, 12) == 0, where + "no sync");
            auto bcd = [](uint8_t x) { return (x >> 4) * 10 + (x & 15); };
            int address = (bcd(r[12]) * 60 + bcd(r[13])) * 75 + bcd(r[14]) - 150;
            Require(address == int64_t(sector), where + "wrong address");
            uint32_t edc = 0, have = 0;
            if (r[15] == 2) {
                Require(offset == 24, where + "sector mode changed");
                Require(std::memcmp(r.data() + 16, r.data() + 20, 4) == 0, where + "sub-header copies differ");
                Require(!(r[18] & 0x20), where + "Form 2 sector inside a data file");
                for (size_t k = 16; k < 2072; ++k) edc = (edc >> 8) ^ table[(edc ^ r[k]) & 255];
                have = U32(r, 2072);
            } else {
                Require(r[15] == 1 && offset == 16, where + "sector mode changed");
                for (size_t k = 0; k < 2064; ++k) edc = (edc >> 8) ^ table[(edc ^ r[k]) & 255];
                have = U32(r, 2064);
            }
            Require(edc == have, where + "EDC mismatch");
        }
    }
    uint32_t stride = 2048, offset = 0;
    Bytes primary;
    std::map<std::string, IsoFile> files;
private:
    std::shared_ptr<Blob> blob_;
    std::set<uint32_t> seen_;
};

std::vector<std::string> StringTable(const Bytes& b) {
    if (b.size() < 16) return {};
    uint32_t n = U32(b, 0);
    if (n < 16 || n > 20000 || uint64_t(4) + 4 * uint64_t(n) > b.size() || U32(b, 4) != 4 + 4 * n) return {};
    std::vector<std::string> out;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t a = U32(b, 4 + 4 * i), z = i + 1 == n ? uint32_t(b.size()) : U32(b, 8 + 4 * i);
        if (a > z || z > b.size()) return {};
        auto end = std::find(b.begin() + a, b.begin() + z, uint8_t(0));
        out.emplace_back(b.begin() + a, end);
    }
    return out;
}

size_t MeshFaces(const Bytes& b) {
    Require(b.size() >= 32 && b.size() <= 4 * 1024 * 1024 && U32(b, 0) == 0x96, "ride mesh is missing or oversized");
    uint32_t nsub = U32(b, 4), trailer = U32(b, 20), end = U32(b, 24), extra = U32(b, 28);
    uint64_t table = 32 + uint64_t(4) * extra;
    Require(nsub <= 1000 && table + 8 * uint64_t(nsub) <= b.size(), "ride mesh sub-table is damaged");
    size_t count = 0;
    for (uint32_t i = 0; i < nsub; ++i) {
        uint32_t o = U32(b, size_t(table + 8 * i));
        uint32_t e = i + 1 < nsub ? U32(b, size_t(table + 8 * (i + 1))) : (trailer ? trailer : end);
        Require(U32(b, size_t(table + 8 * i + 4)) == 0 && o <= e && e <= b.size() && e - o >= 72,
                "ride mesh sub-table is damaged");
        uint32_t nv = U32(b, o + 8);
        uint64_t q = uint64_t(o) + 72 + uint64_t(12) * nv;
        Require(q <= e, "ride mesh vertices are damaged");
        size_t prev = std::numeric_limits<size_t>::max();
        for (size_t pos = size_t(q); pos + 4 <= e;) {
            uint16_t n = U16(b, pos), st = U16(b, pos + 2);
            bool valid = n > 0 && n < 5000 && st < 0x4000 && uint64_t(pos) + 4 + uint64_t(14) * n <= e &&
                         (pos == prev || (pos >= 4 && U32(b, pos - 4) == 0));
            if (valid) for (size_t k = 0; k < n; ++k) {
                size_t p = pos + 4 + 14 * k;
                uint16_t a = U16(b, p), c = U16(b, p + 2), d = U16(b, p + 4);
                if (a >= nv || c >= nv || d >= nv || a == c || a == d || c == d) { valid = false; break; }
            }
            if (valid) { count += n; pos += 4 + 14 * n; prev = pos; }
            else pos += 2;
        }
    }
    return count;
}

void CheckAtlas(const Bytes& b) {
    Require(b.size() >= 16, "texture atlas header is damaged");
    uint64_t count = uint64_t(U16(b, 6)) * U16(b, 8) * 4;
    uint64_t start = 16 + uint64_t(12) * U16(b, 0) + uint64_t(8) * U16(b, 10);
    Require(count > 0 && count <= 256 && start + 8192 <= b.size(), "texture atlas header is damaged");
    size_t p = size_t(start + 8192);
    for (uint64_t block = 1; block < count; ++block) {
        size_t have = 0;
        bool stop = false;
        while (p < b.size()) {
            uint8_t a = b[p++];
            if (a < 128) {
                size_t n = size_t(a) + 1;
                Require(n <= b.size() - p && have + n <= 8192, "texture atlas literal is damaged");
                have += n;
                p += n;
            } else {
                Require(p < b.size(), "texture atlas stream is truncated");
                uint8_t n = b[p++];
                if (!n) { stop = true; break; }
                Require(size_t(256 - a) <= have && have + n <= 8192, "texture atlas back-reference is damaged");
                have += n;
            }
        }
        Require(stop && have == 8192, "texture atlas does not decode");
    }
}

using Frame = std::array<uint32_t, 43>;
std::vector<Frame> ThemeFrames(const Bytes& d) {
    std::vector<uint32_t> w;
    for (size_t p = 0; p + 4 <= d.size(); p += 4) w.push_back(U32(d, p));
    std::vector<size_t> gpLoads, sites;
    for (size_t i = 0; i < w.size(); ++i) {
        if (i + 1 < w.size() && w[i] >> 16 == 0x3c1c && w[i + 1] >> 16 == 0x279c) gpLoads.push_back(i);
        if (w[i] == kCopy) sites.push_back(i);
    }
    Require(gpLoads.size() == 1, "expected one gp load in theme tables");
    std::vector<size_t> clusters;
    for (size_t i = 0; i + 7 < sites.size(); ++i) if (sites[i + 7] - sites[i] <= 0x1000 / 4) clusters.push_back(i);
    Require(clusters.size() == 1, "expected one group of eight theme table copies");
    size_t start = sites[clusters[0]], end = sites[clusters[0] + 7];
    while (start > 0 && !(w[start] >> 16 == 0x27bd && (w[start] & 0x8000))) --start;
    while (end < w.size() && w[end] != 0x03e00008) ++end;
    Require(start != 0 && end + 1 < w.size(), "theme table builder bounds not found");
    Require(end - start <= 16384, "theme table builder is too large");
    std::map<uint32_t, size_t> calls;
    for (size_t i = start; i < end + 2; ++i) if (w[i] >> 26 == 3) ++calls[w[i]];
    uint32_t zero = 0;
    size_t highest = 0;
    for (size_t i = start; i < end + 2; ++i) if (calls[w[i]] > highest) { highest = calls[w[i]]; zero = w[i]; }
    std::array<uint32_t, 32> reg{};
    size_t gp = gpLoads[0];
    reg[28] = ((w[gp] & 65535) << 16) + S16(w[gp + 1]);
    reg[29] = 0x80200000;
    std::map<uint32_t, uint32_t> mem;
    std::vector<Frame> out;
    for (size_t i = start; i < end + 2; ++i) {
        uint32_t ins = w[i], op = ins >> 26, rs = (ins >> 21) & 31, rt = (ins >> 16) & 31, rd = (ins >> 11) & 31;
        uint32_t imm = ins & 65535, a = reg[rs] + S16(imm);
        if (ins == kCopy) {
            Frame frame{};
            for (size_t k = 0; k < frame.size(); ++k) frame[k] = mem[reg[29] + 16 + uint32_t(4 * k)];
            out.push_back(frame);
        }
        if (op == 15) reg[rt] = imm << 16;
        else if (op == 9) reg[rt] = a;
        else if (op == 13) reg[rt] = reg[rs] | imm;
        else if (op == 0x23) {
            auto it = mem.find(a);
            reg[rt] = it != mem.end() ? it->second : (a >= kBase && uint64_t(a - kBase) + 4 <= d.size() ? U32(d, (a - kBase) & ~3u) : 0);
        } else if (op == 0x2b) mem[a] = reg[rt];
        else if (op == 0 && (ins & 63) == 0x21) reg[rd] = reg[rs] + reg[rt];
        else if (ins == zero) for (uint32_t k = 0; k < 4 * 43; k += 4) mem[reg[4] + k] = 0;
    }
    return out;
}

void FindTables(Ps1Disc& disc, const Bytes& binary) {
    std::vector<std::string> ids;
    size_t matches = 0;
    for (const auto& b : disc.entries) {
        auto t = StringTable(b);
        size_t symbolic = 0;
        for (const auto& s : t) if (s.compare(0, 4, "STR_") == 0) ++symbolic;
        if (symbolic > t.size() / 2) { ++matches; ids = std::move(t); }
    }
    Require(matches == 1, "expected one symbolic string-ID table");
    const std::regex ridePattern("STR_GRAPHICS_([A-Z]+)_RIDES_([A-Z0-9_]+?)_\\2 *");
    std::map<std::string, std::vector<size_t>> rides;
    for (size_t e = 0; e < disc.entries.size(); ++e) {
        const Bytes& b = disc.entries[e];
        if (b.size() < 32 || U32(b, 0) != 0x96) continue;
        uint32_t trailer = U32(b, 20);
        if (!trailer || uint64_t(trailer) + 8 > b.size()) continue;
        uint32_t id = U32(b, trailer + 4);
        std::smatch m;
        if (id < ids.size() && std::regex_match(ids[id], m, ridePattern)) rides[m[1].str() + "/" + m[2].str()].push_back(e);
    }
    const std::map<std::string, size_t> targets = {{"HALLOW/JACK", 672}, {"HALLOW/JUMPER", 260}, {"HALLOW/SLIDE", 275},
        {"HALLOW/OBVER", 202}, {"JUNGLE/BUNGIE", 352}, {"JUNGLE/INCAPOT", 268}, {"JUNGLE/DIPLODIP", 286},
        {"SPACE/JUMPER", 276}, {"FANTASY/SNAIL", 254}, {"FANTASY/FLYFISH", 305}};
    for (const auto& target : targets) {
        auto it = rides.find(target.first);
        Require(it != rides.end() && it->second.size() == 1, "ride " + target.first + " was not found exactly once");
        Require(MeshFaces(disc.entries[it->second[0]]) == target.second, "ride " + target.first + " has an unexpected face count");
    }
    auto frames = ThemeFrames(binary);
    std::set<std::array<size_t, 3>> themes;
    std::map<size_t, std::string> targetByEntry;
    for (const auto& target : targets) targetByEntry[rides.at(target.first)[0]] = target.first;
    std::map<std::string, std::set<std::array<size_t, 3>>> listed;
    for (const Frame& frame : frames) {
        std::array<size_t, 3> atl = {frame[40], frame[41], frame[42]};
        if (!themes.insert(atl).second) continue;
        for (size_t e : atl) {
            Require(e < disc.entries.size(), "theme atlas index runs past FOLIO directory");
            CheckAtlas(disc.entries[e]);
        }
        disc.themes.push_back(atl);
        for (size_t row = 0; row < 8; ++row) for (size_t side = 0; side < 2; ++side) {
            uint32_t p = frame[2 + 4 * row + side], n = frame[4 + 4 * row + side];
            if (p < kBase || uint64_t(p - kBase) >= binary.size() || (p & 3) || n > 500) continue;
            Require(uint64_t(p - kBase) + uint64_t(4) * n <= binary.size(), "theme ride list runs past binary");
            for (uint32_t i = 0; i < n; ++i) {
                size_t e = U32(binary, p - kBase + 4 * i);
                auto it = targetByEntry.find(e);
                if (it != targetByEntry.end()) listed[it->second].insert({e, atl[0], atl[1 + side]});
            }
        }
    }
    Require(themes.size() == 4, "expected four theme tables");
    // Tower is scenery, absent from the game's ride lists. Its atlas is the other Halloween ride atlas from the
    // one the listed clown uses. The theme's own builder supplies both entry numbers on every disc version.
    Require(listed["HALLOW/JACK"].size() == 1, "clown ride has no unique texture atlas");
    const auto& clown = *listed["HALLOW/JACK"].begin();
    for (const auto& atl : themes) if (atl[0] == clown[1] && atl[1] == clown[2])
        listed["HALLOW/OBVER"].insert({rides.at("HALLOW/OBVER")[0], atl[0], atl[2]});
    for (const auto& target : targets) {
        const auto& choices = listed[target.first];
        Require(choices.size() == 1, "ride " + target.first + " has no unique texture atlas");
        const auto& found = *choices.begin();
        disc.rides[target.first] = Ps1RideEntry{found[0], found[1], found[2]};
    }
}

}  // namespace

bool ReadPs1Folio(const std::vector<uint8_t>& bytes, std::vector<std::vector<uint8_t>>& entries, std::string& reason) {
    entries.clear();
    try {
        uint32_t n = U32(bytes, 0), second = U32(bytes, 4);
        uint64_t tableEnd = 8 + uint64_t(8) * n + uint64_t(12) * second;
        Require(n >= 300 && n <= 1000 && tableEnd <= 4096 && tableEnd <= bytes.size(), "FOLIO.GAZ directory is not the expected shape");
        uint64_t last = tableEnd;
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t o = U32(bytes, 8 + 8 * i), size = U32(bytes, 12 + 8 * i);
            Require(o >= last && uint64_t(o) + size <= bytes.size() && o % 2048 == 0,
                    "FOLIO.GAZ directory entry is out of order or out of bounds");
            last = uint64_t(o) + size;
        }
        entries.reserve(n);
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t o = U32(bytes, 8 + 8 * i), size = U32(bytes, 12 + 8 * i);
            entries.emplace_back(bytes.begin() + o, bytes.begin() + o + size);
        }
        reason.clear();
        return true;
    } catch (const std::exception& e) { entries.clear(); reason = e.what(); return false; }
}

Ps1Status RecognisePs1(std::shared_ptr<Blob> blob, Ps1Disc& disc, std::string& reason) {
    disc = Ps1Disc{};
    reason.clear();
    if (!blob) return Ps1Status::NotPlayStation;
    bool playstation = false;
    try {
        DiscReader reader(std::move(blob));
        if (!reader.Init()) return Ps1Status::NotPlayStation;
        std::string system(reinterpret_cast<const char*>(reader.primary.data() + 8), 32);
        if (Trim(system) != "PLAYSTATION") return Ps1Status::NotPlayStation;
        playstation = true;
        Require(reader.Sectors() >= U32(reader.primary, 80), "image is truncated: shorter than its volume");
        Require(U16(reader.primary, 128) == 2048, "ISO logical block size is not 2048");
        reader.Walk(U32(reader.primary, 158), U32(reader.primary, 166), "");
        Bytes cnf = reader.File("SYSTEM.CNF", 65536);
        std::string boot;
        std::istringstream lines(std::string(cnf.begin(), cnf.end()));
        std::string line;
        while (std::getline(lines, line)) {
            size_t eq = line.find('=');
            if (eq != std::string::npos && Upper(Trim(line.substr(0, eq))) == "BOOT") {
                boot = Upper(Trim(line.substr(eq + 1)));
                size_t slash = boot.find_last_of('\\');
                if (slash != std::string::npos) boot.erase(0, slash + 1);
                boot.resize(boot.find(';') == std::string::npos ? boot.size() : boot.find(';'));
                break;
            }
        }
        const std::map<std::string, std::string> regions = {{"SLUS_010.69", "USA"}, {"SLES_026.88", "Europe"}, {"SLPS_026.43", "Japan"}};
        auto region = regions.find(boot);
        Require(region != regions.end(), "SYSTEM.CNF does not name a supported PlayStation version of this game");
        Bytes exe = reader.File(boot, 16 * 1024 * 1024);
        Require(exe.size() >= 8 && std::memcmp(exe.data(), "PS-X EXE", 8) == 0, boot + " is not a PS1 executable");
        for (const std::string name : {"FOLIO.GAZ", "TPW.BIN", "TPW.OVL"}) Require(reader.files.count(name) == 1, name + " is missing");
        reader.CheckSectors("FOLIO.GAZ");
        reader.CheckSectors("TPW.BIN");
        Ps1Disc found;
        found.boot = boot;
        found.region = region->second;
        Require(ReadPs1Folio(reader.File("FOLIO.GAZ"), found.entries, reason), reason);
        FindTables(found, reader.File("TPW.BIN", 16 * 1024 * 1024));
        disc = std::move(found);
        return Ps1Status::Supported;
    } catch (const std::exception& e) {
        reason = e.what();
        for (char& c : reason) if (c == '\r' || c == '\n') c = ' ';
        return playstation ? Ps1Status::Rejected : Ps1Status::NotPlayStation;
    }
}

}  // namespace tpw::assets

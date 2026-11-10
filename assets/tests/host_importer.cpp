// Host tests of the importer (assets/importer.cpp). Synthetic drops always run (empty files matched by their path, a
// different version, unknown files, a repeated drop, launcher.ini, removing an edition, the staging folder, refusals).
// With the player's own discs they import for real and compare the trees with devtools/import_originals.py's CHECKSUMS
// (US), import_eur.py's EXPECTED (European) and the European folder under TPW_GDI_ROOT/game-eur; the discs are read in
// place and the output trees are deleted at the end. A test whose input is absent is skipped with a message.
// 7-Zip and RAR archives: synthetic fixtures (tests/archives) and RAR written in memory, always.
// The language packs: synthetic drops always (an empty file matched by its language folder, the status lines); with a
// language disc (TPW_TEST_LANGUAGE_DISC and TPW_TEST_LANGUAGE) a real import that must make that language available.
// With a PlayStation disc and an imported PC tree (TPW_TEST_PS1 and TPW_TEST_PS1_DONORS) the rides are converted and
// every ride must have a ride camera that is upright and a proper rotation.
//
// environment: TPW_GDI_ROOT (the ROOT folder), TPW_TEST_TRANSFER (the disc archive folder), TPW_TEST_EXPECT (the folder
// with allitems.exe), TPW_TEST_SOURCES (the file naming the discs under TPW_TEST_TRANSFER; sources.txt there by default),
// TPW_TEST_SCRATCH (where the output trees go; the disc tests need about 3 GB there),
// TPW_TEST_LANGUAGE_DISC (a disc, image or folder with a language on it) and TPW_TEST_LANGUAGE (that language, as
// ManifestLanguages() spells it; several separated by commas), TPW_TEST_PS1 (the PlayStation disc's .bin or .iso) and
// TPW_TEST_PS1_DONORS (a data root with the US or European tree imported; its edition folders are linked, not written)
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <thread>
#include <zlib.h>

#include "../icon.h"
#include "../gold_ride_table.h"
#include "../importer.h"
#include "../sam_check.h"
#include "../sha256.h"
#include "../sniff.h"
#include "../ps1_disc.h"
#include "../ps1_convert.h"
#include "../source.h"
#include "test_archives.h"

#ifndef TPW_DEVTOOLS_DIR
#define TPW_DEVTOOLS_DIR "devtools"
#endif

using namespace tpw::assets;
namespace fs = std::filesystem;

namespace {

int g_fail = 0, g_pass = 0, g_skip = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (cond) {                                                                    \
            g_pass++;                                                                  \
        } else {                                                                       \
            g_fail++;                                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                \
        }                                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                                                           \
    do {                                                                                                         \
        unsigned long long va = static_cast<unsigned long long>(a), vb = static_cast<unsigned long long>(b);      \
        if (va == vb) {                                                                                          \
            g_pass++;                                                                                            \
        } else {                                                                                                 \
            g_fail++;                                                                                            \
            std::printf("FAIL %s:%d: %s = %llu, expected %s = %llu\n", __FILE__, __LINE__, #a, va, #b, vb);      \
        }                                                                                                        \
    } while (0)

void Skip(const std::string& what) {
    g_skip++;
    std::printf("SKIP %s\n", what.c_str());
}

std::string Env(const char* name) {
    const char* v = std::getenv(name);
    return v && *v ? v : "";
}

// The player's own discs are named in a file of theirs, not here: TPW_TEST_SOURCES, or sources.txt in TPW_TEST_TRANSFER, one
// "key = path under TPW_TEST_TRANSFER" per line (keys: mac-zip, eur-a, eur-first, gold-zip, gold-rides (a download of the 35 Gold rides without RideConfig.txt), and for the Windows-disc
// editions pc-us, the US 2.0 Windows CD, pc-us-first, the US first release 1.0, and pc-eur, a European Windows-only CD, such as
// the English one sold as Sim Theme Park, volume SIM_THEME_PARK: its files are the European edition's; ps1, the US
// PlayStation disc's .bin, converted after one of the PC sources). A missing key skips its test.
std::string SourceFor(const std::string& transfer, const std::string& key) {
    std::string file = Env("TPW_TEST_SOURCES");
    if (file.empty()) file = transfer + "/sources.txt";
    std::ifstream in(file);
    std::string line;
    auto trim = [](std::string s) {
        size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
        return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
    };
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos || trim(line.substr(0, eq)) != key) continue;
        std::string rel = trim(line.substr(eq + 1));
        return rel.empty() ? rel : transfer + "/" + rel;
    }
    return "";
}

bool Exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

void WriteFile(const std::string& path, const std::string& bytes) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream(path, std::ios::binary) << bytes;
}

std::string ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string HashFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "unreadable";
    Sha256 sha;
    std::vector<char> buf(1 << 20);
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0) sha.Update(buf.data(), n);
    std::fclose(f);
    return sha.Final().Hex();
}

struct FileInfo {
    uint64_t size = 0;
    std::string sha;
};

// Every file under root: relative path -> size and, when hash is set, SHA-256.
std::map<std::string, FileInfo> Tree(const std::string& root, bool hash) {
    std::map<std::string, FileInfo> out;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return out;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
        if (ec || !it->is_regular_file()) continue;
        FileInfo info;
        info.size = it->file_size();
        if (hash) info.sha = HashFile(it->path().string());
        out[it->path().lexically_relative(root).generic_string()] = info;
    }
    return out;
}

bool IsManifestSize(uint64_t size) {
    for (size_t i = 0; i < ManifestHashCount(); ++i) {
        if (ManifestHashes()[i].size == size) return true;
    }
    return false;
}

// A junk string whose length no manifest content has.
std::string Junk(uint64_t from) {
    while (IsManifestSize(from)) ++from;
    return std::string(static_cast<size_t>(from), 'j');
}

// Drains the queue: (events, true when the last one is Finished).
std::vector<ImportProgress> Events(Importer& imp) { return imp.Progress().Drain(); }

void RemoveTree(const std::string& path) {
    std::error_code ec;
    fs::remove_all(path, ec);
}

// ---- the Python lists, read as text (the same reading as tests/host_manifest.cpp)

bool ListText(const std::string& py, const char* name, std::string* out) {
    std::string key = std::string("\n") + name + " = \"\"\"";
    size_t at = py.find(key);
    if (at == std::string::npos) return false;
    size_t start = at + key.size();
    size_t end = py.find("\"\"\"", start);
    if (end == std::string::npos) return false;
    *out = py.substr(start, end - start);
    return true;
}

std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) out.push_back(line);
    return out;
}

bool StartsWith(const std::string& s, const std::string& p) { return s.compare(0, p.size(), p) == 0; }
bool EndsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}

// The US game folder import_originals.py makes, as far as the manifest holds it: relative path -> size and SHA-256.
bool UsExpected(std::map<std::string, FileInfo>& out) {
    std::string py = ReadText(std::string(TPW_DEVTOOLS_DIR) + "/import_originals.py"), list;
    if (!ListText(py, "CHECKSUMS", &list)) return false;
    const std::string game = "disc-hfs/Theme Park Data/";
    std::string folder;
    for (const std::string& line : Lines(list)) {
        if (line.empty()) continue;
        if (StartsWith(line, "= ")) {
            folder = line.substr(2);
            continue;
        }
        size_t a = line.find(' '), b = line.find(' ', a + 1);
        if (a != 64 || b == std::string::npos) return false;
        std::string path = folder + line.substr(b + 1);
        if (!StartsWith(path, game)) continue;
        std::string rel = path.substr(game.size());
        bool font_fork = StartsWith(rel, "data/fonts/") && EndsWith(rel, ".rsrc");
        bool side = EndsWith(rel, ".finfo") || (EndsWith(rel, ".rsrc") && !font_fork);
        bool icon = rel.find("Icon\\r") != std::string::npos;
        if (rel != "SimTheme Park" && (!StartsWith(rel, "data/") || side || icon)) continue;
        FileInfo info;
        info.size = std::strtoull(line.substr(a + 1, b - a - 1).c_str(), nullptr, 10);
        info.sha = line.substr(0, 64);
        out[rel] = info;
    }
    return !out.empty();
}

// import_eur.py's EXPECTED: relative path -> SHA-256.
bool EurExpected(std::map<std::string, std::string>& out) {
    std::string py = ReadText(std::string(TPW_DEVTOOLS_DIR) + "/import_eur.py"), list;
    if (!ListText(py, "EXPECTED", &list)) return false;
    for (const std::string& line : Lines(list)) {
        if (line.size() > 66 && line[64] == ' ' && line[65] == ' ') out[line.substr(66)] = line.substr(0, 64);
    }
    return !out.empty();
}

size_t CountKind(const std::vector<ImportProgress>& ev, ImportProgress::Kind kind) {
    size_t n = 0;
    for (const ImportProgress& e : ev) n += e.kind == kind;
    return n;
}

void PrintStatus(const char* what, const ImportReport& r) {
    std::printf("  %s: %s\n", what, r.Summary().c_str());
    for (int i : {0, 1, 2, 4, 5}) {
        const EditionStatus& s = r.status[i];
        std::printf("    %-6s %-8s records %zu/%zu required %zu/%zu files %zu/%zu missing mac %zu pc %zu gold %zu\n",
                    EditionName(s.edition), s.StateWord(), s.present, s.records, s.requiredPresent, s.required,
                    s.filesPresent, s.files, s.missing[kSourceMacCd], s.missing[kSourcePcCd], s.missing[kSourceGoldCd]);
    }
}

// ---- synthetic drops

// A complete tiny PlayStation image made here: repeated triangles, zero texture pages, symbolic names and a table
// builder made of constant stores. It exercises discovery without copying any bytes from a player's disc.
void Ps1Put(std::vector<uint8_t>& b, size_t p, uint32_t v, size_t bytes = 4) {
    if (b.size() < p + bytes) b.resize(p + bytes);
    for (size_t i = 0; i < bytes; ++i) b[p + i] = uint8_t(v >> (8 * i));
}

std::vector<uint8_t> Ps1FolioFixture() {
    std::vector<std::vector<uint8_t>> entries(300);
    const char* names[] = {"HALLOW_RIDES_JACK_JACK", "HALLOW_RIDES_JUMPER_JUMPER", "HALLOW_RIDES_SLIDE_SLIDE",
        "HALLOW_RIDES_OBVER_OBVER", "JUNGLE_RIDES_BUNGIE_BUNGIE", "JUNGLE_RIDES_INCAPOT_INCAPOT",
        "JUNGLE_RIDES_DIPLODIP_DIPLODIP", "SPACE_RIDES_JUMPER_JUMPER", "FANTASY_RIDES_SNAIL_SNAIL",
        "FANTASY_RIDES_FLYFISH_FLYFISH"};
    const size_t faces[] = {672, 260, 275, 202, 352, 268, 286, 276, 254, 305};
    for (size_t i = 0; i < 10; ++i) {
        auto& mesh = entries[i];
        size_t trailer = 152 + 14 * faces[i];
        mesh.resize(trailer + 8);
        Ps1Put(mesh, 0, 0x96); Ps1Put(mesh, 4, 1); Ps1Put(mesh, 20, uint32_t(trailer));
        Ps1Put(mesh, 24, uint32_t(trailer)); Ps1Put(mesh, 32, 40); Ps1Put(mesh, 48, 3);
        Ps1Put(mesh, 148, uint32_t(faces[i]), 2);
        for (size_t f = 0; f < faces[i]; ++f) { Ps1Put(mesh, 152 + 14 * f + 2, 1, 2); Ps1Put(mesh, 152 + 14 * f + 4, 2, 2); }
        Ps1Put(mesh, trailer, 3); Ps1Put(mesh, trailer + 4, uint32_t(i));
    }
    auto& ids = entries[10];
    ids.resize(4 + 16 * 4);
    Ps1Put(ids, 0, 16);
    for (size_t i = 0; i < 16; ++i) {
        Ps1Put(ids, 4 + 4 * i, uint32_t(ids.size()));
        std::string s = i < 10 ? std::string("STR_GRAPHICS_") + names[i] : "STR_TEST_" + std::to_string(i);
        ids.insert(ids.end(), s.begin(), s.end()); ids.push_back(0);
    }
    std::vector<uint8_t> atlas(16 + 8192);
    Ps1Put(atlas, 6, 1, 2); Ps1Put(atlas, 8, 1, 2);
    for (size_t k = 0; k < 3; ++k) {
        atlas.push_back(0); atlas.push_back(0);
        size_t left = 8191;
        while (left) { size_t n = std::min<size_t>(left, 255); atlas.push_back(255); atlas.push_back(uint8_t(n)); left -= n; }
        atlas.push_back(128); atlas.push_back(0);
    }
    for (size_t i = 11; i < 23; ++i) entries[i] = atlas;
    std::vector<uint8_t> folio(4096);
    Ps1Put(folio, 0, 300);
    for (size_t i = 0; i < entries.size(); ++i) {
        Ps1Put(folio, 8 + 8 * i, uint32_t(folio.size()));
        Ps1Put(folio, 12 + 8 * i, uint32_t(entries[i].size()));
        folio.insert(folio.end(), entries[i].begin(), entries[i].end());
        folio.resize((folio.size() + 2047) / 2048 * 2048);
    }
    return folio;
}

std::vector<uint8_t> Ps1BinaryFixture() {
    std::vector<uint32_t> w = {0x3c1c8001, 0x279c0000, 0x27bdff00};
    for (uint32_t i = 0; i < 43; ++i) w.push_back(0xafa00000 | (16 + 4 * i));
    auto field = [&](uint32_t slot, uint32_t value) {
        w.push_back(0x3c080000 | (value >> 16));
        w.push_back(0x35080000 | (value & 65535));
        w.push_back(0xafa80000 | (16 + 4 * slot));
    };
    const std::vector<uint32_t> lists[4][2] = {{{0, 1, 2}, {}}, {{4, 5}, {6}}, {{7}, {}}, {{9}, {8}}};
    std::vector<uint8_t> b(8192);
    for (uint32_t theme = 0; theme < 4; ++theme) {
        for (uint32_t side = 0; side < 2; ++side) {
            uint32_t at = 4096 + 128 * theme + 64 * side;
            for (size_t i = 0; i < lists[theme][side].size(); ++i) Ps1Put(b, at + 4 * i, lists[theme][side][i]);
        }
        for (size_t duplicate = 0; duplicate < 2; ++duplicate) {
            field(2, 0x80010000 + 4096 + 128 * theme); field(3, 0x80010000 + 4096 + 128 * theme + 64);
            field(4, uint32_t(lists[theme][0].size())); field(5, uint32_t(lists[theme][1].size()));
            for (uint32_t a = 0; a < 3; ++a) field(40 + a, 11 + 3 * theme + a);
            w.push_back(0x8daf0000);
        }
    }
    w.push_back(0x03e00008); w.push_back(0);
    CHECK(w.size() * 4 < 4096);
    for (size_t i = 0; i < w.size(); ++i) Ps1Put(b, i * 4, w[i]);
    return b;
}

std::vector<uint8_t> Ps1ImageFixture(const std::string& boot = "SLUS_010.69") {
    struct File { std::string name; std::vector<uint8_t> bytes; uint32_t sector; };
    std::string cnf = "BOOT=cdrom:\\" + boot + ";1\r\n";
    std::vector<File> files = {{"SYSTEM.CNF", {cnf.begin(), cnf.end()}, 0},
        {boot, {'P', 'S', '-', 'X', ' ', 'E', 'X', 'E'}, 0}, {"FOLIO.GAZ", Ps1FolioFixture(), 0},
        {"TPW.BIN", Ps1BinaryFixture(), 0}, {"TPW.OVL", {0}, 0}};
    uint32_t sector = 21;
    for (auto& f : files) { f.sector = sector; sector += uint32_t((f.bytes.size() + 2047) / 2048); }
    std::vector<uint8_t> image(size_t(sector) * 2048);
    size_t pvd = 16 * 2048;
    image[pvd] = 1; std::memcpy(image.data() + pvd + 1, "CD001", 5);
    std::fill(image.begin() + pvd + 8, image.begin() + pvd + 40, ' ');
    std::memcpy(image.data() + pvd + 8, "PLAYSTATION", 11);
    Ps1Put(image, pvd + 80, sector); Ps1Put(image, pvd + 128, 2048, 2);
    auto record = [&](size_t p, const std::string& name, uint32_t at, uint32_t bytes, uint8_t flags) {
        size_t n = 33 + name.size() + (name.size() % 2 == 0);
        image[p] = uint8_t(n); Ps1Put(image, p + 2, at); Ps1Put(image, p + 10, bytes);
        image[p + 25] = flags; image[p + 32] = uint8_t(name.size()); std::memcpy(image.data() + p + 33, name.data(), name.size());
        return n;
    };
    record(pvd + 156, std::string(1, '\0'), 20, 2048, 2);
    size_t pos = 20 * 2048;
    pos += record(pos, std::string(1, '\0'), 20, 2048, 2);
    pos += record(pos, std::string(1, '\1'), 20, 2048, 2);
    for (const auto& f : files) {
        pos += record(pos, f.name + ";1", f.sector, uint32_t(f.bytes.size()), 0);
        std::copy(f.bytes.begin(), f.bytes.end(), image.begin() + size_t(f.sector) * 2048);
    }
    return image;
}

std::vector<uint8_t> Ps1RawFixture(const std::vector<uint8_t>& cooked, size_t stride) {
    std::vector<uint8_t> raw(cooked.size() / 2048 * stride);
    auto bcd = [](size_t x) { return uint8_t((x / 10) * 16 + x % 10); };
    for (size_t sector = 0; sector < cooked.size() / 2048; ++sector) {
        size_t p = sector * stride, address = sector + 150;
        for (size_t k = 1; k < 11; ++k) raw[p + k] = 255;
        raw[p + 12] = bcd(address / (60 * 75)); raw[p + 13] = bcd(address / 75 % 60);
        raw[p + 14] = bcd(address % 75); raw[p + 15] = 2;
        std::copy(cooked.begin() + sector * 2048, cooked.begin() + (sector + 1) * 2048, raw.begin() + p + 24);
        uint32_t edc = 0;
        for (size_t k = p + 16; k < p + 2072; ++k) {
            edc ^= raw[k];
            for (size_t bit = 0; bit < 8; ++bit) edc = (edc >> 1) ^ ((edc & 1) ? 0xd8018001u : 0);
        }
        Ps1Put(raw, p + 2072, edc);
    }
    return raw;
}

void TestPlayStation(const std::string& scratch) {
    std::printf("PlayStation recognition and refusals\n");
    Ps1Disc disc;
    std::string why;
    auto image = Ps1ImageFixture();
    CHECK(RecognisePs1(MakeMemoryBlob(image), disc, why) == Ps1Status::Supported);
    CHECK(disc.region == "USA" && disc.boot == "SLUS_010.69");
    CHECK_EQ(disc.entries.size(), 300); CHECK_EQ(disc.rides.size(), 10); CHECK_EQ(disc.themes.size(), 4);
    CHECK_EQ(disc.rides.at("HALLOW/OBVER").entry, 3);
    CHECK_EQ(disc.rides.at("HALLOW/OBVER").rideAtlas, 13);
    CHECK(RecognisePs1(MakeMemoryBlob(Ps1ImageFixture("SLES_026.88")), disc, why) == Ps1Status::Supported);
    CHECK(disc.region == "Europe");
    CHECK(RecognisePs1(MakeMemoryBlob(Ps1ImageFixture("SLPS_026.43")), disc, why) == Ps1Status::Supported);
    CHECK(disc.region == "Japan");
    auto raw = Ps1RawFixture(image, 2352);
    CHECK(RecognisePs1(MakeMemoryBlob(raw), disc, why) == Ps1Status::Supported);
    CHECK(RecognisePs1(MakeMemoryBlob(Ps1RawFixture(image, 2448)), disc, why) == Ps1Status::Supported);
    raw[23 * 2352 + 24 + 500] ^= 1;
    CHECK(RecognisePs1(MakeMemoryBlob(raw), disc, why) == Ps1Status::Rejected);
    CHECK(why.find("EDC mismatch") != std::string::npos);
    raw = Ps1RawFixture(image, 2352); raw[23 * 2352 + 12] ^= 1;
    CHECK(RecognisePs1(MakeMemoryBlob(raw), disc, why) == Ps1Status::Rejected);
    CHECK(why.find("wrong address") != std::string::npos);
    raw = Ps1RawFixture(image, 2352); raw[23 * 2352] = 1;
    CHECK(RecognisePs1(MakeMemoryBlob(raw), disc, why) == Ps1Status::Rejected);
    CHECK(why.find("no sync") != std::string::npos);
    raw = Ps1RawFixture(image, 2352); raw[23 * 2352 + 16] = 1;
    CHECK(RecognisePs1(MakeMemoryBlob(raw), disc, why) == Ps1Status::Rejected);
    CHECK(why.find("sub-header copies differ") != std::string::npos);
    raw = Ps1RawFixture(image, 2352); raw[23 * 2352 + 18] = raw[23 * 2352 + 22] = 0x20;
    CHECK(RecognisePs1(MakeMemoryBlob(raw), disc, why) == Ps1Status::Rejected);
    CHECK(why.find("Form 2") != std::string::npos);
    CHECK(RecognisePs1(MakeMemoryBlob(Ps1ImageFixture("SLUS_999.99")), disc, why) == Ps1Status::Rejected);
    CHECK(why.find("supported PlayStation") != std::string::npos);
    CHECK(RecognisePs1(MakeMemoryBlob({1, 2, 3}), disc, why) == Ps1Status::NotPlayStation);
    image[16 * 2048 + 8] = 'X';
    CHECK(RecognisePs1(MakeMemoryBlob(image), disc, why) == Ps1Status::NotPlayStation);
    image = Ps1ImageFixture(); image.resize(image.size() - 2048);
    CHECK(RecognisePs1(MakeMemoryBlob(image), disc, why) == Ps1Status::Rejected);
    CHECK(why.find("truncated") != std::string::npos);
    image = Ps1ImageFixture(); image[22 * 2048] = 0;
    CHECK(RecognisePs1(MakeMemoryBlob(image), disc, why) == Ps1Status::Rejected);
    CHECK(why.find("not a PS1 executable") != std::string::npos);
    image = Ps1ImageFixture(); Ps1Put(image, 23 * 2048 + 8, 1);
    CHECK(RecognisePs1(MakeMemoryBlob(image), disc, why) == Ps1Status::Rejected);
    CHECK(why.find("directory entry") != std::string::npos);
    auto folio = Ps1FolioFixture();
    std::vector<std::vector<uint8_t>> entries;
    CHECK(ReadPs1Folio(folio, entries, why)); CHECK_EQ(entries.size(), 300);
    Ps1Put(folio, 16, 0);
    CHECK(!ReadPs1Folio(folio, entries, why)); CHECK(entries.empty());
    CHECK(!ReadPs1Folio({}, entries, why));
    folio = Ps1FolioFixture(); Ps1Put(folio, 4, 1000);
    CHECK(!ReadPs1Folio(folio, entries, why));
    folio = Ps1FolioFixture(); Ps1Put(folio, 12, uint32_t(folio.size()));
    CHECK(!ReadPs1Folio(folio, entries, why));

    std::string root = scratch + "/ps1-data", drop = scratch + "/ps1-drop";
    ImporterConfig cfg; cfg.dataRoot = root;
    Importer imp(cfg); CHECK(imp.Open(why));
    std::string imagePath = drop + "/disc.iso";
    auto good = Ps1ImageFixture();
    WriteFile(imagePath, std::string(good.begin(), good.end()));
    auto missingDonors = imp.Import({imagePath});
    CHECK(!missingDonors.playStationConverted);
    CHECK_EQ(missingDonors.playStation.size(), 1);
    CHECK(missingDonors.playStation[0].find("drop your US or European PC disc") != std::string::npos);
    CHECK(!Exists(root + "/assets/ps1"));
    good.resize(good.size() - 2048);
    WriteFile(imagePath, std::string(good.begin(), good.end()));
    auto refused = imp.Import({imagePath});
    CHECK_EQ(refused.errors.size(), 1);
    CHECK(refused.errors[0].find("truncated") != std::string::npos);
    CHECK(!refused.playStationConverted && !Exists(root + "/assets/ps1"));
    WriteFile(root + "/assets/ps1/data/levels/hallow/rides/_test_201/_test.sam", "preserved");
    for (Edition edition : {kEditionUs, kEditionEur, kEditionGold, kEditionLanguages}) imp.VerifyEdition(edition, why);
    CHECK(ReadText(root + "/assets/ps1/data/levels/hallow/rides/_test_201/_test.sam") == "preserved");
    for (const std::string ext : {"chd", "ecm", "pbp"}) {
        std::string file = drop + "/unsupported." + ext;
        WriteFile(file, "unsupported");
        auto report = imp.Import({file});
        CHECK_EQ(report.errors.size(), 1);
        CHECK(report.errors[0].find(".cue/.bin") != std::string::npos);
    }
    std::string staged = root + "/assets/.staging/ps1-atomic", destination = root + "/assets/ps1/atomic";
    WriteFile(staged + "/first", "first complete file"); WriteFile(staged + "/second", "second complete file");
    CHECK(InstallPs1Ride(staged, destination, why));
    CHECK(!Exists(staged));
    CHECK(ReadText(destination + "/first") == "first complete file" && ReadText(destination + "/second") == "second complete file");
    CHECK(!InstallPs1Ride(staged, destination, why));
    CHECK(ReadText(destination + "/first") == "first complete file");
    WriteFile(staged + "/replacement", "complete replacement");
    CHECK(InstallPs1Ride(staged, destination, why));
    CHECK(!Exists(destination + "/first") && ReadText(destination + "/replacement") == "complete replacement");
    std::string backup = root + "/assets/ps1/.ps1-previous-atomic";
    CHECK(!Exists(backup));
    // An interruption after moving the old ride aside: the next import restores it before replacing it.
    fs::rename(destination, backup);
    WriteFile(staged + "/recovered", "complete recovered replacement");
    CHECK(InstallPs1Ride(staged, destination, why));
    CHECK(!Exists(backup) && ReadText(destination + "/recovered") == "complete recovered replacement");
    // An interruption after publishing the new ride: a stale old backup must not prevent another drop.
    WriteFile(backup + "/old", "old complete ride");
    WriteFile(staged + "/latest", "latest complete replacement");
    CHECK(InstallPs1Ride(staged, destination, why));
    CHECK(!Exists(backup) && ReadText(destination + "/latest") == "latest complete replacement");
    CHECK(!InstallPs1Ride(staged, destination, why));
    CHECK(ReadText(destination + "/latest") == "latest complete replacement");
    RemoveTree(root); RemoveTree(drop);
}

// Every converted PlayStation ride has a ride camera (flags 0x1031, id 1; TPCamera.cpp finds no camera without one) whose
// rows, taken through its part's matrix, are a proper rotation with Y up at rest, as the PC ride cameras are. Every
// rider's head in the Clown's bells and the Skulls' skulls sits down in the part carrying it and keeps its
// sprite's reach (a head's rise across, turned to the camera: its corners are 0.5 times the square root of 2 world units
// from its middle) from every wall of that part (its faces steeper than 45 degrees), so its face never shows through
// them; only its top shows, through the part's top.
void TestPs1Cameras(const std::string& disc, const std::string& donors, const std::string& scratch) {
    std::printf("PlayStation ride cameras\n");
    std::string root = scratch + "/ps1-cameras", why;
    RemoveTree(root);
    fs::create_directories(root + "/assets");
    for (const char* edition : {"us", "eur", "us-pc", "eur-pc"})
        if (Exists(donors + "/assets/" + edition)) fs::create_directory_symlink(donors + "/assets/" + edition, root + "/assets/" + edition);
    Ps1Disc ps1;
    auto blob = OpenFileBlob(disc, why);
    CHECK(blob && RecognisePs1(blob, ps1, why) == Ps1Status::Supported);
    CHECK(ConvertPs1(ps1, root, why));
    auto u32 = [](const std::string& b, size_t p) {
        uint32_t v = 0;
        for (int k = 3; k >= 0; k--) v = v << 8 | uint8_t(b.at(p + k));
        return v;
    };
    auto rows = [&](const std::string& b, size_t p) {
        std::array<std::array<double, 3>, 3> m;
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) {
                uint32_t v = u32(b, p + 16 * r + 4 * c);
                float f;
                std::memcpy(&f, &v, 4);
                m[r][c] = f;
            }
        return m;
    };
    int rides = 0;
    for (auto& theme : fs::directory_iterator(root + "/assets/ps1/data/levels")) {
        if (!fs::is_directory(theme.path() / "rides")) continue;
        for (auto& ride : fs::directory_iterator(theme.path() / "rides")) {
            std::string dir = ride.path().filename().string();
            std::string md2 = ReadText((ride.path() / (dir.substr(0, dir.rfind('_')) + ".md2")).string());
            if (dir.rfind("_obver", 0) == 0) continue;  // the Tower is a feature, not a ride
            ++rides;
            int cameras = 0;
            size_t minis = u32(md2, 0x48) & 0xffff, mo = u32(md2, 0x74), eo = u32(md2, 0x7c);
            for (size_t i = 0; i < minis; i++) {
                if (u32(md2, eo + 20 * i) != 0x1031) continue;
                ++cameras;
                CHECK_EQ(u32(md2, eo + 20 * i + 4), 1);
                auto cam = rows(md2, mo + 0x58 * i + 0x10), part = rows(md2, u32(md2, mo + 0x58 * i + 4) + 0x10);
                std::array<std::array<double, 3>, 3> w{};
                for (int r = 0; r < 3; r++)
                    for (int c = 0; c < 3; c++)
                        for (int k = 0; k < 3; k++) w[r][c] += cam[r][k] * part[k][c];
                double det = w[0][0] * (w[1][1] * w[2][2] - w[1][2] * w[2][1]) -
                             w[0][1] * (w[1][0] * w[2][2] - w[1][2] * w[2][0]) +
                             w[0][2] * (w[1][0] * w[2][1] - w[1][1] * w[2][0]);
                // No rider's head on the camera's part sits just in front of it to fill the view (it looks along
                // its Z): those ahead of it are at least two head rises away, as on a car behind another.
                auto at = [&](size_t j) { return rows(md2, mo + 0x58 * j + 0x20)[2]; };
                int heads = 0;
                double nearest = 1e9;
                for (size_t j = 0; j < minis; j++) {
                    if (u32(md2, eo + 20 * j) != 0xb1 || u32(md2, mo + 0x58 * j + 4) != u32(md2, mo + 0x58 * i + 4))
                        continue;
                    ++heads;
                    double along = 0, far = 0;
                    for (int c = 0; c < 3; c++) {
                        along += (at(j)[c] - at(i)[c]) * cam[2][c];
                        far += (at(j)[c] - at(i)[c]) * (at(j)[c] - at(i)[c]);
                    }
                    if (along > 0) nearest = std::min(nearest, std::sqrt(far));
                }
                std::printf("  %s: camera up %.2f, determinant %.3f, %d heads on its part", dir.c_str(), w[1][1], det,
                            heads);
                if (nearest < 1e9) std::printf(", the nearest ahead %.2f away", nearest);
                std::printf("\n");
                CHECK(std::abs(det - 1) < 1e-3);
                CHECK(w[1][1] > 0.7);
                CHECK(nearest > 2);
            }
            CHECK_EQ(cameras, 1);
            // A part's sides at height y in its own frame: the convex hull (anticlockwise in x, z) of where the lines
            // between its vertices cross y. Its vertices are stored four at a time, x x x x y y y y z z z z.
            auto vertex = [&](size_t node, size_t k) {
                std::array<double, 3> v;
                for (int c = 0; c < 3; c++) {
                    uint32_t w = u32(md2, u32(md2, node + 0x60) + 48 * (k / 4) + 16 * c + 4 * (k % 4));
                    float f;
                    std::memcpy(&f, &w, 4);
                    v[c] = f;
                }
                return v;
            };
            auto inside = [&](size_t node, std::array<double, 3> h) {
                size_t count = u32(md2, node + 0x58) & 0xffff;
                std::vector<std::array<double, 2>> p, hull;
                for (size_t a = 0; a < count; a++)
                    for (size_t b = a + 1; b < count; b++) {
                        auto va = vertex(node, a), vb = vertex(node, b);
                        if (va[1] == vb[1] || (va[1] - h[1]) * (vb[1] - h[1]) > 0) continue;
                        double s = (h[1] - va[1]) / (vb[1] - va[1]);
                        p.push_back({va[0] + s * (vb[0] - va[0]), va[2] + s * (vb[2] - va[2])});
                    }
                std::sort(p.begin(), p.end());
                auto turn = [](auto& o, auto& a, auto& b) {
                    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
                };
                for (int pass = 0; pass < 2 && !p.empty(); pass++) {
                    size_t start = hull.size();
                    for (auto& q : p) {
                        while (hull.size() >= start + 2 && turn(hull[hull.size() - 2], hull.back(), q) <= 0) hull.pop_back();
                        hull.push_back(q);
                    }
                    hull.pop_back();
                    std::reverse(p.begin(), p.end());
                }
                if (hull.size() < 3) return -1e9;
                double d = 1e9;
                for (size_t i = 0; i < hull.size(); i++) {
                    auto &a = hull[i], &b = hull[(i + 1) % hull.size()];
                    double ex = b[0] - a[0], ez = b[1] - a[1];
                    d = std::min(d, (ex * (h[2] - a[1]) - ez * (h[0] - a[0])) / std::sqrt(ex * ex + ez * ez));
                }
                return d;
            };
            // How far h is from the part's nearest wall. Its faces are four words each (face normal, then three
            // corners), and each corner's word at the node's +0x94 is the vertex it is at.
            auto sub = [](std::array<double, 3> a, std::array<double, 3> b) {
                return std::array<double, 3>{a[0] - b[0], a[1] - b[1], a[2] - b[2]};
            };
            auto dot = [](std::array<double, 3> a, std::array<double, 3> b) {
                return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
            };
            auto clear = [&](size_t node, std::array<double, 3> h) {
                size_t faces = u32(md2, node + 0x5c) & 0xffff;
                double d = 1e9;
                for (size_t i = 0; i < faces; i++) {
                    std::array<std::array<double, 3>, 3> f;
                    for (int c = 0; c < 3; c++) {
                        size_t corner = u32(md2, u32(md2, node + 0x70) + 8 * i + 2 * (c + 1)) & 0xffff;
                        f[c] = vertex(node, u32(md2, u32(md2, node + 0x94) + 2 * corner) & 0xffff);
                    }
                    auto ab = sub(f[1], f[0]), ac = sub(f[2], f[0]);
                    std::array<double, 3> n = {ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2],
                                               ab[0] * ac[1] - ab[1] * ac[0]};
                    if (dot(n, n) < 1e-18 || std::abs(n[1]) >= std::sqrt(dot(n, n) / 2)) continue;
                    // The nearest point of the triangle to h: inside it, or else on its nearest edge.
                    auto ap = sub(h, f[0]);
                    double nn = dot(n, n), u = dot(ap, n) / nn;
                    std::array<double, 3> q = {h[0] - u * n[0], h[1] - u * n[1], h[2] - u * n[2]};
                    bool in = true;
                    for (int c = 0; c < 3; c++) {
                        auto e = sub(f[(c + 1) % 3], f[c]), r = sub(q, f[c]);
                        std::array<double, 3> x = {e[1] * r[2] - e[2] * r[1], e[2] * r[0] - e[0] * r[2],
                                                   e[0] * r[1] - e[1] * r[0]};
                        if (dot(x, n) < 0) in = false;
                    }
                    if (in) {
                        d = std::min(d, std::abs(u) * std::sqrt(nn));
                        continue;
                    }
                    for (int c = 0; c < 3; c++) {
                        auto e = sub(f[(c + 1) % 3], f[c]), r = sub(h, f[c]);
                        double t = std::clamp(dot(r, e) / dot(e, e), 0.0, 1.0);
                        auto g = sub(r, {t * e[0], t * e[1], t * e[2]});
                        d = std::min(d, std::sqrt(dot(g, g)));
                    }
                }
                return d;
            };
            size_t stand = u32(md2, 0x70);
            int carried = 0, down = 0;
            bool shell = dir.rfind("_jack", 0) == 0 || dir.rfind("_skulls", 0) == 0;
            for (size_t j = 0; shell && j < minis; j++) {
                size_t node = u32(md2, mo + 0x58 * j + 4);
                if (u32(md2, eo + 20 * j) != 0xb1 || node == stand) continue;
                ++carried;
                auto h = rows(md2, mo + 0x58 * j + 0x20)[2];
                float top;
                uint32_t w = u32(md2, node + 0x78 + 16);
                std::memcpy(&top, &w, 4);
                if (h[1] >= top || inside(node, h) <= 0) continue;
                ++down;
                double wall = clear(node, h);
                std::printf("  %s: head %zu sits %.2f under its part's top, %.2f from its walls\n", dir.c_str(), j + 1,
                            top - h[1], wall);
                CHECK(wall > 0.5 * std::sqrt(2.0) - 1e-3);
            }
            if (shell) CHECK(carried > 0 && down == carried);
        }
    }
    CHECK_EQ(rides, 7);
    RemoveTree(root);
}

void TestPs1Grass() {
    std::printf("PlayStation ride ground grass key\n");
    // A row of the Diplo-Dip ground's own palette colours: water, sand, seven grass, a rock; then Space's green pad and
    // a Halloween hedge, which are not grass.
    using Texel = std::array<uint8_t, 4>;
    const Texel water{24, 156, 197, 255}, sand{164, 123, 57, 255}, grass{24, 139, 41, 255}, rock{131, 98, 57, 255};
    std::vector<Texel> row{water, sand, grass, grass, grass, grass, grass, grass, grass, rock};
    KeyPs1Grass(row, 10, 1);
    CHECK(row[0] == water && row[1] == sand && row[9] == rock);
    CHECK(row[4][3] == 0 && row[5][3] == 0 && row[6][3] == 0);
    // Further in it keeps its own colour, for a ground drawn without blending.
    CHECK((row[5] == Texel{24, 139, 41, 0}));
    // Next to the rest, grass fades in the nearest texel's colour rather than its own green.
    CHECK((row[2] == Texel{164, 123, 57, 170}) && (row[3] == Texel{164, 123, 57, 85}));
    CHECK((row[8] == Texel{131, 98, 57, 170}) && (row[7] == Texel{131, 98, 57, 85}));
    std::vector<Texel> other{{0, 230, 0, 255}, {41, 82, 32, 255}, {123, 123, 49, 255}};
    auto kept = other;
    KeyPs1Grass(other, 3, 1);
    CHECK(other == kept);
    std::vector<Texel> lawn(16, grass);
    KeyPs1Grass(lawn, 4, 4);
    CHECK(std::all_of(lawn.begin(), lawn.end(), [](const Texel& t) { return t[3] == 0; }));
}

void TestPs1PlainGround() {
    std::printf("PlayStation ride ground plain ground key\n");
    // Colours from the disc's palettes: Halloween's flagstone (tile 145) and the Clown's olive lawn and orange ring,
    // an Eye Slide hedge; Space's purple ground and the Romper's brown pad and green light; the jungle's grass.
    using Texel = std::array<uint8_t, 4>;
    const Texel stone{98, 90, 123, 255}, stone2{115, 98, 131, 255}, lawn{98, 82, 0, 255}, ring{172, 115, 0, 255},
        hedge{32, 41, 24, 255}, purple{90, 32, 115, 255}, pad{115, 41, 16, 255}, light{0, 230, 0, 255},
        grass{24, 139, 41, 255};
    CHECK(Ps1PlainGround("hallow", stone) && Ps1PlainGround("hallow", stone2) && Ps1PlainGround("hallow", lawn));
    CHECK(!Ps1PlainGround("hallow", ring) && !Ps1PlainGround("hallow", hedge) && !Ps1PlainGround("hallow", purple));
    CHECK(Ps1PlainGround("space", purple));
    CHECK(!Ps1PlainGround("space", pad) && !Ps1PlainGround("space", light) && !Ps1PlainGround("space", stone));
    CHECK(!Ps1PlainGround("jungle", grass) && !Ps1PlainGround("jungle", stone));
    // A 15 x 15 patch of flagstone with a hedge outline round its middle 3 x 3, a lone fleck in a corner and a blob of
    // five texels: the stone the edge reaches is keyed, fading in next to the hedge; the enclosed stone, the blob and
    // the hedge stay; the fleck is keyed with the stone.
    const Texel fleck{60, 50, 60, 255};
    std::vector<Texel> patch(15 * 15, stone);
    for (int i = 5; i <= 9; i++)
        patch[5 * 15 + i] = patch[9 * 15 + i] = patch[i * 15 + 5] = patch[i * 15 + 9] = hedge;
    patch[1 * 15 + 1] = fleck;
    for (int at : {12 * 15 + 1, 12 * 15 + 2, 12 * 15 + 3, 13 * 15 + 1, 13 * 15 + 2})
        patch[at] = fleck;
    KeyPs1Patch(patch, 15, 15, "hallow");
    CHECK(patch[7 * 15 + 7] == stone && patch[5 * 15 + 7] == hedge);
    CHECK(patch[12 * 15 + 1] == fleck && patch[13 * 15 + 2] == fleck);
    CHECK((patch[7 * 15 + 0] == Texel{98, 90, 123, 0}) && (patch[1 * 15 + 1] == Texel{60, 50, 60, 0}));
    CHECK((patch[7 * 15 + 4] == Texel{32, 41, 24, 170}) && (patch[7 * 15 + 3] == Texel{32, 41, 24, 85}));
    // Space's purple keyed round the pad, the pad and its light kept (five texels, more than a fleck).
    std::vector<Texel> romper{purple, purple, purple, pad, pad, pad, pad, light, purple, purple, purple};
    KeyPs1Patch(romper, 11, 1, "space");
    CHECK(romper[3] == pad && romper[6] == pad && romper[7] == light);
    CHECK((romper[2] == Texel{115, 41, 16, 170}) && (romper[1] == Texel{115, 41, 16, 85}) && romper[0][3] == 0);
    CHECK((romper[8] == Texel{0, 230, 0, 170}) && romper[10][3] == 0);
}

void TestSynthetic(const std::string& scratch) {
    std::printf("synthetic drops\n");
    std::string root = scratch + "/synthetic-data", drop = scratch + "/synthetic-drop";
    RemoveTree(root);
    RemoveTree(drop);

    // a destination whose content is the empty file, and one with real content
    std::string emptyDest, knownDest;
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        const ManifestRecord& r = ManifestRecords()[i];
        if (!(r.editions & kEditionUs) || (r.flags & (kFlagRsrc | kFlagFinfo))) continue;
        if (emptyDest.empty() && RecordHash(r, 0).size == 0) emptyDest = r.dest;
        if (knownDest.empty() && RecordHash(r, 0).size > 0 && std::string(r.dest).find('/') != std::string::npos) knownDest = r.dest;
    }
    CHECK(!emptyDest.empty() && !knownDest.empty());
    size_t emptyTargets = 0;  // (edition, record) pairs the empty file at that path fills
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        const ManifestRecord& r = ManifestRecords()[i];
        if (emptyDest == r.dest && RecordHash(r, 0).size == 0) emptyTargets += __builtin_popcount(r.editions);
    }

    WriteFile(drop + "/Theme Park Data/" + emptyDest, "");
    WriteFile(drop + "/Theme Park Data/" + knownDest, Junk(1));
    WriteFile(drop + "/notes.txt", Junk(7));
    WriteFile(drop + "/stray/empty.dat", "");
    // something an earlier run left in the staging folder
    WriteFile(root + "/assets/.staging/import-0.part", "half");

    ImporterConfig cfg;
    cfg.dataRoot = root;
    Importer imp(cfg);
    std::string err;
    CHECK(imp.Open(err));
    CHECK(!Exists(root + "/assets/.staging/import-0.part"));
    CHECK(Exists(root + "/launcher.ini"));
    CHECK_EQ(imp.Status(kEditionUs).present, 0);

    ImportReport r = imp.Import({drop});
    PrintStatus("first drop", r);
    CHECK(r.errors.empty());
    CHECK_EQ(r.written, emptyTargets);
    CHECK_EQ(r.alreadyThere, 0);
    CHECK_EQ(r.unknownFiles, 2);  // notes.txt, and an empty file whose path is no destination
    CHECK_EQ(r.differentVersions.size(), 1);
    CHECK(r.differentVersions.size() == 1 && r.differentVersions[0] == knownDest);
    CHECK(!r.ChangedNothing());
    CHECK(Exists(imp.GameFolder(kEditionUs) + "/" + emptyDest));
    CHECK(!Exists(imp.GameFolder(kEditionUs) + "/" + knownDest));  // never a file that is not verified
    CHECK_EQ(imp.Status(kEditionUs).present, r.status[0].present);
    CHECK(r.status[0].present >= 1 && !r.status[0].unlocked);
    std::vector<ImportProgress> ev = Events(imp);
    CHECK(!ev.empty() && ev.back().kind == ImportProgress::Kind::Finished);
    CHECK_EQ(CountKind(ev, ImportProgress::Kind::Written), emptyTargets);
    CHECK_EQ(CountKind(ev, ImportProgress::Kind::DifferentVersion), 1);
    for (const ImportProgress& e : ev) CHECK(e.path.find("notes.txt") == std::string::npos);  // unknown files are never listed
    CHECK(Tree(root + "/assets/.staging", false).empty());

    // the same drop again changes nothing, and says so
    ImportReport again = imp.Import({drop});
    CHECK(again.ChangedNothing());
    CHECK_EQ(again.written, 0);
    CHECK_EQ(again.alreadyThere, emptyTargets);
    CHECK(again.Summary().find("Nothing new") == 0);
    Events(imp);

    // refusals: a missing path, a drop inside the data folder; the data folder itself reads nothing back
    ImportReport missing = imp.Import({drop + "/no such file"});
    CHECK_EQ(missing.errors.size(), 1);
    ImportReport inside = imp.Import({root + "/assets/us"});
    CHECK_EQ(inside.errors.size(), 1);
    CHECK_EQ(inside.entries, 0);
    ImportReport self = imp.Import({root});
    CHECK_EQ(self.written, 0);
    CHECK_EQ(self.unknownFiles, 1);  // launcher.ini
    // a lone file is read as itself
    ImportReport single = imp.Import({drop + "/notes.txt"});
    CHECK_EQ(single.unknownFiles, 1);
    CHECK_EQ(single.entries, 1);
    Events(imp);

    // launcher.ini: states, data folder, last choice, keys of later units kept
    LauncherIni ini;
    CHECK(LoadLauncherIni(root + "/launcher.ini", ini, err));
    CHECK(ini.dataFolder == root);
    CHECK(ini.editions["us"] == "partial");
    CHECK(ini.editions["gold"] == "absent");
    ini.extra.emplace_back("window.scale", "2");
    CHECK(SaveLauncherIni(root + "/launcher.ini", ini, err));
    {
        Importer again2(cfg);
        CHECK(again2.Open(err));
        CHECK(again2.SetLastChoice("us", err));
        LauncherIni back;
        CHECK(LoadLauncherIni(root + "/launcher.ini", back, err));
        CHECK(back.lastChoice == "us");
        CHECK(back.extra.size() == 1 && back.extra[0].first == "window.scale" && back.extra[0].second == "2");
        CHECK_EQ(again2.Status(kEditionUs).present, r.status[0].present);  // found again by Open
    }
    CHECK(ResolveDataRoot(root) == root);
    LauncherIni none;
    CHECK(LoadLauncherIni(scratch + "/no-such.ini", none, err) && none.editions.empty());

    // removing an edition
    CHECK(imp.RemoveEdition(kEditionUs, err));
    CHECK(!Exists(root + "/assets/us"));
    CHECK_EQ(imp.Status(kEditionUs).present, 0);
    CHECK(imp.Ini().editions["us"] == "absent");

    // a damaged file is found by VerifyEdition and removed
    if (emptyTargets > 1) {
        std::string eurFile = imp.GameFolder(kEditionEur) + "/" + emptyDest;
        if (Exists(eurFile)) {
            CHECK_EQ(imp.VerifyEdition(kEditionEur, err), 0);
            WriteFile(eurFile, "");  // still empty: still good
            CHECK_EQ(imp.VerifyEdition(kEditionEur, err), 0);
        }
    }

    RemoveTree(root);
    RemoveTree(drop);
}

// ---- the language packs

const LanguageStatus* FindStatus(const std::vector<LanguageStatus>& all, const std::string& name) {
    for (const LanguageStatus& l : all)
        if (l.name == name) return &l;
    return nullptr;
}

// The lines --verify prints, from plain data.
void TestLanguageLines() {
    std::printf("language lines\n");
    LanguageStatus l;
    l.name = "French";
    l.text = 60, l.filter = 1, l.meshes = 3, l.speech = 20;
    CHECK(l.Line() == "French: absent");
    l.state = LanguageStatus::State::Available;
    CHECK(l.Line() == "French: available");
    l.oldTables = l.baseDateFonts = l.baseDateFontsPresent = true;
    l.name = "Spanish";
    CHECK(l.Line() == "Spanish: available, older text tables, date fonts from the base edition");
    l = LanguageStatus();
    l.name = "German";
    l.state = LanguageStatus::State::Partial;
    l.text = l.textPresent = 61, l.filter = l.filterPresent = 1, l.meshes = l.meshesPresent = 3, l.speech = 20;
    l.speechPresent = 17;
    CHECK(l.Line() == "German: partial, 3 of 20 speech files missing");
    l.textPresent = 51, l.meshesPresent = 0;
    CHECK(l.Line() == "German: partial, 10 of 61 text files, 3 of 3 meshes and 3 of 20 speech files missing");
    l.filterPresent = 0, l.textPresent = 61, l.meshesPresent = 3, l.speechPresent = 20;
    CHECK(l.Line() == "German: partial, 1 of 1 filter files missing");
    l = LanguageStatus();
    l.name = "Dutch";
    l.state = LanguageStatus::State::Partial;
    l.baseDateFonts = true;
    l.text = l.textPresent = 56;
    CHECK(l.Line() == "Dutch: partial, text and speech present, needs the US or European edition (its date fonts)");
    l.textPresent = 55;
    CHECK(l.Line() == "Dutch: partial, 1 of 56 text files missing; needs the US or European edition (its date fonts)");
    CHECK(std::strcmp(l.StateWord(), "partial") == 0);
}

// An empty language file is matched by its name and a folder named after its language (the overlay discs keep each
// language in a folder of its own); without such a folder it is an unknown file. Nothing else of a language makes it
// available or partial.
void TestLanguageSynthetic(const std::string& scratch) {
    std::printf("language drops\n");
    std::string root = scratch + "/language-data", drop = scratch + "/language-drop";
    RemoveTree(root);
    RemoveTree(drop);

    // the languages whose alloweds.txt is the empty file
    std::vector<std::string> emptyLanguages;
    for (size_t l = 0; l < ManifestLanguageCount(); ++l) {
        const std::string name = ManifestLanguages()[l].name;
        const ManifestRecord* r =
            FindByDest(kEditionLanguages, ("lang/" + name + "/data/Language/" + name + "/alloweds.txt").c_str());
        if (r && r->hash_count == 1 && RecordHash(*r, 0).size == 0) emptyLanguages.push_back(name);
    }
    CHECK(emptyLanguages.size() >= 2);
    if (emptyLanguages.size() < 2) return;
    const std::string first = emptyLanguages[0], second = emptyLanguages[1];
    std::string lower = second;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    WriteFile(drop + "/" + first + "/Filter/" + first + "/alloweds.txt", "");  // an overlay disc's layout
    WriteFile(drop + "/disc2/" + lower + "/alloweds.txt", "");                  // any case
    WriteFile(drop + "/Filter/alloweds.txt", "");                               // no language folder: unknown

    ImporterConfig cfg;
    cfg.dataRoot = root;
    Importer imp(cfg);
    std::string err;
    CHECK(imp.Open(err));
    std::vector<LanguageStatus> before = imp.Languages();
    CHECK_EQ(before.size(), ManifestLanguageCount());
    for (const LanguageStatus& l : before) CHECK(l.state == LanguageStatus::State::Absent);

    ImportReport r = imp.Import({drop});
    CHECK(r.errors.empty());
    CHECK_EQ(r.written, 2);
    CHECK_EQ(r.unknownFiles, 1);
    const std::string lang = imp.GameFolder(kEditionLanguages);
    CHECK(lang == root + "/assets/lang");
    CHECK(Exists(lang + "/" + first + "/data/Language/" + first + "/alloweds.txt"));
    CHECK(Exists(lang + "/" + second + "/data/Language/" + second + "/alloweds.txt"));
    CHECK(Tree(root + "/assets/us", false).empty() && Tree(root + "/assets/eur", false).empty());
    CHECK_EQ(r.languages.size(), ManifestLanguageCount());
    for (size_t l = 0; l < r.languages.size() && l < ManifestLanguageCount(); ++l)
        CHECK(r.languages[l].name == ManifestLanguages()[l].name);
    // an optional file alone is no part of a language
    const LanguageStatus* got = FindStatus(r.languages, first);
    CHECK(got != nullptr && got->state == LanguageStatus::State::Absent && got->optionalPresent == 1);
    CHECK(got != nullptr && got->Line() == first + ": absent");
    CHECK(got != nullptr && got->speech == 20 && got->meshes == 3 && got->filter == 1 && got->speechPresent == 0);
    // the flags come from the manifest's language table
    const LanguageStatus* spanish = FindStatus(r.languages, "Spanish");
    CHECK(spanish != nullptr && spanish->oldTables && spanish->baseDateFonts && !spanish->baseDateFontsPresent);
    const LanguageStatus* korean = FindStatus(r.languages, "Korean");
    CHECK(korean != nullptr && !korean->oldTables && !korean->baseDateFonts);

    // found again by Open, checked again, removed
    {
        Importer again(cfg);
        CHECK(again.Open(err));
        const LanguageStatus* back = FindStatus(again.Languages(), first);
        CHECK(back != nullptr && back->optionalPresent == 1);
    }
    CHECK_EQ(imp.VerifyEdition(kEditionLanguages, err), 0);
    WriteFile(lang + "/" + second + "/data/Language/" + second + "/alloweds.txt", "changed");
    CHECK_EQ(imp.VerifyEdition(kEditionLanguages, err), 1);
    CHECK(!Exists(lang + "/" + second + "/data/Language/" + second + "/alloweds.txt"));
    CHECK(imp.RemoveEdition(kEditionLanguages, err));
    CHECK(!Exists(lang));
    const LanguageStatus* gone = FindStatus(imp.Languages(), first);
    CHECK(gone != nullptr && gone->optionalPresent == 0);

    // a drop that may not write the language packs leaves them alone
    ImporterConfig noLang = cfg;
    noLang.editions = kEditionUs | kEditionEur | kEditionGold;
    Importer without(noLang);
    CHECK(without.Open(err));
    ImportReport r2 = without.Import({drop});
    CHECK_EQ(r2.written, 0);
    CHECK(!Exists(lang));

    RemoveTree(root);
    RemoveTree(drop);
}

// A real language disc: every language it names becomes available, and each file written under assets/lang/<Name>/
// is its record's content.
void TestLanguageDisc(const std::string& disc, const std::string& names, const std::string& scratch) {
    std::printf("language disc\n");
    std::string root = scratch + "/language-disc";
    RemoveTree(root);
    ImporterConfig cfg;
    cfg.dataRoot = root;
    Importer imp(cfg);
    std::string err;
    CHECK(imp.Open(err));
    ImportReport r = imp.Import({disc});
    CHECK(r.errors.empty());
    for (const std::string& e : r.errors) std::printf("  error: %s\n", e.c_str());
    for (const LanguageStatus& l : r.languages) std::printf("  %s\n", l.Line().c_str());
    std::stringstream list(names);
    std::string name;
    while (std::getline(list, name, ',')) {
        const LanguageStatus* l = FindStatus(r.languages, name);
        CHECK(l != nullptr && l->state == LanguageStatus::State::Available);
        size_t checked = 0;
        for (const auto& [rel, info] : Tree(imp.GameFolder(kEditionLanguages) + "/" + name, true)) {
            const ManifestRecord* rec = FindByDest(kEditionLanguages, ("lang/" + name + "/" + rel).c_str());
            CHECK(rec != nullptr);
            bool accepts = false;
            for (unsigned k = 0; rec && k < rec->hash_count; ++k) {
                const ManifestHash& h = RecordHash(*rec, k);
                Sha256Digest d;
                std::memcpy(d.bytes, h.sha256, sizeof d.bytes);
                accepts = accepts || (h.size == info.size && d.Hex() == info.sha);
            }
            CHECK(accepts);
            ++checked;
        }
        CHECK(l != nullptr && checked >= l->text + l->filter + l->meshes + l->speech);
    }
    CHECK_EQ(imp.VerifyEdition(kEditionLanguages, err), 0);
    RemoveTree(root);
}

// ---- the startup check (assets/verified.tsv): an unchanged install is not rehashed; a file whose size or modification
// time changed is rehashed and removed when it no longer matches; one that kept both is not seen (by design)

bool LineFor(const std::string& tsv, const std::string& edition, const std::string& dest) {
    return ("\n" + tsv).find("\n" + edition + "\t" + dest + "\t") != std::string::npos;
}

void TestStartupCheck(const std::string& scratch) {
    std::printf("startup check\n");
    std::string root = scratch + "/startup-data";
    RemoveTree(root);

    // a US destination with the empty content, and the smallest one with real content named by no other US record
    std::map<std::string, int> usDests;
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        const ManifestRecord& r = ManifestRecords()[i];
        if (r.editions & kEditionUs) usDests[r.dest]++;
    }
    const ManifestRecord* emptyRec = nullptr;
    const ManifestRecord* sizedRec = nullptr;
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        const ManifestRecord& r = ManifestRecords()[i];
        if (!(r.editions & kEditionUs) || (r.flags & (kFlagRsrc | kFlagFinfo)) || usDests[r.dest] != 1) continue;
        if (r.hash_count != 1) continue;
        uint64_t size = RecordHash(r, 0).size;
        if (size == 0 && !emptyRec) emptyRec = &r;
        if (size > 0 && (!sizedRec || size < RecordHash(*sizedRec, 0).size)) sizedRec = &r;
    }
    CHECK(emptyRec && sizedRec);
    if (!emptyRec || !sizedRec) return;
    const uint64_t size = RecordHash(*sizedRec, 0).size;

    ImporterConfig cfg;
    cfg.dataRoot = root;
    std::string emptyPath, sizedPath, tsvPath;
    {
        Importer layout(cfg);
        emptyPath = layout.DestinationPath(kEditionUs, *emptyRec);
        sizedPath = layout.DestinationPath(kEditionUs, *sizedRec);
        tsvPath = layout.VerifiedPath();
    }
    // the empty file is the right content; the other is junk of an accepted size, which the stat sweep counts as present
    WriteFile(emptyPath, "");
    WriteFile(sizedPath, std::string(static_cast<size_t>(size), 'j'));
    std::string err;
    size_t present = 0;

    // (4) no verified.tsv: nothing is rehashed or removed, the record is written from the stat sweep
    {
        Importer imp(cfg);
        CHECK(imp.Open(err));
        OpenCheck c = imp.LastOpenCheck();
        CHECK(!c.hadRecord);
        CHECK_EQ(c.rehashed, 0);
        CHECK_EQ(c.Removed(), 0);
        CHECK(c.Notice().empty());
        CHECK(Exists(sizedPath));
        present = imp.Status(kEditionUs).present;
        CHECK(present >= 2);
        CHECK(Exists(tsvPath));
        std::string tsv = ReadText(tsvPath);
        CHECK(LineFor(tsv, "us", emptyRec->dest));
        CHECK(LineFor(tsv, "us", sizedRec->dest));
        CHECK(!Exists(tsvPath + ".tmp"));
    }

    // (1) an unchanged install: every present file compared, none rehashed
    {
        Importer imp(cfg);
        auto t0 = std::chrono::steady_clock::now();
        CHECK(imp.Open(err));
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        OpenCheck c = imp.LastOpenCheck();
        CHECK(c.hadRecord);
        CHECK_EQ(c.checked, present);
        CHECK_EQ(c.rehashed, 0);
        CHECK_EQ(c.Removed(), 0);
        CHECK_EQ(imp.Status(kEditionUs).present, present);
        std::printf("  unchanged: %llu files compared, none rehashed, Open %.1f ms\n",
                    static_cast<unsigned long long>(c.checked), ms);
    }

    // (3) other content of the same size with the same modification time: not seen (by design), the file stays
    {
        std::error_code ec;
        fs::file_time_type when = fs::last_write_time(sizedPath, ec);
        CHECK(!ec);
        WriteFile(sizedPath, std::string(static_cast<size_t>(size), 'k'));
        fs::last_write_time(sizedPath, when, ec);
        CHECK(!ec);
        Importer imp(cfg);
        CHECK(imp.Open(err));
        OpenCheck c = imp.LastOpenCheck();
        CHECK_EQ(c.rehashed, 0);
        CHECK_EQ(c.Removed(), 0);
        CHECK(Exists(sizedPath));
        CHECK_EQ(imp.Status(kEditionUs).present, present);
    }

    // a changed modification time on a file that still matches: rehashed and kept
    {
        std::error_code ec;
        fs::last_write_time(emptyPath, fs::last_write_time(emptyPath, ec) + std::chrono::seconds(7), ec);
        CHECK(!ec);
        Importer imp(cfg);
        CHECK(imp.Open(err));
        OpenCheck c = imp.LastOpenCheck();
        CHECK_EQ(c.rehashed, 1);
        CHECK_EQ(c.Removed(), 0);
        CHECK(Exists(emptyPath));
        CHECK_EQ(imp.Status(kEditionUs).present, present);
    }

    // (2) the same size, another modification time and wrong content: rehashed at Open, removed, and the notice says so
    {
        std::error_code ec;
        fs::last_write_time(sizedPath, fs::last_write_time(sizedPath, ec) + std::chrono::seconds(5), ec);
        CHECK(!ec);
        Importer imp(cfg);
        CHECK(imp.Open(err));
        OpenCheck c = imp.LastOpenCheck();
        CHECK_EQ(c.rehashed, 1);
        CHECK_EQ(c.removed[0], 1);
        CHECK_EQ(c.Removed(), 1);
        CHECK(!Exists(sizedPath));
        CHECK(Exists(emptyPath));
        CHECK_EQ(imp.Status(kEditionUs).present, present - 1);
        CHECK(c.Notice() == "1 file of the US edition had changed and was removed; drop its disc to restore it.");
        std::printf("  notice: %s\n", c.Notice().c_str());
        std::string tsv = ReadText(tsvPath);
        CHECK(!LineFor(tsv, "us", sizedRec->dest));
        CHECK(LineFor(tsv, "us", emptyRec->dest));
        LauncherIni ini;
        CHECK(LoadLauncherIni(root + "/launcher.ini", ini, err));
        CHECK(ini.editions["us"] != "unlocked");
    }
    // and the next start finds nothing new
    {
        Importer imp(cfg);
        CHECK(imp.Open(err));
        CHECK_EQ(imp.LastOpenCheck().rehashed, 0);
        CHECK_EQ(imp.LastOpenCheck().Removed(), 0);
    }

    // the notice for more files and more editions
    OpenCheck several;
    several.removed[1] = 3;
    CHECK(several.Notice() == "3 files of the European edition had changed and were removed; drop its disc to restore them.");
    several.removed[0] = 1;
    CHECK(several.Notice() == "1 file of the US edition and 3 files of the European edition had changed and were removed; "
                              "drop their discs to restore them.");

    RemoveTree(root);
}

// ---- edited .sam files (tpw-port #94): the startup check and Check again keep a player's edited settings file, and
// every start names it again; a changed file of any other kind (a .sam's own .rsrc side file included) is still removed

const ManifestRecord* RecordOf(const char* dest, Edition edition) {
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        const ManifestRecord& r = ManifestRecords()[i];
        if ((r.editions & edition) && std::strcmp(r.dest, dest) == 0) return &r;
    }
    return nullptr;
}

void TestEditedSettings(const std::string& scratch) {
    std::printf("edited .sam files\n");
    std::string root = scratch + "/edited-sam-data";
    RemoveTree(root);
    // the US Standard.sam (BMCTV's edit kept its size), the European med.sam (an edit that changes the size), the
    // European med.sam's resource fork and another US file (no .sam), each written with its right size
    const ManifestRecord* usSam = RecordOf("data/levels/Standard.sam", kEditionUs);
    const ManifestRecord* eurSam = RecordOf("data/med.sam", kEditionEur);
    const ManifestRecord* eurRsrc = RecordOf("data/med.sam.rsrc", kEditionEur);
    std::map<std::string, int> usDests;
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        if (ManifestRecords()[i].editions & kEditionUs) usDests[ManifestRecords()[i].dest]++;
    }
    const ManifestRecord* usOther = nullptr;
    for (size_t i = 0; i < ManifestRecordCount() && !usOther; ++i) {
        const ManifestRecord& r = ManifestRecords()[i];
        const char* dot = std::strrchr(r.dest, '.');
        if ((r.editions & kEditionUs) && !(r.flags & (kFlagRsrc | kFlagFinfo)) && r.hash_count == 1 &&
            usDests[r.dest] == 1 && RecordHash(r, 0).size > 0 && !(dot && std::strcmp(dot, ".sam") == 0))
            usOther = &r;
    }
    CHECK(usSam && eurSam && eurRsrc && usOther);
    if (!usSam || !eurSam || !eurRsrc || !usOther) return;
    auto sized = [](const ManifestRecord* r, char c) { return std::string(static_cast<size_t>(RecordHash(*r, 0).size), c); };

    ImporterConfig cfg;
    cfg.dataRoot = root;
    std::string usSamPath, eurSamPath, eurRsrcPath, usOtherPath, tsvPath;
    {
        Importer layout(cfg);
        usSamPath = layout.DestinationPath(kEditionUs, *usSam);
        eurSamPath = layout.DestinationPath(kEditionEur, *eurSam);
        eurRsrcPath = layout.DestinationPath(kEditionEur, *eurRsrc);
        usOtherPath = layout.DestinationPath(kEditionUs, *usOther);
        tsvPath = layout.VerifiedPath();
    }
    WriteFile(usSamPath, sized(usSam, 'a'));
    WriteFile(eurSamPath, sized(eurSam, 'a'));
    WriteFile(eurRsrcPath, sized(eurRsrc, 'a'));
    WriteFile(usOtherPath, sized(usOther, 'a'));
    std::string err;
    size_t usPresent = 0, eurPresent = 0;
    {   // the first start writes the record (nothing rehashed)
        Importer imp(cfg);
        CHECK(imp.Open(err));
        CHECK(!imp.LastOpenCheck().hadRecord);
        usPresent = imp.Status(kEditionUs).present;
        eurPresent = imp.Status(kEditionEur).present;
        CHECK_EQ(usPresent, 2);
        CHECK_EQ(eurPresent, 2);
    }

    // the player edits: Standard.sam keeps its size, med.sam grows; the other two change too
    WriteFile(usSamPath, sized(usSam, 'b'));
    WriteFile(eurSamPath, sized(eurSam, 'b') + "\r\nBankAccountInfo.InitialCash 1000000\r\n");
    WriteFile(eurRsrcPath, sized(eurRsrc, 'b'));
    WriteFile(usOtherPath, sized(usOther, 'b'));
    for (const std::string& path : {usSamPath, eurRsrcPath, usOtherPath}) {
        std::error_code ec;
        fs::last_write_time(path, fs::last_write_time(path, ec) + std::chrono::seconds(5), ec);
        CHECK(!ec);
    }
    // in manifest order (by destination)
    const std::vector<std::string> named = {std::string("us/") + usSam->dest, std::string("eur/") + eurSam->dest};
    {
        Importer imp(cfg);
        CHECK(imp.Open(err));
        OpenCheck c = imp.LastOpenCheck();
        CHECK_EQ(c.rehashed, 4);
        CHECK_EQ(c.removed[0], 1);
        CHECK_EQ(c.removed[1], 1);
        CHECK(c.edited == named);
        CHECK(Exists(usSamPath) && Exists(eurSamPath));
        CHECK(!Exists(eurRsrcPath) && !Exists(usOtherPath));
        CHECK_EQ(imp.Status(kEditionUs).present, usPresent - 1);
        CHECK_EQ(imp.Status(kEditionEur).present, eurPresent - 1);
        CHECK(ReadText(eurSamPath).find("InitialCash 1000000") != std::string::npos);
        std::string tsv = ReadText(tsvPath);
        CHECK(!LineFor(tsv, "us", usSam->dest) && !LineFor(tsv, "eur", eurSam->dest));
        CHECK(c.Kept() == "kept 2 edited .sam files, which the game reads: us/data/levels/Standard.sam, eur/data/med.sam "
                          "(Customized settings on the edition's tile restores an original).");
        std::printf("  %s\n  %s\n", c.Kept().c_str(), c.Notice().c_str());
    }
    {   // every start names them again; nothing else is rehashed
        Importer imp(cfg);
        CHECK(imp.Open(err));
        OpenCheck c = imp.LastOpenCheck();
        CHECK_EQ(c.rehashed, 2);
        CHECK_EQ(c.Removed(), 0);
        CHECK(c.edited == named);
        // Check again keeps them too, and leaves them out of the record
        CHECK_EQ(imp.VerifyEdition(kEditionUs, err), 0);
        CHECK_EQ(imp.VerifyEdition(kEditionEur, err), 0);
        CHECK(Exists(usSamPath) && Exists(eurSamPath));
        CHECK_EQ(imp.Status(kEditionUs).present, usPresent - 1);
        std::string tsv = ReadText(tsvPath);
        CHECK(!LineFor(tsv, "us", usSam->dest) && !LineFor(tsv, "eur", eurSam->dest));
        // the tile's badge counts them; the list has each with its read check and no original (an edit is never copied)
        CHECK_EQ(imp.Status(kEditionUs).edited, 1);
        CHECK_EQ(imp.Status(kEditionEur).edited, 1);
        CHECK_EQ(imp.Status(kEditionGold).edited, 0);
        std::vector<EditedFile> us = imp.EditedFiles(kEditionUs);
        CHECK_EQ(us.size(), 1);
        if (us.size() == 1) {
            CHECK(us[0].dest == usSam->dest && us[0].path == usSamPath);
            CHECK(!us[0].original);
            CHECK_EQ(us[0].unreadableLine, 1);   // "bbbb...": no group name the parser takes
        }
        CHECK(!Exists(imp.OriginalPath(kEditionUs, *usSam)) && !Exists(imp.OriginalPath(kEditionEur, *eurSam)));
        CHECK(!imp.RestoreOriginal(kEditionUs, usSam->dest, err));
        CHECK(err.find("drop the disc again") != std::string::npos);
        // a kept copy that no longer hashes to the original is no original: not offered, not restored
        WriteFile(imp.OriginalPath(kEditionUs, *usSam), sized(usSam, 'c'));
        us = imp.EditedFiles(kEditionUs);
        CHECK(us.size() == 1 && !us[0].original);
        CHECK(!imp.RestoreOriginal(kEditionUs, usSam->dest, err));
        CHECK(ReadText(usSamPath) == sized(usSam, 'b'));
        CHECK_EQ(imp.Status(kEditionUs).edited, 1);
        // only a .sam of the edition
        CHECK(!imp.RestoreOriginal(kEditionUs, usOther->dest, err));
        CHECK(!imp.RestoreOriginal(kEditionUs, eurSam->dest + std::string("x"), err));
    }
    // the original back: the edited file deleted, the next start finds it missing (a drop would write it)
    fs::remove(usSamPath);
    {
        Importer imp(cfg);
        CHECK(imp.Open(err));
        OpenCheck c = imp.LastOpenCheck();
        CHECK(c.edited == std::vector<std::string>{std::string("eur/") + eurSam->dest});
        CHECK(c.Kept() == "kept 1 edited .sam file, which the game reads: eur/data/med.sam (Customized settings on the "
                          "edition's tile restores an original).");
        CHECK_EQ(imp.Status(kEditionUs).present, usPresent - 2);
        CHECK_EQ(imp.Status(kEditionUs).edited, 0);
        // removing the edition takes its kept originals with it (assets/us/originals)
        CHECK(Exists(imp.OriginalPath(kEditionUs, *usSam)));
        CHECK(imp.RemoveEdition(kEditionUs, err));
        CHECK(!Exists(imp.OriginalPath(kEditionUs, *usSam)));
    }
    RemoveTree(root);
}

// ---- the read check (assets/sam_check.h): the game's balance parser rules, with any value the parser takes

void TestSamReadCheck() {
    std::printf("the .sam read check\n");
    // a table laid out as the game's: a group's fields above its heading (type 1), an array group between a type 2 and
    // its type 3 (name: the group, arrayCount: its capacity), blank rows between groups
    auto row = [](int32_t type, const char* name, int32_t minI = 0, int32_t maxI = 0, float minF = 0, float maxF = 0,
                  int32_t count = 0, uint32_t flags = 0) {
        SamRow r{};
        r.type = type;
        std::snprintf(r.name, sizeof r.name, "%s", name);
        r.minInteger = minI;
        r.maxInteger = maxI;
        r.minFloat = minF;
        r.maxFloat = maxF;
        r.arrayCount = count;
        r.flags = flags;
        return r;
    };
    const std::vector<SamRow> table = {
        row(0, ""),
        row(4, "Cash"), row(5, "People"), row(6, "Level", 0, 5), row(7, "Rate"), row(8, "Speed"),
        row(9, "Mix", 0, 0, 0.0f, 1.0f), row(10, "Name"), row(1, "Bank"),
        row(0, ""),
        row(2, ""), row(4, "Price"), row(11, "Shape", 0, 0, 0, 0, 0, 1), row(3, "Ride", 0, 0, 0, 0, 2),
        row(12, ""),
    };
    struct Case {
        const char* text;
        int line;
    };
    const Case cases[] = {
        // any value the parser takes is fine: huge, negative, saturating
        {"Bank.Cash 2147483647999\r\nBank.Cash -99999999999\r\nBank.Rate -12345.5\r\n", 0},
        {"# a comment\n\n   \nBank.Cash 1 and words the parser never reads\n", 0},
        {"Bank.Cash.People.Level 5 6 4\n", 0},
        {"Bank.Name \"Hello park\"\nBank.Mix 0.5\nBank.Speed 0\n", 0},
        {"Ride[1].Price 10\n", 0},
        // a map: the line after its closing --- is skipped, whatever it holds
        {"Ride[0].Shape\n---\n. * Q\n@ +\n---\n!! skipped !!\nBank.Cash 1\n", 0},
        // where the game stops
        {"Bank.Cash 1\r\nBank.Cash x\r\n", 2},
        {"Bank.Cash 1\nBank.People -1\n", 2},
        {"Bank.Level 5\n", 1},
        {"Bank.Level -1\n", 1},
        {"Bank.Level 4\n", 0},
        {"Bank.Speed -0.5\n", 1},
        {"Bank.Mix 1.0\n", 1},
        {"Bank.Rate 1.2.3\n", 1},
        {"Bank.Rate 1e5\n", 1},
        {"Bank.Name \"open\n", 1},
        {"\nBank.Gold 5\n", 2},
        {"Vault.Cash 1\n", 1},
        {"Ride[2].Price 10\n", 1},
        {"Bank[0].Cash 1\n", 1},
        {"Ride.Price 1\n", 1},
        {"Bank Cash 1\n", 1},
        {"Bank.Cash\n5\n", 2},          // the value is read from the next line, which is then read as a line of its own
        {"Ride[0].Shape\n---\n. Z\n---\n", 3},
        {"Ride[0].Shape\n. *\n---\n", 2},
        {"Ride[0].Shape.Price\n---\n.\n---\n", 1},
        {"Ride[0].Shape\n---\n.\n", 2},     // never closed: the line it opens on
    };
    for (const Case& c : cases) {
        const int line = SamUnreadableLine(table, std::string(c.text));
        if (line != c.line) std::printf("  \"%s\": line %d, expected %d\n", c.text, line, c.line);
        CHECK_EQ(line, c.line);
    }
    // the game's text ends at a NUL
    CHECK_EQ(SamUnreadableLine(table, std::string("Bank.Cash 1\n\0Bank.Cash x\n", 26)), 0);
    // no table (a name nothing registered): only what every table holds
    const std::vector<SamRow> none;
    CHECK_EQ(SamUnreadableLine(none, "Anything.Goes 5 7\nWhatever[3].X.Y \"s\"\nMap.Shape\n---\nZZZ\n---\n\nA.B 1\n"), 0);
    CHECK_EQ(SamUnreadableLine(none, "A.B 1\nA B 1\n"), 2);
    CHECK_EQ(SamUnreadableLine(none, "A.B 1\n---\n"), 2);
    CHECK_EQ(SamUnreadableLine(none, "A[x].B 1\n"), 1);
    // by file name, any case and folder, as the launcher registers the game's tables
    SetSamTable("Test.sam", table.data(), table.size());
    CHECK_EQ(SamUnreadableLine(std::string("data/levels/TEST.SAM"), std::string("Vault.Cash 1\n")), 1);
    CHECK_EQ(SamUnreadableLine(std::string("data/levels/other.sam"), std::string("Vault.Cash 1\n")), 0);
    CHECK_EQ(SamTableFor("x/test.sam").size(), table.size());
}

// ---- the kept originals, with the player's own US game folder (TPW_GAME_DIR, the folder that holds "data"): a copy of
// every verified .sam made at the first start, restored over an edit; an edit made before any copy gets one from a drop

void TestOriginals(const std::string& scratch) {
    const std::string game = Env("TPW_GAME_DIR");
    if (game.empty()) {
        Skip("the kept originals: TPW_GAME_DIR (a US game folder) is not set");
        return;
    }
    std::printf("kept originals of .sam files\n");
    std::vector<const ManifestRecord*> sams;
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        const ManifestRecord& r = ManifestRecords()[i];
        const size_t n = std::strlen(r.dest);
        if ((r.editions & kEditionUs) && n > 4 && std::strcmp(r.dest + n - 4, ".sam") == 0) sams.push_back(&r);
    }
    const ManifestRecord* standard = RecordOf("data/levels/Standard.sam", kEditionUs);
    CHECK(standard && !sams.empty());
    if (!standard) return;
    const std::string original = ReadText(game + "/" + standard->dest);
    if (original.empty()) {
        Skip("the kept originals: no data/levels/Standard.sam in TPW_GAME_DIR");
        return;
    }
    const std::string edited = original + "\r\nBankAccountInfo.InitialCash 123456789\r\n";
    std::string err;
    ImporterConfig cfg;
    cfg.dataRoot = scratch + "/originals-data";
    RemoveTree(cfg.dataRoot);
    {   // every .sam as the disc has it: the first start keeps a copy of each
        Importer layout(cfg);
        for (const ManifestRecord* r : sams) {
            const std::string bytes = ReadText(game + "/" + r->dest);
            if (!bytes.empty()) WriteFile(layout.DestinationPath(kEditionUs, *r), bytes);
        }
    }
    {
        Importer imp(cfg);
        CHECK(imp.Open(err));
        size_t copies = 0;
        for (const ManifestRecord* r : sams) {
            if (!Exists(imp.DestinationPath(kEditionUs, *r))) continue;
            ++copies;
            CHECK(ReadText(imp.OriginalPath(kEditionUs, *r)) == ReadText(imp.DestinationPath(kEditionUs, *r)));
        }
        std::printf("  %zu originals kept\n", copies);
        CHECK(copies > 0);
        CHECK(fs::path(imp.OriginalPath(kEditionUs, *standard)).generic_string() ==
              cfg.dataRoot + "/assets/us/originals/data/levels/Standard.sam");
        WriteFile(imp.DestinationPath(kEditionUs, *standard), edited);
    }
    {   // the edit is kept and listed with its original; Restore original puts the disc's file back
        Importer imp(cfg);
        CHECK(imp.Open(err));
        CHECK_EQ(imp.Status(kEditionUs).edited, 1);
        std::vector<EditedFile> list = imp.EditedFiles(kEditionUs);
        CHECK(list.size() == 1 && list[0].original && list[0].unreadableLine == 0);
        CHECK(ReadText(imp.OriginalPath(kEditionUs, *standard)) == original);   // never the edit
        CHECK(imp.RestoreOriginal(kEditionUs, standard->dest, err));
        CHECK(ReadText(imp.DestinationPath(kEditionUs, *standard)) == original);
        CHECK_EQ(imp.Status(kEditionUs).edited, 0);
        CHECK(imp.EditedFiles(kEditionUs).empty());
    }
    {   // and it stays verified: the next start rehashes nothing
        Importer imp(cfg);
        CHECK(imp.Open(err));
        CHECK_EQ(imp.LastOpenCheck().rehashed, 0);
        CHECK(imp.LastOpenCheck().edited.empty());
    }
    RemoveTree(cfg.dataRoot);

    // an install edited before it had copies: no copy from the edit; dropping the disc's file keeps one and leaves the
    // edit in place; then it can be restored
    {
        Importer layout(cfg);
        WriteFile(layout.DestinationPath(kEditionUs, *standard), edited);
    }
    const std::string drop = scratch + "/originals-drop";
    RemoveTree(drop);
    WriteFile(drop + "/Standard.sam", original);
    {
        Importer imp(cfg);
        CHECK(imp.Open(err));
        CHECK(!Exists(imp.OriginalPath(kEditionUs, *standard)));
        CHECK_EQ(imp.VerifyEdition(kEditionUs, err), 0);
        std::vector<EditedFile> list = imp.EditedFiles(kEditionUs);
        CHECK(list.size() == 1 && !list[0].original);
        imp.Import({drop});
        CHECK(ReadText(imp.DestinationPath(kEditionUs, *standard)) == edited);
        CHECK(ReadText(imp.OriginalPath(kEditionUs, *standard)) == original);
        list = imp.EditedFiles(kEditionUs);
        CHECK(list.size() == 1 && list[0].original);
        CHECK(imp.RestoreOriginal(kEditionUs, standard->dest, err));
        CHECK(ReadText(imp.DestinationPath(kEditionUs, *standard)) == original);
    }
    RemoveTree(cfg.dataRoot);
    RemoveTree(drop);
}

// ---- damaged containers: an error naming the file and the reason, never a drop that quietly holds nothing

void Put16(std::string& v, uint32_t x) {
    v += char(x & 255);
    v += char((x >> 8) & 255);
}
void Put32(std::string& v, uint32_t x) {
    Put16(v, x & 0xffff);
    Put16(v, x >> 16);
}

// A Bullfrog DWFB wad (wad.cpp) of stored entries: the 0x58-byte header, 40-byte entries, the names, the data.
std::string WadOf(const std::vector<std::pair<std::string, std::string>>& files) {
    std::string out(0x58, '\0');
    std::memcpy(&out[0], "DWFB", 4);
    out[4] = 2;
    std::string names, data;
    const uint32_t table = 0x58 + 40 * uint32_t(files.size());
    uint32_t nameAt = table;
    for (const auto& f : files) nameAt += uint32_t(f.first.size() + 1);
    uint32_t dataAt = nameAt;
    std::string entries;
    uint32_t nameOff = table;
    for (const auto& f : files) {
        Put32(entries, 0);
        Put32(entries, nameOff);
        Put32(entries, uint32_t(f.first.size() + 1));
        Put32(entries, dataAt);
        Put32(entries, uint32_t(f.second.size()));
        Put32(entries, 0);
        Put32(entries, uint32_t(f.second.size()));
        entries.append(12, '\0');   // the rest of the 40-byte entry
        names += f.first + '\0';
        data += f.second;
        nameOff += uint32_t(f.first.size() + 1);
        dataAt += uint32_t(f.second.size());
    }
    std::string count;
    Put32(count, uint32_t(files.size()));
    Put32(count, table);
    Put32(count, uint32_t(names.size()));
    out.replace(0x48, 12, count);
    return out + entries + names + data;
}

// A zip of one member, built here from bytes. `crcXor` is XORed into the CRC both headers carry (0: a good zip);
// `packed` stores the member deflated; `cutEnd` drops that many bytes from the end (the central directory).
std::string ZipOf(const std::string& name, const std::string& data, bool packed, uint32_t crcXor, size_t cutEnd = 0) {
    std::string body = data;
    if (packed) {
        z_stream z{};
        deflateInit2(&z, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        body.assign(deflateBound(&z, uLong(data.size())) + 16, '\0');
        z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        z.avail_in = uInt(data.size());
        z.next_out = reinterpret_cast<Bytef*>(&body[0]);
        z.avail_out = uInt(body.size());
        deflate(&z, Z_FINISH);
        body.resize(z.total_out);
        deflateEnd(&z);
    }
    uint32_t crc = uint32_t(crc32(crc32(0, nullptr, 0), reinterpret_cast<const Bytef*>(data.data()), uInt(data.size()))) ^ crcXor;
    uint16_t method = packed ? 8 : 0;
    std::string out;
    Put32(out, 0x04034b50);
    Put16(out, 20);
    Put16(out, 0);
    Put16(out, method);
    Put32(out, 0);
    Put32(out, crc);
    Put32(out, uint32_t(body.size()));
    Put32(out, uint32_t(data.size()));
    Put16(out, uint32_t(name.size()));
    Put16(out, 0);
    out += name;
    out += body;
    uint32_t cdStart = uint32_t(out.size());
    Put32(out, 0x02014b50);
    Put16(out, 20);
    Put16(out, 20);
    Put16(out, 0);
    Put16(out, method);
    Put32(out, 0);
    Put32(out, crc);
    Put32(out, uint32_t(body.size()));
    Put32(out, uint32_t(data.size()));
    Put16(out, uint32_t(name.size()));
    Put16(out, 0);
    Put16(out, 0);
    Put16(out, 0);
    Put16(out, 0);
    Put32(out, 0);
    Put32(out, 0);
    out += name;
    uint32_t cdSize = uint32_t(out.size()) - cdStart;
    Put32(out, 0x06054b50);
    Put16(out, 0);
    Put16(out, 0);
    Put16(out, 1);
    Put16(out, 1);
    Put32(out, cdSize);
    Put32(out, cdStart);
    Put16(out, 0);
    if (cutEnd) out.resize(out.size() - cutEnd);
    return out;
}

bool AnyContains(const std::vector<std::string>& lines, const std::string& text) {
    for (const std::string& l : lines) {
        if (l.find(text) != std::string::npos) return true;
    }
    return false;
}

size_t FileCount(const std::string& dir) {
    size_t n = 0;
    for (const auto& kv : Tree(dir, false)) n += kv.first.find("/.staging/") == std::string::npos && kv.first != "launcher.ini";
    return n;
}

// ---- the films: a folder holds each as the Mac CD's .mov or the PC CD's .tgq (FilmCounterpart), counted once

void TestFilms(const std::string& scratch) {
    std::printf("films: the Mac CD's or the PC CD's\n");
    std::string root = scratch + "/films-data";
    RemoveTree(root);
    ImporterConfig cfg;
    cfg.dataRoot = root;
    // Files of the films' sizes (sparse, no content), which the stat sweep of Open() counts as present when there is no
    // verified.tsv (TestStartupCheck).
    auto place = [&](Edition e, const char* extension) {
        Importer layout(cfg);
        size_t n = 0;
        for (size_t i = 0; i < ManifestRecordCount(); ++i) {
            const ManifestRecord& r = ManifestRecords()[i];
            const std::string dest = r.dest;
            if (!FilmCounterpart(r, e) || dest.compare(dest.size() - 4, 4, extension) != 0) continue;
            const std::string path = layout.DestinationPath(e, r);
            WriteFile(path, "");
            fs::resize_file(path, RecordHash(r, 0).size);
            ++n;
        }
        CHECK_EQ(n, 9);
        fs::remove(layout.VerifiedPath());
    };
    auto status = [&](Edition e) {
        Importer imp(cfg);
        std::string err;
        CHECK(imp.Open(err));
        return imp.Status(e);
    };
    for (Edition e : {kEditionUs, kEditionEur}) {
        const size_t required = CountRecords(e, true) - 9;   // a film and its .tgq are one required file
        EditionStatus none = status(e);
        CHECK_EQ(none.required, required);
        CHECK_EQ(none.requiredPresent, 0);
        CHECK_EQ(none.missing[kSourceMacCd] + none.missing[kSourcePcCd], required);   // a film missing is the .mov
        // the PC CD's films alone: each counts for its .mov
        place(e, ".tgq");
        EditionStatus pc = status(e);
        CHECK_EQ(pc.present, 9);
        CHECK_EQ(pc.required, required);
        CHECK_EQ(pc.requiredPresent, 9);
        CHECK_EQ(none.missing[kSourceMacCd] - pc.missing[kSourceMacCd], 9);
        CHECK_EQ(none.missing[kSourcePcCd], pc.missing[kSourcePcCd]);
        // and the Mac CD's too: the same 9 required files
        place(e, ".mov");
        EditionStatus both = status(e);
        CHECK_EQ(both.present, 18);
        CHECK_EQ(both.requiredPresent, 9);
        CHECK_EQ(both.missing[kSourceMacCd], pc.missing[kSourceMacCd]);
        // the Mac CD's alone, as before
        Importer layout(cfg);
        for (size_t i = 0; i < ManifestRecordCount(); ++i) {
            const ManifestRecord& r = ManifestRecords()[i];
            const std::string dest = r.dest;
            if (FilmCounterpart(r, e) && dest.compare(dest.size() - 4, 4, ".tgq") == 0) fs::remove(layout.DestinationPath(e, r));
        }
        fs::remove(layout.VerifiedPath());
        EditionStatus mac = status(e);
        CHECK_EQ(mac.present, 9);
        CHECK_EQ(mac.requiredPresent, 9);
        CHECK_EQ(mac.missing[kSourceMacCd], pc.missing[kSourceMacCd]);
        CHECK_EQ(mac.missing[kSourcePcCd], pc.missing[kSourcePcCd]);
        std::printf("  %s: required %zu; with the PC films %zu present, missing mac %zu pc %zu; with the Mac films the same\n",
                    EditionName(e), required, pc.requiredPresent, pc.missing[kSourceMacCd], pc.missing[kSourcePcCd]);
        RemoveTree(root);
    }
}

void TestDamagedContainers(const std::string& scratch) {
    std::printf("damaged containers\n");
    std::string root = scratch + "/damaged-data", drop = scratch + "/damaged-drop";
    RemoveTree(root);
    RemoveTree(drop);
    // a member the manifest knows by size, so that the importer reads it
    uint64_t size = 0;
    for (size_t i = 0; i < ManifestHashCount(); ++i) {
        uint64_t s = ManifestHashes()[i].size;
        if (s > 0 && (size == 0 || s < size)) size = s;
    }
    CHECK(size > 0);
    const std::string member(static_cast<size_t>(size), 'j');

    ImporterConfig cfg;
    cfg.dataRoot = root;
    Importer imp(cfg);
    std::string err;
    CHECK(imp.Open(err));

    // a deflated member whose CRC is wrong: one error naming the zip and the member, and nothing written
    WriteFile(drop + "/bad-crc.zip", ZipOf("damaged.dat", member, true, 0x5a5a5a5a));
    ImportReport crc = imp.Import({drop + "/bad-crc.zip"});
    CHECK_EQ(crc.errors.size(), 1);
    CHECK(AnyContains(crc.errors, "bad-crc.zip") && AnyContains(crc.errors, "damaged.dat") &&
          AnyContains(crc.errors, "checksum mismatch"));
    CHECK_EQ(crc.written, 0);
    CHECK_EQ(FileCount(root), 0);
    CHECK(Tree(root + "/assets/.staging", false).empty());
    std::vector<ImportProgress> ev = Events(imp);
    CHECK_EQ(CountKind(ev, ImportProgress::Kind::Error), 1);
    CHECK(crc.Summary().find("1 errors") != std::string::npos);

    // the same zip, good, writes nothing here either (the member is no game file by content) and has no error
    WriteFile(drop + "/good.zip", ZipOf("damaged.dat", member, true, 0));
    ImportReport good = imp.Import({drop + "/good.zip"});
    CHECK(good.errors.empty());
    CHECK_EQ(good.containers, 1);
    Events(imp);

    // a deflated member with a flipped bit in its compressed data
    {
        std::string z = ZipOf("flipped.dat", member, true, 0);
        z[30 + 11 + 1] = char(z[30 + 11 + 1] ^ 0x55);
        WriteFile(drop + "/flipped.zip", z);
        ImportReport r = imp.Import({drop + "/flipped.zip"});
        CHECK_EQ(r.errors.size(), 1);
        CHECK(AnyContains(r.errors, "flipped.zip") && AnyContains(r.errors, "flipped.dat"));
        CHECK_EQ(r.written, 0);
        Events(imp);
    }

    // a truncated zip dropped on its own: an error with its name, not a lone file of unknown content
    WriteFile(drop + "/cut.zip", ZipOf("damaged.dat", member, true, 0, 30));
    ImportReport cut = imp.Import({drop + "/cut.zip"});
    CHECK_EQ(cut.errors.size(), 1);
    CHECK(AnyContains(cut.errors, "cut.zip"));
    CHECK_EQ(cut.written, 0);
    CHECK_EQ(cut.unknownFiles, 0);
    Events(imp);

    // a truncated zip inside a folder, and a plain file that only has a container's name (stays silent)
    WriteFile(drop + "/folder/inner-cut.zip", ZipOf("damaged.dat", Junk(3000), false, 0, 30));
    WriteFile(drop + "/folder/plain.zip", Junk(5000));
    ImportReport folder = imp.Import({drop + "/folder"});
    CHECK_EQ(folder.errors.size(), 1);
    CHECK(AnyContains(folder.errors, "folder/inner-cut.zip"));
    CHECK(!AnyContains(folder.errors, "plain.zip"));
    CHECK_EQ(folder.written, 0);
    ev = Events(imp);
    CHECK_EQ(CountKind(ev, ImportProgress::Kind::Error), 1);
    ImportReport plain = imp.Import({drop + "/folder/plain.zip"});
    CHECK(plain.errors.empty());
    CHECK_EQ(plain.unknownFiles, 1);
    Events(imp);

    // nesting: the limit (6 zips inside each other) opens, one more is an error that says so
    for (int zips : {kMaxNesting, kMaxNesting + 1}) {
        std::string level = Junk(3000);
        for (int i = 0; i < zips; i++) level = ZipOf(i == 0 ? "leaf.dat" : "n.zip", level, false, 0);
        std::string path = drop + "/chain" + std::to_string(zips) + ".zip";
        WriteFile(path, level);
        ImportReport r = imp.Import({path});
        if (zips <= kMaxNesting) {
            CHECK(r.errors.empty());
            CHECK_EQ(r.containers, zips);
            CHECK_EQ(r.unknownFiles, 1);
        } else {
            CHECK_EQ(r.errors.size(), 1);
            CHECK(AnyContains(r.errors, "nested too deeply") && AnyContains(r.errors, "chain7.zip/n.zip"));
            CHECK_EQ(r.containers, kMaxNesting);
        }
        Events(imp);
    }

    // the Windows disc's data/fonts.wad is opened as a container (its members are matched by content); the same bytes
    // under another name are one plain file, and a damaged fonts.wad is an error that names it
    {
        std::string wad = WadOf({{"fonts\\one.ttf", member}, {"fonts\\two.ttf", Junk(700)}});
        WriteFile(drop + "/pc/Data/fonts.wad", wad);
        WriteFile(drop + "/other/Data/other.wad", wad);
        ImportReport fonts = imp.Import({drop + "/pc"});
        ImportReport other = imp.Import({drop + "/other"});
        CHECK(fonts.errors.empty() && other.errors.empty());
        CHECK_EQ(fonts.containers, other.containers + 1);
        CHECK_EQ(fonts.entries, other.entries + 2);
        CHECK_EQ(fonts.unknownFiles, 2);   // two.ttf and the wad; one.ttf has a manifest size, so it is read and hashed
        CHECK_EQ(other.unknownFiles, 1);
        CHECK_EQ(fonts.written, 0);
        std::string bad = wad;
        bad[0x48] = char(0xff);   // more entries than the file holds
        WriteFile(drop + "/badwad/Data/fonts.wad", bad);
        ImportReport damaged = imp.Import({drop + "/badwad"});
        CHECK_EQ(damaged.errors.size(), 1);
        CHECK(AnyContains(damaged.errors, "fonts.wad"));
        Events(imp);
    }
    CHECK_EQ(FileCount(root), 0);
    RemoveTree(root);
    RemoveTree(drop);
}

// ---- the discs

// ---- 7-Zip and RAR archives (tests/archives, test_archives.h): opened like a zip, with their members going the same way

std::string ArchiveFixture(const std::string& name) {
    return tpw::assets::test::UuDecode(std::string(TPW_ARCHIVE_FIXTURES) + "/" + name + ".uu");
}

bool StagingEmpty(const std::string& root) { return Tree(root + "/assets/.staging", false).empty(); }

void TestArchives(const std::string& scratch) {
    std::printf("7-Zip and RAR archives\n");
    using tpw::assets::test::RarMember;
    using tpw::assets::test::RarOf;
    std::string root = scratch + "/archive-data", drop = scratch + "/archive-drop";
    RemoveTree(root);
    RemoveTree(drop);
    const std::string sevenZip = ArchiveFixture("ps1-pair.7z");
    if (sevenZip.empty()) {
        Skip(std::string("archives: no fixtures in ") + TPW_ARCHIVE_FIXTURES);
        return;
    }
    auto raw = Ps1RawFixture(Ps1ImageFixture(), 2352);
    const std::string bin(raw.begin(), raw.end());
    const std::string cue = "FILE \"Synthetic Disc.bin\" BINARY\r\n  TRACK 01 MODE2/2352\r\n    INDEX 01 00:00:00\r\n";
    ImporterConfig cfg;
    cfg.dataRoot = root;
    Importer imp(cfg);
    std::string err;
    CHECK(imp.Open(err));

    // The loose pair, then the same pair in a 7-Zip archive, a RAR archive and a RAR archive in two parts: the same result.
    WriteFile(drop + "/loose/Synthetic Disc.bin", bin);
    WriteFile(drop + "/loose/Synthetic Disc.cue", cue);
    ImportReport loose = imp.Import({drop + "/loose/Synthetic Disc.cue"});
    CHECK(loose.errors.empty());
    CHECK_EQ(loose.playStation.size(), 1);
    CHECK(!loose.playStation.empty() && loose.playStation[0].find("drop your US or European PC disc") != std::string::npos);
    Events(imp);
    const std::vector<RarMember> pair = {{"Synthetic Disc.bin", bin}, {"Synthetic Disc.cue", cue}};
    WriteFile(drop + "/pair.7z", sevenZip);
    WriteFile(drop + "/pair.rar", RarOf(pair)[0]);
    std::vector<std::string> parts = RarOf(pair, 2);
    WriteFile(drop + "/volumes/pair.part1.rar", parts[0]);
    WriteFile(drop + "/volumes/pair.part2.rar", parts[1]);
    for (const std::string& path : {drop + "/pair.7z", drop + "/pair.rar", drop + "/volumes/pair.part1.rar",
                                    drop + "/volumes/pair.part2.rar", drop + "/volumes"}) {
        ImportReport r = imp.Import({path});
        CHECK(r.errors.empty());
        CHECK_EQ(r.containers, loose.containers + 2);   // the archive and the image in it, as from a zip
        CHECK(r.playStation == loose.playStation);
        CHECK(r.unknownFiles == 1);   // the cue, as from a zip
        CHECK(StagingEmpty(root));
        std::vector<ImportProgress> ev = Events(imp);
        CHECK(std::any_of(ev.begin(), ev.end(), [](const ImportProgress& e) {
            return e.kind == ImportProgress::Kind::Scanning && e.path == "unpacking Synthetic Disc.bin";
        }));
        if (!r.errors.empty() || r.playStation != loose.playStation) {
            std::printf("  %s: %s\n", path.c_str(), r.errors.empty() ? r.Summary().c_str() : r.errors[0].c_str());
        }
    }
    // Both parts dropped together open the set once.
    ImportReport both = imp.Import({drop + "/volumes/pair.part2.rar", drop + "/volumes/pair.part1.rar"});
    CHECK(both.errors.empty() && both.containers == loose.containers + 2 && both.playStation.size() == 1);
    if (!both.errors.empty() || both.playStation.size() != 1) std::printf("  both parts: %s\n", both.Summary().c_str());
    // A part without the first: an error naming the first part, not an empty drop.
    WriteFile(drop + "/alone/pair.part2.rar", parts[1]);
    ImportReport alone = imp.Import({drop + "/alone/pair.part2.rar"});
    CHECK_EQ(alone.errors.size(), 1);
    CHECK(AnyContains(alone.errors, "pair.part2.rar") && AnyContains(alone.errors, "pair.part1.rar"));
    // The parts inside a zip.
    {
        std::string inner = ZipOf("pair.part1.rar", parts[0], false, 0);
        WriteFile(drop + "/zipped/one.zip", inner);
        ImportReport partial = imp.Import({drop + "/zipped/one.zip"});
        CHECK_EQ(partial.errors.size(), 1);   // the second part is missing: the volume set is incomplete
        CHECK(AnyContains(partial.errors, "pair.part1.rar"));
    }
    Events(imp);

    // A nested folder in a 7-Zip archive, with the pair zipped in it: the disc is found.
    WriteFile(drop + "/nested.7z", ArchiveFixture("nested.7z"));
    ImportReport nested = imp.Import({drop + "/nested.7z"});
    CHECK(nested.errors.empty());
    CHECK_EQ(nested.containers, loose.containers + 3);   // the 7z, the zip in it and the image in that
    CHECK(nested.playStation == loose.playStation);
    // An archive inside a folder is opened like a zip there (the 7z is under the 2 KB below which no container is looked
    // for, so the stored RAR stands in).
    WriteFile(drop + "/folder/sub/pair.rar", RarOf(pair)[0]);
    ImportReport inFolder = imp.Import({drop + "/folder"});
    CHECK(inFolder.errors.empty() && inFolder.playStation == loose.playStation);
    if (!inFolder.errors.empty() || inFolder.playStation != loose.playStation) {
        std::printf("  in a folder: %s %s\n", inFolder.Summary().c_str(), inFolder.errors.empty() ? "" : inFolder.errors[0].c_str());
    }

    // Password-protected, an unsupported method, cut short: one error naming the file, saying why.
    struct Bad {
        std::string name, bytes, says;
    };
    const std::vector<Bad> bad = {
        {"locked.7z", ArchiveFixture("encrypted-data.7z"), "password"},
        {"locked-names.7z", ArchiveFixture("encrypted-names.7z"), "password"},
        {"locked.rar", ArchiveFixture("test_read_format_rar4_encrypted.rar"), "password"},
        {"locked5.rar", ArchiveFixture("test_read_format_rar5_encrypted.rar"), "password"},
        {"bzip2.7z", ArchiveFixture("method-bzip2.7z"), "compression method"},
        {"cut.7z", sevenZip.substr(0, sevenZip.size() / 2), "damaged or incomplete"},
        {"cut.rar", RarOf(pair)[0].substr(0, 100000), "damaged or incomplete"},
    };
    for (const Bad& b : bad) {
        WriteFile(drop + "/bad/" + b.name, b.bytes);
        ImportReport r = imp.Import({drop + "/bad/" + b.name});
        CHECK_EQ(r.errors.size(), 1);
        CHECK(AnyContains(r.errors, b.name + ": this archive") && AnyContains(r.errors, b.says));
        CHECK(r.Summary().find("1 errors") != std::string::npos);
        CHECK(StagingEmpty(root));
        if (r.errors.size() != 1 || !AnyContains(r.errors, b.says)) {
            std::printf("  %s: %s\n", b.name.c_str(), r.errors.empty() ? r.Summary().c_str() : r.errors[0].c_str());
        }
    }
    Events(imp);

    // Cancel while a big member is unpacking: stopped, no error, nothing left in the staging folder.
    {
        WriteFile(drop + "/big/big.rar", RarOf({{"big.bin", std::string(96u << 20, 'b')}})[0]);
        std::string startErr;
        CHECK(imp.Start({drop + "/big/big.rar"}, startErr));
        bool cancelled = false;
        size_t hashing = 0;
        while (imp.Running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            for (ImportProgress& e : imp.Progress().Drain()) {
                if (!cancelled && e.kind == ImportProgress::Kind::Scanning && e.path.rfind("unpacking ", 0) == 0) {
                    imp.Cancel();
                    cancelled = true;
                }
                if (e.kind == ImportProgress::Kind::Hashing) hashing++;
            }
        }
        ImportReport r = imp.Wait();
        for (const ImportProgress& e : Events(imp)) hashing += e.kind == ImportProgress::Kind::Hashing;
        // Timing decides whether the cancel lands while unpacking (the usual case) or after; both must leave nothing.
        std::printf("  cancel landed %s\n", hashing == 0 ? "while unpacking" : "after unpacking");
        CHECK(cancelled && r.cancelled);
        CHECK(r.errors.empty());
        CHECK(StagingEmpty(root));
    }
    RemoveTree(root);
    RemoveTree(drop);
}

struct Discs {
    std::string toast, eurImg, eurFirst, goldZip, goldRides, allitems, gameEur, pcUs, pcUsFirst, pcEur, ps1;
};

bool CompareUs(const std::string& folder) {
    std::map<std::string, FileInfo> want;
    if (!UsExpected(want)) {
        Skip("US tree comparison: import_originals.py not found under " TPW_DEVTOOLS_DIR);
        return false;
    }
    std::map<std::string, FileInfo> have = Tree(folder, true);
    CHECK_EQ(want.size(), 770);
    CHECK_EQ(have.size(), want.size());
    size_t same = 0;
    for (const auto& kv : want) {
        auto it = have.find(kv.first);
        if (it != have.end() && it->second.size == kv.second.size && it->second.sha == kv.second.sha) {
            ++same;
        } else {
            std::printf("    US differs: %s\n", kv.first.c_str());
        }
    }
    for (const auto& kv : have) {
        if (!want.count(kv.first)) std::printf("    US extra: %s\n", kv.first.c_str());
    }
    CHECK_EQ(same, want.size());
    std::printf("  US tree: %zu of %zu files equal to CHECKSUMS\n", same, want.size());
    return true;
}

void CompareEur(const std::string& folder, const std::string& gameEur) {
    std::map<std::string, std::string> want;
    if (!EurExpected(want)) {
        Skip("European tree comparison: import_eur.py not found under " TPW_DEVTOOLS_DIR);
        return;
    }
    std::map<std::string, FileInfo> have = Tree(folder, true);
    CHECK_EQ(want.size(), 913);
    CHECK_EQ(have.size(), want.size());
    size_t same = 0;
    for (const auto& kv : want) {
        auto it = have.find(kv.first);
        if (it != have.end() && it->second.sha == kv.second) ++same;
        else std::printf("    EUR differs: %s\n", kv.first.c_str());
    }
    for (const auto& kv : have) {
        if (!want.count(kv.first)) std::printf("    EUR extra: %s\n", kv.first.c_str());
    }
    CHECK_EQ(same, want.size());
    std::printf("  European tree: %zu of %zu files equal to EXPECTED\n", same, want.size());
    if (gameEur.empty()) {
        Skip("comparison with game-eur: TPW_GDI_ROOT/game-eur/tpw-eur not found");
        return;
    }
    std::map<std::string, FileInfo> ref = Tree(gameEur, false);
    size_t equal = 0;
    for (const auto& kv : ref) {
        auto it = have.find(kv.first);
        if (it != have.end() && it->second.size == kv.second.size) ++equal;
    }
    CHECK_EQ(ref.size(), have.size());
    CHECK_EQ(equal, ref.size());
    std::printf("  European tree: %zu of %zu names and sizes equal to game-eur\n", equal, ref.size());
}

// Runs a drop on the worker thread, checking the progress events as a UI would read them.
ImportReport RunAsync(Importer& imp, const std::vector<std::string>& paths, bool cancelEarly) {
    std::string err;
    CHECK(imp.Start(paths, err));
    std::string err2;
    CHECK(!imp.Start(paths, err2));  // one at a time
    std::vector<ImportProgress> all;
    bool cancelled = false;
    while (imp.Running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        for (ImportProgress& e : imp.Progress().Drain()) {
            if (cancelEarly && !cancelled && e.kind == ImportProgress::Kind::Hashing && e.n >= 3) {
                imp.Cancel();
                cancelled = true;
            }
            all.push_back(std::move(e));
        }
    }
    ImportReport r = imp.Wait();
    for (ImportProgress& e : imp.Progress().Drain()) all.push_back(std::move(e));
    CHECK(!all.empty() && all.back().kind == ImportProgress::Kind::Finished);
    uint64_t lastN = 0, total = 0, maxN = 0;
    bool monotonic = true;
    for (const ImportProgress& e : all) {
        if (e.kind != ImportProgress::Kind::Hashing) continue;
        monotonic = monotonic && e.n >= lastN && e.bytes <= e.totalBytes;
        lastN = e.n;
        total = e.total;
        maxN = std::max<uint64_t>(maxN, e.n);
    }
    CHECK(monotonic);
    if (!cancelEarly) CHECK_EQ(maxN, total);
    std::printf("  %zu progress events, %llu of %llu files\n", all.size(), static_cast<unsigned long long>(maxN),
                static_cast<unsigned long long>(total));
    return r;
}

void TestUsAndEur(const Discs& d, const std::string& scratch) {
    if (d.toast.empty() || d.eurImg.empty()) {
        Skip("US and European import: the US toast zip or the European image is not under TPW_TEST_TRANSFER");
        return;
    }
    std::printf("US toast, then the European image\n");
    std::string root = scratch + "/us-eur";
    RemoveTree(root);
    ImporterConfig cfg;
    cfg.dataRoot = root;
    Importer imp(cfg);
    std::string err;
    CHECK(imp.Open(err));

    // a cancelled drop leaves nothing half-written
    ImportReport stopped = RunAsync(imp, {d.toast}, true);
    CHECK(stopped.cancelled);
    CHECK(Tree(root + "/assets/.staging", false).empty());
    PrintStatus("cancelled toast", stopped);

    // (a) the toast alone: the US tree; the European Mac files
    ImportReport us = RunAsync(imp, {d.toast}, false);
    PrintStatus("toast", us);
    CHECK(us.errors.empty());
    CHECK(!us.cancelled);
    CHECK(us.status[0].unlocked);
    CHECK_EQ(us.status[0].present, 770);
    CHECK_EQ(us.status[1].missing[kSourceMacCd], 0);
    CHECK(us.status[1].missing[kSourcePcCd] > 0);
    CHECK(!us.status[1].unlocked);
    size_t eurMac = 0, eurMacPresent = 0;
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        const ManifestRecord& r = ManifestRecords()[i];
        if (!(r.editions & kEditionEur) || r.source != kSourceMacCd) continue;
        ++eurMac;
        if (Exists(imp.DestinationPath(kEditionEur, r))) ++eurMacPresent;
        else std::printf("    European Mac record missing: %s\n", r.dest);
    }
    CHECK_EQ(eurMacPresent, eurMac);
    std::printf("  European files from the Mac CD: %zu of %zu; still missing %zu files from %s\n", eurMacPresent, eurMac,
                us.status[1].missingFiles[kSourcePcCd], SourceDiscName(kSourcePcCd));
    CHECK(Tree(root + "/assets/.staging", false).empty());
    CompareUs(imp.GameFolder(kEditionUs));

    // (b) add the PC disc: the European tree
    ImportReport eur = imp.Import({d.eurImg});
    Events(imp);
    PrintStatus("european", eur);
    for (const std::string& v : eur.differentVersions) std::printf("    different version (not used): %s\n", v.c_str());
    CHECK(eur.errors.empty());
    CHECK(eur.status[1].unlocked);
    CHECK_EQ(eur.status[1].present, 913);
    CHECK_EQ(eur.status[1].filesPresent, 845);
    // none: the PC tuning files are the European Windows edition's own; the other languages' speech files are not news
    CHECK_EQ(eur.differentVersions.size(), 0);
    CHECK(eur.status[5].unlocked);   // the same image holds the European Windows edition
    CHECK_EQ(eur.status[5].present, 817);
    CompareEur(imp.GameFolder(kEditionEur), d.gameEur);

    // (c) both again: nothing changes
    std::map<std::string, FileInfo> before = Tree(root + "/assets", false);
    ImportReport again = imp.Import({d.toast, d.eurImg});
    Events(imp);
    PrintStatus("both again", again);
    CHECK(again.ChangedNothing());
    CHECK_EQ(again.written, 0);
    CHECK(again.alreadyThere >= 913);
    std::map<std::string, FileInfo> after = Tree(root + "/assets", false);
    CHECK(before.size() == after.size());
    bool same = before.size() == after.size();
    for (auto a = before.begin(), b = after.begin(); same && a != before.end(); ++a, ++b) {
        same = a->first == b->first && a->second.size == b->second.size;
    }
    CHECK(same);
    LauncherIni ini;
    CHECK(LoadLauncherIni(root + "/launcher.ini", ini, err));
    CHECK(ini.editions["us"] == "unlocked" && ini.editions["eur"] == "unlocked" && ini.editions["gold"] == "absent");
    CHECK(ini.editions["eur-pc"] == "unlocked" && ini.editions["us-pc"] == "partial");
    RemoveTree(root);
}

// The Windows discs alone, no Mac file: the European image and a European Windows-only CD (even one sold as Sim Theme
// Park) unlock the European Windows edition and nothing else, the US 2.0 CD the US one; the US first release (1.0)
// unlocks nothing and reports its files as different versions.
void TestWindowsDiscs(const Discs& d, const std::string& scratch) {
    struct Case {
        const char* name;
        std::string path;
        int index;                // EditionIndex of the edition the disc is for
        bool unlocks;
    } cases[] = {{"the European Windows image", d.eurImg, 5, true},
                 {"a European Windows CD", d.pcEur, 5, true},
                 {"the US 2.0 Windows CD", d.pcUs, 4, true},
                 {"the US first release", d.pcUsFirst, 4, false}};
    for (const Case& c : cases) {
        if (c.path.empty()) {
            Skip(std::string("Windows disc import of ") + c.name + ": not under TPW_TEST_TRANSFER");
            continue;
        }
        std::printf("%s alone\n", c.name);
        std::string root = scratch + "/windows";
        RemoveTree(root);
        ImporterConfig cfg;
        cfg.dataRoot = root;
        Importer imp(cfg);
        std::string err;
        CHECK(imp.Open(err));
        ImportReport r = imp.Import({c.path});
        Events(imp);
        PrintStatus(c.name, r);
        for (const std::string& e : r.errors) std::printf("    error: %s\n", e.c_str());
        CHECK(r.errors.empty());
        const EditionStatus& s = r.status[c.index];
        CHECK_EQ(s.unlocked, c.unlocks);
        for (int i : {0, 1, 2, 4, 5}) {
            if (i != c.index) CHECK(!r.status[i].unlocked);
        }
        if (c.unlocks) {
            CHECK_EQ(s.present, c.index == 5 ? 817u : 749u);   // the films too: an installed folder lacks them
            CHECK_EQ(r.differentVersions.size(), 0);
            // the sign fonts, unpacked from data/fonts.wad, are plain TrueType files
            size_t fonts = 0;
            for (size_t i = 0; i < ManifestRecordCount(); ++i) {
                const ManifestRecord& m = ManifestRecords()[i];
                std::string dest = m.dest;
                if (!(m.editions & s.edition) || dest.size() < 4 || dest.compare(dest.size() - 4, 4, ".TTF") != 0) continue;
                std::ifstream f(imp.DestinationPath(s.edition, m), std::ios::binary);
                char head[4] = {};
                f.read(head, 4);
                if (f && std::memcmp(head, "\0\1\0\0", 4) == 0) ++fonts;
            }
            CHECK_EQ(fonts, 17);
        } else {
            CHECK(s.filesPresent > 0 && !r.differentVersions.empty());
            std::printf("  %zu of %zu files present, %zu different versions\n", s.filesPresent, s.files,
                        r.differentVersions.size());
            for (const std::string& v : r.differentVersions) std::printf("    different version (not used): %s\n", v.c_str());
        }
        RemoveTree(root);
    }
}

void TestFirstPressing(const Discs& d, const std::string& scratch) {
    if (d.eurFirst.empty()) {
        Skip("first pressing import: the image is not under TPW_TEST_TRANSFER");
        return;
    }
    std::printf("first pressing alone\n");
    std::string root = scratch + "/eur-first";
    RemoveTree(root);
    ImporterConfig cfg;
    cfg.dataRoot = root;
    Importer imp(cfg);
    std::string err;
    CHECK(imp.Open(err));
    ImportReport r = imp.Import({d.eurFirst});
    Events(imp);
    PrintStatus("first pressing", r);
    const EditionStatus& s = r.status[1];
    CHECK(r.errors.empty());
    CHECK(!s.unlocked);
    CHECK_EQ(s.files, 845);
    // every European file whose bytes are on this disc, some of them under another name there (found by content, not path)
    CHECK_EQ(s.filesPresent, 715);
    std::printf("  European: %zu of %zu files; still missing %zu from %s and %zu from %s; %zu different versions\n",
                s.filesPresent, s.files, s.missingFiles[kSourcePcCd], SourceDiscName(kSourcePcCd),
                s.missingFiles[kSourceMacCd], SourceDiscName(kSourceMacCd), r.differentVersions.size());
    CHECK_EQ(s.missingFiles[kSourcePcCd] + s.missingFiles[kSourceMacCd], s.files - s.filesPresent);
    RemoveTree(root);
}

// The built-in ride table (gold_ride_table.h) against the real RideConfig.txt just imported: the same 35 rows, in order.
void GoldTableMatchesList(const Importer& imp) {
    const ManifestRecord* list = FindByDest(kEditionGold, "gold/RideConfig.txt");
    CHECK(list != nullptr);
    std::ifstream in(imp.DestinationPath(kEditionGold, *list), std::ios::binary);
    std::string line, name;
    std::vector<std::pair<std::string, std::string>> rows;   // as platform/gold_rides.cpp reads them
    while (std::getline(in, line)) {
        size_t a = line.find_first_not_of(" \t\r\n"), b = line.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) continue;
        line = line.substr(a, b - a + 1);
        if (line == "x" || line == "X") { name.clear(); continue; }
        if (name.empty()) { name = line; continue; }
        std::string folder;
        for (char c : line) folder += (c == '\\') ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        rows.push_back({name, folder});
        name.clear();
    }
    CHECK_EQ(rows.size(), kGoldRideTableSize);
    for (size_t i = 0; i < rows.size() && i < kGoldRideTableSize; ++i) {
        CHECK(rows[i].first == kGoldRideTable[i].wad);
        CHECK(rows[i].second == kGoldRideTable[i].folder);
    }
}

void TestGold(const Discs& d, const std::string& scratch) {
    std::printf("Gold\n");
    struct Case {
        const char* name;
        std::string path;
    } cases[] = {{"allitems.exe", d.allitems}, {"the Gold zip", d.goldZip}};
    for (const Case& c : cases) {
        if (c.path.empty()) {
            Skip(std::string("Gold import from ") + c.name + ": not found");
            continue;
        }
        std::string root = scratch + "/gold";
        RemoveTree(root);
        ImporterConfig cfg;
        cfg.dataRoot = root;
        Importer imp(cfg);
        std::string err;
        CHECK(imp.Open(err));
        ImportReport r = imp.Import({c.path});
        Events(imp);
        PrintStatus(c.name, r);
        CHECK(r.errors.empty());
        CHECK(r.status[2].unlocked);
        CHECK_EQ(r.status[2].present, 36);
        CHECK_EQ(Tree(imp.GameFolder(kEditionGold), false).size(), 36);
        CHECK_EQ(r.status[0].present, 0);
        GoldTableMatchesList(imp);
        {   // RideConfig.txt is optional: the 35 rides alone unlock Gold (a download of the rides taken out of the installer)
            const ManifestRecord* list = FindByDest(kEditionGold, "gold/RideConfig.txt");
            CHECK(list != nullptr);
            std::error_code ec;
            CHECK(std::filesystem::remove(imp.DestinationPath(kEditionGold, *list), ec));
            ImporterConfig again;
            again.dataRoot = root;
            Importer without(again);
            CHECK(without.Open(err));
            EditionStatus s = without.Status(kEditionGold);
            CHECK(s.unlocked);
            CHECK_EQ(s.filesPresent, 35);
            CHECK_EQ(s.required, 35);
            CHECK_EQ(s.requiredPresent, 35);
            CHECK(std::string(s.StateWord()) == "unlocked");
        }
        CHECK(imp.RemoveEdition(kEditionGold, err));
        CHECK(!imp.Status(kEditionGold).unlocked);
        CHECK(!Exists(root + "/assets/gold"));
        RemoveTree(root);
    }
    if (d.goldRides.empty()) {
        Skip("Gold import from the rides alone: no gold-rides in the sources");
        return;
    }
    // A download of the 35 rides without RideConfig.txt: Gold is unlocked by the wads, and nothing else is written.
    std::string root = scratch + "/gold-rides";
    RemoveTree(root);
    ImporterConfig cfg;
    cfg.dataRoot = root;
    Importer imp(cfg);
    std::string err;
    CHECK(imp.Open(err));
    ImportReport r = imp.Import({d.goldRides});
    Events(imp);
    PrintStatus("the Gold rides alone", r);
    CHECK(r.errors.empty());
    CHECK(r.status[2].unlocked);
    CHECK_EQ(r.status[2].present, 35);
    CHECK_EQ(r.status[2].filesPresent, 35);
    CHECK_EQ(r.status[2].required, 35);
    CHECK_EQ(Tree(imp.GameFolder(kEditionGold), false).size(), 35);
    CHECK_EQ(r.status[0].present, 0);
    CHECK(imp.RemoveEdition(kEditionGold, err));
    RemoveTree(root);
}

// ---- the game's icon (icon.h): a .ico made here, its PNG, and a drop that carries one

// A .ico picture: `bpp` 4 (a 16-colour palette, entry i = blue i*16, green i*8, red i*4; pixel (x, y) is entry (x + y) % 16)
// or 24 (pixel (x, y) is blue x, green y, red 200); the AND mask clears the top-left pixel.
std::string IcoPicture(int w, int h, int bpp) {
    std::string out;
    auto le16 = [&out](unsigned v) { out += static_cast<char>(v & 255); out += static_cast<char>((v >> 8) & 255); };
    auto le32 = [&le16](unsigned v) { le16(v & 0xffff); le16(v >> 16); };
    const unsigned colours = bpp == 4 ? 16 : 0;
    le32(40); le32(static_cast<unsigned>(w)); le32(static_cast<unsigned>(2 * h)); le16(1); le16(static_cast<unsigned>(bpp));
    le32(0); le32(0); le32(0); le32(0); le32(colours); le32(0);
    for (unsigned i = 0; i < colours; ++i) { out += static_cast<char>(i * 16); out += static_cast<char>(i * 8); out += static_cast<char>(i * 4); out += '\0'; }
    const size_t xorStride = ((size_t(w) * bpp + 31) / 32) * 4, andStride = ((size_t(w) + 31) / 32) * 4;
    for (int y = h - 1; y >= 0; --y) {   // bottom-up
        std::string row(xorStride, '\0');
        for (int x = 0; x < w; ++x) {
            if (bpp == 4) row[x / 2] |= static_cast<char>(((x + y) % 16) << (x % 2 == 0 ? 4 : 0));
            else { row[x * 3] = static_cast<char>(x); row[x * 3 + 1] = static_cast<char>(y); row[x * 3 + 2] = static_cast<char>(200); }
        }
        out += row;
    }
    for (int y = h - 1; y >= 0; --y) {
        std::string row(andStride, '\0');
        if (y == 0) row[0] = static_cast<char>(0x80);
        out += row;
    }
    return out;
}

std::string IcoFile(const std::vector<std::string>& pictures, const std::vector<int>& widths, const std::vector<int>& depths) {
    std::string out;
    auto le16 = [&out](unsigned v) { out += static_cast<char>(v & 255); out += static_cast<char>((v >> 8) & 255); };
    auto le32 = [&le16](unsigned v) { le16(v & 0xffff); le16(v >> 16); };
    le16(0); le16(1); le16(static_cast<unsigned>(pictures.size()));
    size_t offset = 6 + 16 * pictures.size();
    for (size_t i = 0; i < pictures.size(); ++i) {
        out += static_cast<char>(widths[i]); out += static_cast<char>(widths[i]); out += static_cast<char>(depths[i] == 4 ? 16 : 0); out += '\0';
        le16(1); le16(static_cast<unsigned>(depths[i])); le32(static_cast<unsigned>(pictures[i].size())); le32(static_cast<unsigned>(offset));
        offset += pictures[i].size();
    }
    for (const std::string& p : pictures) out += p;
    return out;
}

void TestIcon(const std::string& scratch) {
    std::printf("the game's icon\n");
    CHECK(IconFileKind("Theme Park Data/SimTheme Park", true) == IconFile::ResourceFork);
    CHECK(IconFileKind("Theme Park World 2.1.4", true) == IconFile::ResourceFork);
    CHECK(IconFileKind("Theme Park Data/SimTheme Park", false) == IconFile::None);
    CHECK(IconFileKind("TP.ico", false) == IconFile::Ico);
    CHECK(IconFileKind("disc/tp.ICO", false) == IconFile::Ico);
    CHECK(IconFileKind("TP.EXE", false) == IconFile::Exe);
    CHECK(IconFileKind("ThemeParkWorld.exe", false) == IconFile::Exe);
    CHECK(IconFileKind("Setup.exe", false) == IconFile::None);
    CHECK(IconFileKind("data/levels/level.sam", false) == IconFile::None);

    // the .ico: the 8 x 8 16-colour picture beats the 4 x 4 true-colour one (more pixels first)
    const std::string ico = IcoFile({IcoPicture(4, 4, 24), IcoPicture(8, 8, 4)}, {4, 8}, {24, 4});
    IconImage icon;
    CHECK(DecodeIcon(IconFile::Ico, reinterpret_cast<const uint8_t*>(ico.data()), ico.size(), icon));
    CHECK_EQ(icon.width, 8);
    CHECK_EQ(icon.height, 8);
    CHECK_EQ(icon.bitDepth, 4);
    CHECK_EQ(icon.rgba.size(), 8u * 8 * 4);
    CHECK_EQ(icon.rgba[3], 0);                      // top left: masked out
    CHECK_EQ(icon.rgba[4], 4);                      // (1, 0): entry 1, red 4
    CHECK_EQ(icon.rgba[5], 8);
    CHECK_EQ(icon.rgba[6], 16);
    CHECK_EQ(icon.rgba[7], 255);
    CHECK_EQ(icon.rgba[(7 * 8 + 7) * 4 + 2], 14 * 16);   // (7, 7): entry 14, blue 224
    IconImage small;
    const std::string one = IcoFile({IcoPicture(4, 4, 24)}, {4}, {24});
    CHECK(DecodeIcon(IconFile::Ico, reinterpret_cast<const uint8_t*>(one.data()), one.size(), small));
    CHECK_EQ(small.width, 4);
    CHECK_EQ(small.bitDepth, 24);
    CHECK_EQ(small.rgba[(1 * 4 + 2) * 4 + 0], 200);   // (2, 1): red 200, green 1, blue 2
    CHECK_EQ(small.rgba[(1 * 4 + 2) * 4 + 1], 1);
    CHECK_EQ(small.rgba[(1 * 4 + 2) * 4 + 2], 2);
    CHECK(BetterIcon(icon, small));
    CHECK(!BetterIcon(small, icon));
    CHECK(BetterIcon(small, IconImage{}));
    CHECK(!BetterIcon(icon, icon));   // the same again is no better
    IconImage none;
    CHECK(!DecodeIcon(IconFile::Ico, reinterpret_cast<const uint8_t*>("\0\0\1\0"), 4, none));
    CHECK(!DecodeIcon(IconFile::Exe, reinterpret_cast<const uint8_t*>(ico.data()), ico.size(), none));

    // the PNG keeps the pixels and says where they came from
    const std::vector<uint8_t> png = EncodeIconPng(icon, "TP.ico");
    CHECK(png.size() > 8 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G');
    IconImage back;
    std::string source;
    CHECK(DecodeIconPng(png.data(), png.size(), back, &source));
    CHECK(source == "TP.ico, 8x8, 4-bit");
    CHECK_EQ(back.width, 8);
    CHECK_EQ(back.bitDepth, 4);
    CHECK(back.rgba == icon.rgba);

    // the .ico the launcher writes (EncodeIco): a 64 x 64 picture gives the five sizes up to its own, 32-bit bitmaps,
    // and reads back as itself (the biggest wins); a 256 x 256 one gives all seven, the biggest as a PNG
    auto ramp = [](int n) {
        IconImage img;
        img.width = img.height = n;
        img.bitDepth = 32;
        img.rgba.resize(size_t(n) * n * 4);
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                uint8_t* p = &img.rgba[(size_t(y) * n + x) * 4];
                p[0] = uint8_t(x * 255 / (n - 1)), p[1] = uint8_t(y * 255 / (n - 1)), p[2] = 77, p[3] = x < n / 2 ? 255 : 0;
            }
        return img;
    };
    const IconImage sixtyFour = ramp(64);
    const std::vector<uint8_t> ico64 = EncodeIco(sixtyFour);
    CHECK(ico64.size() > 6 && ico64[0] == 0 && ico64[1] == 0 && ico64[2] == 1 && ico64[3] == 0);
    CHECK_EQ(ico64[4], 5);
    CHECK_EQ(ico64[6 + 4 * 16], 64);   // the fifth entry: 64 x 64
    CHECK_EQ(ico64[6 + 16], 24);       // the second: 24 x 24
    IconImage again;
    CHECK(DecodeIcon(IconFile::Ico, ico64.data(), ico64.size(), again));
    CHECK_EQ(again.width, 64);
    CHECK_EQ(again.bitDepth, 32);
    CHECK(again.rgba == sixtyFour.rgba);
    const std::vector<uint8_t> ico256 = EncodeIco(ramp(256));
    CHECK_EQ(ico256[4], 7);
    CHECK_EQ(ico256[6 + 6 * 16], 0);   // the seventh: 256, written as 0
    CHECK(DecodeIcon(IconFile::Ico, ico256.data(), ico256.size(), again));
    CHECK_EQ(again.width, 128);        // the PNG picture is skipped, the 128 one is scaled down: the left half opaque
    CHECK_EQ(again.rgba[3], 255);
    CHECK_EQ(again.rgba[(64 * 128 + 127) * 4 + 3], 0);
    CHECK_EQ(again.rgba[(127 * 128 + 63) * 4 + 1], 255);   // bottom row: green 254 and 255 averaged, rounded up
    const size_t pngAt = size_t(ico256[6 + 6 * 16 + 12]) | size_t(ico256[6 + 6 * 16 + 13]) << 8 | size_t(ico256[6 + 6 * 16 + 14]) << 16;
    const size_t pngSize = size_t(ico256[6 + 6 * 16 + 8]) | size_t(ico256[6 + 6 * 16 + 9]) << 8 | size_t(ico256[6 + 6 * 16 + 10]) << 16;
    CHECK(pngAt + pngSize == ico256.size());
    CHECK(DecodeIconPng(ico256.data() + pngAt, pngSize, again, nullptr));
    CHECK_EQ(again.width, 256);
    CHECK(again.rgba == ramp(256).rgba);
    CHECK(EncodeIco(IconImage{}).empty());
    std::string icoErr;
    CHECK(WriteIconIco(scratch + "/icon-test.ico", sixtyFour, icoErr));
    CHECK(DecodeIcon(IconFile::Ico, ico64.data(), ico64.size(), again));
    // a second write of the same path replaces the file, and leaves no .part (std::rename refused that on Windows:
    // tpw-port issue #62, PR #63); so does a second icon.png
    CHECK(WriteIconIco(scratch + "/icon-test.ico", ramp(256), icoErr));
    CHECK(icoErr.empty());
    CHECK(ReadText(scratch + "/icon-test.ico") == std::string(ico256.begin(), ico256.end()));
    CHECK(!Exists(scratch + "/icon-test.ico.part"));
    CHECK(WriteIconPng(scratch + "/icon-test.png", icon, "TP.ico", icoErr));
    CHECK(WriteIconPng(scratch + "/icon-test.png", sixtyFour, "TP.EXE", icoErr));
    CHECK(ReadIconPng(scratch + "/icon-test.png", again, &source));
    CHECK(again.width == 64 && source == "TP.EXE, 64x64, 32-bit");
    CHECK(!Exists(scratch + "/icon-test.png.part"));
    std::remove((scratch + "/icon-test.ico").c_str());
    std::remove((scratch + "/icon-test.png").c_str());

    // a drop with TP.ico in it writes icon.png beside launcher.ini; a lesser icon later does not replace it
    std::string root = scratch + "/icon-data", drop = scratch + "/icon-drop";
    RemoveTree(root);
    RemoveTree(drop);
    WriteFile(drop + "/TP.ico", ico);
    WriteFile(drop + "/README.txt", "a disc");
    ImporterConfig cfg;
    cfg.dataRoot = root;
    Importer imp(cfg);
    std::string err;
    CHECK(imp.Open(err));
    ImportReport r = imp.Import({drop});
    CHECK(r.errors.empty());
    CHECK(r.icon == "TP.ico, 8x8, 4-bit");
    IconImage kept;
    CHECK(ReadIconPng(root + "/icon.png", kept, &source));
    CHECK(source == "TP.ico, 8x8, 4-bit");
    CHECK(kept.rgba == icon.rgba);
    WriteFile(drop + "/TP.ico", one);
    r = imp.Import({drop});
    CHECK(r.errors.empty());
    CHECK(r.icon.empty());
    CHECK(ReadIconPng(root + "/icon.png", kept, &source));
    CHECK(source == "TP.ico, 8x8, 4-bit");
    RemoveTree(root);
    RemoveTree(drop);
}


}  // namespace

// The PlayStation disc's own entrance and exit cells, from each ride's trailer and the pads and planks painted on its
// ground, as the .sam's shape holds them after the half turn onto the PC's ground: column counted from the disc's far
// side, then row counted from the entrance side, which the shape lists first. The Tower is scenery.
void TestPlayStationRides(const Discs& d, const std::string& scratch) {
    std::string pc = !d.pcUs.empty() ? d.pcUs : !d.eurImg.empty() ? d.eurImg : d.toast;
    if (d.ps1.empty() || pc.empty()) {
        Skip("PlayStation ride conversion: the ps1 disc or a PC source is not under TPW_TEST_TRANSFER");
        return;
    }
    std::printf("a PC source, then the PlayStation disc\n");
    std::string root = scratch + "/ps1-rides";
    RemoveTree(root);
    ImporterConfig cfg;
    cfg.dataRoot = root;
    Importer imp(cfg);
    std::string err;
    CHECK(imp.Open(err));
    CHECK(imp.Import({pc}).errors.empty());
    ImportReport r = imp.Import({d.ps1});
    for (const std::string& line : r.playStation) std::printf("    %s\n", line.c_str());
    CHECK(r.playStationConverted);
    struct Ride {
        const char* sam;
        int width, depth, entranceColumn, entranceRow;
        char entrance;
        int exit;
    } rides[] = {{"hallow/rides/_jack_201/_jack.sam", 4, 4, 1, 3, 'N', 2},
                 {"hallow/rides/_skulls_202/_skulls.sam", 4, 4, 1, 3, 'N', 1},
                 {"hallow/rides/_eyesld_204/_eyesld.sam", 4, 4, 0, 3, 'N', 2},
                 {"jungle/rides/_aztec_205/_aztec.sam", 6, 3, 2, 2, 'N', 3},
                 {"jungle/rides/_diplo_206/_diplo.sam", 5, 5, 2, 0, 'S', 3},
                 {"jungle/rides/_incapot_207/_incapot.sam", 4, 4, 0, 3, 'N', 3},
                 {"space/rides/_romper_208/_romper.sam", 5, 5, 2, 4, 'N', 2},
                 {"hallow/features/_obver_203/_obver.sam", 3, 3, -1, -1, 0, -1}};
    for (const Ride& ride : rides) {
        std::string text = ReadText(root + "/assets/ps1/data/levels/" + ride.sam);
        text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line) && line != "Info.Shape") {}
        std::getline(in, line);
        std::vector<std::string> want(ride.depth, std::string(ride.width, '*')), got;
        // The exit is always on the first row, its arrow out past it.
        if (ride.exit >= 0) want[0][ride.exit] = '8';
        if (ride.entranceRow >= 0) want[ride.entranceRow][ride.entranceColumn] = ride.entrance;
        while (std::getline(in, line) && line != "---") got.push_back(line);
        if (got != want) std::printf("    %s: the entrance or exit is not on the PlayStation disc's cell\n", ride.sam);
        CHECK(got == want);
    }
    RemoveTree(root);
}

int main() {
    std::string scratch = Env("TPW_TEST_SCRATCH");
    bool haveScratch = !scratch.empty();
    if (!haveScratch) scratch = (fs::temp_directory_path() / "tpw-importer-test").string();
    scratch += "/importer";
    fs::create_directories(scratch);

    TestSynthetic(scratch);
    TestPlayStation(scratch);
    TestPs1Grass();
    TestPs1PlainGround();
    TestIcon(scratch);
    TestStartupCheck(scratch);
    TestEditedSettings(scratch);
    TestSamReadCheck();
    TestOriginals(scratch);
    TestFilms(scratch);
    TestDamagedContainers(scratch);
    TestArchives(scratch);
    TestLanguageLines();
    TestLanguageSynthetic(scratch);
    const std::string languageDisc = Env("TPW_TEST_LANGUAGE_DISC"), languages = Env("TPW_TEST_LANGUAGE");
    if (languageDisc.empty() || languages.empty()) Skip("the language disc: TPW_TEST_LANGUAGE_DISC or TPW_TEST_LANGUAGE is not set");
    else TestLanguageDisc(languageDisc, languages, scratch);
    const std::string ps1 = Env("TPW_TEST_PS1"), ps1Donors = Env("TPW_TEST_PS1_DONORS");
    if (ps1.empty() || ps1Donors.empty()) Skip("the PlayStation ride cameras: TPW_TEST_PS1 or TPW_TEST_PS1_DONORS is not set");
    else TestPs1Cameras(ps1, ps1Donors, scratch);

    std::string transfer = Env("TPW_TEST_TRANSFER"), expect = Env("TPW_TEST_EXPECT"), gdi = Env("TPW_GDI_ROOT");
    Discs d;
    auto pick = [](const std::string& path) { return Exists(path) ? path : std::string(); };
    if (!transfer.empty()) {
        auto source = [&](const char* key) {
            std::string path = SourceFor(transfer, key);
            return path.empty() ? path : pick(path);
        };
        d.toast = source("mac-zip");
        d.eurImg = source("eur-a");
        d.eurFirst = source("eur-first");
        d.goldZip = source("gold-zip");
        d.goldRides = source("gold-rides");
        d.pcUs = source("pc-us");
        d.pcUsFirst = source("pc-us-first");
        d.pcEur = source("pc-eur");
        d.ps1 = source("ps1");
    }
    if (!expect.empty()) d.allitems = pick(expect + "/allitems.exe");
    if (!gdi.empty()) d.gameEur = pick(gdi + "/game-eur/tpw-eur/Theme Park Data");

    if (!haveScratch) {
        Skip("the disc imports: TPW_TEST_SCRATCH is not set (they write about 3 GB)");
    } else if (transfer.empty()) {
        Skip("the disc imports: TPW_TEST_TRANSFER is not set");
    } else {
        TestUsAndEur(d, scratch);
        TestFirstPressing(d, scratch);
        TestGold(d, scratch);
        TestWindowsDiscs(d, scratch);
        TestPlayStationRides(d, scratch);
    }
    RemoveTree(scratch);

    std::printf("importer: %d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
    return g_fail == 0 ? 0 : 1;
}

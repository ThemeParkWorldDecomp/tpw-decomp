// host_manifest.cpp: checks the compiled-in manifest (assets/manifest.cpp, assets/manifest_data.inc) without a window or
// a disc: its structure (sorted, no duplicate destination per edition, hashes and the by-content index consistent), its
// counts per edition and group, and, when devtools/import_originals.py and devtools/import_eur.py are beside it, that
// every entry of their CHECKSUMS and EXPECTED lists is there with the same hash. Run by ctest ("manifest"); exits
// nonzero on the first failure.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "gold_ride_table.h"
#include "manifest.h"

#ifndef TPW_DEVTOOLS_DIR
#define TPW_DEVTOOLS_DIR "devtools"
#endif

using namespace tpw::assets;

static int gChecks = 0;

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        ++gChecks;                                                                          \
        if (!(condition)) {                                                                 \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition);     \
            std::exit(1);                                                                   \
        }                                                                                   \
    } while (0)

#define CHECK_EQ(a, b)                                                                                      \
    do {                                                                                                    \
        ++gChecks;                                                                                          \
        unsigned long long va = static_cast<unsigned long long>(a), vb = static_cast<unsigned long long>(b); \
        if (va != vb) {                                                                                     \
            std::fprintf(stderr, "FAILED %s:%d: %s = %llu, expected %s = %llu\n", __FILE__, __LINE__, #a, va, #b, vb); \
            std::exit(1);                                                                                   \
        }                                                                                                   \
    } while (0)

static bool EndsWith(const std::string& s, const char* suffix) {
    size_t n = std::strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

static bool StartsWith(const std::string& s, const char* prefix) { return s.compare(0, std::strlen(prefix), prefix) == 0; }

static std::string Hex(const uint8_t* sha) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 32; ++i) {
        out += digits[sha[i] >> 4];
        out += digits[sha[i] & 15];
    }
    return out;
}

static bool FromHex(const std::string& text, uint8_t* out) {
    if (text.size() != 64) return false;
    for (int i = 0; i < 32; ++i) {
        unsigned v = 0;
        for (int j = 0; j < 2; ++j) {
            char c = text[i * 2 + j];
            unsigned d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : 99;
            if (d > 15) return false;
            v = v * 16 + d;
        }
        out[i] = static_cast<uint8_t>(v);
    }
    return true;
}

// ---- structure

static void TestStructure() {
    const ManifestRecord* records = ManifestRecords();
    size_t n = ManifestRecordCount();
    CHECK(n > 0);
    CHECK(ManifestHashCount() >= n);

    std::set<std::pair<std::string, int>> seen;
    uint32_t next_hash = 0;
    for (size_t i = 0; i < n; ++i) {
        const ManifestRecord& r = records[i];
        std::string dest = r.dest;
        // sorted by destination bytes, then edition bits
        if (i > 0) {
            int c = std::strcmp(records[i - 1].dest, r.dest);
            CHECK(c < 0 || (c == 0 && records[i - 1].editions < r.editions));
        }
        // a plain relative ASCII path
        CHECK(!dest.empty() && dest[0] != '/' && dest.find("..") == std::string::npos && dest.find('\\') == std::string::npos);
        for (unsigned char c : dest) CHECK(c >= 0x20 && c < 0x7f);
        CHECK(r.editions != 0 && (r.editions & ~(kEditionUs | kEditionEur | kEditionGold | kEditionLanguages | kEditionUsPc | kEditionEurPc)) == 0);
        CHECK(r.group < kGroupCount);
        CHECK(r.source == kSourceMacCd || r.source == kSourcePcCd || r.source == kSourceGoldCd ||
              r.source == kSourceLanguageDisc);
        // no duplicate (dest, edition)
        for (int e : {kEditionUs, kEditionEur, kEditionGold, kEditionLanguages, kEditionUsPc, kEditionEurPc}) {
            if (r.editions & e) CHECK(seen.insert({dest, e}).second);
        }
        // the flags say what the name says
        CHECK(((r.flags & kFlagRsrc) != 0) == EndsWith(dest, ".rsrc"));
        CHECK(((r.flags & kFlagFinfo) != 0) == EndsWith(dest, ".finfo"));
        CHECK(((r.flags & kFlagFinfo) == 0) || ((r.flags & kFlagRequired) == 0));
        CHECK(((r.editions & kEditionGold) != 0) == StartsWith(dest, "gold/"));
        CHECK(((r.group == kGroupGold)) == ((r.editions & kEditionGold) != 0));
        CHECK(r.group != kGroupLanguageAmerican || (r.editions & ~(kEditionUs | kEditionUsPc)) == 0);
        CHECK(r.group != kGroupLanguageEnglish || (r.editions & ~(kEditionEur | kEditionEurPc)) == 0);
        CHECK(r.group != kGroupProgram || (r.hash_count == 2 && r.editions == kEditionUs));
        // a language pack's file: lang/<Name>/<game path>, that edition alone, a known language
        const bool lang = (r.editions & kEditionLanguages) != 0;
        CHECK(lang == StartsWith(dest, "lang/"));
        CHECK(lang == (r.group == kGroupLanguagePack));
        CHECK(lang == (r.source == kSourceLanguageDisc));
        CHECK(!lang || r.editions == kEditionLanguages);
        if (lang) {
            size_t slash = dest.find('/', 5);
            CHECK(slash != std::string::npos && FindLanguage(dest.substr(5, slash - 5).c_str()) != nullptr);
            CHECK(StartsWith(dest.substr(slash + 1), "data/"));
            CHECK((r.flags & (kFlagRsrc | kFlagFinfo | kFlagTuning)) == 0);
        }
        // hashes: consecutive, point back
        CHECK(r.hash_count >= 1);
        CHECK_EQ(r.first_hash, next_hash);
        next_hash += r.hash_count;
        for (unsigned k = 0; k < r.hash_count; ++k) {
            const ManifestHash& h = RecordHash(r, k);
            CHECK_EQ(h.record, i);
            // an empty file has the well-known empty digest (and any other file does not)
            bool empty_digest = Hex(h.sha256) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
            CHECK(empty_digest == (h.size == 0));
        }
    }
    CHECK_EQ(next_hash, ManifestHashCount());

    // lookups: every record by destination and edition, every content by size and hash
    for (size_t i = 0; i < n; ++i) {
        const ManifestRecord& r = records[i];
        for (int e : {kEditionUs, kEditionEur, kEditionGold, kEditionLanguages, kEditionUsPc, kEditionEurPc}) {
            if (r.editions & e) CHECK(FindByDest(static_cast<Edition>(e), r.dest) == &r);
            else CHECK(FindByDest(static_cast<Edition>(e), r.dest) != &r);
        }
        for (unsigned k = 0; k < r.hash_count; ++k) {
            const ManifestHash& h = RecordHash(r, k);
            std::vector<const ManifestRecord*> hits = FindByContent(h.size, h.sha256);
            bool found = false;
            for (const ManifestRecord* hit : hits) {
                found = found || hit == &r;
                bool accepts = false;  // every hit really accepts it
                for (unsigned m = 0; m < hit->hash_count; ++m) {
                    const ManifestHash& g = RecordHash(*hit, m);
                    accepts = accepts || (g.size == h.size && std::memcmp(g.sha256, h.sha256, 32) == 0);
                }
                CHECK(accepts);
            }
            CHECK(found);
            // the same digest with another size is not this content
            std::vector<const ManifestRecord*> wrong = FindByContent(h.size + 1, h.sha256);
            for (const ManifestRecord* w : wrong) {
                bool accepts = false;
                for (unsigned m = 0; m < w->hash_count; ++m) accepts = accepts || (RecordHash(*w, m).size == h.size + 1);
                CHECK(accepts);
            }
        }
    }
    uint8_t none[32] = {1, 2, 3};
    CHECK(FindByContent(12345, none).empty());
    CHECK(FindByDest(kEditionUs, "no/such/file") == nullptr);
    CHECK(FindByDest(kEditionEur, "gold/x") == nullptr);
}

// ---- counts (the generator prints the same figures)

static void TestCounts() {
    CHECK_EQ(ManifestRecordCount(), 3140);   // the PC CDs' films share the US and European records
    CHECK_EQ(ManifestHashCount(), 3149);
    CHECK_EQ(CountRecords(kEditionUs), 779);
    CHECK_EQ(CountRecords(kEditionEur), 913);
    CHECK_EQ(CountRecords(kEditionGold), 36);
    CHECK_EQ(CountRecords(kEditionUs, true), 776);   // all but the program (optional) and bfj.mov and BFj2.mov (never opened)
    CHECK_EQ(CountRecords(kEditionEur, true), 860);
    CHECK_EQ(CountRecords(kEditionGold, true), 35);   // all but RideConfig.txt (optional: the built-in table places the rides)
    CHECK_EQ(CountRecords(kEditionUs | kEditionEur), 779 + 913 - 71);

    std::map<std::pair<int, int>, size_t> by_group;      // (edition, group)
    std::map<std::pair<int, int>, size_t> by_source;     // (edition, source)
    size_t rsrc[3] = {0, 0, 0}, finfo[3] = {0, 0, 0}, tuning[3] = {0, 0, 0}, shared = 0;
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        const ManifestRecord& r = ManifestRecords()[i];
        if ((r.editions & kEditionUs) && (r.editions & kEditionEur)) ++shared;
        int idx = 0;
        for (int e : {kEditionUs, kEditionEur, kEditionGold}) {
            if (r.editions & e) {
                ++by_group[{e, r.group}];
                ++by_source[{e, r.source}];
                rsrc[idx] += (r.flags & kFlagRsrc) != 0;
                finfo[idx] += (r.flags & kFlagFinfo) != 0;
                tuning[idx] += (r.flags & kFlagTuning) != 0;
            }
            ++idx;
        }
    }
    CHECK_EQ(shared, 71);   // 62, and the PC CD's 9 films
    CHECK_EQ((by_group[{kEditionUs, kGroupCore}]), 663);
    CHECK_EQ((by_group[{kEditionUs, kGroupFontsRsrc}]), 34);
    CHECK_EQ((by_group[{kEditionUs, kGroupMovies}]), 20);
    CHECK_EQ((by_group[{kEditionUs, kGroupLanguageAmerican}]), 61);
    CHECK_EQ((by_group[{kEditionUs, kGroupProgram}]), 1);
    CHECK_EQ((by_group[{kEditionEur, kGroupCore}]), 763);
    CHECK_EQ((by_group[{kEditionEur, kGroupFontsRsrc}]), 51);
    CHECK_EQ((by_group[{kEditionEur, kGroupMovies}]), 33);
    CHECK_EQ((by_group[{kEditionEur, kGroupLanguageEnglish}]), 66);
    CHECK_EQ((by_group[{kEditionGold, kGroupGold}]), 36);
    // sources: the US files all come from the Mac CD but the PC CD's 9 films; of the European 913, 798 from the PC disc
    CHECK_EQ((by_source[{kEditionUs, kSourceMacCd}]), 770);
    CHECK_EQ((by_source[{kEditionUs, kSourcePcCd}]), 9);
    CHECK_EQ((by_source[{kEditionEur, kSourcePcCd}]), 798);
    CHECK_EQ((by_source[{kEditionEur, kSourceMacCd}]), 115);  // 45 files, 2 tuning files, 21 .rsrc, 47 .finfo
    CHECK_EQ((by_source[{kEditionGold, kSourceGoldCd}]), 36);
    CHECK_EQ(rsrc[0], 17);
    CHECK_EQ(rsrc[1], 21);
    CHECK_EQ(finfo[0], 0);
    CHECK_EQ(finfo[1], 47);
    CHECK_EQ(tuning[1], 6);  // med.sam and sound.sam, with their .rsrc and .finfo
    CHECK_EQ(tuning[0], 0);

    // the program: both accepted builds, one record (by SHA-256 of the data fork; the SHA-1s are in the generator)
    const ManifestRecord* program = FindByDest(kEditionUs, "SimTheme Park");
    CHECK(program != nullptr && program->group == kGroupProgram && program->hash_count == 2);
    CHECK(program != nullptr && (program->flags & kFlagRequired) == 0);  // optional
    CHECK(Hex(RecordHash(*program, 0).sha256) == "04809cd4ccee5433c7fb0b7c93d32f6a7aa629c1849181c0b7906415e5e295f5");
    CHECK_EQ(RecordHash(*program, 0).size, 2274758);
    CHECK(Hex(RecordHash(*program, 1).sha256) == "29da2392beefc1ee00b097536b7f1b202044cbf0743c0eb3a6235254fd318000");
    CHECK_EQ(RecordHash(*program, 1).size, 2746951);

    CHECK(std::strcmp(GroupName(kGroupFontsRsrc), "fonts-rsrc") == 0);
    CHECK(std::strcmp(EditionName(kEditionEur), "eur") == 0);
    CHECK(std::strcmp(SourceName(kSourcePcCd), "pc") == 0);
    CHECK(std::strcmp(GroupName(kGroupLanguagePack), "language-pack") == 0);
    CHECK(std::strcmp(EditionName(kEditionLanguages), "lang") == 0);
    CHECK(std::strcmp(EditionName(kEditionUsPc), "us-pc") == 0);
    CHECK(std::strcmp(EditionName(kEditionEurPc), "eur-pc") == 0);
    for (Edition e : {kEditionUs, kEditionEur, kEditionGold, kEditionLanguages, kEditionUsPc, kEditionEurPc}) {
        Edition back = kEditionGold;
        CHECK(EditionFromName(EditionName(e), back) && back == e);
    }
    Edition none = kEditionGold;
    CHECK(!EditionFromName("pc", none) && none == kEditionGold);
    CHECK(IsUsGame(kEditionUs) && IsUsGame(kEditionUsPc) && !IsUsGame(kEditionEur) && !IsUsGame(kEditionEurPc));
    CHECK(IsWindowsEdition(kEditionUsPc) && IsWindowsEdition(kEditionEurPc) && !IsWindowsEdition(kEditionEur));
    CHECK(std::strcmp(SourceName(kSourceLanguageDisc), "lang") == 0);
}

// ---- the Windows discs' editions: PC files only, the Mac game folder's layout

static void TestPcEditions() {
    CHECK_EQ(CountRecords(kEditionUsPc), 749);
    CHECK_EQ(CountRecords(kEditionUsPc, true), 748);   // all but fonts.wad, read through its members
    CHECK_EQ(CountRecords(kEditionEurPc), 817);
    CHECK_EQ(CountRecords(kEditionEurPc, true), 816);
    for (Edition e : {kEditionUsPc, kEditionEurPc}) {
        const char* lang = e == kEditionUsPc ? "American" : "English";
        size_t fonts = 0, movies = 0, text = 0;
        for (size_t i = 0; i < ManifestRecordCount(); ++i) {
            const ManifestRecord& r = ManifestRecords()[i];
            if (!(r.editions & e)) continue;
            const std::string dest = r.dest;
            CHECK(r.source == kSourcePcCd);
            CHECK((r.flags & (kFlagRsrc | kFlagFinfo | kFlagTuning)) == 0);
            CHECK(r.group != kGroupProgram && r.group != kGroupFontsRsrc && StartsWith(dest, "data/"));
            CHECK(!EndsWith(dest, ".mov") && !(StartsWith(dest, "data/levels/") && dest.find("/_") != std::string::npos));   // no Mac film, no "_" bonus ride
            if (StartsWith(dest, "data/fonts/")) {
                CHECK(EndsWith(dest, ".TTF") && (r.flags & kFlagRequired));
                ++fonts;
            }
            if (r.group == kGroupMovies) {
                CHECK(EndsWith(dest, ".tgq") && (r.flags & kFlagRequired));   // the port plays them
                ++movies;
            }
            if (StartsWith(dest, "data/Language/")) {
                CHECK(StartsWith(dest, (std::string("data/Language/") + lang + "/").c_str()));
                ++text;
            }
        }
        CHECK_EQ(fonts, 17);
        CHECK_EQ(movies, 9);
        CHECK_EQ(text, 66);
        const ManifestRecord* wad = FindByDest(e, "data/fonts.wad");
        CHECK(wad != nullptr && !(wad->flags & kFlagRequired));
        for (const char* res : {"512", "640", "800", "1024"}) {
            std::string init = std::string(e == kEditionUsPc ? "data/init/" : "data/Init/") + res + "/";
            const ManifestRecord* legal = FindByDest(e, (init + "legal.tga").c_str());
            const ManifestRecord* original = FindByDest(e, (init + "Legal_" + lang + ".tga").c_str());
            CHECK(legal != nullptr && original != nullptr);
            if (legal && original) CHECK(std::memcmp(RecordHash(*legal, 0).sha256, RecordHash(*original, 0).sha256, 32) == 0);
        }
    }
    // the PC disc's own tuning, not the Mac release's
    const ManifestRecord* eurMed = FindByDest(kEditionEur, "data/med.sam");
    const ManifestRecord* pcMed = FindByDest(kEditionEurPc, "data/med.sam");
    CHECK(eurMed != nullptr && pcMed != nullptr && eurMed != pcMed && (eurMed->flags & kFlagTuning));
    // the European PC files of the European edition are the same records
    CHECK(FindByDest(kEditionEurPc, "data/levels/jungle/Music/MusicHD.sdt") ==
          FindByDest(kEditionEur, "data/levels/jungle/Music/MusicHD.sdt"));
}

// ---- the Python lists, read as text

static bool ReadFile(const std::string& path, std::string* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    *out = ss.str();
    return true;
}

// The text between `NAME = """` and the closing `"""` of the python file's top-level constant.
static bool ListText(const std::string& py, const char* name, std::string* out) {
    std::string key = std::string("\n") + name + " = \"\"\"";
    size_t at = py.find(key);
    if (at == std::string::npos) return false;
    size_t start = at + key.size();
    size_t end = py.find("\"\"\"", start);
    if (end == std::string::npos) return false;
    *out = py.substr(start, end - start);
    return true;
}

// The python source writes the Finder's "Icon" file name with an escape (backslash, r) inside a plain string.
static std::string Unescape(std::string s) {
    for (size_t at; (at = s.find("\\r")) != std::string::npos;) s.replace(at, 2, "\r");
    return s;
}

static std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        out.push_back(text.substr(pos, nl - pos));
        pos = nl + 1;
    }
    return out;
}

static bool HasHash(const ManifestRecord& r, const uint8_t* sha, const uint64_t* size) {
    for (unsigned k = 0; k < r.hash_count; ++k) {
        const ManifestHash& h = RecordHash(r, k);
        if (std::memcmp(h.sha256, sha, 32) == 0 && (!size || h.size == *size)) return true;
    }
    return false;
}

static void TestPythonLists() {
    std::string originals, eur;
    bool have_originals = ReadFile(std::string(TPW_DEVTOOLS_DIR) + "/import_originals.py", &originals);
    bool have_eur = ReadFile(std::string(TPW_DEVTOOLS_DIR) + "/import_eur.py", &eur);
    if (!have_originals || !have_eur) {
        std::printf("SKIP the checks against import_originals.py and import_eur.py (not found under %s)\n", TPW_DEVTOOLS_DIR);
        return;
    }

    // US: CHECKSUMS "SHA-256 SIZE NAME" lines under "= FOLDER/" headings
    std::string list;
    CHECK(ListText(originals, "CHECKSUMS", &list));
    const std::string game = "disc-hfs/Theme Park Data/";
    const std::string patch_program = "SimTPPatch_204b3/Sim Theme Park Patch/patch 2.04b3/SimTheme Park";
    std::string folder;
    size_t expected_us = 0, entries = 0;
    bool saw_cd_program = false, saw_patch_program = false;
    for (const std::string& line : Lines(list)) {
        if (line.empty()) continue;
        if (StartsWith(line, "= ")) {
            folder = line.substr(2);
            continue;
        }
        size_t a = line.find(' '), b = line.find(' ', a + 1);
        CHECK(a == 64 && b != std::string::npos);
        uint8_t sha[32];
        CHECK(FromHex(line.substr(0, 64), sha));
        uint64_t size = std::strtoull(line.substr(a + 1, b - a - 1).c_str(), nullptr, 10);
        std::string path = Unescape(folder + line.substr(b + 1));
        ++entries;
        if (path == patch_program) {
            const ManifestRecord* r = FindByDest(kEditionUs, "SimTheme Park");
            CHECK(r && HasHash(*r, sha, &size));
            saw_patch_program = true;
            continue;
        }
        if (!StartsWith(path, game.c_str())) continue;
        std::string rel = path.substr(game.size());
        if (rel == "SimTheme Park") {
            const ManifestRecord* r = FindByDest(kEditionUs, "SimTheme Park");
            CHECK(r && HasHash(*r, sha, &size));
            saw_cd_program = true;
            continue;
        }
        bool font_fork = StartsWith(rel, "data/fonts/") && EndsWith(rel, ".rsrc");
        bool side = EndsWith(rel, ".finfo") || (EndsWith(rel, ".rsrc") && !font_fork);
        bool icon = rel.size() >= 5 && rel.compare(rel.size() - 5, 5, "Icon\r") == 0;
        if (!StartsWith(rel, "data/") || side || icon) {
            // not part of the manifest: libraries, the ReadMe, Finder info files, other resource forks, the Icon files
            CHECK(FindByDest(kEditionUs, rel.c_str()) == nullptr);
            continue;
        }
        const ManifestRecord* r = FindByDest(kEditionUs, rel.c_str());
        CHECK(r != nullptr);
        CHECK(r != nullptr && HasHash(*r, sha, &size));
        const bool unopened = rel == "data/Movies/bfj.mov" || rel == "data/Movies/BFj2.mov";
        CHECK(r != nullptr && ((r->flags & kFlagRequired) != 0) == !unopened);
        ++expected_us;
    }
    CHECK(entries > 1000);
    CHECK(saw_cd_program && saw_patch_program);
    // every US record but the program and the PC CD's 9 films came from CHECKSUMS
    CHECK_EQ(expected_us + 1 + 9, CountRecords(kEditionUs));

    // Europe: EXPECTED "SHA-256  PATH" lines, all of them (913) and nothing else
    CHECK(ListText(eur, "EXPECTED", &list));
    size_t n = 0;
    std::set<std::string> listed;
    for (const std::string& line : Lines(list)) {
        if (line.empty()) continue;
        CHECK(line.size() > 66 && line[64] == ' ' && line[65] == ' ');
        uint8_t sha[32];
        CHECK(FromHex(line.substr(0, 64), sha));
        std::string rel = line.substr(66);
        CHECK(listed.insert(rel).second);
        const ManifestRecord* r = FindByDest(kEditionEur, rel.c_str());
        CHECK(r != nullptr);
        CHECK(r != nullptr && HasHash(*r, sha, nullptr));
        ++n;
    }
    CHECK_EQ(n, CountRecords(kEditionEur));
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        if (ManifestRecords()[i].editions & kEditionEur) CHECK(listed.count(ManifestRecords()[i].dest) == 1);
    }
}

// ---- the language packs (LANGUAGES.md: what each disc gave; the generator prints the same figures)

static void TestLanguages() {
    CHECK_EQ(CountRecords(kEditionLanguages), 1354);
    CHECK_EQ(CountRecords(kEditionLanguages, true), 1234);
    struct Expected {
        const char* name;
        size_t records, required;
        unsigned flags;
    };
    const Expected expected[] = {
        {"English", 96, 85, 0},
        {"American", 84, 80, 0},
        {"French", 96, 85, 0},
        {"German", 96, 85, 0},
        {"Danish", 96, 85, 0},
        {"Swedish", 96, 85, 0},
        {"Spanish", 92, 81, kLanguageOldTables | kLanguageBaseDateFonts},
        {"Italian", 92, 81, kLanguageOldTables | kLanguageBaseDateFonts},
        {"Dutch", 92, 81, kLanguageOldTables | kLanguageBaseDateFonts},
        {"Polish", 92, 81, kLanguageOldTables | kLanguageBaseDateFonts},
        {"Japanese", 93, 85, 0},
        {"Korean", 81, 80, 0},
        {"TraditionalChinese", 81, 80, kLanguageOldTables},
        {"SimplifiedChinese", 81, 80, kLanguageOldTables},
        {"Hebrew", 86, 80, 0},
    };
    CHECK_EQ(ManifestLanguageCount(), std::size(expected));
    if (ManifestLanguageCount() != std::size(expected)) return;
    for (size_t l = 0; l < std::size(expected); ++l) {
        const ManifestLanguage& language = ManifestLanguages()[l];
        const Expected& e = expected[l];
        CHECK(std::strcmp(language.name, e.name) == 0);
        CHECK_EQ(language.flags, e.flags);
        CHECK(FindLanguage(e.name) == &language);
        const std::string prefix = std::string("lang/") + e.name + "/";
        const std::string text = prefix + "data/Language/" + e.name + "/";
        size_t records = 0, required = 0, meshes = 0, speech = 0, dates = 0;
        for (size_t i = 0; i < ManifestRecordCount(); ++i) {
            const ManifestRecord& r = ManifestRecords()[i];
            const std::string dest = r.dest;
            if (!StartsWith(dest, prefix.c_str())) continue;
            ++records;
            if (!(r.flags & kFlagRequired)) continue;
            ++required;
            meshes += EndsWith(dest, ".MD2");
            speech += !StartsWith(dest, text.c_str());
            dates += StartsWith(dest, (text + "DATE").c_str());
        }
        CHECK_EQ(records, e.records);
        CHECK_EQ(required, e.required);
        CHECK_EQ(meshes, 3);
        CHECK_EQ(speech, 20);
        // the four DATE fonts: in the pack, or (kLanguageBaseDateFonts) the base edition's
        CHECK_EQ(dates, (e.flags & kLanguageBaseDateFonts) ? 0u : 4u);
        for (const char* file : {"UITEXT.str", "residx.dat", "swears.txt"}) {
            const ManifestRecord* r = FindByDest(kEditionLanguages, (text + file).c_str());
            CHECK(r != nullptr && (r->flags & kFlagRequired));
        }
        // the old tables' residx.dat is the short one; a new residx with old tables is the Chinese discs'
        const ManifestRecord* residx = FindByDest(kEditionLanguages, (text + "residx.dat").c_str());
        if (residx) CHECK(((RecordHash(*residx, 0).size == 1343)) == ((e.flags & kLanguageBaseDateFonts) != 0));
    }
    CHECK(FindLanguage("Klingon") == nullptr);
    CHECK(FindLanguage("french") == nullptr);
    // the English and American packs are the European and US editions' own files
    const std::pair<Edition, const char*> editions[] = {{kEditionEur, "English"}, {kEditionUs, "American"}};
    for (const auto& [edition, name] : editions) {
        for (const char* file : {"UITEXT.str", "residx.dat", "DATEBIG.bf4"}) {
            const std::string path = std::string("data/Language/") + name + "/" + file;
            const ManifestRecord* base = FindByDest(edition, path.c_str());
            const ManifestRecord* pack = FindByDest(kEditionLanguages, (std::string("lang/") + name + "/" + path).c_str());
            CHECK(base != nullptr && pack != nullptr);
            if (base && pack) CHECK(HasHash(*base, RecordHash(*pack, 0).sha256, &RecordHash(*pack, 0).size));
        }
    }
}

// ---- the films: each the program opens is the Mac CD's .mov or the PC CD's .tgq, in both editions (FilmCounterpart)

static void TestFilms() {
    size_t pairs = 0;
    for (Edition e : {kEditionUs, kEditionEur}) {
        for (const char* name : {"bf", "bub", "buc", "grav", "jug", "mir", "plan", "roc", "roll"}) {
            const std::string movies = std::string("data/Movies/") + name;
            const ManifestRecord* mov = FindByDest(e, (movies + ".mov").c_str());
            const ManifestRecord* tgq = FindByDest(e, (movies + ".tgq").c_str());
            CHECK(mov && tgq && (mov->flags & kFlagRequired) && (tgq->flags & kFlagRequired));
            CHECK(mov && mov->source == kSourceMacCd && tgq && tgq->source == kSourcePcCd);
            CHECK(mov && FilmCounterpart(*mov, e) == tgq);
            CHECK(tgq && FilmCounterpart(*tgq, e) == mov);
            CHECK(mov && FilmCounterpart(*mov, kEditionGold) == nullptr);
            ++pairs;
        }
        for (const char* name : {"data/Movies/bfj.mov", "data/Movies/BFj2.mov"}) {
            const ManifestRecord* r = FindByDest(e, name);
            CHECK(r && !(r->flags & kFlagRequired) && FilmCounterpart(*r, e) == nullptr);
        }
    }
    CHECK_EQ(pairs, 18);
    // side files and everything outside data/Movies have none
    const ManifestRecord* rsrc = FindByDest(kEditionEur, "data/Movies/bf.mov.rsrc");
    CHECK(rsrc && FilmCounterpart(*rsrc, kEditionEur) == nullptr);
    size_t with = 0;
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        for (Edition e : {kEditionUs, kEditionEur, kEditionGold, kEditionLanguages}) with += FilmCounterpart(ManifestRecords()[i], e) != nullptr;
    }
    CHECK_EQ(with, 36);
}

// The built-in Gold ride table (assets/gold_ride_table.h): one row for each of the 35 Gold wads of the manifest, none
// for anything else, unique names, a folder of the form data/levels/<theme>/<rides|features|sideshow>.
static void TestGoldRideTable() {
    CHECK_EQ(kGoldRideTableSize, 35);
    std::set<std::string> rows;
    for (const GoldRideRow& row : kGoldRideTable) {
        const std::string wad = row.wad, folder = row.folder;
        CHECK(rows.insert(wad).second);
        CHECK(StartsWith(wad, "_") && EndsWith(wad, ".wad"));
        bool lower = true;
        for (char c : wad + folder) lower = lower && !(c >= 'A' && c <= 'Z') && c != '\\';
        CHECK(lower);
        CHECK(StartsWith(folder, "data/levels/"));
        const std::string place = folder.substr(std::strlen("data/levels/"));
        const size_t slash = place.find('/');
        CHECK(slash != std::string::npos && place.find('/', slash + 1) == std::string::npos);
        const std::string kind = place.substr(slash + 1);
        CHECK(kind == "rides" || kind == "features" || kind == "sideshow");
        CHECK(place.substr(0, slash) == "fantasy" || place.substr(0, slash) == "hallow" || place.substr(0, slash) == "jungle" ||
              place.substr(0, slash) == "space");
    }
    // Every Gold wad of the manifest has a row, and every row a wad.
    size_t wads = 0;
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        const ManifestRecord& r = ManifestRecords()[i];
        if (!(r.editions & kEditionGold)) continue;
        const std::string dest = r.dest;
        if (!EndsWith(dest, ".wad")) {
            CHECK(dest == "gold/RideConfig.txt");
            CHECK((r.flags & kFlagRequired) == 0);
            continue;
        }
        ++wads;
        CHECK(StartsWith(dest, "gold/"));
        CHECK(rows.count(dest.substr(5)) == 1);
        CHECK((r.flags & kFlagRequired) != 0);
    }
    CHECK_EQ(wads, 35);
}

int main() {
    TestStructure();
    TestGoldRideTable();
    TestCounts();
    TestFilms();
    TestLanguages();
    TestPcEditions();
    TestPythonLists();
    std::printf("manifest: %d checks passed\n", gChecks);
    return 0;
}

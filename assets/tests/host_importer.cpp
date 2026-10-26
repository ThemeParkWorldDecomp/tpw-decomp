// Host tests of the importer (assets/importer.cpp). Synthetic drops always run (empty files matched by their path, a
// different version, unknown files, a repeated drop, launcher.ini, removing an edition, the staging folder, refusals).
// With the player's own discs they import for real and compare the trees with devtools/import_originals.py's CHECKSUMS
// (US), import_eur.py's EXPECTED (European) and the European folder under TPW_GDI_ROOT/game-eur; the discs are read in
// place and the output trees are deleted at the end. A test whose input is absent is skipped with a message.
//
// environment: TPW_GDI_ROOT (the ROOT folder), TPW_TEST_TRANSFER (the disc archive folder), TPW_TEST_EXPECT (the folder
// with allitems.exe), TPW_TEST_SCRATCH (where the output trees go; the disc tests need about 3 GB there)
#include <chrono>
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
#include "../importer.h"
#include "../sha256.h"
#include "../sniff.h"

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
    for (int i = 0; i < 3; ++i) {
        const EditionStatus& s = r.status[i];
        std::printf("    %-4s %-8s records %zu/%zu required %zu/%zu files %zu/%zu missing mac %zu pc %zu gold %zu\n",
                    EditionName(s.edition), s.StateWord(), s.present, s.records, s.requiredPresent, s.required,
                    s.filesPresent, s.files, s.missing[kSourceMacCd], s.missing[kSourcePcCd], s.missing[kSourceGoldCd]);
    }
}

// ---- synthetic drops

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

// ---- damaged containers: an error naming the file and the reason, never a drop that quietly holds nothing

void Put16(std::string& v, uint32_t x) {
    v += char(x & 255);
    v += char((x >> 8) & 255);
}
void Put32(std::string& v, uint32_t x) {
    Put16(v, x & 0xffff);
    Put16(v, x >> 16);
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
    CHECK_EQ(FileCount(root), 0);
    RemoveTree(root);
    RemoveTree(drop);
}

// ---- the discs

struct Discs {
    std::string toast, img66484, mdf66100, goldZip, allitems, gameEur;
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
    if (d.toast.empty() || d.img66484.empty()) {
        Skip("US and European import: the US toast zip or the 66484 image is not under TPW_TEST_TRANSFER");
        return;
    }
    std::printf("US toast, then 66484\n");
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
    ImportReport eur = imp.Import({d.img66484});
    Events(imp);
    PrintStatus("66484", eur);
    for (const std::string& v : eur.differentVersions) std::printf("    different version (not used): %s\n", v.c_str());
    CHECK(eur.errors.empty());
    CHECK(eur.status[1].unlocked);
    CHECK_EQ(eur.status[1].present, 913);
    CHECK_EQ(eur.status[1].filesPresent, 845);
    CHECK_EQ(eur.differentVersions.size(), 2);  // the PC tuning files; the other languages' speech files are not news
    CompareEur(imp.GameFolder(kEditionEur), d.gameEur);

    // (c) both again: nothing changes
    std::map<std::string, FileInfo> before = Tree(root + "/assets", false);
    ImportReport again = imp.Import({d.toast, d.img66484});
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
    RemoveTree(root);
}

void Test66100(const Discs& d, const std::string& scratch) {
    if (d.mdf66100.empty()) {
        Skip("66100 import: the image is not under TPW_TEST_TRANSFER");
        return;
    }
    std::printf("66100 alone\n");
    std::string root = scratch + "/eur-66100";
    RemoveTree(root);
    ImporterConfig cfg;
    cfg.dataRoot = root;
    Importer imp(cfg);
    std::string err;
    CHECK(imp.Open(err));
    ImportReport r = imp.Import({d.mdf66100});
    Events(imp);
    PrintStatus("66100", r);
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
        CHECK(imp.RemoveEdition(kEditionGold, err));
        CHECK(!imp.Status(kEditionGold).unlocked);
        CHECK(!Exists(root + "/assets/gold"));
        RemoveTree(root);
    }
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
    std::remove((scratch + "/icon-test.ico").c_str());

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

int main() {
    std::string scratch = Env("TPW_TEST_SCRATCH");
    bool haveScratch = !scratch.empty();
    if (!haveScratch) scratch = (fs::temp_directory_path() / "tpw-importer-test").string();
    scratch += "/importer";
    fs::create_directories(scratch);

    TestSynthetic(scratch);
    TestIcon(scratch);
    TestStartupCheck(scratch);
    TestDamagedContainers(scratch);

    std::string transfer = Env("TPW_TEST_TRANSFER"), expect = Env("TPW_TEST_EXPECT"), gdi = Env("TPW_GDI_ROOT");
    Discs d;
    auto pick = [](const std::string& path) { return Exists(path) ? path : std::string(); };
    if (!transfer.empty()) {
        d.toast = pick(transfer + "/2 SimThemePark Mac US CD v2.0.1 Feral Aspyr/"
                                  "SimThemePark_Mac_US-CD-Image_v2.0.1_2000_Feral-Aspyr_PPC-Classic.toast.zip");
        const std::string eu = transfer + "/1 ThemeParkWorld PC UK-European CD Bullfrog EA/";
        d.img66484 = pick(eu + "ThemeParkWorld_PC_EA-Classics-CD-Image_2000-11_Bullfrog-EA_Win9x_redump-66484/ThemeParkWorld.img");
        d.mdf66100 = pick(eu + "ThemeParkWorld_PC_European-CD-Image-EN-IT-NL-PL_1999-10_Bullfrog-EA_Win9x_redump-66100/TPWORLD.mdf");
        d.goldZip = pick(transfer + "/4 SimThemePark PC Gold Edition Bonus Rides and Attractions CD/"
                                    "SimThemePark_PC_US-Gold-Edition-Bonus-Rides-and-Attractions-CD-Image_2000-07_Bullfrog-EA_Win9x.zip");
    }
    if (!expect.empty()) d.allitems = pick(expect + "/allitems.exe");
    if (!gdi.empty()) d.gameEur = pick(gdi + "/game-eur/tpw-eur/Theme Park Data");

    if (!haveScratch) {
        Skip("the disc imports: TPW_TEST_SCRATCH is not set (they write about 3 GB)");
    } else if (transfer.empty()) {
        Skip("the disc imports: TPW_TEST_TRANSFER is not set");
    } else {
        TestUsAndEur(d, scratch);
        Test66100(d, scratch);
        TestGold(d, scratch);
    }
    RemoveTree(scratch);

    std::printf("importer: %d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
    return g_fail == 0 ? 0 : 1;
}

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

#include "../importer.h"
#include "../sha256.h"

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

}  // namespace

int main() {
    std::string scratch = Env("TPW_TEST_SCRATCH");
    bool haveScratch = !scratch.empty();
    if (!haveScratch) scratch = (fs::temp_directory_path() / "tpw-importer-test").string();
    scratch += "/importer";
    fs::create_directories(scratch);

    TestSynthetic(scratch);

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

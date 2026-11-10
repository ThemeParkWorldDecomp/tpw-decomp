// importer.cpp: the launcher's importer engine (importer.h). Phase one opens what was dropped and lists every file in
// it, opening nested containers; a file whose size is one the manifest holds becomes work, every other file is placed
// at once (a different version when its path is a known destination, else unknown). Phase two hashes the work in order,
// staging a file while it is hashed when one of the records of its size is still missing, and installs each match.
#include "importer.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <system_error>
#include <unordered_map>

#include "icon.h"
#include "ps1_disc.h"
#include "ps1_convert.h"
#include "sam_check.h"
#include "sha256.h"
#include "sniff.h"
#include "../platform/path_text.h"

#ifdef __APPLE__
#include <sys/xattr.h>
#endif
#if defined(__APPLE__) || defined(__unix__)
#include <sys/stat.h>
#endif

namespace tpw::assets {

namespace {

namespace fs = std::filesystem;

// the editions launcher.ini and the report list
const Edition kAllEditions[5] = {kEditionUs, kEditionEur, kEditionGold, kEditionUsPc, kEditionEurPc};
// everything a file can be
const Edition kFileEditions[6] = {kEditionUs, kEditionEur, kEditionGold, kEditionLanguages, kEditionUsPc, kEditionEurPc};
const uint64_t kSmall = 1ull << 20;          // read whole and written from memory
const uint64_t kProgressStep = 16ull << 20;  // a Hashing event every this many bytes of a big file
const size_t kChunk = 1 << 20;

// us 0, eur 1, gold 2, language packs 3, us-pc 4, eur-pc 5 (ImportReport::status, OpenCheck::removed)
int EditionIndex(Edition e) {
    switch (e) {
        case kEditionUs: return 0;
        case kEditionEur: return 1;
        case kEditionGold: return 2;
        case kEditionLanguages: return 3;
        case kEditionUsPc: return 4;
        case kEditionEurPc: return 5;
    }
    return 3;
}

std::string Lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

// True when the (lower-case) path ends with the (lower-case) destination at a folder boundary.
bool EndsWithDest(const std::string& path, const std::string& dest) {
    if (path.size() < dest.size() || path.compare(path.size() - dest.size(), dest.size(), dest) != 0) return false;
    return path.size() == dest.size() || path[path.size() - dest.size() - 1] == '/';
}

std::string BaseName(const std::string& path) {
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string Extension(const std::string& path) {
    std::string name = BaseName(path);
    size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? std::string() : Lower(name.substr(dot + 1));
}

// Files inside a drop that are opened as containers (a dropped file is always sniffed, whatever its name).
bool ContainerExtension(const std::string& ext) {
    static const char* const kExts[] = {"zip", "toast", "iso", "img", "bin", "mdf", "exe", "cab",
                                        "hfs", "dsk", "cdr", "image", "macbin", "7z", "rar"};
    for (const char* e : kExts) {
        if (ext == e) return true;
    }
    return false;
}

std::string UnsupportedDiscFormat(const std::string& path) {
    const std::string ext = Extension(path);
    if (ext == "chd" || ext == "ecm" || ext == "pbp")
        return "." + ext + " disc images cannot be read. Drop a .cue/.bin or .iso image instead.";
    return {};
}

// A part of a multi-volume archive, by its name: "x.part1.rar", "x.part2.rar", ... (any count of digits); "x.rar", "x.r00",
// "x.r01", ... (the older RAR naming); "x.7z.001", "x.7z.002", ... (7-Zip's split files).
struct VolumeName {
    enum Style { Part, OldRar, Split7z } style = Part;
    std::string stem, suffix;   // the name before and after the number, in the name's own case
    size_t width = 0;
    bool upper = false;         // OldRar: "X.RAR", "X.R00"
    int index = 0;              // which part the name is, from 0
};

bool AllDigits(const std::string& s) {
    return !s.empty() && s.size() <= 4 && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

bool ParseVolume(const std::string& name, VolumeName& v) {
    const std::string low = Lower(name);
    const size_t n = low.size();
    if (n > 4 && low.compare(n - 4, 4, ".rar") == 0) {
        const size_t part = low.rfind(".part");
        if (part != std::string::npos && part + 5 < n - 4 && AllDigits(low.substr(part + 5, n - 4 - part - 5))) {
            v.style = VolumeName::Part;
            v.stem = name.substr(0, part + 5);
            v.suffix = name.substr(n - 4);
            v.width = n - 4 - part - 5;
            v.index = std::stoi(low.substr(part + 5, v.width)) - 1;
            return v.index >= 0;
        }
        v.style = VolumeName::OldRar;
        v.stem = name.substr(0, n - 3);
        v.upper = name[n - 3] == 'R';
        v.index = 0;
        return true;
    }
    if (n > 4 && low[n - 4] == '.' && low[n - 3] == 'r' && AllDigits(low.substr(n - 2))) {
        v.style = VolumeName::OldRar;
        v.stem = name.substr(0, n - 3);
        v.upper = name[n - 3] == 'R';
        v.index = std::stoi(low.substr(n - 2)) + 1;
        return true;
    }
    if (n > 7 && low.compare(n - 7, 4, ".7z.") == 0 && AllDigits(low.substr(n - 3))) {
        v.style = VolumeName::Split7z;
        v.stem = name.substr(0, n - 3);
        v.width = 3;
        v.index = std::stoi(low.substr(n - 3)) - 1;
        return v.index >= 0;
    }
    return false;
}

std::string VolumePart(const VolumeName& v, int i) {
    auto padded = [](int number, size_t width) {
        std::string digits = std::to_string(number);
        return std::string(width > digits.size() ? width - digits.size() : 0, '0') + digits;
    };
    switch (v.style) {
        case VolumeName::Part: return v.stem + padded(i + 1, v.width) + v.suffix;
        case VolumeName::Split7z: return v.stem + padded(i + 1, v.width);
        case VolumeName::OldRar: break;
    }
    if (i == 0) return v.stem + (v.upper ? "RAR" : "rar");
    return v.stem + (v.upper ? "R" : "r") + padded(i - 1, 2);
}

// The set of parts `name` belongs to, by name, in order from the first, as far as `exists` finds them one after the other
// (parts is empty when the first is missing; `first` names it). False when the name is no part's (a plain archive, or
// "x.rar" with no "x.r00" beside it).
bool VolumeSet(const std::string& name, const std::function<bool(const std::string&)>& exists,
               std::vector<std::string>& parts, std::string& first) {
    VolumeName v;
    if (!ParseVolume(name, v)) return false;
    if (v.style == VolumeName::OldRar && v.index == 0 && !exists(VolumePart(v, 1))) return false;
    first = VolumePart(v, 0);
    parts.clear();
    for (int i = 0; i < 1000 && exists(VolumePart(v, i)); ++i) parts.push_back(VolumePart(v, i));
    return true;
}

std::string MissingFirstPart(const std::string& first) {
    return "this is a part of an archive in several parts, and its first part (" + first +
           ") is not beside it. Drop all the parts together, from one folder.";
}

// The path the evidence check compares with destinations: a resource fork is the "<path>.rsrc" side file.
std::string EvidencePath(const Entry& e) { return Lower(e.kind == EntryKind::Rsrc ? e.path + ".rsrc" : e.path); }

bool Accepts(const ManifestRecord& r, uint64_t size, const Sha256Digest& d) {
    for (unsigned i = 0; i < r.hash_count; ++i) {
        const ManifestHash& h = RecordHash(r, i);
        if (h.size == size && std::memcmp(h.sha256, d.bytes, 32) == 0) return true;
    }
    return false;
}

bool SizeAccepted(const ManifestRecord& r, uint64_t size) {
    for (unsigned i = 0; i < r.hash_count; ++i) {
        if (RecordHash(r, i).size == size) return true;
    }
    return false;
}

// A settings file the player may edit (tpw-port #94): a destination ending in ".sam", in any case (its .rsrc and .finfo
// side files are not). Present whatever its size; one that no longer matches is kept as the player's edit (importer.h).
bool PlayerEditable(const ManifestRecord& r) {
    size_t n = std::strlen(r.dest);
    return n > 4 && Lower(r.dest + n - 4) == ".sam";
}

// Content that proves nothing by itself: an empty file, or a Finder info side file (ten bytes many files share).
bool Trivial(const ManifestRecord& r, uint64_t size) { return size == 0 || (r.flags & kFlagFinfo) != 0; }

// The language a "lang/<Name>/..." destination belongs to, or "".
std::string LanguageOf(const ManifestRecord& r) {
    if (!(r.editions & kEditionLanguages) || std::strncmp(r.dest, "lang/", 5) != 0) return "";
    const char* end = std::strchr(r.dest + 5, '/');
    return end ? std::string(r.dest + 5, end) : "";
}

// The evidence for a language pack's trivial file (its empty alloweds.txt): the (lower-case) path has the destination's
// file name and a folder named after the language, as every disc lays a language out ("German/Filter/German/...").
bool LanguageEvidence(const std::string& path, const ManifestRecord& r) {
    std::string name = Lower(LanguageOf(r));
    if (name.empty() || BaseName(path) != Lower(BaseName(r.dest))) return false;
    size_t start = 0;
    for (size_t slash; (slash = path.find('/', start)) != std::string::npos; start = slash + 1) {
        if (path.compare(start, slash - start, name) == 0 && slash - start == name.size()) return true;
    }
    return false;
}

bool SideFile(const ManifestRecord& r) { return (r.flags & (kFlagRsrc | kFlagFinfo)) != 0; }

// One stat of a path: false unless it is a regular file. `mtime` is the modification time in nanoseconds (the startup
// check compares it only with what this same call returned earlier, so the epoch does not matter).
bool StatFile(const std::string& path, uint64_t& size, int64_t& mtime) {
#if defined(__APPLE__) || defined(__unix__)
    struct stat st;
    if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return false;
    size = static_cast<uint64_t>(st.st_size);
#if defined(__APPLE__)
    mtime = static_cast<int64_t>(st.st_mtimespec.tv_sec) * 1000000000 + st.st_mtimespec.tv_nsec;
#else
    mtime = static_cast<int64_t>(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif
    return true;
#else
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return false;
    size = fs::file_size(path, ec);
    if (ec) return false;
    fs::file_time_type t = fs::last_write_time(path, ec);
    if (ec) return false;
    mtime = std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count();
    return true;
#endif
}

// assets/verified.tsv's key for a file: "<edition>\t<dest>".
std::string StampKey(Edition edition, const ManifestRecord& r) { return std::string(EditionName(edition)) + "\t" + r.dest; }

// Writes `text` to a temporary file beside `path`, then renames it over `path`.
bool ReplaceFile(const std::string& path, const std::string& text, std::string& err) {
    std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) {
        err = "cannot write " + tmp;
        return false;
    }
    bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    ok = std::fclose(f) == 0 && ok;
    std::error_code ec;
    if (ok) fs::rename(tmp, path, ec);
    if (!ok || ec) {
        std::remove(tmp.c_str());
        err = "cannot write " + path;
        return false;
    }
    return true;
}

uint32_t RecordIndex(const ManifestRecord* r) { return static_cast<uint32_t>(r - ManifestRecords()); }

bool Under(const std::string& path, const std::string& dir) {
    return path == dir || (path.size() > dir.size() && path.compare(0, dir.size(), dir) == 0 && path[dir.size()] == '/');
}

// A forward stream over a file.
class PlainFileStream : public Stream {
public:
    PlainFileStream(FILE* f, uint64_t size) : f_(f), size_(size) {}
    ~PlainFileStream() override { std::fclose(f_); }
    uint64_t Size() const override { return size_; }
    int64_t Read(void* buf, size_t n, std::string& err) override {
        if (n == 0) return 0;
        size_t got = std::fread(buf, 1, n, f_);
        if (got == 0 && std::ferror(f_)) {
            err = "read failed";
            return -1;
        }
        return static_cast<int64_t>(got);
    }

private:
    FILE* f_;
    uint64_t size_;
};

// One dropped file that is no container, as a Source of its own (its resource fork and Finder info too, on macOS).
class SingleFileSource : public Source {
public:
    const char* Kind() const override { return "folder"; }
    const std::vector<Entry>& Entries() const override { return entries_; }
    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        if (entry.id >= disk_.size()) {
            err = "not an entry of this file";
            return nullptr;
        }
        FILE* f = std::fopen(disk_[static_cast<size_t>(entry.id)].c_str(), "rb");
        if (!f) {
            err = "cannot open " + entry.path;
            return nullptr;
        }
        return std::make_unique<PlainFileStream>(f, entry.size);
    }
    bool Init(const std::string& path, std::string& err) {
        std::error_code ec;
        uint64_t size = fs::file_size(path, ec);
        if (ec) {
            err = "cannot read " + path;
            return false;
        }
        Entry e;
        e.path = BaseName(path);
        e.size = size;
#ifdef __APPLE__
        uint8_t finfo[32];
        if (getxattr(path.c_str(), "com.apple.FinderInfo", finfo, sizeof finfo, 0, XATTR_NOFOLLOW) >= 8) {
            bool any = false;
            for (int i = 0; i < 8; i++) any = any || finfo[i] != 0;
            if (any) {
                e.hasFinderInfo = true;
                std::memcpy(e.finderType, finfo, 4);
                std::memcpy(e.finderCreator, finfo + 4, 4);
            }
        }
#endif
        disk_.push_back(path);
        entries_.push_back(e);
#ifdef __APPLE__
        ssize_t fork = getxattr(path.c_str(), "com.apple.ResourceFork", nullptr, 0, 0, XATTR_NOFOLLOW);
        if (fork > 0) {
            Entry r;
            r.path = e.path;
            r.size = static_cast<uint64_t>(fork);
            r.kind = EntryKind::Rsrc;
            r.id = 1;
            disk_.push_back(path + "/..namedfork/rsrc");
            entries_.push_back(r);
        }
#endif
        return true;
    }

private:
    std::vector<Entry> entries_;
    std::vector<std::string> disk_;
};

// The FILE lines of a .cue sheet, as paths beside it.
std::vector<std::string> CueFiles(const std::string& cue) {
    std::vector<std::string> out;
    std::ifstream in(cue, std::ios::binary);
    std::string line;
    std::string dir = tpw::PathText(fs::path(cue).parent_path());
    while (std::getline(in, line)) {
        size_t at = line.find_first_not_of(" \t");
        if (at == std::string::npos || line.compare(at, 5, "FILE ") != 0) continue;
        size_t q1 = line.find('"', at), q2 = q1 == std::string::npos ? q1 : line.find('"', q1 + 1);
        std::string name;
        if (q2 != std::string::npos) {
            name = line.substr(q1 + 1, q2 - q1 - 1);
        } else {
            std::istringstream ss(line.substr(at + 5));
            ss >> name;
        }
        if (!name.empty()) out.push_back(dir.empty() ? name : dir + "/" + name);
    }
    return out;
}

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string OneLine(std::string s) {
    for (char& c : s) {
        if (c == '\n' || c == '\r') c = ' ';
    }
    return s;
}

}  // namespace

// ---- launcher.ini

bool LoadLauncherIni(const std::string& path, LauncherIni& out, std::string& err) {
    out = LauncherIni();
    std::error_code ec;
    if (!fs::exists(path, ec)) return true;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        err = "cannot read " + path;
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::string t = Trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') continue;
        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(t.substr(0, eq)), value = Trim(t.substr(eq + 1));
        if (key.empty()) continue;
        if (key == "data_folder") {
            out.dataFolder = value;
        } else if (key == "last_choice") {
            out.lastChoice = value;
        } else if (key.compare(0, 8, "edition.") == 0 && key.size() > 8) {
            out.editions[key.substr(8)] = value;
        } else {
            out.extra.emplace_back(key, value);
        }
    }
    return true;
}

bool SaveLauncherIni(const std::string& path, const LauncherIni& ini, std::string& err) {
    std::ostringstream ss;
    ss << "# Theme Park World launcher settings (written by the launcher)\n";
    ss << "data_folder=" << OneLine(ini.dataFolder) << "\n";
    ss << "last_choice=" << OneLine(ini.lastChoice) << "\n";
    for (const auto& kv : ini.editions) ss << "edition." << OneLine(kv.first) << "=" << OneLine(kv.second) << "\n";
    for (const auto& kv : ini.extra) ss << OneLine(kv.first) << "=" << OneLine(kv.second) << "\n";
    std::string text = ss.str();
    std::string tmp = path + ".tmp";
    std::error_code ec;
    fs::path parent = fs::path(path).parent_path();
    if (!parent.empty()) fs::create_directories(parent, ec);
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) {
        err = "cannot write " + tmp;
        return false;
    }
    bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    ok = std::fclose(f) == 0 && ok;
    if (!ok) {
        std::remove(tmp.c_str());
        err = "cannot write " + tmp + " (disk full?)";
        return false;
    }
    fs::rename(tmp, path, ec);
    if (ec) {
        std::remove(tmp.c_str());
        err = "cannot replace " + path + ": " + ec.message();
        return false;
    }
    return true;
}

std::string ResolveDataRoot(const std::string& prefPath) {
    LauncherIni ini;
    std::string err;
    std::string base = prefPath;
    while (base.size() > 1 && base.back() == '/') base.pop_back();
    if (LoadLauncherIni(base + "/launcher.ini", ini, err) && !ini.dataFolder.empty()) return ini.dataFolder;
    return base;
}

// ---- status, progress, report

const char* EditionStatus::StateWord() const {
    if (unlocked) return "unlocked";
    return present == 0 ? "absent" : "partial";
}

const char* SourceDiscName(DiscSource source) {
    switch (source) {
        case kSourceMacCd: return "the US Mac CD";
        case kSourcePcCd: return "the European PC CD";   // the Windows editions' texts name their own disc
        case kSourceGoldCd: return "the Gold CD";
        case kSourceLanguageDisc: return "a disc with that language";
    }
    return "?";
}

const char* LanguageStatus::StateWord() const {
    switch (state) {
        case State::Available: return "available";
        case State::Partial: return "partial";
        case State::Absent: return "absent";
    }
    return "?";
}

std::string LanguageStatus::Line() const {
    std::string line = name + ": " + StateWord();
    if (state == State::Absent) return line;
    if (state == State::Available) {
        if (oldTables) line += ", older text tables";
        if (baseDateFonts) line += ", date fonts from the base edition";
        return line;
    }
    std::vector<std::string> missing;
    auto count = [&](size_t total, size_t present, const char* what) {
        if (present < total)
            missing.push_back(std::to_string(total - present) + " of " + std::to_string(total) + " " + what);
    };
    count(text, textPresent, "text files");
    count(filter, filterPresent, "filter files");
    count(meshes, meshesPresent, "meshes");
    count(speech, speechPresent, "speech files");
    for (size_t i = 0; i < missing.size(); ++i) {
        line += i == 0 ? ", " : (i + 1 == missing.size() ? " and " : ", ");
        line += missing[i];
    }
    if (!missing.empty()) line += " missing";
    if (baseDateFonts && !baseDateFontsPresent) {
        line += missing.empty() ? ", text and speech present, needs" : "; needs";
        line += " the US or European edition (its date fonts)";
    }
    return line;
}

void ProgressQueue::Push(const ImportProgress& p) {
    std::lock_guard<std::mutex> lock(mutex_);
    latest_ = p;
    if (p.kind == ImportProgress::Kind::Hashing && queue_.size() >= kBound) return;
    queue_.push_back(p);
}

bool ProgressQueue::Pop(ImportProgress& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;
    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

std::vector<ImportProgress> ProgressQueue::Drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ImportProgress> out(std::make_move_iterator(queue_.begin()), std::make_move_iterator(queue_.end()));
    queue_.clear();
    return out;
}

ImportProgress ProgressQueue::Latest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_;
}

void ProgressQueue::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
    latest_ = ImportProgress();
}

std::string ImportReport::Summary() const {
    std::ostringstream ss;
    if (aborted) ss << "Stopped by an error. ";
    else if (cancelled) ss << "Stopped. ";
    if (written == 0 && !cancelled && !aborted && !playStation.empty()) {
        ss << playStation.front();
    } else if (written == 0 && !cancelled && !aborted) {
        ss << "Nothing new: ";
        if (alreadyThere) ss << alreadyThere << " files were already in place.";
        else ss << "no file of the game was found.";
    } else {
        ss << written << " files written, " << alreadyThere << " already in place.";
    }
    if (!differentVersions.empty()) ss << " " << differentVersions.size() << " different versions (not used).";
    if (unknownFiles) ss << " " << unknownFiles << " other files ignored.";
    if (!errors.empty()) ss << " " << errors.size() << " errors.";
    return ss.str();
}

// ---- the importer

struct Importer::Work {
    std::shared_ptr<Source> source;
    Entry entry;
    std::string display;
};

struct Importer::Run {
    ImportReport report;
    SniffOptions sniff;
    std::vector<Work> work;
    std::vector<Ps1Disc> playStation;  // converted after ordinary files, so a combined PC/PS1 drop finds its donors
    uint64_t totalBytes = 0, doneBytes = 0;
    std::set<std::string> different;             // destinations a file of the drop has another version of
    std::set<std::string> matchedDests;          // destinations a file of the drop matched
    std::set<std::pair<int, uint32_t>> counted;  // (edition index, record) written or counted as already there
    std::set<std::string> opened;                // canonical paths of files and folders already read in this drop
    IconImage icon;                              // the best game icon seen so far (icon.h), and the file it came from
    std::string iconSource;
    bool stop = false;
};

struct Importer::Stamp {
    Edition edition;
    uint32_t record;
    uint64_t size;
    int64_t mtime;
};

Importer::Importer(ImporterConfig config) : config_(std::move(config)) {
    std::string root = config_.dataRoot;
    while (root.size() > 1 && root.back() == '/') root.pop_back();
    config_.dataRoot = root;
    assetsDir_ = root + "/assets";
    stagingDir_ = assetsDir_ + "/.staging";
    iniPath_ = config_.iniPath.empty() ? root + "/launcher.ini" : config_.iniPath;
}

Importer::~Importer() {
    Cancel();
    if (worker_.joinable()) worker_.join();
}

std::string Importer::GameFolder(Edition edition) const {
    switch (edition) {
        case kEditionUs: return assetsDir_ + "/us/Theme Park Data";
        case kEditionEur: return assetsDir_ + "/eur/Theme Park Data";
        case kEditionGold: return assetsDir_ + "/gold";
        case kEditionLanguages: return assetsDir_ + "/lang";
        case kEditionUsPc: return assetsDir_ + "/us-pc/Theme Park Data";
        case kEditionEurPc: return assetsDir_ + "/eur-pc/Theme Park Data";
    }
    return assetsDir_;
}

std::string Importer::DestinationPath(Edition edition, const ManifestRecord& record) const {
    std::string dest = record.dest;
    if (edition == kEditionGold && dest.compare(0, 5, "gold/") == 0) dest = dest.substr(5);
    if (edition == kEditionLanguages && dest.compare(0, 5, "lang/") == 0) dest = dest.substr(5);
    return GameFolder(edition) + "/" + dest;
}

bool Importer::Open(std::string& err) {
    if (config_.dataRoot.empty()) {
        err = "no data folder";
        return false;
    }
    try {
        std::error_code ec;
        fs::create_directories(assetsDir_, ec);
        if (ec) {
            err = "cannot create " + assetsDir_ + ": " + ec.message();
            return false;
        }
        assetsCanon_ = tpw::GenericPathText(fs::canonical(assetsDir_, ec));
        if (ec) assetsCanon_ = assetsDir_;
        // what an earlier run left half-written (or unpacked) goes; nothing in it was ever verified
        fs::remove_all(stagingDir_, ec);
        fs::create_directories(stagingDir_, ec);
        if (ec) {
            err = "cannot create " + stagingDir_ + ": " + ec.message();
            return false;
        }
        LauncherIni ini;
        std::string iniErr;
        if (!LoadLauncherIni(iniPath_, ini, iniErr)) {
            err = iniErr;
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ini_ = ini;
        }
        std::vector<Stamp> found;
        if (!ScanInstalled(&found)) {
            err = "out of memory";
            return false;
        }
        CheckChanged(found);
        for (const Stamp& st : found) KeepOriginal(st.edition, st.record, nullptr);
        RecordStamps(&found);  // a record that cannot be written only means the next start rehashes nothing
        opened_ = true;
        std::lock_guard<std::mutex> lock(mutex_);
        return SaveIniLocked(err);
    } catch (...) {
        err = "cannot open the data folder " + config_.dataRoot;
        return false;
    }
}

bool Importer::ScanInstalled(std::vector<Stamp>* found) {
    try {
        const ManifestRecord* records = ManifestRecords();
        size_t n = ManifestRecordCount();
        std::vector<uint8_t> present(n, 0);
        std::map<uint64_t, std::vector<uint32_t>> bySize;
        std::multimap<std::string, uint32_t> byPath;
        for (size_t i = 0; i < n; ++i) {
            const ManifestRecord& r = records[i];
            uint32_t idx = static_cast<uint32_t>(i);
            if (r.editions & config_.editions) {
                for (unsigned k = 0; k < r.hash_count; ++k) {
                    std::vector<uint32_t>& v = bySize[RecordHash(r, k).size];
                    if (v.empty() || v.back() != idx) v.push_back(idx);
                }
                byPath.emplace(Lower(r.dest), idx);
            }
            for (Edition e : kFileEditions) {
                if (!(r.editions & e)) continue;
                uint64_t size = 0;
                int64_t mtime = 0;
                if (!StatFile(DestinationPath(e, r), size, mtime) || !(SizeAccepted(r, size) || PlayerEditable(r)))
                    continue;
                present[i] |= e;
                if (found) found->push_back(Stamp{e, idx, size, mtime});
            }
        }
        std::lock_guard<std::mutex> lock(mutex_);
        present_ = std::move(present);
        edited_.assign(n, 0);
        bySize_ = std::move(bySize);
        byPath_ = std::move(byPath);
        return true;
    } catch (...) {
        return false;
    }
}

// The startup check (importer.h). With no verified.tsv (the first start of a launcher that has the check, or a data
// folder from before it) nothing is rehashed: the files were verified when they were written, and Open only writes the
// record from this stat sweep. A file whose size and modification time both stay the same is never rehashed here.
void Importer::CheckChanged(std::vector<Stamp>& found) {
    OpenCheck check;
    std::unordered_map<std::string, std::pair<uint64_t, int64_t>> recorded;
    {
        std::ifstream in(VerifiedPath(), std::ios::binary);
        check.hadRecord = static_cast<bool>(in);
        std::string line;
        while (check.hadRecord && std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            size_t t3 = line.rfind('\t');
            size_t t2 = t3 == std::string::npos || t3 == 0 ? std::string::npos : line.rfind('\t', t3 - 1);
            if (t2 == std::string::npos) continue;
            char* end = nullptr;
            uint64_t size = std::strtoull(line.c_str() + t2 + 1, &end, 10);
            if (end != line.c_str() + t3) continue;
            int64_t mtime = std::strtoll(line.c_str() + t3 + 1, &end, 10);
            if (*end != '\0') continue;
            recorded[line.substr(0, t2)] = {size, mtime};
        }
    }
    if (check.hadRecord) {
        const ManifestRecord* records = ManifestRecords();
        // Several records can name one file (other accepted contents): each changed file is hashed once.
        std::map<std::string, std::pair<bool, Sha256Digest>> hashed;
        std::set<std::string> editedPaths;
        std::vector<Stamp> kept;
        kept.reserve(found.size());
        for (const Stamp& st : found) {
            const ManifestRecord& r = records[st.record];
            ++check.checked;
            auto it = recorded.find(StampKey(st.edition, r));
            if (it != recorded.end() && it->second.first == st.size && it->second.second == st.mtime) {
                kept.push_back(st);
                continue;
            }
            std::string path = DestinationPath(st.edition, r);
            auto h = hashed.find(path);
            if (h == hashed.end()) {
                ++check.rehashed;
                std::pair<bool, Sha256Digest> result{false, Sha256Digest{}};
                FILE* f = std::fopen(path.c_str(), "rb");
                if (f) {
                    PlainFileStream s(f, st.size);
                    std::string readErr;
                    result.first = HashStream(s, result.second, readErr);
                }
                h = hashed.emplace(path, result).first;
            }
            if (h->second.first && Accepts(r, st.size, h->second.second)) {
                kept.push_back(st);
                continue;
            }
            if (h->second.first && PlayerEditable(r)) {
                // the player's edit: kept and read by the game, but not recorded, so every start says so again
                MarkEdited(st.edition, st.record);
                editedPaths.insert(path);
                check.edited.push_back(std::string(EditionName(st.edition)) + "/" + r.dest);
                continue;
            }
            MarkPresent(st.edition, st.record, false);
            ++check.removed[EditionIndex(st.edition)];
        }
        // a file goes only when no record of it still accepts it
        std::set<std::string> used = editedPaths;
        if (!hashed.empty()) {
            for (const Stamp& st : kept) {
                std::string path = DestinationPath(st.edition, records[st.record]);
                if (hashed.count(path)) used.insert(path);
            }
        }
        for (const auto& kv : hashed) {
            if (!used.count(kv.first)) std::remove(kv.first.c_str());
        }
        found = std::move(kept);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    openCheck_ = check;
}

std::string OpenCheck::Notice() const {
    static const char* const kNames[6] = {"the US edition", "the European edition", "the Gold bonus rides",
                                          "the language packs", "the US edition (Windows)",
                                          "the European edition (Windows)"};
    std::vector<std::string> parts;
    for (int i = 0; i < 6; ++i) {
        if (removed[i] == 0) continue;
        parts.push_back(std::to_string(removed[i]) + (removed[i] == 1 ? " file of " : " files of ") + kNames[i]);
    }
    if (parts.empty()) return "";
    std::string text;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) text += i + 1 == parts.size() ? " and " : ", ";
        text += parts[i];
    }
    bool one = Removed() == 1;
    text += one ? " had changed and was removed; " : " had changed and were removed; ";
    text += parts.size() == 1 ? "drop its disc to restore " : "drop their discs to restore ";
    text += one ? "it." : "them.";
    return text;
}

std::string OpenCheck::Kept() const {
    if (edited.empty()) return "";
    bool one = edited.size() == 1;
    std::string text = "kept " + std::to_string(edited.size()) + (one ? " edited .sam file" : " edited .sam files");
    text += ", which the game reads: ";
    for (size_t i = 0; i < edited.size(); ++i) text += (i > 0 ? ", " : "") + edited[i];
    text += " (Customized settings on the edition's tile restores an original).";
    return text;
}

OpenCheck Importer::LastOpenCheck() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return openCheck_;
}

// Writes assets/verified.tsv: from `found` (Open's sweep), else from a stat of every present file.
bool Importer::RecordStamps(const std::vector<Stamp>* found) {
    try {
        std::lock_guard<std::mutex> writer(stampMutex_);
        const ManifestRecord* records = ManifestRecords();
        std::vector<Stamp> swept;
        if (!found) {
            std::vector<uint8_t> present, edited;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                present = present_;
                edited = edited_;
            }
            for (size_t i = 0; i < present.size(); ++i) {
                for (Edition e : kFileEditions) {
                    if (!(present[i] & e) || (i < edited.size() && (edited[i] & e))) continue;
                    uint64_t size = 0;
                    int64_t mtime = 0;
                    if (StatFile(DestinationPath(e, records[i]), size, mtime)) {
                        swept.push_back(Stamp{e, static_cast<uint32_t>(i), size, mtime});
                    }
                }
            }
            found = &swept;
        }
        // nothing verified and no record yet (a new data folder): no file is made; an existing record is rewritten even
        // when it empties, so a file put there by hand later is rehashed
        std::error_code ec;
        if (found->empty() && !fs::exists(VerifiedPath(), ec)) return true;
        std::string text = "# verified files: edition, destination, size, modification time in ns (the launcher's startup check)\n";
        text.reserve(found->size() * 64);
        for (const Stamp& st : *found) {
            text += StampKey(st.edition, records[st.record]);
            text += "\t" + std::to_string(st.size) + "\t" + std::to_string(st.mtime) + "\n";
        }
        std::string err;
        return ReplaceFile(VerifiedPath(), text, err);
    } catch (...) {
        return false;
    }
}

void Importer::MarkPresent(Edition edition, uint32_t record, bool present) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (record >= present_.size()) return;
    if (present) present_[record] |= edition;
    else present_[record] &= static_cast<uint8_t>(~edition);
    if (record < edited_.size()) edited_[record] &= static_cast<uint8_t>(~edition);
}

void Importer::MarkEdited(Edition edition, uint32_t record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (record < edited_.size()) edited_[record] |= edition;
}

namespace {

// A whole small file, or false.
bool ReadSmallFile(const std::string& path, std::vector<uint8_t>& out) {
    out.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t buf[16384];
    size_t n;
    bool ok = true;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
        out.insert(out.end(), buf, buf + n);
        if (out.size() > kSmall) {
            ok = false;
            break;
        }
    }
    ok = ok && !std::ferror(f);
    std::fclose(f);
    return ok;
}

}  // namespace

std::string Importer::OriginalPath(Edition edition, const ManifestRecord& record) const {
    return assetsDir_ + "/" + EditionName(edition) + "/originals/" + record.dest;
}

// The copy of a verified .sam (importer.h): made when there is none of an accepted size, from `verified` (bytes that
// matched the manifest in a drop) or from the installed file, and only when those bytes hash to the manifest's.
void Importer::KeepOriginal(Edition edition, uint32_t record, const std::vector<uint8_t>* verified) {
    const ManifestRecord& r = ManifestRecords()[record];
    if (!PlayerEditable(r)) return;
    std::string copy = OriginalPath(edition, r);
    uint64_t size = 0;
    int64_t mtime = 0;
    if (StatFile(copy, size, mtime) && SizeAccepted(r, size)) return;
    std::vector<uint8_t> bytes;
    if (verified) bytes = *verified;
    else if (!ReadSmallFile(DestinationPath(edition, r), bytes)) return;
    if (!Accepts(r, bytes.size(), Sha256::Of(bytes.data(), bytes.size()))) return;
    std::error_code ec;
    fs::create_directories(fs::path(copy).parent_path(), ec);
    std::string err;
    if (!ec) ReplaceFile(copy, std::string(bytes.begin(), bytes.end()), err);
}

std::vector<EditedFile> Importer::EditedFiles(Edition edition) const {
    std::vector<EditedFile> files;
    std::vector<uint8_t> edited;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        edited = edited_;
    }
    const ManifestRecord* records = ManifestRecords();
    for (size_t i = 0; i < edited.size() && i < ManifestRecordCount(); ++i) {
        if (!(edited[i] & edition) || !(records[i].editions & edition)) continue;
        EditedFile f;
        f.dest = records[i].dest;
        f.path = DestinationPath(edition, records[i]);
        std::vector<uint8_t> bytes;
        f.original = ReadSmallFile(OriginalPath(edition, records[i]), bytes) &&
                     Accepts(records[i], bytes.size(), Sha256::Of(bytes.data(), bytes.size()));
        if (ReadSmallFile(f.path, bytes)) f.unreadableLine = SamUnreadableLine(f.dest, std::string(bytes.begin(), bytes.end()));
        files.push_back(f);
    }
    return files;
}

bool Importer::RestoreOriginal(Edition edition, const std::string& dest, std::string& err) {
    if (running_) {
        err = "an import is running";
        return false;
    }
    const ManifestRecord* r = FindByDest(edition, dest.c_str());
    if (!r || !PlayerEditable(*r)) {
        err = dest + " is not a settings file of this edition";
        return false;
    }
    std::vector<uint8_t> bytes;
    if (!ReadSmallFile(OriginalPath(edition, *r), bytes) ||
        !Accepts(*r, bytes.size(), Sha256::Of(bytes.data(), bytes.size()))) {
        err = "no verified copy of " + dest + " is kept; drop the disc again for the original";
        return false;
    }
    std::string tmp = NewStagingPath();
    std::string target = DestinationPath(edition, *r);
    if (!ReplaceFile(tmp, std::string(bytes.begin(), bytes.end()), err)) return false;
    std::error_code ec;
    fs::rename(tmp, target, ec);
    if (ec) {
        std::remove(tmp.c_str());
        err = "cannot write " + target + ": " + ec.message();
        return false;
    }
    MarkPresent(edition, RecordIndex(r), true);  // no longer the player's edit
    RecordStamps(nullptr);
    return true;
}

bool Importer::IsPresent(Edition edition, uint32_t record) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return record < present_.size() && (present_[record] & edition) != 0;
}

EditionStatus Importer::Status(Edition edition) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return StatusLocked(edition);
}

EditionStatus Importer::StatusLocked(Edition edition) const {
    EditionStatus s;
    s.edition = edition;
    const ManifestRecord* records = ManifestRecords();
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        const ManifestRecord& r = records[i];
        if (!(r.editions & edition)) continue;
        bool have = i < present_.size() && (present_[i] & edition) != 0;
        bool required = (r.flags & kFlagRequired) != 0;
        bool file = !SideFile(r);
        // A film the folder may hold as the Mac CD's .mov or the PC CD's .tgq (FilmCounterpart): one required file, the
        // .mov's, there when either is.
        bool satisfied = have;
        if (const ManifestRecord* other = required ? FilmCounterpart(r, edition) : nullptr) {
            if (other->flags & kFlagRequired) {
                size_t j = RecordIndex(other);
                if (Extension(r.dest) == "tgq") required = false;
                else satisfied = have || (j < present_.size() && (present_[j] & edition) != 0);
            }
        }
        ++s.records;
        s.required += required;
        s.requiredPresent += required && satisfied;
        s.files += file;
        if (have) {
            ++s.present;
            if (i < edited_.size() && (edited_[i] & edition)) ++s.edited;
            s.filesPresent += file;
            s.bytesPresent += RecordHash(r, 0).size;
        } else if (required && !satisfied && r.source < 3) {
            ++s.missing[r.source];
            if (file) ++s.missingFiles[r.source];
        }
    }
    s.unlocked = s.required > 0 && s.requiredPresent == s.required;
    return s;
}

std::vector<LanguageStatus> Importer::Languages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return LanguagesLocked();
}

std::vector<LanguageStatus> Importer::LanguagesLocked() const {
    const ManifestRecord* records = ManifestRecords();
    size_t n = ManifestRecordCount();
    auto have = [&](Edition e, size_t i) { return i < present_.size() && (present_[i] & e) != 0; };
    // the date fonts of an installed edition, for the packs that use the base edition's
    bool dateFonts = false;
    const std::pair<Edition, const char*> bases[] = {{kEditionEur, "data/Language/English/"},
                                                     {kEditionUs, "data/Language/American/"},
                                                     {kEditionEurPc, "data/Language/English/"},
                                                     {kEditionUsPc, "data/Language/American/"}};
    for (const auto& base : bases) {
        bool all = true;
        for (const char* font : {"DATEBIG.bf4", "DATEMED.bf4", "DATESMALL.bf4", "DATETINY.bf4"}) {
            const ManifestRecord* r = FindByDest(base.first, (std::string(base.second) + font).c_str());
            all = all && r && have(base.first, RecordIndex(r));
        }
        dateFonts = dateFonts || all;
    }
    std::vector<LanguageStatus> out;
    for (size_t l = 0; l < ManifestLanguageCount(); ++l) {
        const ManifestLanguage& lang = ManifestLanguages()[l];
        LanguageStatus s;
        s.name = lang.name;
        s.oldTables = (lang.flags & kLanguageOldTables) != 0;
        s.baseDateFonts = (lang.flags & kLanguageBaseDateFonts) != 0;
        s.baseDateFontsPresent = dateFonts;
        const std::string prefix = "lang/" + s.name + "/", text = prefix + "data/Language/" + s.name + "/";
        bool own = false;  // one of its own files (a content no other language has) is present
        const ManifestRecord* begin = std::partition_point(records, records + n, [&](const ManifestRecord& r) {
            return std::strcmp(r.dest, prefix.c_str()) < 0;
        });
        for (const ManifestRecord* r = begin; r != records + n && std::strncmp(r->dest, prefix.c_str(), prefix.size()) == 0; ++r) {
            if (!(r->editions & kEditionLanguages)) continue;
            bool p = have(kEditionLanguages, RecordIndex(r));
            std::string dest = r->dest;
            if (!(r->flags & kFlagRequired)) {
                ++s.optional;
                s.optionalPresent += p;
                continue;
            }
            std::string file = BaseName(dest);
            std::string ext = Extension(file);
            size_t* total = &s.text;
            size_t* present = &s.textPresent;
            if (dest.compare(0, text.size(), text) != 0) total = &s.speech, present = &s.speechPresent;
            else if (Lower(file) == "swears.txt") total = &s.filter, present = &s.filterPresent;
            else if (ext == "md2") total = &s.meshes, present = &s.meshesPresent;
            ++*total;
            *present += p;
            if (!p || own) continue;
            bool shared = false;
            for (unsigned k = 0; k < r->hash_count && !shared; ++k) {
                const ManifestHash& h = RecordHash(*r, k);
                for (const ManifestRecord* o : FindByContent(h.size, h.sha256)) {
                    std::string other = LanguageOf(*o);
                    shared = shared || (!other.empty() && other != s.name);
                }
            }
            own = !shared;
        }
        bool complete = s.textPresent == s.text && s.filterPresent == s.filter && s.meshesPresent == s.meshes &&
                        s.speechPresent == s.speech && (!s.baseDateFonts || s.baseDateFontsPresent);
        s.state = complete ? LanguageStatus::State::Available
                           : (own ? LanguageStatus::State::Partial : LanguageStatus::State::Absent);
        out.push_back(std::move(s));
    }
    return out;
}

std::vector<uint32_t> Importer::RecordsByPath(const std::string& lowerPath) const {
    std::vector<uint32_t> out;
    // every suffix of the path that starts at a folder boundary
    size_t start = 0;
    for (;;) {
        std::string tail = lowerPath.substr(start);
        auto range = byPath_.equal_range(tail);
        for (auto it = range.first; it != range.second; ++it) out.push_back(it->second);
        size_t slash = lowerPath.find('/', start);
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return out;
}

LauncherIni Importer::Ini() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ini_;
}

bool Importer::SetLastChoice(const std::string& choice, std::string& err) {
    std::lock_guard<std::mutex> lock(mutex_);
    ini_.lastChoice = choice;
    return SaveIniLocked(err);
}

bool Importer::SaveIniLocked(std::string& err) {
    ini_.dataFolder = config_.dataRoot;
    for (Edition e : kAllEditions) ini_.editions[EditionName(e)] = StatusLocked(e).StateWord();
    return SaveLauncherIni(iniPath_, ini_, err);
}

std::string Importer::NewStagingPath() {
    return stagingDir_ + "/import-" + std::to_string(staged_++) + ".part";
}

ImportReport Importer::Import(const std::vector<std::string>& paths) {
    if (running_.exchange(true)) {
        ImportReport r;
        r.errors.push_back("an import is already running");
        return r;
    }
    cancel_ = false;
    ImportReport r = DoImport(paths);
    running_ = false;
    return r;
}

bool Importer::Start(const std::vector<std::string>& paths, std::string& err) {
    if (running_.exchange(true)) {
        err = "an import is already running";
        return false;
    }
    if (worker_.joinable()) worker_.join();
    cancel_ = false;
    try {
        worker_ = std::thread([this, paths]() {
            workerReport_ = DoImport(paths);
            running_ = false;
        });
    } catch (...) {
        running_ = false;
        err = "cannot start the import thread";
        return false;
    }
    return true;
}

bool Importer::Running() const { return running_; }

void Importer::Cancel() { cancel_ = true; }

ImportReport Importer::Wait() {
    if (worker_.joinable()) worker_.join();
    return workerReport_;
}

ImportReport Importer::DoImport(const std::vector<std::string>& paths) {
    Run run;
    try {
        if (!opened_) {
            run.report.errors.push_back("the importer was not opened");
        } else {
            run.sniff.spillDir = stagingDir_;
            run.sniff.spillOver = config_.spillOver;
            // A 7-Zip or RAR archive is unpacked as it opens: the cancel button stops it, and the counter says what it is doing.
            run.sniff.cancel = &cancel_;
            run.sniff.progress = [this, &run](const std::string& what) {
                ImportProgress ev;
                ev.kind = ImportProgress::Kind::Scanning;
                ev.total = run.work.size();
                ev.path = what;
                progress_.Push(ev);
            };
            for (const std::string& p : paths) {
                if (cancel_) break;
                EnumeratePath(run, p);
            }
            uint64_t total = run.work.size();
            for (size_t i = 0; i < run.work.size() && !cancel_ && !run.stop; ++i) {
                ImportProgress ev;
                ev.kind = ImportProgress::Kind::Hashing;
                ev.n = i + 1;
                ev.total = total;
                ev.bytes = run.doneBytes;
                ev.totalBytes = run.totalBytes;
                ev.path = run.work[i].display;
                progress_.Push(ev);
                Process(run, run.work[i]);
                run.doneBytes += run.work[i].entry.size;
                run.work[i].source.reset();  // a spilled image goes as soon as nothing needs it
            }
            for (const Ps1Disc& disc : run.playStation) {
                if (cancel_ || run.stop) break;
                std::string reason;
                const bool converted = ConvertPs1(disc, fs::u8path(config_.dataRoot), reason);
                const std::string line = converted
                    ? "PlayStation rides: 7 rides and the Tower converted from the " + disc.region + " disc (" + disc.boot + ")."
                    : "PlayStation disc recognised (" + disc.region + ", " + disc.boot + "); " + OneLine(reason);
                run.report.playStation.push_back(line);
                run.report.playStationConverted = run.report.playStationConverted || converted;
                ImportProgress ev;
                ev.kind = ImportProgress::Kind::Content;
                ev.message = line;
                progress_.Push(ev);
            }
            run.report.cancelled = cancel_ && !run.stop;
        }
    } catch (...) {
        run.report.errors.push_back("out of memory");
        run.report.aborted = true;
    }
    run.work.clear();
    // The drop's best game icon becomes <data folder>/icon.png when there is none yet or it beats the one there (icon.h).
    if (!run.icon.Empty() && opened_ && !cancel_ && !run.report.aborted) {
        const std::string iconPath = config_.dataRoot + "/icon.png";
        IconImage have;
        std::string err;
        if (!ReadIconPng(iconPath, have, nullptr) || BetterIcon(run.icon, have)) {
            if (WriteIconPng(iconPath, run.icon, run.iconSource, err)) {
                char text[64];
                std::snprintf(text, sizeof text, ", %dx%d, %d-bit", run.icon.width, run.icon.height, run.icon.bitDepth);
                run.report.icon = run.iconSource + text;
                ImportProgress ev;
                ev.kind = ImportProgress::Kind::Written;
                ev.path = "icon.png (the game's icon, from " + run.iconSource + ")";
                progress_.Push(ev);
            } else {
                run.report.errors.push_back("icon.png: " + err);
            }
        }
    }
    // a destination the drop also holds the right file for is not news (a disc's other languages, say)
    for (const std::string& dest : run.different) {
        if (run.matchedDests.count(dest)) continue;
        run.report.differentVersions.push_back(dest);
        ImportProgress ev;
        ev.kind = ImportProgress::Kind::DifferentVersion;
        ev.path = dest;
        progress_.Push(ev);
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (Edition e : kAllEditions) run.report.status[EditionIndex(e)] = StatusLocked(e);
        run.report.languages = LanguagesLocked();
        std::string err;
        if (opened_ && !SaveIniLocked(err)) run.report.errors.push_back(err);
    }
    if (opened_) RecordStamps(nullptr);
    ImportProgress done;
    done.kind = ImportProgress::Kind::Finished;
    done.n = done.total = run.report.hashed;
    done.bytes = done.totalBytes = run.totalBytes;
    done.message = run.report.Summary();
    progress_.Push(done);
    return run.report;
}

void Importer::EnumeratePath(Run& run, const std::string& path) {
    std::error_code ec;
    fs::path canon = fs::weakly_canonical(fs::path(path), ec);
    std::string key = ec ? path : tpw::GenericPathText(canon);
    auto fail = [&](const std::string& message) {
        run.report.errors.push_back(message);
        ImportProgress ev;
        ev.kind = ImportProgress::Kind::Error;
        ev.path = BaseName(key);
        ev.message = message;
        progress_.Push(ev);
    };
    if (!fs::exists(canon, ec)) {
        fail("cannot find " + path);
        return;
    }
    if (!run.opened.insert(key).second) return;
    if (Under(key, assetsCanon_)) {
        fail(path + " is inside the launcher's data folder; nothing was read from it");
        return;
    }
    ImportProgress ev;
    ev.kind = ImportProgress::Kind::Scanning;
    ev.total = run.work.size();
    ev.path = BaseName(key);
    progress_.Push(ev);
    std::string err;
    if (fs::is_directory(canon, ec)) {
        std::unique_ptr<Source> folder = OpenFolder(key, err);
        if (!folder) {
            fail(err);
            return;
        }
        Enumerate(run, std::shared_ptr<Source>(std::move(folder)), BaseName(key), key, 0);
        return;
    }
    if (Extension(key) == "cue") {
        std::vector<std::string> files = CueFiles(key);
        if (files.empty()) {
            ++run.report.entries;
            ++run.report.unknownFiles;
            run.report.unknownBytes += fs::file_size(canon, ec);
        }
        for (const std::string& f : files) EnumeratePath(run, f);
        return;
    }
    // A part of a multi-volume archive opens the whole set, once, whichever part comes first.
    {
        const std::string dir = key.substr(0, key.find_last_of('/') + 1);
        std::vector<std::string> parts;
        std::string first;
        auto exists = [&](const std::string& name) { return fs::is_regular_file(fs::path(dir + name), ec); };
        if (VolumeSet(BaseName(key), exists, parts, first)) {
            if (parts.empty()) {
                fail(BaseName(key) + ": " + MissingFirstPart(first));
                return;
            }
            bool done = false;
            for (const std::string& part : parts) {
                if (dir + part != key && !run.opened.insert(dir + part).second) done = true;
            }
            if (done) return;
            std::vector<std::shared_ptr<Blob>> volumes;
            for (const std::string& part : parts) {
                volumes.push_back(OpenFileBlob(dir + part, err));
                if (!volumes.back()) {
                    fail(err);
                    return;
                }
            }
            if (OpenVolumes(run, volumes, first, 0)) return;
        }
    }
    std::shared_ptr<Blob> blob = OpenFileBlob(key, err);
    if (!blob) {
        fail(err);
        return;
    }
    if (PlayStationImage(run, blob, BaseName(key))) return;
    const std::string format = UnsupportedDiscFormat(key);
    if (!format.empty()) {
        fail(format);
        return;
    }
    std::string sniffErr;
    SniffStatus sniffStatus = SniffStatus::NotContainer;
    std::shared_ptr<Source> source = OpenSniffed(blob, run.sniff, 0, sniffStatus, sniffErr);
    blob.reset();
    if (source) {
        ++run.report.containers;
        Enumerate(run, source, BaseName(key), std::string(), 1);
        return;
    }
    if (sniffStatus == SniffStatus::Failed) {   // a damaged container is an error, never a file with nothing in it
        if (!cancel_) fail(BaseName(key) + ": " + sniffErr);   // a cancelled unpacking says nothing
        return;
    }
    auto single = std::make_shared<SingleFileSource>();
    if (!single->Init(key, err)) {
        fail(err);
        return;
    }
    Enumerate(run, single, std::string(), std::string(), 0);
}

bool Importer::OpenVolumes(Run& run, const std::vector<std::shared_ptr<Blob>>& volumes, const std::string& display,
                           int depth) {
    std::string err;
    SniffStatus status = SniffStatus::NotContainer;
    std::shared_ptr<Source> source = OpenVolumesSniffed(volumes, run.sniff, depth, status, err);
    if (source) {
        ++run.report.containers;
        ImportProgress ev;
        ev.kind = ImportProgress::Kind::Scanning;
        ev.total = run.work.size();
        ev.path = display;
        progress_.Push(ev);
        Enumerate(run, source, display, std::string(), depth + 1);
        return true;
    }
    // A lone "x.part1.rar" that is no archive is a plain file; the parts of a set whose first is none are an error.
    if (status == SniffStatus::NotContainer && volumes.size() == 1) return false;
    if (cancel_) return true;
    run.report.errors.push_back(display + ": " + err);
    ImportProgress ev;
    ev.kind = ImportProgress::Kind::Error;
    ev.path = display;
    ev.message = display + ": " + err;
    progress_.Push(ev);
    return true;
}

void Importer::Enumerate(Run& run, const std::shared_ptr<Source>& source, const std::string& display,
                         const std::string& folderRoot, int depth) {
    if (const auto blob = source->ImageBlob()) {
        if (PlayStationImage(run, blob, display)) return;
    }
    // a drop that holds the data folder does not read it back
    std::string skip;
    if (!folderRoot.empty() && Under(assetsCanon_, folderRoot) && assetsCanon_ != folderRoot) {
        skip = assetsCanon_.substr(folderRoot.size() + 1);
    }
    const std::vector<Entry>& entries = source->Entries();
    std::map<std::string, const Entry*> byPath;   // for the parts of a multi-volume archive inside a container
    std::set<std::string> setsDone;               // the first parts of the sets opened from inside a container
    for (const Entry& e : entries) {
        if (cancel_ || run.stop) return;
        if (!skip.empty() && Under(e.path, skip)) continue;
        std::string name = display.empty() ? e.path : display + "/" + e.path;
        // A part of a multi-volume archive opens the whole set, once, whichever part comes first; the others are skipped.
        if (e.kind == EntryKind::Data && depth < kMaxNesting) {
            const std::string dir = e.path.substr(0, e.path.find_last_of('/') + 1);
            std::string diskDir;
            if (!folderRoot.empty()) {
                std::error_code ec;
                fs::path canon = fs::weakly_canonical(fs::path(folderRoot + "/" + e.path), ec);
                std::string key = ec ? folderRoot + "/" + e.path : tpw::GenericPathText(canon);
                diskDir = key.substr(0, key.find_last_of('/') + 1);
            } else if (byPath.empty()) {
                for (const Entry& x : entries) {
                    if (x.kind == EntryKind::Data) byPath.emplace(x.path, &x);
                }
            }
            std::vector<std::string> parts;
            std::string first;
            auto exists = [&](const std::string& part) {
                std::error_code ec;
                return folderRoot.empty() ? byPath.count(dir + part) != 0 : fs::is_regular_file(fs::path(diskDir + part), ec);
            };
            if (VolumeSet(BaseName(e.path), exists, parts, first)) {
                const std::string firstName = display.empty() ? dir + first : display + "/" + dir + first;
                if (parts.empty()) {
                    const std::string message = name + ": " + MissingFirstPart(first);
                    run.report.errors.push_back(message);
                    ImportProgress ev;
                    ev.kind = ImportProgress::Kind::Error;
                    ev.path = name;
                    ev.message = message;
                    progress_.Push(ev);
                    continue;
                }
                bool done = false;
                if (folderRoot.empty()) {
                    done = !setsDone.insert(dir + first).second;
                } else {
                    for (const std::string& part : parts) {
                        if (!run.opened.insert(diskDir + part).second) done = true;
                    }
                }
                if (done) continue;
                std::vector<std::shared_ptr<Blob>> volumes;
                std::string err;
                for (const std::string& part : parts) {
                    volumes.push_back(folderRoot.empty() ? OpenEntryBlob(source, *byPath[dir + part], err)
                                                         : OpenFileBlob(diskDir + part, err));
                    if (!volumes.back()) break;
                }
                if (!volumes.back()) {
                    const std::string message = firstName + ": " + err;
                    run.report.errors.push_back(message);
                    ImportProgress ev;
                    ev.kind = ImportProgress::Kind::Error;
                    ev.path = firstName;
                    ev.message = message;
                    progress_.Push(ev);
                    continue;
                }
                if (OpenVolumes(run, volumes, firstName, depth)) continue;
            }
        }
        const std::string format = UnsupportedDiscFormat(e.path);
        if (e.kind == EntryKind::Data && !format.empty()) {
            run.report.errors.push_back(name + ": " + format);
            ImportProgress ev;
            ev.kind = ImportProgress::Kind::Error;
            ev.path = name;
            ev.message = name + ": " + format;
            progress_.Push(ev);
            continue;
        }
        if (e.unreadable) {   // no game file is in such a cab folder: an installer's own files, ignored
            ++run.report.entries;
            ++run.report.unknownFiles;
            run.report.unknownBytes += e.size;
            continue;
        }
        bool known = bySize_.count(e.size) != 0;
        // The game's icon (icon.h): the Mac program's resource fork, TP.ico or the PC program, read whole and decoded; the
        // best picture of the drop is written as icon.png at the end. Never an error: a file that will not read is skipped.
        if (e.size > 0 && e.size <= kIconFileLimit) {
            const IconFile kind = IconFileKind(e.path, e.kind == EntryKind::Rsrc);
            if (kind != IconFile::None) {
                std::string err;
                std::vector<uint8_t> bytes;
                std::unique_ptr<Stream> in = source->Open(e, err);
                IconImage found;
                if (in && ReadAll(*in, kIconFileLimit, bytes, err) && DecodeIcon(kind, bytes.data(), bytes.size(), found) &&
                    BetterIcon(found, run.icon)) {
                    run.icon = std::move(found);
                    const size_t slash = e.path.find_last_of('/');
                    run.iconSource = (slash == std::string::npos ? e.path : e.path.substr(slash + 1)) +
                                     (e.kind == EntryKind::Rsrc ? " (resource fork)" : "");
                }
            }
        }
        if (e.kind == EntryKind::Data && !known && e.size >= 2048 && ContainerExtension(Extension(e.path))) {
            std::string err;
            std::shared_ptr<Source> child;
            SniffStatus childStatus = SniffStatus::NotContainer;
            bool seen = false;
            if (!folderRoot.empty()) {
                // a file of a folder: read in place, with random access
                std::string disk = folderRoot + "/" + e.path;
                std::error_code ec;
                fs::path canon = fs::weakly_canonical(fs::path(disk), ec);
                std::string key = ec ? disk : tpw::GenericPathText(canon);
                seen = !run.opened.insert(key).second;
                if (!seen) {
                    std::shared_ptr<Blob> blob = OpenFileBlob(key, err);
                    if (blob) child = OpenSniffed(blob, run.sniff, depth, childStatus, err);
                    else childStatus = SniffStatus::Failed;   // the folder file cannot be read: OpenFileBlob said why
                }
            } else {
                child = OpenEntrySniffed(source, e, run.sniff, depth, childStatus, err);
            }
            if (seen) continue;
            if (child) {
                ++run.report.containers;
                ImportProgress ev;
                ev.kind = ImportProgress::Kind::Scanning;
                ev.total = run.work.size();
                ev.path = name;
                progress_.Push(ev);
                Enumerate(run, child, name, std::string(), depth + 1);
                continue;
            }
            // A plain file with a container's name stays silent (it is counted among the other files below); every
            // container that failed to open is an error with its path and the reader's message.
            if (childStatus == SniffStatus::Failed && !cancel_) {   // a cancelled unpacking says nothing
                run.report.errors.push_back(name + ": " + err);
                ImportProgress ev;
                ev.kind = ImportProgress::Kind::Error;
                ev.path = name;
                ev.message = name + ": " + err;   // the log shows the message alone, so it names the file
                progress_.Push(ev);
            }
        }
        // The PC disc's data/fonts.wad holds the 17 sign fonts RefPack-compressed (wad.cpp): its entries are matched as
        // files of their own (the Windows-disc editions' plain data/fonts/*.TTF), and the wad itself as a file too.
        if (e.kind == EntryKind::Data && e.size >= 0x58 && Lower(BaseName(e.path)) == "fonts.wad" && depth < kMaxNesting) {
            std::string err;
            std::shared_ptr<Blob> blob = folderRoot.empty() ? OpenEntryBlob(source, e, err)
                                                            : OpenFileBlob(folderRoot + "/" + e.path, err);
            std::shared_ptr<Source> wad = blob ? std::shared_ptr<Source>(OpenWad(blob, err)) : nullptr;
            if (wad) {
                ++run.report.containers;
                Enumerate(run, wad, name, std::string(), depth + 1);
            } else {
                run.report.errors.push_back(name + ": " + err);
                ImportProgress ev;
                ev.kind = ImportProgress::Kind::Error;
                ev.path = name;
                ev.message = err;
                progress_.Push(ev);
            }
        }
        ++run.report.entries;
        if (known) {
            run.work.push_back(Work{source, e, name});
            run.totalBytes += e.size;
        } else {
            Unmatched(run, e);
        }
    }
}

bool Importer::PlayStationImage(Run& run, const std::shared_ptr<Blob>& blob, const std::string& display) {
    Ps1Disc disc;
    std::string reason;
    const Ps1Status status = RecognisePs1(blob, disc, reason);
    if (status == Ps1Status::NotPlayStation) return false;
    ++run.report.containers;
    if (status == Ps1Status::Supported) {
        run.playStation.push_back(std::move(disc));
    } else {
        const std::string line = display + ": PlayStation disc refused: " + OneLine(reason);
        run.report.errors.push_back(line);
        ImportProgress ev;
        ev.kind = ImportProgress::Kind::Error;
        ev.path = display;
        ev.message = line;
        progress_.Push(ev);
    }
    return true;
}

void Importer::Unmatched(Run& run, const Entry& entry) {
    std::vector<uint32_t> recs = RecordsByPath(EvidencePath(entry));
    const ManifestRecord* records = ManifestRecords();
    for (uint32_t r : recs) {
        // a Finder info file is rebuilt from the Finder info, so another one proves nothing
        if (records[r].flags & kFlagFinfo) continue;
        run.different.insert(records[r].dest);
        return;
    }
    if (entry.kind == EntryKind::Data) ++run.report.unknownFiles;  // a fork is part of its file
    run.report.unknownBytes += entry.size;
}

void Importer::Count(Run& run, Edition edition, uint32_t record, bool written) {
    run.matchedDests.insert(ManifestRecords()[record].dest);
    if (!run.counted.insert({EditionIndex(edition), record}).second) return;
    ImportProgress ev;
    ev.kind = written ? ImportProgress::Kind::Written : ImportProgress::Kind::AlreadyThere;
    ev.path = std::string(EditionName(edition)) + "/" + ManifestRecords()[record].dest;
    if (written) ++run.report.written;
    else ++run.report.alreadyThere;
    progress_.Push(ev);
}

void Importer::Process(Run& run, Work& work) {
    const Entry& e = work.entry;
    const ManifestRecord* records = ManifestRecords();
    std::string evidence = EvidencePath(e);
    auto allowed = [&](const ManifestRecord& r) {
        return (r.editions & config_.editions) != 0 &&
               (!Trivial(r, e.size) || EndsWithDest(evidence, Lower(r.dest)) || LanguageEvidence(evidence, r));
    };
    auto error = [&](const std::string& message) {
        run.report.errors.push_back(work.display + ": " + message);
        ImportProgress ev;
        ev.kind = ImportProgress::Kind::Error;
        ev.path = work.display;
        ev.message = message;
        progress_.Push(ev);
    };
    // is one of the records this size could be still missing somewhere?
    bool need = false;
    auto bySize = bySize_.find(e.size);
    if (bySize != bySize_.end()) {
        for (uint32_t r : bySize->second) {
            if (!allowed(records[r])) continue;
            for (Edition ed : kFileEditions) {
                if ((records[r].editions & config_.editions & ed) && !IsPresent(ed, r)) need = true;
            }
        }
    }

    Sha256Digest digest;
    std::vector<uint8_t> small;
    std::string staged;
    std::string err;
    std::unique_ptr<Stream> in = e.size == 0 ? nullptr : work.source->Open(e, err);
    if (e.size != 0 && !in) {
        error(err);
        return;
    }
    if (e.size <= kSmall) {
        if (in && !ReadAll(*in, kSmall, small, err)) {
            error(err);
            return;
        }
        if (small.size() != e.size) {
            error("the file is shorter than its listed size");
            return;
        }
        digest = Sha256::Of(small.data(), small.size());
    } else {
        FILE* out = nullptr;
        if (need) {
            staged = NewStagingPath();
            out = std::fopen(staged.c_str(), "wbx");
            if (!out) {
                error("cannot create " + staged);
                run.report.aborted = run.stop = true;
                return;
            }
        }
        auto discard = [&]() {
            if (out) std::fclose(out);
            out = nullptr;
            if (!staged.empty()) std::remove(staged.c_str());
        };
        Sha256 sha;
        std::vector<uint8_t> buf(kChunk);
        uint64_t got = 0, nextEvent = kProgressStep;
        for (;;) {
            if (cancel_) {
                discard();
                return;
            }
            int64_t n = in->Read(buf.data(), buf.size(), err);
            if (n < 0) {
                discard();
                error(err);
                return;
            }
            if (n == 0) break;
            sha.Update(buf.data(), static_cast<size_t>(n));
            if (out && std::fwrite(buf.data(), 1, static_cast<size_t>(n), out) != static_cast<size_t>(n)) {
                discard();
                error("cannot write " + staged + " (disk full?)");
                run.report.aborted = run.stop = true;
                return;
            }
            got += static_cast<uint64_t>(n);
            if (got >= nextEvent) {
                nextEvent += kProgressStep;
                ImportProgress ev;
                ev.kind = ImportProgress::Kind::Hashing;
                ev.n = static_cast<uint64_t>(&work - run.work.data()) + 1;
                ev.total = run.work.size();
                ev.bytes = run.doneBytes + got;
                ev.totalBytes = run.totalBytes;
                ev.path = work.display;
                progress_.Push(ev);
            }
        }
        if (got != e.size) {
            discard();
            error("the file is shorter than its listed size");
            return;
        }
        if (out) {
            bool ok = std::fclose(out) == 0;
            out = nullptr;
            if (!ok) {
                discard();
                error("cannot write " + staged + " (disk full?)");
                run.report.aborted = run.stop = true;
                return;
            }
        }
        digest = sha.Final();
    }
    in.reset();
    ++run.report.hashed;
    run.report.bytesHashed += e.size;

    std::vector<const ManifestRecord*> hits;
    for (const ManifestRecord* r : FindByContent(e.size, digest.bytes)) {
        if (allowed(*r)) hits.push_back(r);
    }
    if (hits.empty()) {
        if (!staged.empty()) std::remove(staged.c_str());
        Unmatched(run, e);
        return;
    }
    ++run.report.matched;
    std::vector<std::pair<Edition, uint32_t>> targets;
    for (const ManifestRecord* r : hits) {
        uint32_t idx = RecordIndex(r);
        for (Edition ed : kFileEditions) {
            if (!(r->editions & config_.editions & ed)) continue;
            if (IsPresent(ed, idx)) {
                Count(run, ed, idx, false);
                if (e.size <= kSmall) KeepOriginal(ed, idx, &small);  // a .sam edited before its copy was kept
            } else {
                targets.emplace_back(ed, idx);
            }
        }
    }
    bool ok = true;
    if (e.size <= kSmall) {
        for (const auto& t : targets) {
            if (!(ok = WriteSmall(run, small, t.first, t.second))) break;
        }
    } else if (!targets.empty()) {
        if (staged.empty()) {
            error("internal: a missing file was not staged");
            return;
        }
        ok = Install(run, staged, targets);
        staged.clear();
    }
    if (!staged.empty()) std::remove(staged.c_str());
    if (!ok) return;

    // the Finder info side files of what matched, made from this entry's type and creator and checked by hash
    if (e.kind != EntryKind::Data || !e.hasFinderInfo) return;
    std::vector<uint8_t> finfo(10);
    std::memcpy(finfo.data(), e.finderType, 4);
    finfo[4] = '\t';
    std::memcpy(finfo.data() + 5, e.finderCreator, 4);
    finfo[9] = '\n';
    Sha256Digest fd = Sha256::Of(finfo.data(), finfo.size());
    for (const ManifestRecord* r : hits) {
        if (SideFile(*r)) continue;
        std::string side = std::string(r->dest) + ".finfo";
        for (Edition ed : kFileEditions) {
            if (!(r->editions & config_.editions & ed)) continue;
            const ManifestRecord* fr = FindByDest(ed, side.c_str());
            if (!fr || !Accepts(*fr, finfo.size(), fd)) continue;
            uint32_t idx = RecordIndex(fr);
            if (IsPresent(ed, idx)) Count(run, ed, idx, false);
            else if (!WriteSmall(run, finfo, ed, idx)) return;
        }
    }
}

bool Importer::WriteSmall(Run& run, const std::vector<uint8_t>& bytes, Edition edition, uint32_t record) {
    std::string tmp = NewStagingPath();
    FILE* f = std::fopen(tmp.c_str(), "wbx");
    bool ok = f != nullptr;
    if (f) {
        ok = bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        ok = std::fclose(f) == 0 && ok;
    }
    if (!ok) {
        std::remove(tmp.c_str());
        run.report.errors.push_back("cannot write " + tmp + " (disk full?)");
        run.report.aborted = run.stop = true;
        ImportProgress ev;
        ev.kind = ImportProgress::Kind::Error;
        ev.message = run.report.errors.back();
        progress_.Push(ev);
        return false;
    }
    return Install(run, tmp, {{edition, record}});
}

bool Importer::Install(Run& run, std::string staged, const std::vector<std::pair<Edition, uint32_t>>& targets) {
    const ManifestRecord* records = ManifestRecords();
    for (size_t i = 0; i < targets.size(); ++i) {
        Edition ed = targets[i].first;
        uint32_t idx = targets[i].second;
        fs::path dest(DestinationPath(ed, records[idx]));
        std::error_code ec;
        fs::create_directories(dest.parent_path(), ec);
        std::string from = staged;
        if (!ec && i + 1 < targets.size()) {
            // more destinations follow: this one gets a hard link to the staged file (a copy where the file system has
            // none), the last one the staged file itself. The European disc's files belong to the European edition and
            // to the Windows one, so linking keeps them on disk once.
            from = NewStagingPath();
            fs::create_hard_link(staged, from, ec);
            if (ec) {
                ec.clear();
                fs::copy_file(staged, from, ec);
            }
        }
        if (!ec) fs::rename(from, dest, ec);
        if (ec) {
            if (from != staged) std::remove(from.c_str());
            std::remove(staged.c_str());
            run.report.errors.push_back("cannot write " + tpw::GenericPathText(dest) + ": " + ec.message());
            run.report.aborted = run.stop = true;
            ImportProgress ev;
            ev.kind = ImportProgress::Kind::Error;
            ev.path = std::string(EditionName(ed)) + "/" + records[idx].dest;
            ev.message = run.report.errors.back();
            progress_.Push(ev);
            return false;
        }
        MarkPresent(ed, idx, true);
        KeepOriginal(ed, idx, nullptr);
        Count(run, ed, idx, true);
    }
    return true;
}

long Importer::VerifyEdition(Edition edition, std::string& err) {
    if (running_) {
        err = "an import is running";
        return -1;
    }
    long removed = 0;
    try {
        const ManifestRecord* records = ManifestRecords();
        for (size_t i = 0; i < ManifestRecordCount(); ++i) {
            uint32_t idx = static_cast<uint32_t>(i);
            if (!(records[i].editions & edition) || !IsPresent(edition, idx)) continue;
            std::string path = DestinationPath(edition, records[i]);
            std::error_code ec;
            uint64_t size = fs::file_size(path, ec);
            bool read = false, good = false;
            if (!ec) {
                FILE* f = std::fopen(path.c_str(), "rb");
                if (f) {
                    PlainFileStream s(f, size);
                    Sha256Digest d;
                    std::string readErr;
                    read = HashStream(s, d, readErr);
                    good = read && Accepts(records[i], size, d);
                }
            }
            if (good) {
                KeepOriginal(edition, idx, nullptr);
            } else if (read && PlayerEditable(records[i])) {
                MarkEdited(edition, idx);  // the player's edit, kept as the startup check keeps it
            } else {
                std::remove(path.c_str());
                MarkPresent(edition, idx, false);
                ++removed;
            }
        }
        RecordStamps(nullptr);
        std::lock_guard<std::mutex> lock(mutex_);
        if (!SaveIniLocked(err)) return -1;
    } catch (...) {
        err = "out of memory";
        return -1;
    }
    return removed;
}

bool Importer::RemoveEdition(Edition edition, std::string& err) {
    if (running_) {
        err = "an import is running";
        return false;
    }
    std::error_code ec;
    std::string dir = assetsDir_ + "/" + EditionName(edition);
    fs::remove_all(dir, ec);
    if (ec) {
        err = "cannot remove " + dir + ": " + ec.message();
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (uint8_t& p : present_) p &= static_cast<uint8_t>(~edition);
        for (uint8_t& p : edited_) p &= static_cast<uint8_t>(~edition);
    }
    RecordStamps(nullptr);
    std::lock_guard<std::mutex> lock(mutex_);
    return SaveIniLocked(err);
}

}  // namespace tpw::assets

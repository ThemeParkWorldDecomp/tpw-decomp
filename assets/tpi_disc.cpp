// tpi_disc.cpp: recognise a Theme Park Inc copy by its content (tpi_disc.h) and present it as its installed Data folder.
#include "tpi_disc.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <set>

#include "installshield.h"

namespace tpw::assets {
namespace {

std::string Lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool EndsWith(const std::string& s, const std::string& tail) {
    return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

// The three worlds and the two files each must have, below the Data folder.
const char* const kWorlds[3] = {"arabian", "water", "science"};
const char* const kWorldFiles[2] = {"Standard.sam", "terrain.wad"};
// Finds the Data folder: the file that marks it.
const char kMarker[] = "levels/arabian/standard.sam";
constexpr uint64_t kSmallFile = 1 << 20;

// A view of another source: the entries under one prefix, listed under another ("Data/"), everything else hidden. The
// converter sees the copy's Data folder at the root whatever the copy's own layout was (an installed game's folder, a
// bare Data folder, the cabinets' file group).
class RerootedSource : public Source {
public:
    RerootedSource(std::shared_ptr<Source> inner, std::string strip, std::string add)
        : inner_(std::move(inner)), strip_(std::move(strip)), add_(std::move(add)) {
        for (const Entry& e : inner_->Entries()) {
            if (e.path.size() <= strip_.size() || e.path.compare(0, strip_.size(), strip_) != 0) continue;
            Entry out = e;
            out.path = add_ + e.path.substr(strip_.size());
            out.id = entries_.size();
            inner_index_.push_back(&e - inner_->Entries().data());
            entries_.push_back(std::move(out));
        }
    }
    const char* Kind() const override { return inner_->Kind(); }
    const std::vector<Entry>& Entries() const override { return entries_; }
    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        if (entry.id >= inner_index_.size()) {
            err = "no such entry";
            return nullptr;
        }
        return inner_->Open(inner_->Entries()[inner_index_[size_t(entry.id)]], err);
    }
    std::shared_ptr<Blob> OpenBlob(const Entry& entry, std::string& err) override {
        if (entry.id >= inner_index_.size()) return nullptr;
        return inner_->OpenBlob(inner_->Entries()[inner_index_[size_t(entry.id)]], err);
    }

private:
    std::shared_ptr<Source> inner_;
    std::string strip_, add_;
    std::vector<Entry> entries_;
    std::vector<size_t> inner_index_;
};

// The parent folder of a path ("a/b/c" gives "a/b/", "c" gives "").
std::string Dir(const std::string& path) {
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

// A case-insensitive index of a source's Data entries.
struct Index {
    std::map<std::string, const Entry*> byLower;
    explicit Index(const Source& source) {
        for (const Entry& e : source.Entries()) {
            if (e.kind != EntryKind::Data) continue;
            byLower.emplace(Lower(e.path), &e);
        }
    }
    const Entry* Find(const std::string& path) const {
        auto it = byLower.find(Lower(path));
        return it == byLower.end() ? nullptr : it->second;
    }
};

// The Data folders found in a source: the prefix of each (with its trailing '/', "" for the root), by the marker file.
std::vector<std::string> DataFolders(const Source& source) {
    std::vector<std::string> out;
    for (const Entry& e : source.Entries()) {
        if (e.kind != EntryKind::Data) continue;
        const std::string low = Lower(e.path);
        if (!EndsWith(low, kMarker)) continue;
        if (low.size() > sizeof kMarker - 1 && low[low.size() - sizeof kMarker] != '/') continue;
        out.push_back(e.path.substr(0, e.path.size() - (sizeof kMarker - 1)));
    }
    return out;
}

// Reads a small text file of the source, "" when it is not there or not small.
std::string SmallText(Source& source, const Entry* e) {
    if (!e || e->size > kSmallFile) return {};
    std::string err;
    std::unique_ptr<Stream> s = source.Open(*e, err);
    if (!s) return {};
    std::vector<uint8_t> bytes;
    if (!ReadAll(*s, kSmallFile, bytes, err)) return {};
    return std::string(bytes.begin(), bytes.end());
}

// Which release the Data folder belongs to, from its content. Every disc's own strings name the game "Theme Park Inc"
// (Online.sam's heading, AUTORUN.INF), so the languages below Data/Language tell the releases apart: the European
// release (sold as Theme Park Inc, and as Theme Park Manager in Germany, from the same cabinets) carries Dutch, English,
// French, German and Swedish; the North American SimCoaster carries "American" alone (the Fargus Russian release too,
// its Russian strings under that folder name); the Chinese release carries SimplifiedChinese. `hint` adds what the
// container showed (the cabinets' component names: Fargus's "Rus").
std::string Edition(Source& data, const Index& index, const std::vector<std::string>& hint) {
    std::set<std::string> languages;
    for (const Entry& e : data.Entries()) {
        const std::string low = Lower(e.path);
        if (low.compare(0, 14, "data/language/") != 0) continue;
        const size_t end = low.find('/', 14);
        if (end == std::string::npos) continue;
        languages.insert(low.substr(14, end - 14));
    }
    bool russian = false;
    for (const std::string& h : hint) russian = russian || Lower(h) == "rus";
    if (!russian && languages.count("american")) {
        // Fargus kept the folder name: its strings are Russian (bytes of the Cyrillic code page) where the American
        // ones are ASCII.
        const std::string text = SmallText(data, index.Find("Data/Language/American/THEMENAMES.str"));
        size_t high = 0;
        for (unsigned char c : text) high += c >= 0xc0;
        russian = !text.empty() && high * 4 > text.size();
    }
    if (russian) return "SimCoaster (Russian)";
    if (languages.count("simplifiedchinese") || languages.count("chinese")) return "Theme Park Inc (Chinese)";
    if (languages.count("traditionalchinese")) return "SimCoaster (Taiwan)";
    if (languages.count("american") && !languages.count("english")) return "SimCoaster";
    return "Theme Park Inc";
}

// Whether a source holds level files of this game (Data/levels/<world>/...) without the game itself: the SimCoaster
// Bonus Disk (Extra Value Pack) installs two rides and prebuilt coasters into an installed game's folders.
bool AddOnOnly(const Source& source) {
    for (const Entry& e : source.Entries()) {
        if (e.kind != EntryKind::Data) continue;
        const std::string low = Lower(e.path);
        for (const char* world : kWorlds) {
            const std::string folder = std::string("data/levels/") + world + "/";
            const size_t at = low.find(folder);
            if (at != std::string::npos && (at == 0 || low[at - 1] == '/')) return true;
        }
    }
    return false;
}
const char kAddOnReason[] =
    "this is an add-on for the game (the SimCoaster Bonus Disk's rides and coasters), not the game: the mod needs a "
    "copy of Theme Park Inc or SimCoaster itself";

// The worlds of a Data view: Supported when all three are whole, else Rejected with the reason. disc.worlds is filled.
TpiStatus CheckWorlds(const std::shared_ptr<Source>& data, const std::vector<std::string>& hint, TpiDisc& disc,
                      std::string& reason) {
    const Index index(*data);
    disc.worlds.clear();
    std::vector<std::string> missing;
    for (const char* world : kWorlds) {
        bool whole = true;
        for (const char* file : kWorldFiles) {
            const Entry* e = index.Find(std::string("Data/levels/") + world + "/" + file);
            if (!e || e->unreadable) whole = false;
        }
        disc.worlds.push_back(whole ? world : "");
        if (!whole) missing.push_back(world);
    }
    if (!missing.empty()) {
        std::string list;
        for (size_t i = 0; i < missing.size(); ++i) {
            if (i) list += i + 1 == missing.size() ? " and " : ", ";
            list += missing[i];
        }
        reason = "the " + list + (missing.size() == 1 ? " world is" : " worlds are") +
                 " not in this copy (a demo or an incomplete copy); the mod needs all three";
        return TpiStatus::Rejected;
    }
    disc.edition = Edition(*data, index, hint);
    disc.source = data;
    disc.root.clear();
    reason.clear();
    return TpiStatus::Supported;
}

// The number of a volume file name ("data2.cab" gives 2), 0 for another name.
int VolumeNumber(const std::string& lowerName) {
    if (lowerName.size() < 9 || lowerName.compare(0, 4, "data") != 0 || !EndsWith(lowerName, ".cab")) return 0;
    const std::string digits = lowerName.substr(4, lowerName.size() - 8);
    if (digits.empty() || digits.size() > 3) return 0;
    for (char c : digits) {
        if (c < '0' || c > '9') return 0;
    }
    return std::stoi(digits);
}

// A disc copy: the cabinets beside a data1.hdr at `dir` inside `parent`. NotThemeParkInc when the header is not an
// InstallShield one or the cabinets do not hold the game; Rejected when they do but a volume is missing.
TpiStatus Cabinets(const std::shared_ptr<Source>& parent, const Index& index, const std::string& dir,
                   const SniffOptions& options, TpiDisc& disc, std::string& reason) {
    const Entry* hdr = index.Find(dir + "data1.hdr");
    if (!hdr || hdr->unreadable) return TpiStatus::NotThemeParkInc;
    std::string err;
    std::shared_ptr<Blob> header = OpenEntryBlob(parent, *hdr, err);
    if (!header) {
        reason = "data1.hdr cannot be read: " + err;
        return TpiStatus::NotThemeParkInc;
    }
    int major = 0;
    bool holdsTable = false;
    if (!SniffInstallShield(*header, major, holdsTable)) return TpiStatus::NotThemeParkInc;
    // the volumes present beside the header
    std::map<int, const Entry*> volumes;
    for (const auto& kv : index.byLower) {
        if (Dir(kv.first) != Lower(dir)) continue;
        const int n = VolumeNumber(kv.first.substr(Lower(dir).size()));
        if (n > 0 && !kv.second->unreadable) volumes[n] = kv.second;
    }
    // The reader keeps this and opens a volume when a file in it is first read, long after this function returns, so
    // it captures copies (the entries belong to `parent`, which it holds).
    auto open = [parent, volumes, options](int volume, std::string& e) -> std::shared_ptr<Blob> {
        auto it = volumes.find(volume);
        if (it == volumes.end()) {
            e = "data" + std::to_string(volume) + ".cab is missing";
            return nullptr;
        }
        return OpenEntryBlobSpilled(parent, *it->second, options, e);
    };
    std::shared_ptr<InstallShieldSource> cabinets = OpenInstallShield(header, open, err);
    if (!cabinets) {
        // An InstallShield header of a version the reader does not know, or a damaged one: not knowing what game it
        // installs, this is not the game as far as the import can tell. The reason says what was found.
        reason = "data1.hdr: " + err;
        return TpiStatus::NotThemeParkInc;
    }
    // the file group holding the game's Data folder (named "Data" on the discs seen, found by its content)
    const std::vector<std::string> folders = DataFolders(*cabinets);
    if (folders.empty()) {
        if (AddOnOnly(*cabinets)) {
            reason = kAddOnReason;
            return TpiStatus::Rejected;
        }
        return TpiStatus::NotThemeParkInc;
    }
    std::shared_ptr<Source> data = std::make_shared<RerootedSource>(cabinets, folders.front(), "Data/");
    disc.claimed = dir;
    const TpiStatus status = CheckWorlds(data, cabinets->Groups(), disc, reason);
    if (status != TpiStatus::Supported) return status;
    // every volume the Data group spans must be here (this opens each of them, as the conversion will)
    const std::string group = folders.front().substr(0, folders.front().find('/'));
    std::vector<int> needed;
    if (!cabinets->VolumesOf(group, needed, err)) {
        reason = err + (err.find("missing") != std::string::npos ? ": copy the whole disc, not only part of it" : "");
        disc.source.reset();
        return TpiStatus::Rejected;
    }
    // the first file of the group opens, so the volumes are really cabinets of this set
    {
        const Entry* first = FindTpiEntry(*data, std::string("Data/") + "levels/arabian/Standard.sam");
        std::unique_ptr<Stream> s = first ? data->Open(*first, err) : nullptr;
        uint8_t probe[64];
        if (!s || s->Read(probe, sizeof probe, err) < 0) {
            reason = "the cabinets cannot be read: " + err;
            disc.source.reset();
            return TpiStatus::Rejected;
        }
    }
    disc.cabinetVersion = cabinets->MajorVersion();
    return TpiStatus::Supported;
}

}  // namespace

const Entry* FindTpiEntry(const Source& source, const std::string& path) {
    const std::string low = Lower(path);
    for (const Entry& e : source.Entries()) {
        if (e.kind == EntryKind::Data && Lower(e.path) == low) return &e;
    }
    return nullptr;
}

TpiStatus RecogniseTpi(const std::shared_ptr<Source>& source, TpiDisc& disc, std::string& reason) {
    return RecogniseTpi(source, SniffOptions{}, disc, reason);
}

TpiStatus RecogniseTpi(const std::shared_ptr<Source>& source, const SniffOptions& options, TpiDisc& disc,
                       std::string& reason) {
    reason.clear();
    disc.source.reset();
    disc.root.clear();
    disc.claimed.clear();
    disc.cabinetVersion = 0;
    disc.worlds.clear();
    disc.edition.clear();
    if (!source) return TpiStatus::NotThemeParkInc;
    // An installed game (or a disc's folder copied with the game installed beside it): plain files below a Data folder.
    const std::vector<std::string> folders = DataFolders(*source);
    if (!folders.empty()) {
        const std::string& dataFolder = folders.front();   // "Theme Park Inc/Data/", "Data/" or "" (Data itself)
        std::shared_ptr<Source> data = std::make_shared<RerootedSource>(source, dataFolder, "Data/");
        disc.claimed = dataFolder.empty() ? std::string() : Dir(dataFolder.substr(0, dataFolder.size() - 1));
        return CheckWorlds(data, {}, disc, reason);
    }
    // A disc: InstallShield cabinets, found by their header file.
    const Index index(*source);
    for (const auto& kv : index.byLower) {
        if (kv.first != "data1.hdr" && !EndsWith(kv.first, "/data1.hdr")) continue;
        const std::string dir = Dir(kv.second->path);
        std::string why;
        const TpiStatus status = Cabinets(source, index, dir, options, disc, why);
        if (status == TpiStatus::NotThemeParkInc) {
            if (reason.empty()) reason = why;
            continue;
        }
        reason = why;
        return status;
    }
    if (AddOnOnly(*source)) {
        reason = kAddOnReason;
        return TpiStatus::Rejected;
    }
    return TpiStatus::NotThemeParkInc;
}

namespace {

// Image and archive files worth opening when looking for a copy inside a dropped file or folder.
bool ContainerName(const std::string& lowerPath) {
    const size_t dot = lowerPath.rfind('.');
    if (dot == std::string::npos) return false;
    const std::string ext = lowerPath.substr(dot + 1);
    static const char* const kExts[] = {"zip", "7z", "rar", "iso", "bin", "img", "mdf", "b6i", "nrg", "toast", "cdr"};
    for (const char* e : kExts) {
        if (ext == e) return true;
    }
    return false;
}

TpiStatus Search(const std::shared_ptr<Source>& source, const SniffOptions& options, int depth, TpiDisc& disc,
                 std::string& reason) {
    const TpiStatus status = RecogniseTpi(source, options, disc, reason);
    if (status != TpiStatus::NotThemeParkInc || depth >= kMaxNesting) return status;
    std::string firstReason = reason;
    for (const Entry& e : source->Entries()) {
        if (e.kind != EntryKind::Data || e.unreadable || e.size < 2048 || !ContainerName(Lower(e.path))) continue;
        std::string err;
        SniffStatus sniffed = SniffStatus::NotContainer;
        std::shared_ptr<Source> child = OpenEntrySniffed(source, e, options, depth, sniffed, err);
        if (!child) {
            if (sniffed == SniffStatus::Failed && firstReason.empty()) firstReason = e.path + ": " + err;
            continue;
        }
        std::string why;
        const TpiStatus inner = Search(child, options, depth + 1, disc, why);
        if (inner != TpiStatus::NotThemeParkInc) {
            reason = why;
            return inner;
        }
        if (firstReason.empty()) firstReason = why;
    }
    reason = firstReason;
    return TpiStatus::NotThemeParkInc;
}

}  // namespace

TpiStatus RecogniseTpiPath(const std::string& path, const SniffOptions& options, TpiDisc& disc, std::string& reason) {
    std::string err;
    SniffStatus status = SniffStatus::NotContainer;
    std::shared_ptr<Source> source = OpenPathSniffed(path, options, status, err);
    if (!source) {
        reason = status == SniffStatus::NotContainer ? "not a folder, a disc image or an archive" : err;
        return TpiStatus::NotThemeParkInc;
    }
    return Search(source, options, 1, disc, reason);
}

}  // namespace tpw::assets

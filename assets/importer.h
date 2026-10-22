// importer.h: the launcher's importer engine. The player drops folders, archives, disc images or installers; every file
// reachable in them (containers opened by the sniffer, nested up to kMaxNesting) is looked up in the compiled-in manifest
// by content: size first, then SHA-256 only when the size is one the manifest holds. A match is written to every
// destination that accepts it (a content can belong in several places and in more than one edition), each by staging
// and an atomic rename, so the data folder only ever holds verified files. Nothing is downloaded and nothing that is not
// in the manifest is written.
//
// Data folder (the SDL pref path in the launcher, any folder in a test):
//   assets/us/Theme Park Data/<dest>    the US edition's game folder
//   assets/eur/Theme Park Data/<dest>   the European edition's game folder
//   assets/gold/<name>                  the Gold rides (manifest destinations "gold/<name>")
//   assets/.staging/                    files being written and big images being unpacked; emptied on every start
//   assets/verified.tsv                 size and modification time of every verified file (the startup check)
//   launcher.ini                        data folder, edition states, last choice (LauncherIni)
//
// Rules the matcher keeps:
// - Resource forks are matched by their own content and written as the manifest's "<dest>.rsrc" side files.
// - ".finfo" side files ("TYPE<tab>CREATOR<newline>") are made from the Finder info of the matched Data entry (HFS,
//   MacBinary, a macOS folder) and written only when their bytes hash to the manifest's value; a folder that already
//   holds "x.finfo" files supplies them as ordinary files.
// - An empty file or a Finder info file proves nothing by content, so it matches only when its path also ends with the
//   destination (case-insensitively, at a folder boundary).
// - A file that matches no content but whose path ends with a known destination is a "different version (not used)":
//   reported by destination, unless the same drop also held the right file for it (a disc's other languages). Every
//   other file is unknown: counted, never listed. A file whose bytes match is used whatever its name on the disc.
// - A repeated drop changes nothing: destinations that already hold a verified file are left alone and counted.
//
// Startup check: Open() finds the installed files by a stat sweep (size only). assets/verified.tsv holds one line per
// verified file ("edition<tab>dest<tab>size<tab>mtime", mtime in nanoseconds), written at the end of an import, of
// VerifyEdition, of RemoveEdition and of Open, each time to a temporary file renamed over it. Open compares each present
// file's size and modification time with its line: a file with no line, or another size or time, is rehashed (as
// VerifyEdition does) and removed when it no longer matches; an unchanged install costs only the stat sweep. A change
// that keeps both the size and the modification time is not seen, by design: VerifyEdition ("Check again") rehashes all.
//
// Threading: Import() runs on the caller's thread; Start() runs it on a worker thread. Progress goes through a
// thread-safe queue the UI drains (ProgressQueue); Status(), Ini() and the other const calls may be made from any thread.
// Nothing throws across this interface.
#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "manifest.h"
#include "source.h"

namespace tpw::assets {

// ---- launcher.ini

struct LauncherIni {
    std::string dataFolder;                       // where assets/ lives (empty: the folder launcher.ini is in)
    std::string lastChoice;                       // the edition the player last started ("us", "eur"), or empty
    std::map<std::string, std::string> editions;  // "us", "eur", "gold" -> "unlocked", "partial" or "absent"
    std::vector<std::pair<std::string, std::string>> extra;  // other keys, kept as they were (for later units)
};

// A missing file gives the defaults and true; a malformed line is skipped. False with a message only on a read error.
bool LoadLauncherIni(const std::string& path, LauncherIni& out, std::string& err);
// Written to a temporary file beside it, then renamed over it.
bool SaveLauncherIni(const std::string& path, const LauncherIni& ini, std::string& err);
// The data folder for a pref path: launcher.ini's data_folder when it names one, else the pref path itself.
std::string ResolveDataRoot(const std::string& prefPath);

// ---- state of an edition

struct EditionStatus {
    Edition edition = kEditionUs;
    size_t records = 0, present = 0;             // every record of the edition, optional ones included
    size_t required = 0, requiredPresent = 0;
    size_t files = 0, filesPresent = 0;          // records that are files of their own, not .rsrc/.finfo side files
    size_t missing[3] = {};                      // required records still missing, by the disc they come from (DiscSource)
    size_t missingFiles[3] = {};                 // the same, counting files of their own only
    uint64_t bytesPresent = 0;
    bool unlocked = false;                       // every required record is present
    // "absent" (nothing present), "partial" or "unlocked": the launcher.ini word.
    const char* StateWord() const;
};

// The disc a DiscSource tag means, for "still missing" texts: "the US Mac CD", "the PC CD", "the Gold CD".
const char* SourceDiscName(DiscSource source);

// ---- progress

struct ImportProgress {
    enum class Kind : uint8_t {
        Scanning,          // opening what was dropped and the containers inside it; total grows
        Hashing,           // working on file n of total (bytes of totalBytes done so far, the current file included)
        Written,           // path: the destination just written ("eur/data/...")
        AlreadyThere,      // path: a destination that already held the verified file
        DifferentVersion,  // path: a known destination whose content in the drop differs (not used); sent at the end
        Error,             // message: something could not be read or written (the import goes on unless it says so)
        Finished,          // the last event of an import; message: ImportReport::Summary()
    };
    Kind kind = Kind::Scanning;
    uint64_t n = 0, total = 0;
    uint64_t bytes = 0, totalBytes = 0;
    std::string path;     // the file being read ("<dropped name>/<inside>/<entry>"), or a destination
    std::string message;
};

// A bounded multi-producer queue. Hashing events past the bound are dropped (Latest() still has the newest);
// every other kind is always kept.
class ProgressQueue {
public:
    void Push(const ImportProgress& p);
    bool Pop(ImportProgress& out);
    std::vector<ImportProgress> Drain();
    ImportProgress Latest() const;  // the newest event pushed, for a UI that only shows the current state
    void Clear();

private:
    static constexpr size_t kBound = 4096;
    mutable std::mutex mutex_;
    std::deque<ImportProgress> queue_;
    ImportProgress latest_;
};

// ---- the result of one drop

struct ImportReport {
    bool cancelled = false;
    bool aborted = false;              // stopped by a write error (a full disk); errors says why
    uint64_t entries = 0;              // files seen, the contents of opened containers included
    uint64_t containers = 0;           // archives, images and installers opened
    uint64_t hashed = 0, bytesHashed = 0;
    uint64_t matched = 0;              // files whose content (or Finder info) is in the manifest
    uint64_t written = 0;              // destinations written by this drop
    uint64_t alreadyThere = 0;         // destinations that already held the verified file
    std::vector<std::string> differentVersions;  // destinations ("data/..."), sorted, whose file in the drop has other content
    uint64_t unknownFiles = 0, unknownBytes = 0; // counted, never listed
    std::vector<std::string> errors;
    EditionStatus status[3];           // us, eur, gold after the drop
    bool ChangedNothing() const { return written == 0; }
    // One plain line for the log and the Finished event, e.g. "Nothing new: 1683 files were already in place."
    std::string Summary() const;
};

// ---- the importer

// What Open()'s startup check did (assets/verified.tsv).
struct OpenCheck {
    bool hadRecord = false;      // verified.tsv was there; without it nothing is rehashed, it is only written
    uint64_t checked = 0;        // present files compared with their line
    uint64_t rehashed = 0;       // files with no line or another size or time, hashed again
    uint64_t removed[3] = {0, 0, 0};  // us, eur, gold: rehashed files that no longer matched, removed
    uint64_t Removed() const { return removed[0] + removed[1] + removed[2]; }
    // The home screen's notice when files were removed, e.g. "3 files of the European edition had changed and were
    // removed; drop its disc to restore them." Empty when nothing was.
    std::string Notice() const;
};

struct ImporterConfig {
    std::string dataRoot;           // the data folder (assets/ is created in it)
    std::string iniPath;            // empty: dataRoot/launcher.ini
    unsigned editions = kEditionUs | kEditionEur | kEditionGold;  // which editions a drop may write
    uint64_t spillOver = 64ull << 20;  // big compressed images are unpacked into assets/.staging first (sniff.h)
};

class Importer {
public:
    explicit Importer(ImporterConfig config);
    ~Importer();  // cancels and joins a running import
    Importer(const Importer&) = delete;
    Importer& operator=(const Importer&) = delete;

    // Creates assets/, empties assets/.staging, finds which destinations already hold a file of an accepted size,
    // rehashes the ones whose size or modification time changed since verified.tsv was written (the startup check
    // above), loads launcher.ini and writes it back with the current edition states. Call once before anything else.
    bool Open(std::string& err);
    // The last Open()'s startup check.
    OpenCheck LastOpenCheck() const;
    // Where the startup check's record is kept (assets/verified.tsv).
    std::string VerifiedPath() const { return assetsDir_ + "/verified.tsv"; }

    // One drop: every path is a folder or a file (an archive, an image, a .cue, an installer or a game file).
    // Runs on the calling thread.
    ImportReport Import(const std::vector<std::string>& paths);

    // The same on a worker thread; false (with a message) when one is already running.
    bool Start(const std::vector<std::string>& paths, std::string& err);
    bool Running() const;
    void Cancel();         // the running import stops within the file it is on; nothing half-written stays
    ImportReport Wait();   // joins the worker and returns its report (an empty report when none ran)

    ProgressQueue& Progress() { return progress_; }

    EditionStatus Status(Edition edition) const;
    // Rehashes every present file of an edition; a file that no longer matches is removed. Returns the number removed,
    // or -1 with a message.
    long VerifyEdition(Edition edition, std::string& err);
    // Deletes assets/<edition>/ and forgets it. Not while an import runs.
    bool RemoveEdition(Edition edition, std::string& err);

    LauncherIni Ini() const;
    bool SetLastChoice(const std::string& choice, std::string& err);

    // Where an edition's game folder is (assets/us/Theme Park Data, assets/eur/Theme Park Data, assets/gold), and where a
    // record of it is written.
    std::string GameFolder(Edition edition) const;
    std::string DestinationPath(Edition edition, const ManifestRecord& record) const;

    const ImporterConfig& Config() const { return config_; }

private:
    struct Work;
    struct Run;
    struct Stamp;
    bool ScanInstalled(std::vector<Stamp>* found);
    void CheckChanged(std::vector<Stamp>& found);
    bool RecordStamps(const std::vector<Stamp>* found);
    ImportReport DoImport(const std::vector<std::string>& paths);
    void Enumerate(Run& run, const std::shared_ptr<Source>& source, const std::string& display,
                   const std::string& folderRoot, int depth);
    void EnumeratePath(Run& run, const std::string& path);
    void Process(Run& run, Work& work);
    void Unmatched(Run& run, const Entry& entry);
    bool Install(Run& run, std::string staged, const std::vector<std::pair<Edition, uint32_t>>& targets);
    bool WriteSmall(Run& run, const std::vector<uint8_t>& bytes, Edition edition, uint32_t record);
    void Count(Run& run, Edition edition, uint32_t record, bool written);
    void MarkPresent(Edition edition, uint32_t record, bool present);
    bool IsPresent(Edition edition, uint32_t record) const;
    std::vector<uint32_t> RecordsByPath(const std::string& path) const;
    std::string NewStagingPath();
    bool SaveIniLocked(std::string& err);
    EditionStatus StatusLocked(Edition edition) const;

    ImporterConfig config_;
    std::string assetsDir_, assetsCanon_, stagingDir_, iniPath_;
    mutable std::mutex mutex_;              // guards present_, ini_
    std::vector<uint8_t> present_;          // per record: Edition bits whose destination holds a file
    LauncherIni ini_;
    std::multimap<std::string, uint32_t> byPath_;  // lower-case destination -> record
    std::map<uint64_t, std::vector<uint32_t>> bySize_;  // size of an accepted content -> records (editions allowed)
    ProgressQueue progress_;
    std::atomic<bool> cancel_{false};
    std::atomic<bool> running_{false};
    std::atomic<unsigned> staged_{0};
    std::thread worker_;
    ImportReport workerReport_;
    bool opened_ = false;
    std::mutex stampMutex_;                 // one writer of verified.tsv at a time
    OpenCheck openCheck_;                   // guarded by mutex_
};

}  // namespace tpw::assets

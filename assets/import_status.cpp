// The files a card names and the files Play needs are counted together here.
#include "importer.h"

#include <cstring>

namespace tpw::assets {
namespace {
bool Side(const ManifestRecord& r) { return (r.flags & (kFlagRsrc | kFlagFinfo)) != 0; }

std::string Effect(const ManifestRecord& r) {
    const std::string path = r.dest;
    if (r.group == kGroupMovies) {
        if (path.find("/bf.") != std::string::npos) return "The Bullfrog logo film is skipped. Play still works.";
        if (path.find("/ea.") != std::string::npos) return "The EA logo film is skipped. Play still works.";
        return "This opening film is skipped when chosen at random. The other films still play.";
    }
    if (path == "data/ui.wad" || path.find("data/Init/") == 0)
        return "The game's menus need this file. Play waits for a good copy.";
    if (path.find("data/Language/") == 0 || path.find("data/fonts/") == 0)
        return "The game needs this file to show its text. Play waits for a good copy.";
    if (path.find("Speech/") != std::string::npos)
        return "The game's voices need this file. Play waits for a good copy.";
    if (path.find("data/levels/") == 0)
        return "This world's rides, scenery or settings need this file. Play waits for a good copy.";
    if (r.group == kGroupGold)
        return "The Gold bonus rides need this file. The base game can still play without Gold.";
    return "The game needs this file to load correctly. Play waits for a good copy.";
}
}  // namespace

EditionStatus DescribeEdition(Edition edition, const std::vector<uint8_t>& present,
                              const std::vector<uint8_t>& edited, const std::vector<uint8_t>& repaired) {
    EditionStatus s;
    s.edition = edition;
    const ManifestRecord* records = ManifestRecords();
    const auto have = [&](size_t i) { return i < present.size() && (present[i] & edition) != 0; };
    for (size_t i = 0; i < ManifestRecordCount(); ++i) {
        const ManifestRecord& r = records[i];
        if (!(r.editions & edition)) continue;
        bool required = (r.flags & kFlagRequired) != 0;
        const bool file = !Side(r);
        bool satisfied = have(i);
        if (const ManifestRecord* other = required ? FilmCounterpart(r, edition) : nullptr) {
            if (other->flags & kFlagRequired) {
                if (std::strstr(r.dest, ".tgq")) required = false;
                else satisfied = satisfied || have(static_cast<size_t>(other - records));
            }
        }
        ++s.records;
        s.files += file;
        if (have(i)) {
            ++s.present;
            s.filesPresent += file;
            s.bytesPresent += RecordHash(r, 0).size;
            if (i < edited.size() && (edited[i] & edition)) ++s.edited;
            // a damaged .sam repaired from the built-in originals (sam_defaults.h): playable, so a gap that does not block
            if (i < repaired.size() && (repaired[i] & edition)) {
                ++s.repaired;
                s.gaps.push_back({r.dest, "Damaged settings, repaired from the original game's: your lines the game can read are kept.", false});
            }
        }
        if (required && !satisfied && r.source < 3) {
            ++s.missing[r.source];
            if (file) ++s.missingFiles[r.source];
        }
        // An intro film the folder lacks is skipped by the game itself (GameSys.cpp PlayMeTheLogoAnim, PlayMeARandomFMV:
        // FS_FindFilePath finds no file, nothing plays), so it is a gap that does not block Play (tests/host_intro_sequence.cpp).
        const bool film = required && file && r.group == kGroupMovies;
        if (film) {
            ++s.introFilms;
            s.introFilmsPresent += satisfied;
        }
        if (required && !satisfied) s.gaps.push_back({r.dest, Effect(r), !film});
        if (film) required = false;
        s.required += required;
        s.requiredPresent += required && satisfied;
    }
    s.unlocked = s.required > 0 && s.requiredPresent == s.required;
    return s;
}
}  // namespace tpw::assets

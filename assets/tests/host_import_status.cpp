// Synthetic manifest presence masks only. No original game files are needed.
// The checks are assert()s and the build is Release (-DNDEBUG), which would compile every one of them out.
#undef NDEBUG
#include "importer.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

using namespace tpw::assets;

namespace {
size_t Index(Edition edition, const char* dest) {
    const auto* record = FindByDest(edition, dest);
    assert(record);
    return static_cast<size_t>(record - ManifestRecords());
}

std::vector<uint8_t> Full(Edition edition) {
    std::vector<uint8_t> present(ManifestRecordCount());
    for (size_t i = 0; i < present.size(); ++i)
        if (ManifestRecords()[i].editions & edition) present[i] = edition;
    return present;
}

void ClearFilm(std::vector<uint8_t>& present, Edition edition, size_t i) {
    present[i] = 0;
    const auto* other = FilmCounterpart(ManifestRecords()[i], edition);
    if (other) present[static_cast<size_t>(other - ManifestRecords())] = 0;
}

void EditionCases(Edition edition) {
    auto present = Full(edition);
    auto status = DescribeEdition(edition, present);
    assert(status.unlocked);
    assert(status.required > 0 && status.requiredPresent == status.required);
    assert(status.gaps.empty() && status.edited == 0);
    assert(status.records == CountRecords(edition));
    assert(status.records == status.present);
    assert(status.introFilms > 0 && status.introFilmsPresent == status.introFilms);
    const auto full = status;

    // Removing either or both accepted movie formats is one logical missing film.
    const size_t film = Index(edition, IsWindowsEdition(edition) ? "data/Movies/jug.tgq" : "data/Movies/jug.mov");
    ClearFilm(present, edition, film);
    status = DescribeEdition(edition, present);
    assert(status.unlocked && status.gaps.size() == 1);
    assert(!status.gaps[0].essential && status.gaps[0].dest == ManifestRecords()[film].dest);
    assert(status.gaps[0].effect.find("skipped") != std::string::npos);
    assert(status.gaps[0].effect.find("other films still play") != std::string::npos);
    assert(status.introFilmsPresent + 1 == full.introFilmsPresent);

    // All missing films still allow Play. The counts provide the skip-intro lock's reason.
    for (size_t i = 0; i < present.size(); ++i)
        if (ManifestRecords()[i].group == kGroupMovies) present[i] = 0;
    status = DescribeEdition(edition, present);
    assert(status.unlocked && status.introFilmsPresent == 0);
    assert(status.gaps.size() == status.introFilms);
    for (const auto& gap : status.gaps) {
        assert(!gap.essential && !gap.dest.empty());
        assert(gap.effect.find("skipped") != std::string::npos);
    }

    present = Full(edition);
    present[Index(edition, "data/ui.wad")] = 0;
    status = DescribeEdition(edition, present);
    assert(!status.unlocked && status.gaps.size() == 1);
    assert(status.gaps[0].essential && status.gaps[0].dest == "data/ui.wad");
    assert(status.gaps[0].effect.find("menus") != std::string::npos);
    assert(status.gaps[0].effect.find("Play waits") != std::string::npos);

    // No required file outside the movies has silently become optional, including resource forks.
    for (size_t i = 0; i < present.size(); ++i) {
        const auto& r = ManifestRecords()[i];
        if (!(r.editions & edition) || !(r.flags & kFlagRequired) || r.group == kGroupMovies) continue;
        present = Full(edition);
        present[i] = 0;
        status = DescribeEdition(edition, present);
        assert(!status.unlocked && status.gaps.size() == 1);
        assert(status.gaps[0].essential && status.gaps[0].dest == r.dest);
        assert(!status.gaps[0].effect.empty());
    }

    // Edited settings count only when present. A missing SAM still requires the player's own disc.
    present = Full(edition);
    const size_t sam = Index(edition, "data/med.sam");
    std::vector<uint8_t> edited(present.size());
    edited[sam] = edition;
    status = DescribeEdition(edition, present, edited);
    assert(status.unlocked && status.edited == 1 && status.gaps.empty());
    present[sam] = 0;
    status = DescribeEdition(edition, present, edited);
    assert(!status.unlocked && status.edited == 0);
    assert(status.gaps.size() == 1 && status.gaps[0].dest == "data/med.sam");

    if (!IsWindowsEdition(edition)) {
        // A Mac edition's MOV is satisfied by its TGQ counterpart, and conversely.
        const auto* other = FilmCounterpart(ManifestRecords()[film], edition);
        assert(other && (other->flags & kFlagRequired));
        const size_t tgq = static_cast<size_t>(other - ManifestRecords());
        present = Full(edition);
        present[film] = 0;
        status = DescribeEdition(edition, present);
        assert(status.unlocked && status.gaps.empty());
        assert(status.introFilmsPresent == full.introFilmsPresent);
        present[film] = edition;
        present[tgq] = 0;
        status = DescribeEdition(edition, present);
        assert(status.unlocked && status.gaps.empty());
        assert(status.introFilmsPresent == full.introFilmsPresent);
    }
    // Other edition bits cannot satisfy this edition, and a short vector means absent, without an overread.
    std::vector<uint8_t> wrong(ManifestRecordCount(), edition == kEditionUs ? kEditionEur : kEditionUs);
    status = DescribeEdition(edition, wrong);
    assert(!status.unlocked && status.present == 0 && status.introFilmsPresent == 0);
    status = DescribeEdition(edition, {});
    assert(!status.unlocked && status.present == 0 && !status.gaps.empty());
}
}

int main() {
    EditionCases(kEditionUsPc);
    EditionCases(kEditionEurPc);
    EditionCases(kEditionUs);
    EditionCases(kEditionEur);
}

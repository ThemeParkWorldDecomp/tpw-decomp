// host_unlisted_theme.cpp: an online park whose world (theme) the server does not list (the port's fix unlistedtheme,
// PortParkThemeListed in src/game/online/FS_OnlineList.cpp and its guard in CFSOnlineSystem::GotoPlayerFTPPostGetParkInfoCallback,
// src/game/online/FS_Online.cpp; contributed by samueleagle91, tpw-port PR #130). The original took ReturnOnlineThemeName's NULL as
// the theme to load and opened the park in another world. On a made-up theme list of three worlds:
//   - a park of a listed world is listed, one of a world the list has not got is not, and on an empty list none is;
//   - a world the list has twice (the server's list can repeat an id) is listed;
//   - the goto by park id starts the download for a park of a listed world, and ends as a failed goto (CompleteFTPDownload(1),
//     no download) for a park of an unlisted one; an error answer or no park fails as before, whatever the world.
// The program runs the real PortParkThemeListed, CFSOnlineSystemThemeList::ReturnOnlineThemeName and the goto callback; the
// online system, its theme list, the parks and the two calls the callback makes are the test's stand-ins (the records are the
// fields those functions read, in zeroed storage). Made only with the port's fixes; exits nonzero on a failure.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "FS_Online.h"
#include "FS_OnlineList.h"

// ---- globals of other files, defined here (zero at start)
CFSOnlineSystem* gOnlineSystem = nullptr;

namespace {

int gFailures = 0;
int gChecks = 0;

void Check(bool ok, const std::string& what) {
    gChecks++;
    if (ok) return;
    gFailures++;
    std::printf("FAILED: %s\n", what.c_str());
}

int gCompleteCalls = 0;
int gCompleteFailed = -1;
int gDownloadCalls = 0;
const CFSOnlinePark* gDownloadPark = nullptr;

void ClearCalls() {
    gCompleteCalls = 0;
    gCompleteFailed = -1;
    gDownloadCalls = 0;
    gDownloadPark = nullptr;
}

alignas(16) unsigned char gSystemBlock[sizeof(CFSOnlineSystem)];
alignas(16) unsigned char gThemeListBlock[sizeof(CFSOnlineSystemThemeList)];
alignas(16) unsigned char gThemeBlocks[4][sizeof(CFSOnlineTheme)];
alignas(16) unsigned char gParkBlocks[4][sizeof(CFSOnlinePark)];
char gThemeNames[4][16];
TbPtrList<CFSOnlineTheme> gThemes;

CFSOnlineSystem* System() { return reinterpret_cast<CFSOnlineSystem*>(gSystemBlock); }

// A theme record: its id and its name, the two fields ReturnOnlineThemeName reads.
CFSOnlineTheme* Theme(int index, int32_t id, const char* name) {
    std::memset(gThemeBlocks[index], 0, sizeof(gThemeBlocks[index]));
    CFSOnlineTheme* theme = reinterpret_cast<CFSOnlineTheme*>(gThemeBlocks[index]);
    std::snprintf(gThemeNames[index], sizeof(gThemeNames[index]), "%s", name);
    theme->mThemeID = id;
    theme->mText0C.mData = gThemeNames[index];
    return theme;
}

// A park record of world `theme` (mF4C, the field PortParkThemeListed asks about).
const CFSOnlinePark* Park(int index, int32_t theme) {
    std::memset(gParkBlocks[index], 0, sizeof(gParkBlocks[index]));
    CFSOnlinePark* park = reinterpret_cast<CFSOnlinePark*>(gParkBlocks[index]);
    park->mParkID = 500 + index;
    park->mF4C = theme;
    return park;
}

void Goto(int32_t result, const CFSOnlinePark* park) {
    ClearCalls();
    System()->GotoPlayerFTPPostGetParkInfoCallback(result, nullptr, park);
}

}  // namespace

// ---- the stand-ins: what the guard and the callback call outside the code under test
// OnlineSystem() (FS_OnlineHelpers.h) makes the system when there is none; the test's is set first, so this is never called.
CFSOnlineSystem::CFSOnlineSystem() { std::abort(); }

// The city guard before it (unlistedcity) is not this test's: every park's city is listed.
bool PortParkCityListed(const CFSOnlinePark* park) {
    (void)park;
    return true;
}

void CFSOnlineSystem::CompleteFTPDownload(int32_t failed) {
    gCompleteCalls++;
    gCompleteFailed = failed;
}

void CFSOnlineSystem::LoggedInRequestFTPDownload(const CFSOnlinePark* park) {
    gDownloadCalls++;
    gDownloadPark = park;
}

int main() {
    std::memset(gSystemBlock, 0, sizeof(gSystemBlock));
    std::memset(gThemeListBlock, 0, sizeof(gThemeListBlock));
    CFSOnlineSystemThemeList* themes = reinterpret_cast<CFSOnlineSystemThemeList*>(gThemeListBlock);
    themes->mList = &gThemes;
    System()->mThemeList = themes;
    gOnlineSystem = System();

    // The empty list (no answer from the server yet): no park is listed.
    const CFSOnlinePark* lost = Park(0, 2);
    Check(!PortParkThemeListed(lost), "a park is listed on an empty theme list");

    // Three worlds; world 3 twice (two records, the last one's name is the answer).
    gThemes.AddTail(Theme(0, 1, "Lost Kingdom"));
    gThemes.AddTail(Theme(1, 3, "Halloween"));
    gThemes.AddTail(Theme(2, 4, "Wonderland"));
    gThemes.AddTail(Theme(3, 3, "Halloween2"));

    const CFSOnlinePark* listed = Park(1, 1);
    const CFSOnlinePark* twice = Park(2, 3);
    const CFSOnlinePark* unlisted = Park(3, 2);
    Check(PortParkThemeListed(listed), "a park of world 1 (listed) is not listed");
    Check(PortParkThemeListed(twice), "a park of world 3 (listed twice) is not listed");
    Check(!PortParkThemeListed(unlisted), "a park of world 2 (not on the list) is listed");
    Check(!PortParkThemeListed(Park(0, 0)), "a park of world 0 is listed");
    Check(!PortParkThemeListed(Park(0, -1)), "a park of world -1 (unset) is listed");
    char* name = themes->ReturnOnlineThemeName(3);
    Check(name != nullptr && std::strcmp(name, "Halloween2") == 0, "world 3's name is not the last record's");
    Check(themes->ReturnOnlineThemeName(2) == nullptr, "world 2 has a name");

    // The goto by park id: a listed world's park is downloaded, as before the fix.
    Goto(0, listed);
    Check(gDownloadCalls == 1 && gDownloadPark == listed && gCompleteCalls == 0, "a listed park's goto did not start its download");
    Goto(0, twice);
    Check(gDownloadCalls == 1 && gDownloadPark == twice && gCompleteCalls == 0, "a park of world 3's goto did not start its download");

    // An unlisted world's park: the goto ends as a failed one, with no download.
    Goto(0, unlisted);
    Check(gDownloadCalls == 0, "an unlisted park's goto started a download");
    Check(gCompleteCalls == 1 && gCompleteFailed == 1, "an unlisted park's goto did not end as a failed one");

    // An error answer, or no park: a failed goto, as the original's, whatever the world.
    Goto(1, listed);
    Check(gDownloadCalls == 0 && gCompleteCalls == 1 && gCompleteFailed == 1, "an error answer did not fail the goto (listed park)");
    Goto(1, unlisted);
    Check(gDownloadCalls == 0 && gCompleteCalls == 1 && gCompleteFailed == 1, "an error answer did not fail the goto (unlisted park)");
    Goto(0, nullptr);
    Check(gDownloadCalls == 0 && gCompleteCalls == 1 && gCompleteFailed == 1, "no park did not fail the goto");

    // The world list empties again (a new login before its answer): the park that was listed is not any more.
    gThemes.RemoveAll();
    Check(!PortParkThemeListed(listed), "a park is listed after the list emptied");
    Goto(0, listed);
    Check(gDownloadCalls == 0 && gCompleteCalls == 1 && gCompleteFailed == 1, "the goto started a download on an empty list");

    std::printf("unlisted_theme: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}

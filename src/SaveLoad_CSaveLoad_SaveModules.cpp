// CSaveLoad::SaveModules (main app, SaveLoad.cpp; unit v5628): writes the park's modules to the save file, each followed by its tag.
#include "SaveLoad.h"
#include "Debug.h"
#include "bfl_FileSystem.h"
#include "bfl_ZLib.h"
#include "platform/samsutils.h"

#include <new>

#include "ActionRecorder.h"
#include "AdvisorScoring.h"
#include "CMessageCentre.h"
#include "Cheat.h"
#include "Clock.h"
#include "GameSys.h"
#include "LbFile.h"
#include "Serialise.h"
#include "TPCamera.h"
#include "TPSound.h"
#include "UIInterface.h"
#include "VanillaTime.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"
#include "ui_main.h"

uint32_t gSaveModulesTagTestWords[17] = {};   // 0x102a6cbc .. 0x102a6cfc

namespace {

const char kDelimiterFailed[] = "Couldn't write delimeter";   // (sic)

// Writes a module's tag, the four characters as a big-endian word in the source, byte reversed into the file (the PC's order), and
// answers whether the write's result passes the comparison with the tag's constant word: (result >= 0) == (word == 0).
bool WriteTag(void* file, uint32_t tag, int32_t index) {
    uint32_t word = serialise::SwapWord(tag);
    const bool succeeded = LbFile_Write(file, &word, 4, nullptr).Succeeded();
    return succeeded == (gSaveModulesTagTestWords[index] == 0);
}

}  // namespace

// The modules in order: the action recorder (alone for type 1, the online park), the world, the scripts, the particles, the message
// centre, the clock, the vanilla time, the game system, the rides, the track rides, the flying rides, the RSSE scripts, the camera,
// the coasters, the advisor, the sound, the cheat state, the advisor scoring and the interface. A module that fails, or a tag whose
// write does not pass, answers 0 at once. The debug log (gSaveLoad's, opened with state 0) covers the recorder and the world.
//
// Quirks kept: `this` is not used; a failing action recorder returns without closing the log; the message centre is made when there
// is none (the original tests operator new's answer, which is never NULL); the scripts, the advisor, the sound and the interface
// answer nothing that is read; the advisor's size is traced with the coasters' text, and the interface's with "UI: loaded"; the
// position read after the last trace is not used.
int32_t CSaveLoad::SaveModules(void* file, LoadType type) {
    int32_t start = LbFile_GetPosition(file);
    gSaveLoad.OpenLogfile(0);
    if (gActionRecorder.Serialise(file, 1, 1, 0) != 0) return 0;
    CTRACE(2, "GActionRec: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);
    if (type == kLoadOnline) {
        gSaveLoad.CloseLogfile();
        return 1;
    }

    if (gGameWorld->Save(file) == 0) {
        DEBUG_MESSAGE("Save world FAILED", 0);
        gSaveLoad.CloseLogfile();
        return 0;
    }
    gSaveLoad.CloseLogfile();
    if (!WriteTag(file, 0x57524c44u /* WRLD */, 0)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "World: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    SCR_SaveAllScripts(file);
    if (!WriteTag(file, 0x53505343u /* SPSC */, 1)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Scripts: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (PAR_SaveStatus(file, 0) != 0) {
        DEBUG_MESSAGE("PAR_SaveStatus failed", 0);
        return 0;
    }
    if (!WriteTag(file, 0x50415254u /* PART */, 2)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Particles: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre;
    if (gMessageCentre->Save(file) != 0) {
        DEBUG_MESSAGE("Message centre save failed horribly", 0);
        return 0;
    }
    if (!WriteTag(file, 0x4d455353u /* MESS */, 3)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "MessageCentre: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (gClock.Save(file) == 0) {
        DEBUG_MESSAGE("Clock failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x434c4f4bu /* CLOK */, 4)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Clock: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (!gVanillaTime.Save(file)) {
        DEBUG_MESSAGE("Vanilla time failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x56414e54u /* VANT */, 5)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "VanillaTime: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (GAMESYSTEM_Save(file) == 0) {
        DEBUG_MESSAGE("Game System failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x47535953u /* GSYS */, 6)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "GameSystem: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (SaveRides(file) == 0) {
        DEBUG_MESSAGE("Ride System failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x52535953u /* RSYS */, 7)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "RideSystem: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (BMP_Save(file) == 0) {
        DEBUG_MESSAGE("Track rides failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x5452414bu /* TRAK */, 8)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "TrackRides: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (FLY_Save(file) != 0) {
        DEBUG_MESSAGE("Flying rides failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x464c5952u /* FLYR */, 9)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "FlyingRides: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (RSSE_Save(file) != 0) {
        DEBUG_MESSAGE("RSSE scripts failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x52535345u /* RSSE */, 10)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "RSSE: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (SaveCamera(file) == 0) {
        DEBUG_MESSAGE("Camera failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x4b414d45u /* KAME */, 11)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Camera: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (SaveCoasters(file) == 0) {
        DEBUG_MESSAGE("Coasters failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x434f4153u /* COAS */, 12)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Coasters: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    ADV_Save(file);
    if (!WriteTag(file, 0x41445653u /* ADVS */, 13)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Coasters: saved %d bytes\n", LbFile_GetPosition(file) - start);   // (sic: the coasters' text)
    start = LbFile_GetPosition(file);

    SOUND_SaveStatus(file);
    if (!WriteTag(file, 0x534f554eu /* SOUN */, 14)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Sound: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    // the function's static CCheat, made on first use (no destructor is registered)
    if (gGameWinCheatGuard == 0) {
        new (gGameWinCheat) CCheat();
        gGameWinCheatGuard = 1;
    }
    if (reinterpret_cast<CCheat*>(gGameWinCheat)->Save(file) == 0) {
        DEBUG_MESSAGE("Cheat system failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x43485453u /* CHTS */, 15)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Cheat: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    // (the original inlines the shared static: guard byte 0x102b4100, object 0x102b3ac8, its destructor registered)
    if (TheScoring().Save(file) == 0) {
        DEBUG_MESSAGE("Advisor scoring system failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x41445343u /* ADSC */, 16)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "AdvisorScoring: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    gUIInterface.Save(file);
    CTRACE(2, "UI: loaded %d bytes\n", LbFile_GetPosition(file) - start);
    LbFile_GetPosition(file);
    return 1;
}

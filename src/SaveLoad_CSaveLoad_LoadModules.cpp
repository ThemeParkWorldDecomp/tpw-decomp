// CSaveLoad::LoadModules (main app 0x101589f0, 4,052 bytes): reads every module of a park from the memory file.
#include <cstdint>
#include <new>

#include "ActionRecorder.h"
#include "AdvisorScoring.h"
#include "Cheat.h"
#include "CMessageCentre.h"
#include "Clock.h"
#include "Debug.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "UIInterface.h"
#include "VanillaTime.h"
#include "World.h"
#include "platform/samsutils.h"

// The static CCheat of the game window and its guard byte (0x102da934 and 0x102da940; defined in src/ui_main_gamewin.cpp).
alignas(4) extern uint8_t gGameWinCheat[12];
extern int8_t gGameWinCheatGuard;

namespace {

// Reads the four byte tag that precedes a module. The answer of LbFile_Read is not looked at: the original tests only that the
// transferred count divided by 4 is 1 (so 4 to 7 pass). The tag is the file's bytes read as a little endian word. A short read
// reports "read failed", a different tag reports `text`; both end the load.
bool ReadModuleTag(void* handle, uint32_t tag, const char* text) {
    uint32_t word = 0;
    uint32_t count = 0;
    LbFile_Read(handle, &word, 4, &count);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return false;
    }
    if (word != tag) {
        GAME_ASSERTMSG(0, text);
        return false;
    }
    return true;
}

}  // namespace

// Quirks kept: the logfile calls go to the global gSaveLoad whatever `this` is; a failure never closes the logfile; a type other than
// 1 reads everything (0, 2, ...); the module tags are checked before the module they announce; the answers of ADV_Load,
// SOUND_LoadStatus and UIInterface::Load are not read.
int32_t CSaveLoad::LoadModules(bfl::FileSystem::CFile& file, LoadType type) {
    void* handle = &file;
    int32_t start = LbFile_GetPosition(handle);
    gSaveLoad.OpenLogfile(0);

    if (gActionRecorder.Serialise(handle, 0, 1, 0) != 0) {
        DEBUG_MESSAGE("Restore action recording FAILED", 0);
        return 0;
    }
    CTRACE(2, "GActionRec: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    start = LbFile_GetPosition(handle);
    if (type == kLoadOnline) {
        gSaveLoad.CloseLogfile();
        return 1;
    }
    if (gGameWorld->Load(handle) == 0) {
        DEBUG_MESSAGE("Restore world FAILED", 0);
        return 0;
    }
    gSaveLoad.CloseLogfile();
    CTRACE(2, "World: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // scripts
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x57524c44, "The SAD_AI module did not load the same number of bytes that it saved")) return 0;
    if (SCR_LoadAllScripts(handle) != 0) {
        DEBUG_MESSAGE("Restore scripts FAILED", 0);
        return 0;
    }
    CTRACE(2, "Scripts: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // particles
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x53505343, "The SAD_SPRITE_SCRIPTS module did not load the same number of bytes that it saved")) return 0;
    if (PAR_LoadStatus(handle) != 0) {
        DEBUG_MESSAGE("PAR_SaveStatus FAILED", 0);
        return 0;
    }
    PAR_KillAndHideAll2DObjects();
    CTRACE(2, "Particles: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // message centre (made on first use: the Mac's operator new(0x1d0) and __construct_array of 29 headers)
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x50415254, "The SAD_PARTICLES module did not load the same number of bytes that it saved")) return 0;
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre;
    if (gMessageCentre->Load(handle) != 0) {
        DEBUG_MESSAGE("Message centre load failed horribly", 0);
        return 0;
    }
    CTRACE(2, "MsgCntr: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // clock
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x4d455353, "The SAD_MESSAGE module did not load the same number of bytes that it saved")) return 0;
    if (gClock.Load(handle) == 0) {
        DEBUG_MESSAGE("Clock failed to save", 0);
        return 0;
    }
    CTRACE(2, "Clock: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // vanilla time
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x434c4f4b, "The SAD_CLOCK module did not load the same number of bytes that it saved")) return 0;
    if (!gVanillaTime.Load(handle)) {
        DEBUG_MESSAGE("Vanilla time failed to load", 0);
        return 0;
    }
    CTRACE(2, "VanillaTime: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // game system
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x56414e54, "The SAD_VANILLA_TIME module did not load the same number of bytes that it saved")) return 0;
    if (GAMESYSTEM_Load(handle) == 0) {
        DEBUG_MESSAGE("Game System failed to load", 0);
        return 0;
    }
    CTRACE(2, "GameSystem: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // rides
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x47535953, "The SAD_GAMESYS module did not load the same number of bytes that it saved")) return 0;
    if (LoadRides(handle) == 0) {
        DEBUG_MESSAGE("Ride System failed to load", 0);
        return 0;
    }
    CTRACE(2, "Ridesystem: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // track rides
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x52535953, "The SAD_RIDESYS module did not load the same number of bytes that it saved")) return 0;
    if (BMP_Load(handle) == 0) {
        DEBUG_MESSAGE("Track rides failed to load", 0);
        return 0;
    }
    CTRACE(2, "TrackRides: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // flying rides
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x5452414b, "The SAD_TRACK module did not load the same number of bytes that it saved")) return 0;
    if (FLY_Load(handle) != 0) {
        DEBUG_MESSAGE("Flying rides failed to load", 0);
        return 0;
    }
    CTRACE(2, "FlyingRides: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // RSSE
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x464c5952, "The SAD_FLYERS module did not load the same number of bytes that it saved")) return 0;
    if (RSSE_Load(handle) != 0) {
        DEBUG_MESSAGE("RSSE scripts failed to load", 0);
        return 0;
    }
    CTRACE(2, "RSSE: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // camera
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x52535345, "The SAD_RSSE module did not load the same number of bytes that it saved")) return 0;
    if (LoadCamera(handle) == 0) {
        DEBUG_MESSAGE("Camera failed to load", 0);
        return 0;
    }
    CTRACE(2, "Camera: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // coasters
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x4b414d45, "The SAD_CAMERA module did not load the same number of bytes that it saved")) return 0;
    if (LoadCoasters(handle) == 0) {
        DEBUG_MESSAGE("Coasters failed to load", 0);
        return 0;
    }
    CTRACE(2, "Coasters: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // advisor
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x434f4153, "The SAD_COASTERS module did not load the same number of bytes that it saved")) return 0;
    ADV_Load(handle);
    CTRACE(2, "Advisor: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // sound
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x41445653, "The SAD_ADV module did not load the same number of bytes that it saved")) return 0;
    SOUND_LoadStatus(handle);
    CTRACE(2, "Sound: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // cheat
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x534f554e, "The SAD_SOUND module did not load the same number of bytes that it saved")) return 0;
    if (gGameWinCheatGuard == 0) {
        new (gGameWinCheat) CCheat();
        gGameWinCheatGuard = 1;
    }
    if (reinterpret_cast<CCheat*>(gGameWinCheat)->Load(handle) == 0) {
        DEBUG_MESSAGE("Cheat system failed to load", 0);
        return 0;
    }
    CTRACE(2, "Cheat: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // advisor scoring
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x43485453, "The SAD_CHEAT module did not load the same number of bytes that it saved")) return 0;
    if (TheScoring().Load(handle) == 0) {
        DEBUG_MESSAGE("Advisor scoring system failed to load", 0);
        return 0;
    }
    CTRACE(2, "AdvisorScoring: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // user interface
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x41445343, "The SAD_ADV_SCORING module did not load the same number of bytes that it saved")) return 0;
    gUIInterface.Load(handle);
    CTRACE(2, "UI: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    LbFile_GetPosition(handle);   // the answer is dropped
    return 1;
}

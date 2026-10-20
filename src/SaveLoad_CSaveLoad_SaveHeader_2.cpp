// SaveLoad.cpp (main app), unit v51006: CSaveLoad::SaveHeader (0x10159a08), PostLoadOperations (0x101588c4) and PreLoadOperations
// (0x101587e4).
#include "SaveLoad.h"
#include "Debug.h"
#include "bfl_FileSystem.h"
#include "bfl_ZLib.h"
#include "platform/samsutils.h"

#include <cstring>

#include "Advisor.h"
#include "Clock.h"
#include "GameSys.h"
#include "HLSprite.h"
#include "LLCellSystem.h"
#include "LbFile.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "TbError.h"
#include "Text.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"
#include "ui_main.h"

// Writes what LoadHeader reads, after the version word: the language byte, the legal text (0x500 bytes, little endian UTF-16, of the
// language the text system names), the 0x100-byte ride record (gParkRequiredRides in its first 0x20 bytes), the key word as it is in
// memory (the file holds the bytes 01 22 19 85), the flag word (little endian 1 when a header is given) and the header (Serialise
// with save 1). 1 on success; 0 when a write fails (negative result or a count other than the length) or Serialise answers 1.
// Quirks kept: one count variable serves every write (a write that leaves it alone is judged by the last value); an unknown language
// name selects text 0; any Serialise answer other than 1 counts as success.
int32_t CSaveLoad::SaveHeader(void* file, CSaveLoadHeader* header) {
    uint8_t block[0x501];   // [0] the language byte, then the text (the original's text is at the odd address 0x16d)
    uint8_t* const text = block + 1;
    std::memset(text, 0, 0x500);

    TbDynamicStringTemplate<char> name(gText.GetLanguageName());
    if (name == "English") {
        block[0] = 0;
    } else if (name == "Spanish") {
        block[0] = 1;
    } else if (name == "Italian") {
        block[0] = 2;
    } else if (name == "Swedish") {
        block[0] = 3;
    } else if (name == "German") {
        block[0] = 4;
    } else if (name == "French") {
        block[0] = 5;
    } else if (name == "Japanese") {
        block[0] = 6;
    } else {
        block[0] = 0;
    }

    TbDynamicStringTemplate<char16_t>& legal = gSaveLegalText[block[0]];
    const char16_t* const source = legal.mData;
    std::memcpy(text, source, static_cast<size_t>(legal.Length()) << 1);
    SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(text), 0x280);

    uint32_t written = 0;
    if (!LbFile_Write(file, &block[0], 1, &written).Succeeded() || written != 1) return 0;

    if (!LbFile_Write(file, text, 0x500, &written).Succeeded() || written != 0x500) return 0;

    uint8_t rides[0x100];
    std::memset(rides, 0, 0x100);
    std::memcpy(rides, gParkRequiredRides, 0x20);
    if (!LbFile_Write(file, rides, 0x100, &written).Succeeded() || written != 0x100) return 0;

    // (the key goes out as the Mac holds it, most significant byte first)
    uint32_t key = __builtin_bswap32(gSaveFileKey);
    if (!LbFile_Write(file, &key, 4, &written).Succeeded() || written != 4) return 0;

    uint32_t flag = header != nullptr ? 1u : 0u;   // (little endian in the file: the host's own order)
    if (!LbFile_Write(file, &flag, 4, &written).Succeeded() || written != 4) return 0;

    if (header == nullptr) return 1;
    if (header->Serialise(file, 1, 1, 0) == 1) return 0;
    return 1;
}

// Clears the world before a park is read: the height field, the map, the cell system, the engine objects, the things, the ride in
// first person, the bitmaps, the coasters, the particle system, the advisor (0) and the user interface.
void CSaveLoad::PreLoadOperations() {
    RestoreLevelHeightfield();
    gGameWorld->GetMap()->DeleteMapInEngine();
    LLCC_ResetMap(0x80, 0x80);
    gGameWorld->DeleteEngineObjects();
    gGameWorld->DeleteAllThings();
    gGameWorld->InitThingHeap();
    StopRidingRideIn1stPerson();
    BMP_FreeAll();
    DestroyCoasterInstances();
    RSSE_Close();
    ADV_Kill(0);
    gUIInterface.PreLevelLoad();
}

// Rebuilds what the engine holds after a park is read. The particle system is low spec when the current detail is 0.
void CSaveLoad::PostLoadOperations() {
    InitialiseWeather();
    InitialiseBlobShadow();
    FLY_RestoreMeshes();
    gGameWorld->RestoreEngineObjects();
    gGameWorld->GetMap()->UpdateMapInEngine();
    gGameWorld->GetControlManager()->CheckAllControlsInitialised();
    RecalculateParkDarkening();
    gRestartParkClocks = 1;
    gClock.PostLoad();
    TickRideAnims();
    RegenerateRideSigns();
    gUIInterface.PostLevelLoad();
    ADV_Kill(1);
    if (gPlayerSettings.mCurrent.mDetail == 0) {
        PAR_SetLowSpec(1);
    } else {
        PAR_SetLowSpec(0);
    }
    ESP_FixSpriteIDsFromDetail();
    SOUND_SetPauseOn3DSoundsOnly(0);
}

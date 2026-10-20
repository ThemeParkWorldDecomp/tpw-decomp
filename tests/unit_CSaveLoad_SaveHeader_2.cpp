// Test library for CSaveLoad::SaveHeader, PostLoadOperations and PreLoadOperations (unit v51006;
// libtpw_unit_CSaveLoad_SaveHeader_2.dylib, tests/test_CSaveLoad_SaveHeader_2.py), main app:
//   src/SaveLoad_CSaveLoad_SaveHeader_2.cpp
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue. The globals
// of other files are defined here: those of classes whose constructors live in other files are raw storage under the global's own
// symbol name.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"

#include "Advisor.h"
#include "Clock.h"
#include "GameSys.h"
#include "HLSprite.h"
#include "LLCellSystem.h"
#include "LbFile.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "TbError.h"
#include "Text.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "platform/samsutils.h"
#include "rsse.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
alignas(16) unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
alignas(16) unsigned char gClockStorage[sizeof(CClock)] __asm__("_gClock");
alignas(16) unsigned char gPlayerSettingsStorage[sizeof(CPlayerSettings)] __asm__("_gPlayerSettings");
alignas(16) unsigned char gUIInterfaceStorage[sizeof(UIInterface) + 16] __asm__("_gUIInterface");
alignas(16) unsigned char gSaveLegalTextStorage[7 * sizeof(TbDynamicStringTemplate<char16_t>)] __asm__("_gSaveLegalText");
int32_t gRestartParkClocks;
uint8_t gParkRequiredRides[0x20];
uint32_t gSaveFileKey;
CWorld* gGameWorld;

namespace {

char gLangBuf[64];
uint8_t gBlob[0x4000];
uint32_t gBlobLen;

int64_t Delta(const void* p) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(p) - reinterpret_cast<intptr_t>(gGameWorld)); }

TbDynamicStringTemplate<char16_t>* LegalText() { return reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(gSaveLegalTextStorage); }

}  // namespace

// ---- the string classes: the char instance is built on the stack by SaveHeader; the wide ones are only read (Length, mData)
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() { ev("StrDtor", {lab(this)}); }
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    mCapacity = 0;
    mGrowBy = 0;
    ev("StrCtor", {newlab(this)}, {text});
}
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template<> void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }

template<> bool TbStringBase<char>::operator==(const char* text) const {
    ev("eq", {lab(this)}, {text});
    return ret("eq") != 0;
}
template<> int32_t TbStringBase<char16_t>::Length() const {
    ev("Length", {lab(this)});
    return static_cast<int32_t>(ret("Length"));
}

char* CText::GetLanguageName() {
    ev("GetLanguageName", {lab(this)});
    return gLangBuf;
}

namespace SamsUtilities {
void _USwapBlock16(uint16_t*, int32_t count) { ev("USwap", {count}); }   // (a no-op on the host: its data is already little endian)
}  // namespace SamsUtilities

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    const int32_t err = static_cast<int32_t>(ret("werr"));
    const int32_t mode = static_cast<int32_t>(ret("wmode"));
    ev("Write", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), static_cast<int64_t>(length), err, mode});
    std::memcpy(gBlob + gBlobLen, buffer, length);
    gBlobLen += length;
    switch (mode) {
        case 0: *transferred = length; break;
        case 1: *transferred = length - 1; break;
        case 2: *transferred = 0; break;
        case 3: *transferred = length + 1; break;
        default: break;   // left alone
    }
    return TbError(err);
}

int32_t CSaveLoadHeader::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("Serialise", {this != nullptr ? 1 : 0, static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), save, version, unused});
    return static_cast<int32_t>(ret("Serialise"));
}

// ---- PreLoadOperations and PostLoadOperations
void RestoreLevelHeightfield() { ev("RestoreLevelHeightfield"); }
void CMap::DeleteMapInEngine() { ev("DeleteMapInEngine", {Delta(this)}); }
void LLCC_ResetMap(int32_t width, int32_t height) { ev("LLCC_ResetMap", {width, height}); }
void CWorld::DeleteEngineObjects() { ev("DeleteEngineObjects", {Delta(this)}); }
void CWorld::DeleteAllThings() { ev("DeleteAllThings", {Delta(this)}); }
void CWorld::InitThingHeap() { ev("InitThingHeap", {Delta(this)}); }
void StopRidingRideIn1stPerson() { ev("StopRidingRideIn1stPerson"); }
void BMP_FreeAll() { ev("BMP_FreeAll"); }
void DestroyCoasterInstances() { ev("DestroyCoasterInstances"); }
void RSSE_Close() { ev("RSSE_Close"); }
void ADV_Kill(int32_t mode) { ev("ADV_Kill", {mode}); }
void UIInterface::PreLevelLoad() { ev("PreLevelLoad", {lab(this)}); }

void InitialiseWeather() { ev("InitialiseWeather"); }
void InitialiseBlobShadow() { ev("InitialiseBlobShadow"); }
void FLY_RestoreMeshes() { ev("FLY_RestoreMeshes"); }
void CWorld::RestoreEngineObjects() { ev("RestoreEngineObjects", {Delta(this)}); }
void CMap::UpdateMapInEngine() { ev("UpdateMapInEngine", {Delta(this)}); }
void CControlManager::CheckAllControlsInitialised() { ev("CheckAllControlsInitialised", {Delta(this)}); }
void RecalculateParkDarkening() { ev("RecalculateParkDarkening"); }
void CClock::PostLoad() { ev("ClockPostLoad", {lab(this), gRestartParkClocks}); }
void TickRideAnims() { ev("TickRideAnims"); }
void RegenerateRideSigns() { ev("RegenerateRideSigns"); }
void UIInterface::PostLevelLoad() { ev("PostLevelLoad", {lab(this)}); }
void PAR_SetLowSpec(int32_t on) { ev("PAR_SetLowSpec", {on}); }
void ESP_FixSpriteIDsFromDetail() { ev("ESP_FixSpriteIDsFromDetail"); }
void SOUND_SetPauseOn3DSoundsOnly(int32_t on) { ev("SOUND_SetPauseOn3DSoundsOnly", {on}); }

extern "C" {

uint64_t tpw_sh_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"gClock", gClockStorage},
        {"gText", gTextStorage},
        {"gPlayerSettings", gPlayerSettingsStorage},
        {"gUIInterface", gUIInterfaceStorage},
        {"gRestartParkClocks", &gRestartParkClocks},
        {"gSaveFileKey", &gSaveFileKey},
        {"gParkRequiredRides", gParkRequiredRides},
        {"langbuf", gLangBuf},
        {"legal0", &LegalText()[0]},
        {"legal1", &LegalText()[1]},
        {"legal2", &LegalText()[2]},
        {"legal3", &LegalText()[3]},
        {"legal4", &LegalText()[4]},
        {"legal5", &LegalText()[5]},
        {"legal6", &LegalText()[6]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// what 0: the world pointer; 1 to 7: the text pointer of language what - 1.
void tpw_sh_setptr(int32_t what, uint64_t value) {
    if (what == 0) gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(value));
    else LegalText()[what - 1].mData = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(value));
}

void tpw_sh_reset() {
    gBlobLen = 0;
    std::memset(gBlob, 0, sizeof gBlob);
}

uint32_t tpw_sh_blob(uint8_t* out) {
    std::memcpy(out, gBlob, gBlobLen);
    return gBlobLen;
}

int32_t tpw_sh_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings, mCurrent) + offsetof(CPlayerSettings::Settings, mDetail));
    return n;
}

uint32_t tpw_sh_save(uint64_t file, uint64_t header) {
    CSaveLoad saver;
    return static_cast<uint32_t>(saver.SaveHeader(reinterpret_cast<void*>(static_cast<uintptr_t>(file)),
                                                  reinterpret_cast<CSaveLoadHeader*>(static_cast<uintptr_t>(header))));
}

void tpw_sh_pre() {
    CSaveLoad saver;
    saver.PreLoadOperations();
}

void tpw_sh_post() {
    CSaveLoad saver;
    saver.PostLoadOperations();
}

}  // extern "C"

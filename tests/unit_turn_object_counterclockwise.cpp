// Test library for eight key actions (unit v5034; libtpw_unit_turn_object_counterclockwise.dylib, tests/test_turn_object_counterclockwise.py), main app:
//   src/game/keyboard.cpp   the eight functions, gTestAdvisorIndex and gQuickSaveSuffix
//
// The callees are fakes that record the arguments the real ones read and answer from the test's queues; the globals of other files the
// unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "keytable_actions.h"
#include "ActionRecorder.h"
#include "Advisor.h"
#include "Debug.h"
#include "FS_LoadSave.h"
#include "Game.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "SaveLoad.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "intrface.h"

using namespace calllog;

class CWorld;

// ---- globals of other files, defined here
CGame gGame;
CSaveLoad gSaveLoad;
CFSLoadSave gLoadSave;
CActionRecorder gActionRecorder;
CIntrface gIntrface;
CWorld* gGameWorld;

namespace {
char gNameBuf[64];   // what the string fakes give as the text pointer
}

// ---- the string fakes (the glue imports): the same event names and arguments as the module's PowerPC stubs
#define STRING_FAKES(CH, PFX)                                                                                  \
    template <> TbDynamicStringTemplate<CH>::TbDynamicStringTemplate() {                                      \
        ev(PFX "Ctor", {newlab(this)});                                                                        \
        mLength = 0;                                                                                           \
        mData = nullptr;                                                                                       \
    }                                                                                                          \
    template <> TbDynamicStringTemplate<CH>::TbDynamicStringTemplate(const TbDynamicStringTemplate& other) {  \
        ev(PFX "CopyCtor", {newlab(this), lab(&other)});                                                       \
        mLength = 0;                                                                                           \
        mData = nullptr;                                                                                       \
    }                                                                                                          \
    template <> TbDynamicStringTemplate<CH>::~TbDynamicStringTemplate() { ev(PFX "Dtor", {lab(this)}); }       \
    template <> void TbDynamicStringTemplate<CH>::Format(const CH*, ...) {}                                    \
    template <> void TbDynamicStringTemplate<CH>::ResizeTo(uint32_t) {}                                        \
    template <> void TbDynamicStringTemplate<CH>::AllocBuffer(uint32_t) {}                                     \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbStringBase<CH>&) { \
        return *this;                                                                                          \
    }                                                                                                          \
    template <>                                                                                                \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbDynamicStringTemplate& other) { \
        ev(PFX "Assign", {lab(this), lab(&other)});                                                            \
        mData = reinterpret_cast<CH*>(gNameBuf);                                                               \
        return *this;                                                                                          \
    }                                                                                                          \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH*) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH&) { return *this; } \
    template <>                                                                                                \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const TbStringBase<CH>& text) {       \
        ev(PFX "Append", {lab(this), lab(&text)});                                                             \
        return *this;                                                                                          \
    }                                                                                                          \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH&) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH*) { return *this; }

STRING_FAKES(char, "Str")
STRING_FAKES(char16_t, "WStr")

CGame::CGame() {}
CGame::~CGame() {}

const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() {
    ev("GetCurrentThemeName", {lab(this)});
    return mCurrentThemeName;
}

int32_t StringToUniString(TbDynamicStringTemplate<char16_t>& out, const TbDynamicStringTemplate<char>& in) {
    ev("StringToUniString", {lab(&out), lab(&in)});
    return 0;
}

int32_t CSaveLoad::Load(const char* path, CSaveLoadHeader* header, LoadType type) {
    ev("SaveLoadLoad", {lab(this), lab(path), lab(header), static_cast<int64_t>(type)});
    return static_cast<int32_t>(ret("SaveLoadLoad"));
}

int32_t CSaveLoad::Save(const char* path, CSaveLoadHeader* header, LoadType type) {
    ev("SaveLoadSave", {lab(this), lab(path), lab(header), static_cast<int64_t>(type)});
    return static_cast<int32_t>(ret("SaveLoadSave"));
}

int32_t CFSLoadSave::Load(const TbDynamicStringTemplate<char16_t>& name) {
    ev("FSLoad", {lab(this), lab(&name)});
    return static_cast<int32_t>(ret("FSLoad"));
}

// CFSLoadSave::Save is void in the header; the unit's caller reads the answer, so the fake is an int-returning function on the same symbol.
extern "C++" int32_t CFSLoadSave_SaveFake(CFSLoadSave* self, const TbDynamicStringTemplate<char16_t>& name)
    asm("__ZN11CFSLoadSave4SaveERK23TbDynamicStringTemplateIDsE");
int32_t CFSLoadSave_SaveFake(CFSLoadSave* self, const TbDynamicStringTemplate<char16_t>& name) {
    ev("FSSave", {lab(self), lab(&name)});
    return static_cast<int32_t>(ret("FSSave"));
}

void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }

void DEBUG_MESSAGE(const char* message, uint32_t flags) { ev("DEBUG_MESSAGE", {static_cast<int64_t>(flags)}, {message}); }

void CActionRecorder::ReplayRecording(ReplayType type) { ev("ReplayRecording", {lab(this), static_cast<int64_t>(type)}); }

void CMapCell::PlaceLitter(LitterType type) {
    ev("PlaceLitter", {static_cast<int64_t>(reinterpret_cast<char*>(this) - reinterpret_cast<char*>(gGameWorld)), static_cast<int64_t>(type)});
}

int32_t LLCC_GetRideRotation() {
    ev("LLCC_GetRideRotation", {});
    return static_cast<int32_t>(ret("LLCC_GetRideRotation"));
}

void LLCC_SetRideRotation(int32_t rotation, bool flag) { ev("LLCC_SetRideRotation", {rotation, flag ? 1 : 0}); }

int32_t ADV_Stat() {
    ev("ADV_Stat", {});
    return static_cast<int32_t>(ret("ADV_Stat"));
}

int32_t ADV_Call(AdvisorResponse response) {
    ev("ADV_Call", {response.mId});
    return 0;
}

void ADV_Slap() { ev("ADV_Slap", {}); }

extern "C" {

uint64_t tpw_qs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gGame", &gGame},         {"namebuf", gNameBuf},          {"suffix", &gQuickSaveSuffix}, {"saveload", &gSaveLoad},
        {"recorder", &gActionRecorder}, {"loadsave", &gLoadSave}, {"idx", &gTestAdvisorIndex},   {"f24", &gIntrface.mF24},
        {"world", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_qs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_qs_layout(int64_t*) { return 0; }

uint32_t tpw_qs_turn_object_counterclockwise() { return static_cast<uint32_t>(turn_object_counterclockwise()); }
uint32_t tpw_qs_turn_object_clockwise() { return static_cast<uint32_t>(turn_object_clockwise()); }
uint32_t tpw_qs_quickload_layout() { return static_cast<uint32_t>(quickload_layout()); }
uint32_t tpw_qs_quicksave_layout() { return static_cast<uint32_t>(quicksave_layout()); }
uint32_t tpw_qs_place_vomit() { return static_cast<uint32_t>(place_vomit()); }
uint32_t tpw_qs_quickload() { return static_cast<uint32_t>(quickload()); }
uint32_t tpw_qs_quicksave() { return static_cast<uint32_t>(quicksave()); }
uint32_t tpw_qs_TestAdvisor() { return static_cast<uint32_t>(TestAdvisor()); }

}  // extern "C"

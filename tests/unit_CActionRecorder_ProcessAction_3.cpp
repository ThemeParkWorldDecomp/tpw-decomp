// Test library for the recorder / visitor panel / destructor stand-ins of unit v51072 (libtpw_unit_CActionRecorder_ProcessAction_3.dylib,
// tests/test_CActionRecorder_ProcessAction_3.py), main app:
//   src/game/ActionRecorder.cpp, src/game/ExitErr.cpp, src/game/InGame.cpp, src/game/keyboard.cpp, src/game/ui/ui_visitorStatus.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read. Pointers into the case's block are recorded as the
// offset from the block (gBase), the others as labels.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "bfl_FileSystem.h"
#include "ExitErr.h"
#include "Game.h"
#include "InGame.h"
#include "World.h"
#include "WorldSavvy.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "keytable_actions.h"

using namespace calllog;

// ---- globals of other files, defined here
CActionRecorder gActionRecorder;
// gGame is raw storage under its own symbol (the CGame constructor builds members this library does not link).
alignas(CGame) uint8_t gGameStorage[sizeof(CGame)] __asm__("_gGame");
InterfaceFont gInGameFont;
CExitError gExitError;
CWorld* gGameWorld;
CMap* gWorldMap;
CThing* gStaffRoomPanelThing;

InterfaceFont::InterfaceFont() { ev("InterfaceFont", {lab(this)}); }
InterfaceFont::~InterfaceFont() {}

namespace {
uint8_t* gBase = nullptr;

int64_t Rel(const void* p) { return p == nullptr ? -1 : static_cast<int64_t>(static_cast<int32_t>(static_cast<const uint8_t*>(p) - gBase)); }
}  // namespace

// ---- the fakes
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t a = va_arg(args, int32_t);
    int32_t b = va_arg(args, int32_t);
    va_end(args);
    int64_t sum = 0;
    for (const char* c = format; *c != 0; ++c) sum += static_cast<uint8_t>(*c);
    ev("CTRACE", {static_cast<int64_t>(level), a, b, sum});
}

void CActionRecorder::ReplayRecording(ReplayType type) { ev("ReplayRecording", {lab(this), static_cast<int64_t>(type)}); }
void CGame::TogglePause(bool allSounds) { ev("TogglePause", {lab(this), allSounds ? 1 : 0}); }
void CMap::CleanUpPark() { ev("CleanUpPark", {Rel(this) == -1 ? -1 : static_cast<int64_t>(reinterpret_cast<uint8_t*>(this) - reinterpret_cast<uint8_t*>(gGameWorld))}); }
void ShowStaffPanel(CThing* thing) { ev("ShowStaffPanel", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(thing)))}); }
void UIMain_DestroySpritePreview() { ev("DestroySpritePreview", {}); }
void UIStatusPanel::CloseWindow() { ev("CloseWindow", {Rel(this)}); }
void UIStatusPanel::ObjectGone(CThing* thing) {
    ev("ObjectGone", {Rel(this), static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(thing)))});
}
bfl::FileSystem::CFile::~CFile() { ev("CFileDtor", {Rel(this)}); }
// The rest of CFile's virtuals (the vtable the destructor above emits names them; nothing calls them here).
void bfl::FileSystem::CFile::Close() {}
int32_t bfl::FileSystem::CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t bfl::FileSystem::CFile::GetSize() { return 0; }
int32_t bfl::FileSystem::CFile::GetPos() { return 0; }
void* bfl::FileSystem::CFile::MapFile() { return nullptr; }
TbError bfl::FileSystem::CFile::Flush() { return TbError(0); }
int32_t bfl::FileSystem::CFile::GetMacFileReference() { return 0; }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("RegisterGlobalObject", {lab(object), lab(node), destructor == static_cast<void*>(gInGameFontDtor) ? 1 : 0});
}

namespace v51072_callee {
void OperatorDelete(void* block) { ev("delete", {Rel(block)}); }
void ThingIdListRemoveAll(void* list) { ev("RemoveAll", {Rel(list)}); }
}  // namespace v51072_callee

extern "C" {

uint64_t tpw_v51072_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"recorder", &gActionRecorder},
        {"game", gGameStorage},
        {"font", &gInGameFont},
        {"fontNode", gInGameFontNode},
        {"exitErr", &gExitError},
        {"world", &gGameWorld},
        {"map", &gWorldMap},
        {"staffThing", &gStaffRoomPanelThing},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51072_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
void tpw_v51072_set_base(uint64_t base) { gBase = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(base)); }
uint32_t tpw_v51072_ptr(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_v51072_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CExitError, mText));
    out[n++] = static_cast<int64_t>(sizeof(CExitError));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mWindow));
    return n;
}

// ---- the functions
void tpw_v51072_pa(uint64_t self, int32_t action, uint8_t count, uint64_t text) {
    reinterpret_cast<CActionRecorder*>(self)->ProcessAction(action, count, reinterpret_cast<char16_t*>(text));
}
void tpw_v51072_gnc(uint64_t self) { reinterpret_cast<CActionRecorder*>(self)->GetNextCameraFileEntry(); }
void tpw_v51072_gnt(uint64_t self) { reinterpret_cast<CActionRecorder*>(self)->GetNextTimingFileEntry(); }
void tpw_v51072_srd(uint64_t self) { reinterpret_cast<CActionRecorder*>(self)->SaveRecordingToDisk(); }
void tpw_v51072_wsd(uint64_t s, int16_t flag) { WideStringDestroy(reinterpret_cast<void*>(s), flag); }
void tpw_v51072_mfd(uint64_t s, int16_t flag) { CMemFileDestroy(reinterpret_cast<void*>(s), flag); }
void tpw_v51072_tld(uint64_t s, int16_t flag) { ThingIdListDestroy(reinterpret_cast<void*>(s), flag); }
void tpw_v51072_tae(uint64_t s) { ThingArrayEntryInit(reinterpret_cast<uint32_t*>(s)); }
void tpw_v51072_rem(uint64_t s) { reinterpret_cast<VisitorStatusPanel*>(s)->Remove(); }
void tpw_v51072_loc(uint64_t s) { reinterpret_cast<VisitorStatusPanel*>(s)->Locate(); }
void tpw_v51072_kid(uint64_t s, uint32_t thing) {
    reinterpret_cast<VisitorStatusPanel*>(s)->KidGone(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing)));
}
void tpw_v51072_diw(uint64_t s) { reinterpret_cast<VisitorStatusPanel*>(s)->DeInitWindow(); }
void tpw_v51072_disw(uint64_t s) { reinterpret_cast<VisitorStatusPanel*>(s)->DeInitStatusWindow(); }
void tpw_v51072_dvw(uint64_t s) { reinterpret_cast<VisitorStatusPanel*>(s)->DeInitViewWindow(); }
void tpw_v51072_ivw(uint64_t s) { reinterpret_cast<VisitorStatusPanel*>(s)->InitViewWindow(); }
uint32_t tpw_v51072_gm(uint64_t s) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(reinterpret_cast<CWorldSavvy*>(s)->GetMap()));
}
void tpw_v51072_sin() { __sinit_InGame_cpp(); }
void tpw_v51072_sex() { __sinit_ExitErr_cpp(); }
int32_t tpw_v51072_ccp() { return cheat_cleanup_park(); }
int32_t tpw_v51072_ss() { return show_script(); }
int32_t tpw_v51072_lp() { return LoadPtclLib(); }
int32_t tpw_v51072_sr() { return save_recording(); }
int32_t tpw_v51072_rr() { return recreate_recording(); }
int32_t tpw_v51072_rrr() { return recreate_recording_real_time(); }
int32_t tpw_v51072_rrm() { return recreate_recording_medium_speed(); }
int32_t tpw_v51072_pause() { return pause(); }

}  // extern "C"

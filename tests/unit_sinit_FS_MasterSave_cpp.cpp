// Test library for the unit v51117 functions (libtpw_unit_sinit_FS_MasterSave_cpp.dylib, tests/test_sinit_FS_MasterSave_cpp.py), main app:
//   src/FS_Online_gametype_std___tree_insert_node_at_3_sinit_FS_MasterSave_cpp.cpp
// Fakes record the arguments the real callees read; globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "lobby.h"
#include "LLCellSystem.h"
#include "keytable_actions.h"
#include "TbDynamicStringTemplate.h"

using namespace calllog;

CFSOnlineSystem* gOnlineSystem;
CLBScreenSystem* gLobbyScreens;
char16_t gMasterSaveWideText[160];
uint8_t gMasterSaveString[64];
uint8_t gMasterSaveNode[16];

namespace {
uint8_t gDummy[16];
uint8_t gGlobe[16];
alignas(16) uint8_t gScreens[256];
}  // namespace

void UIMain_DestroyWString(void*) {}

namespace bullfrog_shared {}

template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("WStringCtor", {newlab(this)}, {narrow(text).c_str()});
}

// The vtable of the string class is emitted where the unit defines its constructor: the other virtuals are empty stand-ins.
using WStr = TbDynamicStringTemplate<char16_t>;
template <> void WStr::AllocBuffer(uint32_t) {}
template <> void WStr::Format(const char16_t*, ...) {}
template <> void WStr::ResizeTo(uint32_t) {}
template <> WStr::~TbDynamicStringTemplate() {}
template <> WStr& WStr::operator=(const TbStringBase<char16_t>&) { return *this; }
template <> WStr& WStr::operator=(const WStr&) { return *this; }
template <> WStr& WStr::operator=(const char16_t*) { return *this; }
template <> WStr& WStr::operator=(const char16_t&) { return *this; }
template <> WStr& WStr::operator+=(const TbStringBase<char16_t>&) { return *this; }
template <> WStr& WStr::operator+=(const char16_t&) { return *this; }
template <> WStr& WStr::operator+=(const char16_t*) { return *this; }

CFSOnlineSystem::CFSOnlineSystem() { ev("OnlineCtor", {newlab(this)}); }
void CFSOnlineSystem::NewsConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    ev("NewsConnectionCallback", {lab(this), a, b, static_cast<int64_t>(reinterpret_cast<uintptr_t>(c) & 0xffffffffu),
                                  static_cast<int64_t>(reinterpret_cast<uintptr_t>(d) & 0xffffffffu)});
}
void CFSOnlineSystem::UploadServerConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    ev("UploadServerConnectionCallback", {lab(this), a, b, static_cast<int64_t>(reinterpret_cast<uintptr_t>(c) & 0xffffffffu),
                                          static_cast<int64_t>(reinterpret_cast<uintptr_t>(d) & 0xffffffffu)});
}
void CFSOnlineSystem::RequestFTPPostLoginCallback(int32_t a, const void* c) {
    ev("RequestFTPPostLoginCallback", {lab(this), a, static_cast<int64_t>(reinterpret_cast<uintptr_t>(c) & 0xffffffffu)});
}
void CFSOnlineSystem::GotoPlayerByIDFTPPostLoginCallback(int32_t a, const void* c) {
    ev("GotoPlayerByIDFTPPostLoginCallback", {lab(this), a, static_cast<int64_t>(reinterpret_cast<uintptr_t>(c) & 0xffffffffu)});
}

namespace parkinfo_callee {
void DownloadGlobeSlot4c(void* screen) { ev("DownloadGlobeSlot4c", {lab(screen)}); }
}  // namespace parkinfo_callee

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("Register", {lab(object), lab(node), destructor == reinterpret_cast<void*>(&UIMain_DestroyWString) ? 1 : 0});
}

int32_t LLCC_WriteCompleteHoardingsToBalanceFile() {
    ev("WriteComplete", {});
    return static_cast<int32_t>(ret("WriteComplete"));
}
int32_t LLCC_WriteHoardingsToBalanceFile() {
    ev("WriteHoardings", {});
    return static_cast<int32_t>(ret("WriteHoardings"));
}

extern "C" {

uint64_t tpw_v51117_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"online", &gOnlineSystem}, {"dummy", gDummy}, {"globe", gGlobe}, {"node", gMasterSaveNode},
        {"string", gMasterSaveString}, {"lobby", &gLobbyScreens}, {"vtable", gDummy}, {"screens", gScreens},
        {"pool", gMasterSaveWideText},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

void tpw_v51117_setlobby() {
    CLBScreenSystem* screens = reinterpret_cast<CLBScreenSystem*>(gScreens);
    screens->mDownloadGlobe = reinterpret_cast<CLBScreenComponentDownloadGlobe*>(gGlobe);
    gLobbyScreens = screens;
}

int32_t tpw_v51117_layout(int64_t* out) {
    CLBScreenSystem* screens = reinterpret_cast<CLBScreenSystem*>(gScreens);
    out[0] = reinterpret_cast<char*>(&screens->mDownloadGlobe) - reinterpret_cast<char*>(screens);
    return 1;
}

uint32_t tpw_v51117_call(int32_t op, int32_t a, int32_t b, uint32_t c, uint32_t d) {
    void* pc = reinterpret_cast<void*>(static_cast<uintptr_t>(c));
    void* pd = reinterpret_cast<void*>(static_cast<uintptr_t>(d));
    switch (op) {
        case 0: __sinit_FS_MasterSave_cpp(); return 0;
        case 1: FSOnlineNewsConnectionCallback(a, b, pc, pd); return 0;
        case 2: FSOnlineUploadServerConnectionCallback(a, b, pc, pd); return 0;
        case 3: RequestFTPPostLoginCallbackFn(a, pc); return 0;
        case 4: GotoPlayerByIDFTPPostLoginCallbackFn(a, pc); return 0;
        case 5: SatelliteOnlineParkInfoPanelCancelCallback(); return 0;
        default: break;
    }
    static int32_t (*const table[])() = {
        dbgchan0, dbgchan1, dbgchan2, dbgchan3, dbgchan4, dbgchan5, dbgchan6, dbgchan7, dbgchan8, dbgchan9,
        debug_track_cell_dirs, debug_hoarding_cell_neighbs, debug_track_cell_neighbs, debug_cell_dirs, debug_cell_neighbs,
        force_ride_fix, force_ride_condemned, force_ride_worn, toggle_cell_flash, next_debug_text_mode,
        set_allow_loops_no_research, select_debug_person, save_complete_hoardings, save_hoardings};
    return static_cast<uint32_t>(table[op - 6]());
}

}  // extern "C"

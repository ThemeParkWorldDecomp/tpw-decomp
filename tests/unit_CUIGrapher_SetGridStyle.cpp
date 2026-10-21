// Test library for the unit v51056 functions (libtpw_unit_CUIGrapher_SetGridStyle.dylib, tests/test_CUIGrapher_SetGridStyle.py), main app:
//   src/game/TPEmbed.cpp, src/game/online/bthread.cpp, src/game/save/FS_LoadSave.cpp, src/game/ui/UIGrapher.cpp, src/game/ui/ui_mainmenu.cpp
//   CUIGrapher setters, CFSLoadSave Save/SaveHidden/LoadHidden/destructor, ui_mainmenu.cpp and TPEmbed.cpp small functions,
//   bthread_create / bthread_mutex / bthread_cond::signal, the Argument vector and Connection destructors, operator_new.
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs); the globals of
// other files the unit reads (gTPGlobals, gLobby) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "FS_LoadSave.h"
#include "FS_Online.h"
#include "FinanceGraphPanelMsg.h"
#include "GameSys.h"
#include "LbSCBack.h"
#include "TPGlobals.h"
#include "UI_pump.h"
#include "lobby.h"
#include "online.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"

using namespace calllog;

// ---- globals of other files
// gTPGlobals as raw zero storage under its own symbol: the constructors and destructors of its strings are not linked here, and the Mac's
// bss is zero too.
alignas(64) uint8_t sTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
CLBLobby* gLobby;

namespace {

uint32_t W0(const void* p) { return p ? *static_cast<const uint32_t*>(p) : 0; }

void* ThreadFn(void* arg) { return arg; }

}  // namespace

// ---- the fakes
namespace v51056_callee {
void LoadSaveListDtor(CFSLoadSave* self) { ev("TbTListDtor", {lab(static_cast<void*>(self)), 0}); }
}  // namespace v51056_callee

int32_t CFSLoadSave::SaveInternal(const TbDynamicStringTemplate<char16_t>& name, const TbDynamicStringTemplate<char16_t>& extension) {
    ev("SaveInternal", {lab(this), lab(const_cast<TbDynamicStringTemplate<char16_t>*>(&name)),
                        lab(const_cast<TbDynamicStringTemplate<char16_t>*>(&extension)), W0(this), W0(&name)});
    return static_cast<int32_t>(ret("SaveInternal"));
}

int32_t CFSLoadSave::LoadInternal(const TbDynamicStringTemplate<char16_t>& name, const TbDynamicStringTemplate<char16_t>& extension) {
    ev("LoadInternal", {lab(this), lab(const_cast<TbDynamicStringTemplate<char16_t>*>(&name)),
                        lab(const_cast<TbDynamicStringTemplate<char16_t>*>(&extension)), W0(this), W0(&name)});
    return static_cast<int32_t>(ret("LoadInternal"));
}

void CLBLobby::SendQuit() { ev("SendQuit", {lab(this), W0(this)}); }

void GS_SetCurrentGameStatus(GS_GAME_STATES status) { ev("GS_SetCurrentGameStatus", {static_cast<int64_t>(status)}); }
void HideMainMenu() { ev("HideMainMenu", {}); }
void UI_ByeByeBankrupt() { ev("UI_ByeByeBankrupt", {}); }

namespace ftp_callee {
void OperatorDelete(void* p) { ev("Delete", {lab(p)}); }
void CSockDtor(FTPClient* self, int16_t flag) { ev("CSockDtor", {lab(self), flag}); }
}  // namespace ftp_callee

namespace online_callee {
void ArgumentVectorTearDown(void* vec) { ev("TearDown", {lab(vec)}); }
}  // namespace online_callee

namespace mainmenu_callee {
void WideStringCtor(void* string, const char16_t* text) { ev("WStringCtor", {lab(string), lab(const_cast<char16_t*>(text))}); }
}  // namespace mainmenu_callee

void UIMain_DestroyWString(void* object) {}

namespace NS_MacDoze {
void InitializeCriticalSection(void** section) {
    ev("InitCS", {lab(section)});
    *reinterpret_cast<uint32_t*>(section) = static_cast<uint32_t>(ret("InitCS"));
}
int32_t SetEvent(InternalHandle* handle) {
    ev("SetEvent", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(handle) & 0xffffffffu)});
    return static_cast<int32_t>(ret("SetEvent"));
}
uint32_t _beginthreadex(void* security, uint32_t stackSize, void* (*start)(void*), void* arg, uint32_t flags, uint32_t* id) {
    ev("BeginThread", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(security) & 0xffffffffu), stackSize,
                       lab(reinterpret_cast<void*>(start)), lab(arg), flags, id ? 1 : 0});
    if (id) *id = static_cast<uint32_t>(ret("ThreadId"));
    return static_cast<uint32_t>(ret("ThreadRet"));
}
}  // namespace NS_MacDoze

extern "C" {

void __register_global_object(void* object, void* destructor, void* node) {
    ev("Register", {lab(object), lab(destructor), lab(node)});
}

uint64_t tpw_v51056_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gTPGlobals", &gTPGlobals},
        {"ext4420", &gTPGlobals.mF4420},
        {"ext44ac", &gTPGlobals.mF44AC},
        {"gLobby", &gLobby},
        {"embedA", &gTPEmbedWordA},
        {"embedB", &gTPEmbedWordB},
        {"mmText", gMainMenuText},
        {"mmNode", gMainMenuTextNode},
        {"mmLit", gMainMenuLiteral},
        {"mmDtor", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
        {"threadFn", reinterpret_cast<const void*>(&ThreadFn)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51056_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v51056_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CUIGrapher));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mF2C));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mXAxis));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mYAxis));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mGrid));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mBG));
    out[n++] = static_cast<int64_t>(offsetof(Connection, mF00));
    out[n++] = static_cast<int64_t>(sizeof(CLBLobby*));
    return n;
}

// ---- CUIGrapher
void tpw_v51056_x_range(uint64_t o, int32_t a, int32_t b, int32_t c) { reinterpret_cast<CUIGrapher*>(o)->SetXRange(a, b, c); }
void tpw_v51056_y_range(uint64_t o, int32_t a, int32_t b, int32_t c) { reinterpret_cast<CUIGrapher*>(o)->SetYRange(a, b, c); }
void tpw_v51056_draw_area(uint64_t o, int32_t a, int32_t b, int32_t c, int32_t d) {
    reinterpret_cast<CUIGrapher*>(o)->SetDrawArea(a, b, c, d);
}

// The colour's five bytes: alpha, red, green, blue, index (a TbColour is made without running its constructor).
#define TPW_STYLE(fn, method)                                                                      \
    void fn(uint64_t o, uint32_t a, uint32_t r, uint32_t g, uint32_t b, uint32_t i) {              \
        alignas(TbColour) uint8_t bytes[sizeof(TbColour)];                                         \
        bytes[0] = static_cast<uint8_t>(a);                                                        \
        bytes[1] = static_cast<uint8_t>(r);                                                        \
        bytes[2] = static_cast<uint8_t>(g);                                                        \
        bytes[3] = static_cast<uint8_t>(b);                                                        \
        bytes[4] = static_cast<uint8_t>(i);                                                        \
        reinterpret_cast<CUIGrapher*>(o)->method(*reinterpret_cast<TbColour*>(bytes));             \
    }
TPW_STYLE(tpw_v51056_x_axis, SetXAxisStyle)
TPW_STYLE(tpw_v51056_y_axis, SetYAxisStyle)
TPW_STYLE(tpw_v51056_grid, SetGridStyle)
TPW_STYLE(tpw_v51056_bg, SetBGStyle)

// ---- CFSLoadSave
void tpw_v51056_save_hidden(uint64_t o, uint64_t name) {
    reinterpret_cast<CFSLoadSave*>(o)->SaveHidden(*reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(name));
}
void tpw_v51056_save(uint64_t o, uint64_t name) {
    reinterpret_cast<CFSLoadSave*>(o)->Save(*reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(name));
}
void tpw_v51056_load_hidden(uint64_t o, uint64_t name) {
    reinterpret_cast<CFSLoadSave*>(o)->LoadHidden(*reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(name));
}
uint64_t tpw_v51056_fs_dtor(uint64_t o, int32_t flag) {
    return reinterpret_cast<uintptr_t>(CFSLoadSave::Destroy(reinterpret_cast<CFSLoadSave*>(o), static_cast<int16_t>(flag)));
}

// ---- ui_mainmenu.cpp
void tpw_v51056_sinit_mainmenu() { __sinit_ui_mainmenu_cpp(); }
void tpw_v51056_post_login(int32_t error, uint64_t context) {
    MainLobbyMenuPostOnlineLoginCallbackFn(error, reinterpret_cast<const void*>(context));
}
int32_t tpw_v51056_quit_ok(uint64_t context) { return MainLobbyMenuConfirmQuit_OK(reinterpret_cast<void*>(context)); }
int32_t tpw_v51056_bankrupt_ok(uint64_t context) { return BankruptGameMenuConfirmRestart_OK(reinterpret_cast<void*>(context)); }
int32_t tpw_v51056_main_ok(uint64_t context) { return MainGameMenuConfirmRestart_OK(reinterpret_cast<void*>(context)); }

// ---- TPEmbed.cpp and threads
void tpw_v51056_sinit_embed() { __sinit_TPEmbed_cpp(); }
uint32_t tpw_v51056_thread_create(int32_t useFn, uint64_t arg) {
    return bthread_create(useFn ? &ThreadFn : nullptr, reinterpret_cast<void*>(arg));
}
uint64_t tpw_v51056_mutex_ctor(uint64_t o) {
    return reinterpret_cast<uintptr_t>(bthread_mutex::Construct(reinterpret_cast<bthread_mutex*>(o)));
}
int32_t tpw_v51056_cond_signal(uint64_t o) { return reinterpret_cast<bthread_cond*>(o)->signal(); }
void tpw_v51056_vec_dtor(uint64_t o, int32_t flag) { argdeque_callee::VectorDtor(reinterpret_cast<void*>(o), static_cast<int16_t>(flag)); }
uint64_t tpw_v51056_op_new(uint32_t size, uint64_t where) {
    return reinterpret_cast<uintptr_t>(operator_new(size, reinterpret_cast<void*>(where)));
}
uint64_t tpw_v51056_conn_dtor(uint64_t o, int32_t flag) {
    return reinterpret_cast<uintptr_t>(Connection::Destroy(reinterpret_cast<Connection*>(o), static_cast<int16_t>(flag)));
}

}  // extern "C"

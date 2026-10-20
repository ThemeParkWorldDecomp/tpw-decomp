// Test library for twenty small main-app functions (unit v51058; libtpw_unit_ShowWorldLobbyController.dylib,
// tests/test_ShowWorldLobbyController.py), main app:
//   src/std_less_std_basic_string_operator_ShowWorldLobbyController.cpp   the functions and the storage of the static initialisers
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues; the globals of
// other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "MslWStringTree.h"
#include "TPGlobals.h"
#include "TbDynamicStringTemplate.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "lobby.h"
#include "online.h"
#include "std_deque_errorthingy.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
InterfaceWindow* gUISatelliteParent;   // 0x10396fec
CLBScreenSystem* gLobbyScreens;        // 0x102e49b8
InterfaceWindow* gPublishParkPanel;    // 0x10396278
InterfaceWindow* gUIErrorDialog;       // 0x1039647c
CUI2DMap gUI2DMap;                     // 0x1039742c
TPGlobals gTPGlobals;                  // 0x103b74c8

void UIMain_DestroyWString(void*) {}

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("UI_SendMessage"));
}

void CLBScreenComponentAdvisor::ClearAll() { ev("ClearAll", {lab(this)}); }

int32_t DialogBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DialogBoxCallback", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DialogBoxCallback"));
}

int32_t CUI2DMap::MapCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("MapCallback", {lab(this), lab(window), message, a, b});
    return static_cast<int32_t>(ret("MapCallback"));
}

uint64_t gMslMacAddressHigh;

namespace parkinfo_callee {
void DownloadGlobeSlot4c(void* screen) { ev("DownloadGlobeSlot4c", {lab(screen)}); }
}
namespace mainmenu_callee {
void PublishGlobeSlot34(void* screen) { ev("PublishGlobeSlot34", {lab(screen)}); }
}

bool NocaseWStringLess(const void* a, const void* b) {
    ev("NocaseWStringLess", {lab(a), lab(b)});
    return ret("NocaseWStringLess") != 0;
}

namespace errdeque_callee {
void CursorSub(MslErrorDequeCursor* cursor, int32_t count) { ev("CursorSub", {lab(cursor), count}); }
}

namespace wstring_less_callee {
uint32_t Size(const MslWideString* s) {
    ev("Size", {lab(s)});
    return static_cast<uint32_t>(ret("Size"));
}
const char16_t* Begin(const MslWideString* s) {
    ev("Begin", {lab(s)});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("Begin"))));
}
}  // namespace wstring_less_callee

namespace IME {
int32_t MslWideString_compare(const MslWideString* self, uint32_t pos, uint32_t n1, const char16_t* s, uint32_t n2) {
    ev("Compare", {lab(self), pos, n1, static_cast<int64_t>(reinterpret_cast<uintptr_t>(s) & 0xffffffffu), n2});
    return static_cast<int32_t>(ret("Compare"));
}
}  // namespace IME

template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    std::string t = narrow(text);
    ev("StringCtor", {lab(this)}, {t.c_str()});
}

// the vtable of the string needs every virtual defined; none is called
#define STR_STUBS(C)                                                                                       \
    using S_##C = TbDynamicStringTemplate<C>;                                                              \
    template <> S_##C::TbDynamicStringTemplate() {}                                                        \
    template <> S_##C::TbDynamicStringTemplate(const S_##C&) {}                                            \
    template <> S_##C::TbDynamicStringTemplate(const TbStringBase<C>&) {}                                  \
    template <> void S_##C::AllocBuffer(uint32_t) {}                                                       \
    template <> void S_##C::Format(const C*, ...) {}                                                       \
    template <> void S_##C::ResizeTo(uint32_t) {}                                                          \
    template <> S_##C::~TbDynamicStringTemplate() {}                                                       \
    template <> S_##C& S_##C::operator=(const TbStringBase<C>&) { return *this; }                          \
    template <> S_##C& S_##C::operator=(const S_##C&) { return *this; }                                    \
    template <> S_##C& S_##C::operator=(const C*) { return *this; }                                        \
    template <> S_##C& S_##C::operator=(const C&) { return *this; }                                        \
    template <> S_##C& S_##C::operator+=(const TbStringBase<C>&) { return *this; }                         \
    template <> S_##C& S_##C::operator+=(const C&) { return *this; }                                       \
    template <> S_##C& S_##C::operator+=(const C*) { return *this; }
STR_STUBS(char16_t)
STR_STUBS(char)

TPGlobals::TPGlobals() { ev("TPGlobals", {lab(this)}); }
TPGlobals::~TPGlobals() {}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("RegisterGlobal", {lab(object), lab(destructor), lab(node)});
}

#define OFF(T, m) static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<T*>(0x1000)->m) - 0x1000)
#define PTR(x) reinterpret_cast<void*>(static_cast<uintptr_t>(x))
#define ENTRY(name) uint64_t tpw_wlc_##name(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e)
#define UNUSED (void)a, (void)b, (void)c, (void)d, (void)e

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_wlc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sat", &gUISatelliteParent},
        {"screens", &gLobbyScreens},
        {"panel", &gPublishParkPanel},
        {"errdlg", &gUIErrorDialog},
        {"map", &gUI2DMap},
        {"wtext", gWorldLobbyControllerText},
        {"wnode", gWorldLobbyControllerTextNode},
        {"ptext", gPublishParkPanelText},
        {"pnode", gPublishParkPanelTextNode},
        {"tpg", &gTPGlobals},
        {"tpvec", gTPGlobalsDtorVector},
        {"tpnode", gTPGlobalsNode},
        {"p20", &gOnlinePersonWord20},
        {"p24", &gOnlinePersonWord24},
        {"dtor", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_wlc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the lobby screen system's download globe, publish globe and advisor.
int32_t tpw_wlc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = OFF(CLBScreenSystem, mDownloadGlobe);
    out[n++] = OFF(CLBScreenSystem, mPublishGlobe);
    out[n++] = OFF(CLBScreenSystem, mAdvisor);
    return n;
}

ENTRY(Show) { UNUSED; ShowWorldLobbyController(); return 0; }
ENTRY(Hide) { UNUSED; HideWorldLobbyController(); return 0; }
ENTRY(Remove) { UNUSED; RemoveWorldLobbyController(); return 0; }
ENTRY(SinitWorld) { UNUSED; __sinit_ui_worldLobbyController_cpp(); return 0; }
ENTRY(Map) {
    UNUSED;
    return static_cast<uint32_t>(UI_2DMap_CallbackFn(static_cast<InterfaceWindow*>(PTR(a)), static_cast<int32_t>(b), static_cast<int32_t>(c),
                                                      static_cast<int32_t>(d)));
}
ENTRY(Find) { UNUSED; FindParkOnlineParkInfoPanelCancelCallback(); return 0; }
ENTRY(ArgIter) {
    UNUSED;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(MslArgumentVectorCursorAdd(static_cast<MslErrorDequeCursor*>(PTR(a)), static_cast<int32_t>(b))));
}
ENTRY(Lt) {
    UNUSED;
    return wstring_less_callee::Lt(static_cast<const MslWideString*>(PTR(a)), static_cast<const MslWideString*>(PTR(b)));
}
ENTRY(Ne) {
    UNUSED;
    return argdeque_callee::IterNotEqual(static_cast<const MslArgumentDequeIter*>(PTR(a)), static_cast<const MslArgumentDequeIter*>(PTR(b)));
}
ENTRY(SinitTP) { UNUSED; __sinit_TPGlobals_cpp(); return 0; }
ENTRY(VcPK) { UNUSED; return BuddyMapValueCompare_PairKey(PTR(a), PTR(b), PTR(c)); }
ENTRY(VcKP) { UNUSED; return BuddyMapValueCompare_KeyPair(PTR(a), PTR(b), PTR(c)); }
ENTRY(SinitPerson) { UNUSED; __sinit_OnlinePerson_cpp(); return 0; }
ENTRY(CursorAdd) { UNUSED; errdeque_callee::CursorAdd(static_cast<MslErrorDequeCursor*>(PTR(a)), static_cast<int32_t>(b)); return 0; }
ENTRY(Dec) { UNUSED; errdeque_callee::CdequeDecrement(static_cast<MslErrorDequeCursor*>(PTR(a))); return 0; }
ENTRY(Index) { UNUSED; return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(errdeque_callee::CdequeIndex(PTR(a), static_cast<uint32_t>(b)))); }
void tpw_wlc_sethigh(uint64_t high) { gMslMacAddressHigh = high; }
ENTRY(ErrCb) {
    UNUSED;
    return static_cast<uint32_t>(ErrorMessageCallback(static_cast<InterfaceWindow*>(PTR(a)), static_cast<int32_t>(b), static_cast<int32_t>(c),
                                                       static_cast<int32_t>(d)));
}
ENTRY(SinitPub) { UNUSED; __sinit_ui_publishParkPanel_cpp(); return 0; }
ENTRY(RemovePub) { UNUSED; RemovePublishParkPanel(); return 0; }
ENTRY(PostReq) { UNUSED; PostRequestUploadCallbackFunction(static_cast<int32_t>(a), PTR(b)); return 0; }

}  // extern "C"

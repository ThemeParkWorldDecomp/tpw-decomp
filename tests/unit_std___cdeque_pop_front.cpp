// Test library for the UIInterface forwarders and the small CodeWarrior deque pieces (unit v51076; libtpw_unit_std___cdeque_pop_front.dylib,
// tests/test_std___cdeque_pop_front.py), main app:
//   src/UI_pump_std___cdeque_pop_front.cpp   the forwarders, __cdeque<const void*>::pop_front / max_size, iterator copy, deque ctor
//
// The callees outside the unit are fakes that record the arguments the real ones read (pointers as labels) and answer from the test's
// queue. gPlayerSettings (src/TPSettings.cpp) is defined here with empty Init and destructor, so that it stays zero at start.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPSettings.h"
#include "UIInterface.h"
#include "UI_pump.h"
#include "std_cdeque.h"
#include "ui_Postcard.h"
#include "ui_main.h"
#include "UI_Base.h"

using namespace calllog;

// ---- globals of other files, defined here
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
CPlayerSettings gPlayerSettings;

namespace {
char gObjA[64];
char gObjB[64];
inline int64_t L(const void* p) { return static_cast<int64_t>(lab(const_cast<void*>(p))); }
}  // namespace

// ---- the fakes
void ShowVersionNumberTag() { ev("ShowVersionNumberTag", {}); }
void UI_Congratulations(CObject* o) { ev("UI_Congratulations", {L(o)}); }
void UI_Bankrupt(CObject* o) { ev("UI_Bankrupt", {L(o)}); }
void CoasterButtonsEnabled(uint32_t m) { ev("CoasterButtonsEnabled", {static_cast<int64_t>(m)}); }
void CoasterButtonsDown(uint32_t m) { ev("CoasterButtonsDown", {static_cast<int64_t>(m)}); }
void EndCoasterEditMode() { ev("EndCoasterEditMode", {}); }
void _OnlineUpdateBuddy(char16_t* n, int32_t d) { ev("_OnlineUpdateBuddy", {L(n), d}); }
void _OnlineRemoveBuddy(char16_t* n) { ev("_OnlineRemoveBuddy", {L(n)}); }
void _OnlineAddBuddy(char16_t* n, int32_t d) { ev("_OnlineAddBuddy", {L(n), d}); }
void _OnlineUpdateContact(char16_t* n, int32_t d) { ev("_OnlineUpdateContact", {L(n), d}); }
void _OnlineRemoveContact(char16_t* n) { ev("_OnlineRemoveContact", {L(n)}); }
void _OnlineAddContact(char16_t* n, int32_t d) { ev("_OnlineAddContact", {L(n), d}); }
void OnlineReceiveString(char16_t* n, int32_t d) { ev("OnlineReceiveString", {L(n), d}); }
void UI_ToggleHelpText() { ev("UI_ToggleHelpText", {}); }
bool UI_HelpTextEnabled() {
    ev("UI_HelpTextEnabled", {});
    return ret("help") != 0;
}
void CreateResearchPanel() { ev("CreateResearchPanel", {}); }
void CreateEntryPricePanel() { ev("CreateEntryPricePanel", {}); }
void ToggleGameWindow() { ev("ToggleGameWindow", {}); }
void UI_SwitchGameWindow(int32_t first) { ev("UI_SwitchGameWindow", {first}); }
void TogglePauseUI() { ev("TogglePauseUI", {}); }
void DeInitOnlineUI() { ev("DeInitOnlineUI", {}); }

void std::__cdeque<const void*, std::allocator<const void*>>::resize(uint32_t n, const void* value) {
    ev("CdequeResize", {L(this), static_cast<int64_t>(n), L(value)});
}
void std::__cdeque<const void*, std::allocator<const void*>>::reserve(uint32_t n) {
    ev("CdequeReserve", {L(this), static_cast<int64_t>(n)});
}

extern "C" {

uint64_t tpw_v_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"objA", gObjA},
        {"objB", gObjB},
        {"settings", &gPlayerSettings},
        {"mapsize", &gMslErrorDequeMapSize},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = reinterpret_cast<const char*>(&gPlayerSettings.mCurrent.mTooltipsOn) - reinterpret_cast<const char*>(&gPlayerSettings);
    out[n++] = static_cast<int64_t>(sizeof(CPlayerSettings));
    out[n++] = static_cast<int64_t>(sizeof(std::__cdeque<const void*, std::allocator<const void*>>));
    return n;
}

// The UIInterface forwarders by index (the module's UI table); a and b are pointers or integers by the function.
void tpw_v_ui(int32_t op, uint64_t a, uint64_t b) {
    UIInterface ui;
    char16_t* p = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(a));
    CObject* o = reinterpret_cast<CObject*>(static_cast<uintptr_t>(a));
    int32_t d = static_cast<int32_t>(b);
    switch (op) {
        case 0: ui.ShowVersionTag(); break;
        case 1: ui.Congratulations(o); break;
        case 2: ui.Bankrupt(o); break;
        case 3: ui.ChangeCoasterButtonsEnabled(static_cast<uint32_t>(a)); break;
        case 4: ui.ChangeCoasterButtonsDown(static_cast<uint32_t>(a)); break;
        case 5: ui.EndCoasterEdit(); break;
        case 6: ui.OnlineUpdateBuddy(p, d); break;
        case 7: ui.OnlineRemoveBuddy(p); break;
        case 8: ui.OnlineAddBuddy(p, d); break;
        case 9: ui.OnlineUpdateContact(p, d); break;
        case 10: ui.OnlineRemoveContact(p); break;
        case 11: ui.OnlineAddContact(p, d); break;
        case 12: ui.OnlineReceiveStr(p, d); break;
        case 13: ui.ToggleHelpText(); break;
        case 14: ui.Screen_Research(); break;
        case 15: ui.Screen_EntryPrice(); break;
        case 16: ui.SwitchGameWindow(); break;
        case 17: ui.ShowFirstPersonWindow(static_cast<int32_t>(a)); break;
        case 18: ui.TogglePause(); break;
        case 19: ui.DeInitOnline(); break;
    }
}

void tpw_v_pop_front(uint64_t block) {
    reinterpret_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(static_cast<uintptr_t>(block))->pop_front();
}

uint32_t tpw_v_max_size(uint64_t block) {
    return reinterpret_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(static_cast<uintptr_t>(block))->max_size();
}

// out: the destination's three fields after the copy, then the source's; the answer: 1 when it answered the destination.
uint32_t tpw_v_itercopy(uint32_t d, uint32_t i, uint32_t p, uint32_t* out) {
    MslCdequeIter src;
    src.mDeque = reinterpret_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(static_cast<uintptr_t>(d));
    src.mIndex = i;
    src.mPtr = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(p));
    MslCdequeIter dest;
    std::memset(&dest, 0xdd, sizeof dest);
    MslCdequeIter* r = cdeque_callee::IterCopy(&dest, &src);
    out[0] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dest.mDeque));
    out[1] = dest.mIndex;
    out[2] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dest.mPtr));
    out[3] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(src.mDeque));
    out[4] = src.mIndex;
    out[5] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(src.mPtr));
    return r == &dest ? 1 : 0;
}

void tpw_v_dequector(uint64_t self) { errsinit_callee::DequeCtor(reinterpret_cast<void*>(static_cast<uintptr_t>(self)), nullptr); }

void tpw_v_ecd_resize(uint64_t self, uint32_t n, uint64_t value) {
    ErrorCdequeResize(reinterpret_cast<void*>(static_cast<uintptr_t>(self)), n, reinterpret_cast<errorthingy*>(static_cast<uintptr_t>(value)));
}

void tpw_v_ecd_reserve(uint64_t self, uint32_t n) { ErrorCdequeReserve(reinterpret_cast<void*>(static_cast<uintptr_t>(self)), n); }

}  // extern "C"

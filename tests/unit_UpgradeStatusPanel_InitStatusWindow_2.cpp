// Test library for the upgrade status panel (unit v5905; libtpw_unit_UpgradeStatusPanel_InitStatusWindow_2.dylib), main app:
//   src/game/ui/ui_upgradeStatus.cpp
//
// Every callee is a recording fake with the same event as the PowerPC stub. Windows are named by id (a table the test fills).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"
#include "Ptr32.h"

using namespace calllog;

CWorld* gGameWorld;
CThing* gStaffRoomPanelThing;

namespace {

struct WinEntry {
    int32_t id;
    uintptr_t address;
    int32_t root;
};
WinEntry gWins[32];
int gWinCount = 0;

int64_t I(int64_t v) { return v; }

int64_t WinId(const void* p) {
    for (int i = 0; i < gWinCount; i++)
        if (gWins[i].address == reinterpret_cast<uintptr_t>(p)) return gWins[i].id;
    return -1;
}

int64_t Raw(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }

int64_t Half(const void* thing) { return *reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(thing) + 0xe); }

}  // namespace

int32_t UpgradeStatusMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {WinId(this), I(id)});
    if (id >= 0x5200)
        for (int i = 0; i < gWinCount; i++)
            if (!gWins[i].root && gWins[i].id == id) return reinterpret_cast<InterfaceWindow*>(gWins[i].address);
    return nullptr;
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) {
    ev("SetClientDrawObject", {WinId(this), I(lab(object))});
}

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {WinId(this), Raw(font)}); }

void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {WinId(this), Raw(text)}); }

int32_t InterfaceWindow::Show(int16_t shown) {
    ev("Show", {WinId(this), I(shown)});
    return 0;
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {I(index)});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(ret("font")));
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {I(id)});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(ret("str")));
}

void UIMain_AdvisorMessage(AdvisorMessage message) { ev("UIMain_AdvisorMessage", {I(static_cast<int32_t>(message))}); }

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {Half(this)});
    return nullptr;
}

uint32_t CObject::GetCurrentResidualValue() const {
    ev("GetCurrentResidualValue", {Half(this)});
    return static_cast<uint32_t>(ret("resid"));
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {Raw(this)});
    return reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(ret("park")));
}

int32_t CParkAnalyser::NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const {
    ev("NumberOfObjectInPark", {Raw(this), I(id.mIndex)});
    return static_cast<int32_t>(ret("num"));
}

int32_t UIStatusPanel::FillObjectList(CThing* thing) {
    ev("FillObjectList", {Raw(thing)});
    return static_cast<int32_t>(ret("fill"));
}

void UIStatusPanel::InitEditBox(int32_t id) { ev("InitEditBox", {I(id)}); }

void UIStatusPanel::OpenWindow(int16_t* description, InterfaceWindowCallback callback, int32_t a, int32_t b, int32_t c) {
    int64_t d = description == gUpgradeStatusScript ? 0x102af2ac : Raw(description);
    int64_t f = callback == UpgradeStatusMsg ? 0x10268028 : Raw(reinterpret_cast<void*>(callback));
    ev("OpenWindow", {d, f, I(a), I(b), I(c)});
}

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {I(size)});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {I(newlab(mem))}); }
void CashCtor(void* mem) { ev("CashCtor", {I(newlab(mem))}); }
void NumberCtor(void* mem) { ev("NumberCtor", {I(newlab(mem))}); }
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {WinId(window), I(value)}); }
}  // namespace lender_callee

extern "C" {

uint64_t tpw_us_addr(const char* name) {
    if (std::strcmp(name, "world") == 0) return reinterpret_cast<uintptr_t>(&gGameWorld);
    if (std::strcmp(name, "staff") == 0) return reinterpret_cast<uintptr_t>(&gStaffRoomPanelThing);
    if (std::strcmp(name, "upthing") == 0) return reinterpret_cast<uintptr_t>(&gUpgradeStatusThing);
    return 0;
}

uint64_t tpw_us_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_us_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_us_layout(int64_t*) { return 0; }

void tpw_us_clear() { gWinCount = 0; }

void tpw_us_setwin(int32_t id, uint64_t address, int32_t root) {
    gWins[gWinCount].id = id;
    gWins[gWinCount].address = static_cast<uintptr_t>(address);
    gWins[gWinCount].root = root;
    gWinCount++;
}

void tpw_us_init(uint64_t panel) { reinterpret_cast<UpgradeStatusPanel*>(panel)->InitStatusWindow(); }
void tpw_us_update(uint64_t panel) { reinterpret_cast<UpgradeStatusPanel*>(panel)->UpdateStatusWindow(); }
void tpw_us_initwin(uint64_t panel) { reinterpret_cast<UpgradeStatusPanel*>(panel)->InitWindow(); }
void tpw_us_show(uint64_t panel, uint64_t thing) {
    reinterpret_cast<UpgradeStatusPanel*>(panel)->Show(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing)));
}

}  // extern "C"

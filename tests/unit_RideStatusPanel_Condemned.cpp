// Test library for RideStatusPanel::Condemned and ::InitStatusWindow (unit v5580; libtpw_unit_RideStatusPanel_Condemned.dylib,
// tests/test_RideStatusPanel_Condemned.py), main app:
//   src/game/ui/ui_RideStatus.cpp
//
// The callees outside the unit are fakes that record what the real ones read; the windows are blocks of memory (never constructed), the
// globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "analyser.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "World.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "fake_status_panel_table.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
CThingArrayEntry gThingArray[kThingArraySize];
CThing* gStaffRoomPanelThing;

namespace {

// The windows are blocks of the arena: the panel keeps them in Ptr32s (unit v5L31).
unsigned char (*Windows())[0x140] {
    static auto* windows = static_cast<unsigned char(*)[0x140]>(tpw::ArenaAllocZeroed(15 * 0x140));
    return windows;
}
RideStatusPanel gPanel;

// The panel's table: the slot at +0x1c is logged as the Mac's stub logs it.
uint64_t Slot(void*, uint32_t offset, const void*) {
    if (offset == 0x1c) ev("Slot5", {});
    return 0;
}

int64_t Ptr(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

// ---- the fakes: the same event name and arguments as the module's PowerPC stub
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("assert", {condition}, {format}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(Windows()[id - 0x3e16 + 1]);
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), Ptr(font) & 0xffffffff}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), Ptr(text) & 0xffffffff}); }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(0x5000 + index));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(0x7000 + id));
}
void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) { ev("IndentClient", {lab(window), a, b}); }

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem)}); }
void CashCtor(void* mem) { ev("CashCtor", {newlab(mem)}); }
void NumberCtor(void* mem) { ev("NumberCtor", {newlab(mem)}); }
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }
}  // namespace lender_callee
namespace vsp_callee {
void GradientCtor(void* mem, void* data) { ev("GradientCtor", {newlab(mem)}, {static_cast<const char*>(data)}); }
}  // namespace vsp_callee

extern "C" {

uint64_t tpw_ride_addr(const char* name) {
    if (std::strncmp(name, "win", 3) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Windows()[std::atoi(name + 3)]));
    if (std::strcmp(name, "panel") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gPanel));
    return 0;
}

uint64_t tpw_ride_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ride_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(kThingArraySize);
    return n;
}

// The panel: window (+0x0c), status window (+0x10) and the ops table (+0x20, always there).
void tpw_ride_panel_set(uint64_t window, uint64_t status) {
    gPanel.mWindow.set(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)));
    gPanel.mStatusWindow.set(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(status)));
    gPanel.mVtable.set(FakeStatusPanelTable<Slot>());
}
void tpw_ride_set_thing(uint32_t index, uint64_t value) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(value)); }
void tpw_ride_set_staff(uint64_t value) { gStaffRoomPanelThing = reinterpret_cast<CThing*>(static_cast<uintptr_t>(value)); }

void tpw_ride_Condemned(uint32_t index) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(index);
    gPanel.Condemned(id);
}
void tpw_ride_InitStatusWindow() { gPanel.InitStatusWindow(); }

}  // extern "C"

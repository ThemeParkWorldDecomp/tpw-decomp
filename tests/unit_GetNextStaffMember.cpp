// Test library for GetNextStaffMember and ShowStatusPanel::InitWindow (unit v51001; libtpw_unit_GetNextStaffMember.dylib,
// tests/test_GetNextStaffMember.py), main app:
//   src/game/ui/ui_showStatus.cpp, src/game/ui/ui_staffPanel.cpp
//
// The callees outside the unit are recording fakes. Pointers the fakes hand back (things, windows, the font, the string) come from a
// pool of arena blocks, chosen by the case's queues (index 0 is NULL). The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"
#include "ui_iterators.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "UI_Base.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"

using namespace calllog;

int32_t gStaffPanelLocation;
CThing* gStaffPanelThing;
CThing* gShowStatusThing;

namespace {
void* gPool[16];
}

// ---- the fakes
CThing* UI_IterateThingNext(UIIteratorTypes type, CThing* thing) {
    ev("Iter", {static_cast<int64_t>(type), lab(thing)});
    return static_cast<CThing*>(gPool[ret("Iter", static_cast<int64_t>(type), 0)]);
}

int32_t UIStatusPanel::FillObjectList(CThing* thing) {
    ev("Fill", {lab(this), lab(thing)});
    return static_cast<int32_t>(ret("Fill"));
}

void UIStatusPanel::InitEditBox(int32_t id) { ev("InitEditBox", {lab(this), id}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(gPool[ret("GetWindow", id, 0)]);
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}

void InterfaceScrollBar::SetMinMax(int32_t min, int32_t max) { ev("SetMinMax", {lab(this), min, max}); }

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }

void InterfaceVaryBox::SetMinMaxValues(int32_t* minimum, int32_t* maximum) {
    ev("SetMinMaxValues", {lab(this), *minimum, *maximum});
}

void InterfaceVaryBox::SetInc(int32_t* increment) { ev("SetInc", {lab(this), *increment}); }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return static_cast<InterfaceFont*>(gPool[7]);
}

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }

void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return static_cast<const char16_t*>(gPool[8]);
}

void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }

// The weak defaults in the src file name the two constructors; the strong fakes below replace those defaults, so these only satisfy the linker.
extern "C" void FakeCashCtor(void*) asm("__ZN23InterfaceCashTextObjectC1EPv");
extern "C" void FakeCashCtor(void*) { __builtin_trap(); }
extern "C" void FakeSpriteCtor(void*) asm("__ZN25InterfaceTextObjectSpriteC1EPv");
extern "C" void FakeSpriteCtor(void*) { __builtin_trap(); }

namespace showstatus_callee {
InterfaceDrawObject* NewCashText() {
    void* p = tpw::ArenaAllocZeroed(0x19c);
    ev("CashCtor", {newlab(p), 0});
    return static_cast<InterfaceDrawObject*>(p);
}
InterfaceDrawObject* NewTextSprite() {
    void* p = tpw::ArenaAllocZeroed(0x19c);
    ev("SpriteCtor", {newlab(p), 0});
    return static_cast<InterfaceDrawObject*>(p);
}
}  // namespace showstatus_callee

// the base of the fake CoolRect (not compiled in this library): empty virtuals so the vtables link
CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRect::CoolRect() { ev("CoolRect", {}); }
CoolRect::~CoolRect() { ev("~CoolRect", {}); }

extern "C" {

uint64_t tpw_gns_addr(const char* name) {
    if (std::strcmp(name, "loc") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gStaffPanelLocation));
    return 0;
}

uint64_t tpw_gns_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_gns_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

void tpw_gns_pool(int32_t index, uint64_t address) { gPool[index] = reinterpret_cast<void*>(static_cast<uintptr_t>(address)); }

void tpw_gns_set(int32_t which, int32_t index) {
    CThing* p = static_cast<CThing*>(gPool[index]);
    if (which == 0) gStaffPanelThing = p;
    else gShowStatusThing = p;
}

int32_t tpw_gns_thing() {
    for (int32_t i = 0; i < 16; i++)
        if (gPool[i] == gStaffPanelThing) return i;
    return -1;
}

int32_t tpw_gns_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mWindow));
    return n;
}

void tpw_gns_next() { GetNextStaffMember(); }

void tpw_gns_init(uint64_t panel) { reinterpret_cast<ShowStatusPanel*>(static_cast<uintptr_t>(panel))->InitWindow(); }

}  // extern "C"

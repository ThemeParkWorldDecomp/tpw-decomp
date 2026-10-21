// Test library for ShopStatusPanel::InitStatusWindow and ShopStatusPanel::UpdateWindow (unit v5936;
// libtpw_unit_ShopStatusPanel_InitStatusWindow.dylib, tests/test_ShopStatusPanel_InitStatusWindow.py), main app:
//   src/ShopStatusPanel_InitWindow_ShopStatusPanel_ApplyChanges_ShopStatusPanel_InitStatusWindow.cpp
//
// Every callee outside the unit is a fake that records what the real one reads. Objects are named by a tag word at +0x138 of the
// test's blocks (windows, the shop object), new objects by their order (100 + n).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "fake_status_panel_table.h"

using namespace calllog;

// ---- globals of other files
CThing* gStaffRoomPanelThing;   // 0x103947a4

namespace {

std::vector<const void*> gNew;
uint64_t gWinMap[0x100];

int64_t Ident(const void* p) {
    if (p == nullptr) return 0;
    for (size_t i = 0; i < gNew.size(); i++)
        if (gNew[i] == p) return 100 + static_cast<int64_t>(i);
    uint32_t tag;
    std::memcpy(&tag, static_cast<const char*>(p) + 0x138, 4);
    return static_cast<int32_t>(tag);
}

int64_t Val(const void* p) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(p)); }

// The panel's table (unit v5L31): the slots at +0x1c, +0x20 and +0x3c (the name) are logged.
uint64_t Slot(void* panel, uint32_t offset, const void*) {
    if (offset == 0x1c) ev("Ops5", {1});
    if (offset == 0x20) ev("Ops6", {1});
    if (offset != 0x3c) return 0;
    ev("Ops13", {panel != nullptr ? 1 : 0});
    return ret("Ops13");
}

}  // namespace

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {Ident(this), id});
    uint64_t v = (id >= 0xc000 && id < 0xc100) ? gWinMap[id - 0xc000] : 0;
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(v));
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetObj", {Ident(this), Ident(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {Ident(this), Val(font)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetText", {Ident(this), Val(text)}); }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {Ident(this), on});
    return 0;
}
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {Ident(this), state, redraw});
    return 0;
}
bool CObject::IsClosed() const {
    ev("IsClosed", {Ident(this)});
    return ret("IsClosed") != 0;
}
uint8_t CObject::GetProductQuality() const {
    ev("Quality", {Ident(this)});
    return static_cast<uint8_t>(ret("Quality"));
}
uint8_t CObject::GetAmountOfSpecialIngredient() const {
    ev("Amount", {Ident(this)});
    return static_cast<uint8_t>(ret("Amount"));
}
const unsigned char* CObject::GetSpecialIngredientName() const {
    ev("IngName", {Ident(this)});
    return reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(ret("IngName")));
}
uint32_t CObject::GetPricePerUse() const {
    ev("Price", {Ident(this)});
    return static_cast<uint32_t>(ret("Price"));
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFont", {index});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(0x5000 + index));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(0x6000 + id));
}
char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    ev("ConvertMulti", {Val(text)});
    return reinterpret_cast<char16_t*>(static_cast<uintptr_t>(ret("ConvertMulti")));
}
void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) { ev("Indent", {Ident(window), a, b}); }

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("OperatorNew", {size});
    void* p = tpw::ArenaAllocZeroed(size);
    gNew.push_back(p);
    return p;
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {Ident(mem), 0}); }
void CashCtor(void* mem) { ev("CashCtor", {Ident(mem), 0}); }
void NumberCtor(void*) {}
void PercentCtor(void*) {}
void WindowCtor(void*, InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("WSetValue", {Ident(window), value}); }
}  // namespace lender_callee

namespace parkstatus_callee {
void* OperatorNew(uint32_t) { return nullptr; }
void SpriteCtor(void*) {}
void DrawSpriteCtor(void*) {}
void NumberCtor(void*) {}
void GradientCtor(void* mem, const char* meshName) {
    ev("GradientCtor", {Ident(mem), static_cast<int64_t>(meshName - gShopStatusMeshNames)});
}
void WindowSetValue(InterfaceWindow*, int32_t) {}
void WindowSetText(InterfaceWindow* window, const char16_t* text) { ev("WSetText", {Ident(window), Val(text)}); }
}  // namespace parkstatus_callee

extern "C" {

uint64_t tpw_ssiw_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ops", FakeStatusPanelTable<Slot>()},
        {"winmap", gWinMap},
        {"staff", &gStaffRoomPanelThing},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ssiw_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 value of a block (the panel's windows and table are Ptr32, unit v5L31).
uint32_t tpw_ssiw_off(uint64_t address) {
    return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address)));
}

void tpw_ssiw_reset() {
    gNew.clear();
    std::memset(gWinMap, 0, sizeof(gWinMap));
    gStaffRoomPanelThing = nullptr;
}

int32_t tpw_ssiw_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mStatusWindow));
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mF14));
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mVtable));
    out[n++] = static_cast<int64_t>(sizeof(ShopStatusPanel));
    return n;
}

void tpw_ssiw_init(uint64_t panel) { reinterpret_cast<ShopStatusPanel*>(static_cast<uintptr_t>(panel))->InitStatusWindow(); }
void tpw_ssiw_update(uint64_t panel) { reinterpret_cast<ShopStatusPanel*>(static_cast<uintptr_t>(panel))->UpdateWindow(); }

}  // extern "C"

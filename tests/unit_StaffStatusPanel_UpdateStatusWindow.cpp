// Test library for the staff status panel's InitStatusWindow, UpdateStatusWindow and UpdateWindow (unit v5207;
// libtpw_unit_StaffStatusPanel_UpdateStatusWindow.dylib, tests/test_StaffStatusPanel_UpdateStatusWindow.py), main app:
//   src/StaffStatusPanel_FireMe_StaffStatusPanel_UpdateStatusWindow.cpp
//
// Every callee outside the unit is a fake that records what the real one reads (the same event names and arguments as the module's
// PowerPC stubs). The windows are blocks of the arena with a fake virtual table (Enable, SetValue and SetText record). The draw
// objects' constructors are defined under their mangled names so that no base class or vtable is needed. The globals of other
// files are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files
CThing* gStaffRoomPanelThing = nullptr;
// gText is raw zeroed storage under its own symbol (CText's constructor needs the font code, which this library does not link).
alignas(16) char gTextStorage[sizeof(CText)] __asm__("_gText");
char16_t gUIMainConvertBuffer[0x4000];

namespace {

InterfaceWindow* gWins[18];   // by id - 0x50e
char16_t* gNamePtr = nullptr;
InterfaceFont* gFont = nullptr;
uintptr_t gStrBlock = 0;

// ---- the fake virtual table of the windows (Itanium order: DrawFunction 0, CalcClientArea 1, ~ 2 and 3, WhatAmI 4, ClientDraw 5,
// Enable 6, operator- 7, SetValue 8, GetValue 9, SetText 10)
void* gFakeVt[2 + 12];

int16_t FakeEnable(void* self, int16_t on) {
    ev("Enable", {lab(self), static_cast<int64_t>(on)});
    return 0;
}

int32_t FakeSetValue(void* self, int32_t value) {
    ev("SetValue", {lab(self), static_cast<int64_t>(value)});
    return 0;
}

int32_t FakeSetText(void* self, const char16_t* text) {
    ev("SetText", {lab(self), static_cast<int64_t>(text[0]), static_cast<int64_t>(text[1]), static_cast<int64_t>(text[2])});
    return 0;
}

// The panel's table entries: the function sits in a block (the entry is the arena offset of the block).
void PanelSlot20(StaffStatusPanel* panel) { ev("PanelSlot", {0x20, lab(panel)}); }
void PanelSlot1c(StaffStatusPanel* panel) { ev("PanelSlot", {0x1c, lab(panel)}); }

}  // namespace

// ---- the fakes: one per callee outside the unit
int32_t CStaff::GetGeneralStaffState() const {
    ev("GetGeneralStaffState", {lab(this)});
    return static_cast<int32_t>(ret("state"));
}

int32_t CStaff::GetEmploymentDuration() const {
    ev("GetEmploymentDuration", {lab(this)});
    return static_cast<int32_t>(ret("duration"));
}

uint32_t CStaff::GetSalary() {
    ev("GetSalary", {lab(this)});
    return static_cast<uint32_t>(ret("salary"));
}

int32_t CStaff::GetSkill() const {
    ev("GetSkill", {lab(this)});
    return static_cast<int32_t>(ret("skill"));
}

int32_t CStaff::HasPatrolArea() const {
    ev("HasPatrolArea", {lab(this)});
    return static_cast<int32_t>(ret("patrol"));
}

int32_t CStaff::CanBePickedUp() {
    ev("CanBePickedUp", {lab(this)});
    return static_cast<int32_t>(ret("pickup"));
}

const char16_t* CStaff::GetName() const {
    ev("GetName", {lab(this)});
    return gNamePtr;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), static_cast<int64_t>(id)});
    return gWins[id - 0x50e];
}

void InterfaceWindow::SetFrame(int32_t frame) { ev("SetFrame", {lab(this), static_cast<int64_t>(frame)}); }

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }

void InterfaceWindow::SetClientDrawText(char16_t* text) {
    int64_t offset = text != nullptr ? static_cast<int64_t>(reinterpret_cast<uintptr_t>(text) - gStrBlock) : -1;
    ev("SetClientDrawText", {lab(this), offset});
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }

int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), static_cast<int64_t>(state), static_cast<int64_t>(redraw)});
    return 0;
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    TbIntegerVTag* tag = va_arg(args, TbIntegerVTag*);
    va_end(args);
    ev("FormatString", {lab(this), static_cast<int64_t>(out == gUIMainConvertBuffer), static_cast<int64_t>(size),
                        static_cast<int64_t>(id), static_cast<int64_t>(tag->mMagic), static_cast<int64_t>(tag->mId),
                        static_cast<int64_t>(tag->mValue)});
    out[0] = static_cast<char16_t>(0x41 + (id & 15));
    out[1] = static_cast<char16_t>(tag->mValue & 0xffff);
    out[2] = 0;
    return 0;
}

// The key function of the tag's vtable (the real one is untranslated and never called here).
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return gFont;
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {static_cast<int64_t>(id)});
    return reinterpret_cast<const char16_t*>(gStrBlock + 2 * static_cast<uintptr_t>(id));
}

int32_t UIMain_GetStaffStateFrame(int32_t state) {
    ev("UIMain_GetStaffStateFrame", {static_cast<int64_t>(state)});
    return static_cast<int32_t>(ret("frame"));
}

void UIMain_SetStaffStateText(InterfaceWindow* window, int32_t state) {
    ev("UIMain_SetStaffStateText", {lab(window), static_cast<int64_t>(state)});
}

void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) {
    ev("UIMain_IndentClient", {lab(window), static_cast<int64_t>(a), static_cast<int64_t>(b)});
}

void UIMain_AdvisorMessage(AdvisorMessage message) { ev("UIMain_AdvisorMessage", {static_cast<int64_t>(message)}); }

// The draw objects' constructors under their mangled names (no class machinery needed).
extern "C" void FakeSpriteCtor(void* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
extern "C" void FakeHGradientCtor(void* self, void* data) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");
extern "C" void FakeCashCtor(void* self, void* data) __asm__("__ZN23InterfaceCashTextObjectC1EPv");

extern "C" void FakeSpriteCtor(void* self, void* data) {
    ev("SpriteCtor", {newlab(self), static_cast<int64_t>(reinterpret_cast<intptr_t>(data))});
}

extern "C" void FakeCashCtor(void* self, void* data) {
    ev("CashCtor", {newlab(self), static_cast<int64_t>(reinterpret_cast<intptr_t>(data))});
}

extern "C" void FakeHGradientCtor(void* self, void* data) {
    ev("HGradient", {newlab(self)}, {static_cast<const char*>(data)});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ss_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"buffer", gUIMainConvertBuffer},
        {"thing", &gStaffRoomPanelThing},
        {"table", &reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[6]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ss_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset of a block (what a Ptr32 field holds).
uint32_t tpw_ss_off(uint64_t block) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(block))); }

// kind 0: window `index` (an id - 0x50e) and gives the block the fake virtual table; 1: the name; 2: the font; 3: the string block.
void tpw_ss_set(int32_t kind, int32_t index, uint64_t block) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(block));
    if (gFakeVt[2 + 6] == nullptr) {
        gFakeVt[2 + 6] = reinterpret_cast<void*>(&FakeEnable);
        gFakeVt[2 + 8] = reinterpret_cast<void*>(&FakeSetValue);
        gFakeVt[2 + 10] = reinterpret_cast<void*>(&FakeSetText);
    }
    switch (kind) {
        case 0:
            if (index >= 0) gWins[index] = static_cast<InterfaceWindow*>(p);
            *reinterpret_cast<void***>(p) = &gFakeVt[2];
            break;
        case 1: gNamePtr = static_cast<char16_t*>(p); break;
        case 2: gFont = static_cast<InterfaceFont*>(p); break;
        case 3: gStrBlock = reinterpret_cast<uintptr_t>(p); break;
        default: break;
    }
}

// The host address of the panel table's functions: 0x20 or 0x1c.
uint64_t tpw_ss_slot(int32_t which) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(which == 0x20 ? &PanelSlot20 : &PanelSlot1c));
}

// The offsets and sizes the module relies on.
int32_t tpw_ss_layout(int64_t* out) {
    int32_t n = 0;
    CStaff* staff = static_cast<CStaff*>(tpw::ArenaAllocZeroed(sizeof(CStaff)));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&staff->mHappiness) - reinterpret_cast<char*>(staff));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&staff->mEnergy) - reinterpret_cast<char*>(staff));
    out[n++] = static_cast<int64_t>(sizeof(CStaff));
    out[n++] = static_cast<int64_t>(sizeof(StaffStatusPanel) == 1 ? 1 : 0);
    return n;
}

// ---- the functions
void tpw_ss_init(uint64_t panel) {
    reinterpret_cast<StaffStatusPanel*>(static_cast<uintptr_t>(panel))->InitStatusWindow();
}

void tpw_ss_update_status(uint64_t panel) {
    reinterpret_cast<StaffStatusPanel*>(static_cast<uintptr_t>(panel))->UpdateStatusWindow();
}

void tpw_ss_update(uint64_t panel) { reinterpret_cast<StaffStatusPanel*>(static_cast<uintptr_t>(panel))->UpdateWindow(); }

}  // extern "C"

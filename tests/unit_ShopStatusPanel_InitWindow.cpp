// Test library for the shop status panel (unit v5085; libtpw_unit_ShopStatusPanel_InitWindow.dylib, tests/test_ShopStatusPanel_InitWindow.py), main app:
//   src/game/ui/ui_shopStatus.cpp   ShopStatusPanel::InitWindow, UpdateStatusWindow and gShopStatusObject
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. The windows are
// objects of a small class (InterfaceWindow's virtuals are defined here, empty) whose SetValue and SetText record. The globals of
// other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "History.h"
#include "InterfaceScrollBar.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "fake_status_panel_table.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CThing* gStaffRoomPanelThing;
// raw storage under the symbol gText: CText's constructors belong to other files
alignas(16) uint8_t gTextStorage[sizeof(CText)] __asm__("_gText");
char16_t gUIMainConvertBuffer[0x4000];
CWorld* gGameWorld;

namespace {

// The blocks the test fills (the Mac's are persistent blocks of the emulator).
alignas(16) uint8_t gObjectBlock[0x460];
alignas(16) uint8_t gObject2Block[0x460];
alignas(16) uint8_t gPanelBlock[0x80];
alignas(16) uint8_t gTableBlock[0x40];
alignas(16) uint8_t gWorldBlock[0x40];
alignas(16) uint8_t gAnalyserBlock[0x40];
// The panel's two windows are blocks of the arena: the panel keeps them in Ptr32s (unit v5L31).
uint8_t* ParentBlock() {
    static uint8_t* block = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(0x40));
    return block;
}
uint8_t* StatusBlock() {
    static uint8_t* block = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(0x40));
    return block;
}
alignas(16) uint8_t gIngredientBlock[0x10];
alignas(16) uint8_t gFontBlock[0x10];
alignas(16) uint8_t gOpsBlock[0x10];
const char16_t kText27[] = u"string27";
const char16_t kText28[] = u"string28";
char16_t gConvertedText[] = u"converted";

const int32_t kWindowIds[] = {0xc069, 0xc06a, 0xc06b, 0xc086, 0xc085, 0xc07e, 0xc084, 0xc083,
                              0xc070, 0xc076, 0xc077, 0xc079, 0xc078, 0xc07d, 0xc07c};
constexpr int kWindowCount = sizeof(kWindowIds) / sizeof(kWindowIds[0]);

struct FakeWindow : InterfaceWindow {
    FakeWindow() : InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0) {}
    int32_t SetValue(int32_t value) override {
        ev("SetValue", {lab(this), value});
        return 1;
    }
    int32_t SetText(char16_t* text) override {
        ev("SetText", {lab(this), lab(text)});
        return 1;
    }
};

// The panel's table: the slot at +0x2c is told the state changed.
uint64_t Slot(void* panel, uint32_t offset, const void*) {
    if (offset == 0x2c) ev("PanelNotify", {lab(panel)});
    return 0;
}

FakeWindow** Windows() {
    static FakeWindow* windows[kWindowCount];
    static bool made = false;
    if (!made) {
        made = true;
        for (int i = 0; i < kWindowCount; ++i) windows[i] = new FakeWindow();
    }
    return windows;
}

FakeWindow* WindowFor(int32_t id) {
    FakeWindow** windows = Windows();
    for (int i = 0; i < kWindowCount; ++i)
        if (kWindowIds[i] == id) return windows[i];
    return nullptr;
}

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- CText embeds a TbFontRenderParams; its members are other files'

// ---- the draw objects' constructors: their real ones are other files' (the symbols are given here under the same names)
extern "C" void FakeSpriteCtor(void* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
extern "C" void FakeSpriteCtor(void* self, void* data) { ev("SpriteCtor", {newlab(self), static_cast<int64_t>(reinterpret_cast<intptr_t>(data))}); }
extern "C" void FakeCashCtor(void* self, void* data) __asm__("__ZN23InterfaceCashTextObjectC1EPv");
extern "C" void FakeCashCtor(void* self, void* data) { ev("CashCtor", {newlab(self), static_cast<int64_t>(reinterpret_cast<intptr_t>(data))}); }

namespace shop_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    return tpw::ArenaAllocZeroed(0x800);
}
}  // namespace shop_callee

// ---- CoolRegion and CoolRect: the constructor and destructor record, the rest is empty
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() { ev("CoolRect", {newlab(this)}); }
CoolRect::~CoolRect() { ev("~CoolRect", {lab(this), -1}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

// ---- InterfaceWindow: the constructor and the virtuals are empty; the members the unit calls record
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return ret("win", id, 0) ? WindowFor(id) : nullptr;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 1;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }
void InterfaceScrollBar::SetMinMax(int32_t minimum, int32_t maximum) { ev("SetMinMax", {lab(this), minimum, maximum}); }
void InterfaceVaryBox::SetMinMaxValues(int32_t* minimum, int32_t* maximum) { ev("SetMinMaxValues", {lab(this), *minimum, *maximum}); }
void InterfaceVaryBox::SetInc(int32_t* increment) { ev("SetInc", {lab(this), *increment}); }

// ---- the status panel base
int32_t UIStatusPanel::FillObjectList(CThing* thing) {
    ev("FillObjectList", {lab(this), lab(thing)});
    return static_cast<int32_t>(ret("FillObjectList"));
}
void UIStatusPanel::InitEditBox(int32_t id) { ev("InitEditBox", {lab(this), id}); }

// ---- the text functions
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(gFontBlock);
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return id == 0x28 ? kText28 : kText27;
}
char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    ev("ConvertMulti", {lab(text)});
    return gConvertedText;
}
const unsigned char* CObject::GetSpecialIngredientName() const {
    ev("GetSpecialIngredientName", {lab(this)});
    return ret("ingr") ? gIngredientBlock : nullptr;
}

// ---- the shop's figures
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return nullptr;
}
template <>
int32_t CHistory<uint32_t, 30>::GetSumOfLast(double& sum, int32_t count) const {
    int64_t offset = reinterpret_cast<const uint8_t*>(this) - gObjectBlock;
    ev("Sum", {offset, count});
    sum = static_cast<double>(ret("sum", offset, 0));
    return 0;
}
int32_t CObject::GetProspectiveCostOfGoods(float quality, float ingredient) const {
    ev("Cost", {lab(this), CanonBits(quality), CanonBits(ingredient)});
    return static_cast<int32_t>(ret("Cost"));
}
int32_t CObject::GetCustomerSatisfaction() const {
    ev("Satisfaction", {lab(this)});
    return static_cast<int32_t>(ret("Satisfaction"));
}
int32_t CObject::GetProfitability() const {
    ev("Profitability", {lab(this)});
    return static_cast<int32_t>(ret("Profitability"));
}
uint32_t CObject::GetCurrentResidualValue() const {
    ev("Residual", {lab(this)});
    return static_cast<uint32_t>(ret("Residual"));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gAnalyserBlock);
}
int32_t CParkAnalyser::GetAverageHappinessAroundPoint(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, uint32_t radius) const {
    ev("Happiness", {lab(this), cell.mIndex, radius});
    return static_cast<int32_t>(ret("Happiness"));
}
int32_t UIMain_GetObjectState(CObject* object, int32_t flag) {
    ev("GetObjectState", {lab(object), flag});
    return static_cast<int32_t>(ret("GetObjectState"));
}
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list arguments;
    va_start(arguments, id);
    TbIntegerVTag* first = va_arg(arguments, TbIntegerVTag*);
    TbIntegerVTag* second = va_arg(arguments, TbIntegerVTag*);
    va_end(arguments);
    ev("FormatString", {lab(this), lab(out), size, id, static_cast<int32_t>(first->mId), static_cast<int32_t>(first->mValue),
                        static_cast<int32_t>(second->mId), static_cast<int32_t>(second->mValue)});
    return 0;
}

extern "C" {

// The host address of a block or global by the module's name for it.
uint64_t tpw_shop_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"object", gObjectBlock}, {"object2", gObject2Block}, {"text27", kText27}, {"text28", kText28}, {"converted", gConvertedText},
        {"panel", gPanelBlock}, {"table", gTableBlock}, {"world", gWorldBlock},
        {"analyser", gAnalyserBlock}, {"parent", ParentBlock()}, {"status", StatusBlock()}, {"ingredient", gIngredientBlock},
        {"font", gFontBlock}, {"ops", gOpsBlock}, {"buffer", gUIMainConvertBuffer},
        {"gStaffRoomPanelThing", &gStaffRoomPanelThing}, {"gShopStatusObject", &gShopStatusObject},
        {"gText_table", &reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[6]}, {"gGameWorld", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    if (std::strncmp(name, "win", 3) == 0) {
        int32_t id = static_cast<int32_t>(std::strtol(name + 3, nullptr, 10));
        return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(WindowFor(id)));
    }
    if (std::strcmp(name, "opsobject") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(FakeStatusPanelTable<Slot>()));
    if (std::strcmp(name, "arenabase") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase));
    return 0;
}

// The offsets the module relies on (the host's pointers are 8 bytes, so the panel's differ from the Mac's).
int32_t tpw_shop_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mState));
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mStatusWindow));
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mVtable));
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mQuality));
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mIngredient));
    out[n++] = static_cast<int64_t>(sizeof(ShopStatusPanel));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory6C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory1A4));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory22C));
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    return n;
}

void tpw_shop_init(uint64_t panel) { reinterpret_cast<ShopStatusPanel*>(panel)->InitWindow(); }
void tpw_shop_update(uint64_t panel) { reinterpret_cast<ShopStatusPanel*>(panel)->UpdateStatusWindow(); }

}  // extern "C"

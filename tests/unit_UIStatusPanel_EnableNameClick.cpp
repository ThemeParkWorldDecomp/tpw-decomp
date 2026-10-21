// Test library for the status panel's small members, the SamsUtilities swaps and the rank text object's destructor (unit v51114;
// libtpw_unit_UIStatusPanel_EnableNameClick.dylib, tests/test_UIStatusPanel_EnableNameClick.py), main app:
//   src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_StatusPanel.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The panel's
// table calls (__ptr_glue) are three function blocks the test builds; the window's Enable is a mock window with the real window's vtable
// order. gStaffRoomPanelThing is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "analyser.h"

using namespace calllog;

CThing* gStaffRoomPanelThing = nullptr;   // 0x103947a4

namespace {

int64_t Raw(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) & 0xffffffffu); }

// A window with the vtable order of InterfaceWindow (DrawFunction, CalcClientArea, destructor, WhatAmI, ClientDraw, Enable).
struct MockWindow {
    virtual void A() {}
    virtual void B() {}
    virtual ~MockWindow() {}
    virtual int32_t C() { return 0; }
    virtual void D() {}
    virtual int16_t Enable(int16_t on) {
        ev("Enable", {lab(this), on});
        return static_cast<int16_t>(ret("EnableAnswer"));
    }
};

template <int Offset>
void SlotFake(UIStatusPanel* panel, CThing* thing) {
    ev("Slot", {Offset, lab(panel), Offset == 0x34 ? Raw(thing) : 0});
}

}  // namespace

// ---- the callees outside the unit
char16_t* UIMain_GetObjectName(CObject* object) {
    ev("GetObjectName", {Raw(object)});
    return reinterpret_cast<char16_t*>(static_cast<uintptr_t>(ret("GetObjectName") & 0xffffffffu));
}

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("SendMessage"));
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {Raw(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return id;
}

void UIMain_ZoomToThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("ZoomToThing", {id.mIndex}); }

void CObject::CloseObject() { ev("CloseObject", {Raw(this)}); }
void CObject::Reopen() { ev("Reopen", {Raw(this)}); }

CThing* UI_WrapIterateThingPrevious(UIIteratorTypes type, CThing* thing) {
    ev("WrapPrev", {static_cast<int32_t>(type), Raw(thing)});
    return reinterpret_cast<CThing*>(static_cast<uintptr_t>(ret("WrapPrev") & 0xffffffffu));
}

namespace SamsUtilities {
void _USwapBlock16(uint16_t* words, int32_t count) { ev("USwapBlock16", {lab(words), count}); }
}  // namespace SamsUtilities

namespace rank_callee {
void FormatterDtor(void*, int16_t flag) { ev("FormatterDtor", {flag}); }
void SpriteListDtor(void*, int16_t flag) { ev("SpriteListDtor", {flag}); }
void EngineDataDtor(void*, int16_t flag) { ev("EngineDataDtor", {flag}); }
void DrawObjectDtor(void* self, int16_t flag) { ev("DrawObjectDtor", {lab(self), flag}); }
void Delete(void* self) { ev("Delete", {lab(self)}); }
}  // namespace rank_callee

// The tables the static initialisers store (src/game/ui/ui_StatusPanel.cpp, unit v5L31): blocks of the arena made on first use; the test
// compares only where they are stored.
uint32_t* UIStatusPanelTable() {
    static uint32_t* table = static_cast<uint32_t*>(tpw::ArenaAllocZeroed(0x80));
    return table;
}

extern "C" {

uint64_t tpw_us_addr(const char* name) {
    if (std::strcmp(name, "table") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(UIStatusPanelTable()));
    return 0;
}

uint64_t tpw_us_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_us_offset(uint64_t address) { return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

uint64_t tpw_us_make_window() {
    void* memory = tpw::ArenaAllocZeroed(sizeof(MockWindow));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (memory) MockWindow()));
}

// The function a table entry of the given offset runs (the first 8 bytes of its block).
uint64_t tpw_us_slot_fn(uint32_t offset) {
    switch (offset) {
        case 0xc: return reinterpret_cast<uintptr_t>(&SlotFake<0xc>);
        case 0x34: return reinterpret_cast<uintptr_t>(&SlotFake<0x34>);
        case 0x44: return reinterpret_cast<uintptr_t>(&SlotFake<0x44>);
    }
    return 0;
}

void tpw_us_set_thing(uint64_t value) { gStaffRoomPanelThing = reinterpret_cast<CThing*>(static_cast<uintptr_t>(value)); }
uint64_t tpw_us_get_thing() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gStaffRoomPanelThing)) & 0xffffffffu; }

int32_t tpw_us_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF14));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    return n;
}

// One entry for every function: the panel (or the first argument) and two arguments.
uint64_t tpw_us_call(int32_t op, uint64_t p, uint64_t a, uint64_t b) {
    UIStatusPanel* panel = reinterpret_cast<UIStatusPanel*>(static_cast<uintptr_t>(p));
    CThing* thing = reinterpret_cast<CThing*>(static_cast<uintptr_t>(a));
    switch (op) {
        case 0: panel->EnableNameClick(static_cast<int32_t>(a)); return 0;
        case 1: panel->EditNamePanelGone(); return 0;
        case 2: panel->ApplyChanges(); return 0;
        case 3: return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(panel->GetName())) & 0xffffffffu;
        case 4: panel->ChangeName(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(a))); return 0;
        case 5: panel->Remove(); return 0;
        case 6: panel->Show(thing); return 0;
        case 7: panel->CloseWindow(); return 0;
        case 8: panel->DeInitViewWindow(); return 0;
        case 9: panel->DeInitStatusWindow(); return 0;
        case 10: panel->UpdateViewWindow(); return 0;
        case 11: panel->UpdateStatusWindow(); return 0;
        case 12: panel->InitViewWindow(); return 0;
        case 13: panel->InitStatusWindow(); return 0;
        case 14: panel->DeInitWindow(); return 0;
        case 15: panel->UpdateWindow(); return 0;
        case 16: panel->InitWindow(); return 0;
        case 17: panel->ZoomTo(); return 0;
        case 18: panel->CloseObject(static_cast<int32_t>(a)); return 0;
        case 19: panel->ObjectGone(thing); return 0;
        case 20: panel->GetPreviousObject(); return 0;
        case 21: UIStatusPanel_Construct(panel); return 0;
        case 22: return static_cast<uint32_t>(SamsUtilities::USwap32(static_cast<TagId>(static_cast<uint32_t>(p))));
        case 23: SamsUtilities::USwapBlock16(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(p)), static_cast<int32_t>(a)); return 0;
        case 24: SamsUtilities::USwapInplace32(*reinterpret_cast<int32_t*>(static_cast<uintptr_t>(p))); return 0;
        case 25: SamsUtilities::USwapInplace16(*reinterpret_cast<int16_t*>(static_cast<uintptr_t>(p))); return 0;
        case 26: return static_cast<uint32_t>(SamsUtilities::USwap32(static_cast<TagButtonAction>(static_cast<int32_t>(p))));
        case 27: {
            InterfaceRankTextObject* self = reinterpret_cast<InterfaceRankTextObject*>(static_cast<uintptr_t>(p));
            return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(InterfaceRankTextObject_Dtor(self, static_cast<int16_t>(a))));
        }
    }
    return 0;
}

}  // extern "C"

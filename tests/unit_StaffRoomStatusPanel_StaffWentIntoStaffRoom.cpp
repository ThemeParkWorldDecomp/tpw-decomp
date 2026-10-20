// Test library for the staff room panel's member functions (unit v5291; libtpw_unit_StaffRoomStatusPanel_StaffWentIntoStaffRoom.dylib,
// tests/test_StaffRoomStatusPanel_StaffWentIntoStaffRoom.py), main app:
//   src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom.cpp
//
// The callees outside the unit are recording fakes. The objects of a case (panel, windows, thing, tables) are persistent arena blocks
// numbered like the module's OBJ; the windows are FakeWin objects whose virtual SetValue / SetText record.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "InterfaceWindow.h"
#include "Ptr32.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files
StaffType gStaffRoomStaffType;
CThing* gStaffRoomPanelThing;

int32_t StaffRoomStatusMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace {

// Same virtual order as InterfaceWindow (the slots the translation uses: SetValue, SetText).
struct FakeWin {
    virtual void DrawFunction() {}
    virtual void CalcClientArea() {}
    virtual ~FakeWin() {}
    virtual int32_t WhatAmI() { return 0; }
    virtual void ClientDraw() {}
    virtual int16_t Enable(int16_t) { return 0; }
    virtual int32_t operator-(InterfaceWindow&) { return 0; }
    virtual int32_t SetValue(int32_t value) {
        ev("SetValue", {lab(this), value});
        return 0;
    }
    virtual int32_t GetValue() { return 0; }
    virtual int32_t SetText(char16_t* text) {
        ev("SetText", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(text))});
        return 0;
    }
    virtual char16_t* GetText() { return nullptr; }
    virtual int32_t GetHelpText() { return 0; }
};

const int kObjects = 14;
const uint32_t kSizes[kObjects] = {0x40, 0x120, 0x120, 0x200, 0x120, 0x120, 0x120, 0x40, 0x10, 0x10, 0x10, 0x40, 0x10, 0x10};
void* gObj[kObjects];

uint64_t Slot3c(void* self) {
    ev("Slot3c", {lab(self)});
    return static_cast<uint64_t>(ret("Slot3c"));
}
uint64_t Slot20(void* self) {
    ev("Slot20", {lab(self)});
    return 0;
}
uint64_t Slot1c(void* self) {
    ev("Slot1c", {lab(self)});
    return 0;
}

void Init() {
    if (gObj[0] != nullptr) return;
    for (int i = 0; i < kObjects; i++) gObj[i] = tpw::ArenaAllocZeroed(kSizes[i]);
    for (int i : {1, 2, 4, 5, 6}) new (gObj[i]) FakeWin();
    typedef uint64_t (*Fn)(void*);
    *static_cast<Fn*>(gObj[8]) = Slot1c;
    *static_cast<Fn*>(gObj[9]) = Slot20;
    *static_cast<Fn*>(gObj[10]) = Slot3c;
}

void* FromData(int32_t data) {
    Ptr32<uint8_t> p;
    p.mOffset = static_cast<uint32_t>(data);
    return p.get();
}

}  // namespace

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    Init();
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(id == 0x1e7b ? gObj[1] : gObj[2]);
}

int32_t CStaff::IsRestingInStaffRoom(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> room) const {
    ev("IsResting", {lab(this), room.mIndex});
    return static_cast<int32_t>(ret("IsResting"));
}

StaffType CStaff::GetStaffType() {
    ev("GetStaffType", {lab(this)});
    return static_cast<StaffType>(ret("GetStaffType"));
}

const char16_t* CStaff::GetName() const {
    ev("GetName", {lab(this)});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(ret("GetName")));
}

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    ev("AddItem", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(item[0].mText)), item[1].mInt, lab(FromData(data)), position, update});
    return static_cast<int32_t>(ret("AddItem"));
}

int32_t InterfaceListControl2::FindData(int32_t data) {
    ev("FindData", {lab(this), lab(FromData(data))});
    return static_cast<int32_t>(ret("FindData"));
}

int32_t InterfaceListControl2::RemoveItem(int32_t position) {
    ev("RemoveItem", {lab(this), position});
    return 0;
}

void StaffRoomStatusPanel::FillStaffRoomList(StaffType type, int32_t flag) { ev("Fill", {lab(this), static_cast<int32_t>(type), flag}); }

void UIStatusPanel::OpenWindow(int16_t* description, InterfaceWindowCallback callback, int32_t a, int32_t b, int32_t c) {
    ev("OpenWindow", {lab(this), description == gStaffRoomScript ? 1 : 0, callback == StaffRoomStatusMsg ? 1 : 0, a, b, c});
}

void UIMain_AdvisorMessage(AdvisorMessage message) { ev("Advisor", {static_cast<int32_t>(message)}); }

uint32_t CObject::GetCurrentResidualValue() const {
    ev("Residual", {lab(this)});
    return static_cast<uint32_t>(ret("Residual"));
}

void SCR_KillScript(int32_t handle) { ev("KillScript", {handle}); }

extern "C" {

uint64_t tpw_sr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"room", &gStaffRoomId},
        {"stype", &gStaffRoomStaffType},
        {"showthing", &gStaffRoomShowThing},
        {"pthing", &gStaffRoomPanelThing},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sr_obj(int32_t index) {
    Init();
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gObj[index]));
}

uint32_t tpw_sr_off(uint64_t address) { return address != 0 ? tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))) : 0; }

int32_t tpw_sr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(StaffRoomPanelFields));
    out[n++] = static_cast<int64_t>(offsetof(StaffRoomPanelFields, mTextWindow));
    out[n++] = static_cast<int64_t>(offsetof(StaffRoomPanelFields, mScript));
    out[n++] = static_cast<int64_t>(sizeof(StaffType));
    out[n++] = static_cast<int64_t>(sizeof(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>));
    return n;
}

#define PANEL(p) reinterpret_cast<StaffRoomStatusPanel*>(static_cast<uintptr_t>(p))
#define THING(t) reinterpret_cast<CThing*>(static_cast<uintptr_t>(t))
void tpw_sr_went(uint64_t panel, uint64_t thing) { PANEL(panel)->StaffWentIntoStaffRoom(THING(thing)); }
void tpw_sr_updwin(uint64_t panel) { PANEL(panel)->UpdateWindow(); }
void tpw_sr_left(uint64_t panel, uint64_t thing) { PANEL(panel)->StaffLeftStaffRoom(THING(thing)); }
void tpw_sr_show(uint64_t panel, uint64_t thing) { PANEL(panel)->Show(THING(thing)); }
void tpw_sr_updstat(uint64_t panel) { PANEL(panel)->UpdateStatusWindow(); }
void tpw_sr_deinit(uint64_t panel) { PANEL(panel)->DeInitViewWindow(); }

}  // extern "C"

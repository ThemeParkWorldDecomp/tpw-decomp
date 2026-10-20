// Test library for the staff room panel's list fill and window set-up (unit v5669; libtpw_unit_StaffRoomStatusPanel_FillStaffRoomList.dylib,
// tests/test_StaffRoomStatusPanel_FillStaffRoomList.py), main app:
//   src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_StaffRoomStatusPanel_FillStaffRoomList.cpp
//   FillStaffRoomList, InitStatusWindow, InitWindow, UpdateViewSprite; it defines gStaffRoomColumnColour, gStaffRoomList, gStaffRoomSpawnBlock.
//
// Every callee outside the unit is a recording fake with the event name and arguments of the module's PowerPC stub. The windows and
// objects of a case are arena blocks the module allocates and registers by label (tpw_sr_setobj); the fakes answer with them.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ESP_sprites.h"
#include "Ptr32.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_iterators.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> gStaffRoomId;
CThing* gStaffRoomShowThing = nullptr;
CThing* gStaffRoomPanelThing = nullptr;
StaffType gStaffRoomStaffType = static_cast<StaffType>(0);
int32_t gStaffRoomMsg406 = 1;

namespace {

void* gObjects[64];
int gNewCount = 0;

void* Obj(int label) { return gObjects[label]; }

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Ptr(const void* p) { return lab(const_cast<void*>(p)); }

uint64_t SlotFn(void* panel) {
    ev("Slot28", {lab(panel)});
    return 0;
}

}  // namespace

// ---- the untranslated neighbours (strong, over the weak stand-ins of the unit's file): only their addresses are used
int32_t StaffRoomListFiller(CThing*) { return 0; }
InterfaceWindow* StaffRoomLCFormat() { return nullptr; }
void LCPostFormat(InterfaceWindow*) {}
int32_t StaffRoomPanelLCHandler(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- CoolRect and CoolRegion (the program's definitions are in files this library does not compile)
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() { ev("CoolRect", {}); }
CoolRect::~CoolRect() { ev("~CoolRect", {}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    switch (id) {
        case 0x1e7b: return static_cast<InterfaceWindow*>(Obj(4));
        case 0x1e7c: return static_cast<InterfaceWindow*>(Obj(5));
        case 0x1e84: return static_cast<InterfaceWindow*>(Obj(6));
        case 0x1e85: return static_cast<InterfaceWindow*>(Obj(7));
        case 0x1e86: return static_cast<InterfaceWindow*>(Obj(8));
        case 0x1e8a: return static_cast<InterfaceWindow*>(Obj(9));
        case 0x1e8b: return static_cast<InterfaceWindow*>(Obj(10));
        case 0x10: return static_cast<InterfaceWindow*>(Obj(11));
        case 0x11: return static_cast<InterfaceWindow*>(Obj(12));
        default: return nullptr;
    }
}

void InterfaceListControl2::RemoveAllItems() { ev("RemoveAllItems", {lab(this)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(const_cast<CThing*>(this))});
    return {static_cast<uint16_t>(ret("tid"))};
}

int32_t UI_IterateThingList(UIIteratorTypes mask, int (*fn)(CThing*)) {
    ev("Iterate", {static_cast<int32_t>(mask), Ptr(reinterpret_cast<const void*>(fn))});
    return 0;
}

void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFont", {index});
    return static_cast<InterfaceFont*>(Obj(19));
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), Ptr(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), Ptr(font)}); }

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    switch (id) {
        case 0x38: return static_cast<const char16_t*>(Obj(16));
        case 0x39: return static_cast<const char16_t*>(Obj(17));
        case 0x3a: return static_cast<const char16_t*>(Obj(18));
        default: return nullptr;
    }
}

void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), Ptr(text)}); }

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("OperatorNew", {size});
    return Obj(20 + gNewCount++);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {lab(mem)}); }
void CashCtor(void* mem) { ev("CashCtor", {lab(mem)}); }
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("WindowSetValue", {lab(window), value}); }
}  // namespace lender_callee

int32_t UIStatusPanel::FillObjectList(CThing* thing) {
    ev("FillObjectList", {lab(this), Ptr(thing)});
    return static_cast<int32_t>(ret("count"));
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}

void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), Ptr(reinterpret_cast<const void*>(makeRow)), Ptr(reinterpret_cast<const void*>(attach))});
}

void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) {
    ev("SetColumnType", {lab(this), column, static_cast<int32_t>(type)});
}

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    uint8_t b[8];
    std::memcpy(b, name, 8);
    int64_t w0 = (int64_t(b[0]) << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
    int64_t w1 = (int64_t(b[4]) << 24) | (b[5] << 16) | (b[6] << 8) | b[7];
    ev("FindDrawObject", {w0, w1});
    return static_cast<InterfaceDrawObject*>(Obj(22));
}

void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), Ptr(object)}); }

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), Ptr(reinterpret_cast<const void*>(callback))});
}

void InterfaceListControl2::SetSort(int32_t column, bool descending) { ev("SetSort", {lab(this), column, descending ? 1 : 0}); }

void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MAKECOLUMNBUTTON", {lab(window), Ptr(text), a, b, c, d, e});
}

int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {lab(this), id});
    return 0;
}

void UIStatusPanel::InitEditBox(int32_t id) { ev("InitEditBox", {lab(this), id}); }

int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {lab(this)});
    rect->mLeft = static_cast<int16_t>(ret("rl"));
    rect->mTop = static_cast<int16_t>(ret("rt"));
    rect->mRight = static_cast<int16_t>(ret("rr"));
    rect->mBottom = static_cast<int16_t>(ret("rb"));
    return 0;
}

float CalcZTrans(int32_t z) {
    ev("CalcZTrans", {z});
    uint32_t bits = static_cast<uint32_t>(ret("ctz"));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }

int32_t ESP_SpawnScript(void* block, ESP_Sprites sprite, int32_t a, ACTS acts, float x, float y, float z) {
    ev("ESP_SpawnScript", {lab(block), static_cast<int32_t>(sprite), a, static_cast<int32_t>(acts), CanonBits(x), CanonBits(y), CanonBits(z)});
    return static_cast<int32_t>(ret("esp"));
}

void SCR_SetFloatVariable(int32_t handle, uint32_t index, float value) {
    ev("SCR_SetFloatVariable", {handle, index, CanonBits(value)});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_sr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"colour", &gStaffRoomColumnColour},
        {"msg406", &gStaffRoomMsg406},
        {"panelthing", &gStaffRoomPanelThing},
        {"showthing", &gStaffRoomShowThing},
        {"id", &gStaffRoomId},
        {"list", &gStaffRoomList},
        {"stafftype", &gStaffRoomStaffType},
        {"spawn", gStaffRoomSpawnBlock},
        {"tv_filler", reinterpret_cast<const void*>(&StaffRoomListFiller)},
        {"tv_lcformat", reinterpret_cast<const void*>(&StaffRoomLCFormat)},
        {"tv_lcpost", reinterpret_cast<const void*>(&LCPostFormat)},
        {"tv_handler", reinterpret_cast<const void*>(&StaffRoomPanelLCHandler)},
        {"slotfn", reinterpret_cast<const void*>(&SlotFn)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
uint64_t tpw_sr_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

void tpw_sr_setobj(int32_t label, uint64_t address) { gObjects[label] = reinterpret_cast<void*>(static_cast<uintptr_t>(address)); }
void tpw_sr_reset() { gNewCount = 0; }

int32_t tpw_sr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    return n;
}

void tpw_sr_fill(uint64_t panel, int32_t type, int32_t flag) {
    reinterpret_cast<StaffRoomStatusPanel*>(panel)->FillStaffRoomList(static_cast<StaffType>(type), flag);
}
void tpw_sr_status(uint64_t panel) { reinterpret_cast<StaffRoomStatusPanel*>(panel)->InitStatusWindow(); }
void tpw_sr_window(uint64_t panel) { reinterpret_cast<StaffRoomStatusPanel*>(panel)->InitWindow(); }
void tpw_sr_view(uint64_t panel, uint64_t staff) {
    reinterpret_cast<StaffRoomStatusPanel*>(panel)->UpdateViewSprite(reinterpret_cast<CStaff*>(static_cast<uintptr_t>(staff)));
}

}  // extern "C"

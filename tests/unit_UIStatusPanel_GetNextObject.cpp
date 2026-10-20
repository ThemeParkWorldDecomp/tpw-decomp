// Test library for UIStatusPanel's GetNextObject, FillObjectList, EraseObject, OpenWindow and Move (unit v5090;
// libtpw_unit_UIStatusPanel_GetNextObject.dylib, tests/test_UIStatusPanel_GetNextObject.py), main app:
//   src/ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject.cpp   (also defines the three globals gStatusPanel*)
//
// Every callee outside the unit is a fake that records what the real one reads (pointers as the labels the test registered, new objects as
// 100 + their order) and answers from the test's queues. The windows and the panel are blocks of the arena: a window has a vtable pointer made
// here (only Enable is ever called), the panel is the Mac layout with a table whose entries point at blocks holding host function pointers.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "RideDB.h"
#include "TPSettings.h"
#include "ThingReceivers.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CThing* gStaffRoomPanelThing;
InterfaceWindow* gGameWindow;
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
void CPlayerSettings::Init() {}   // the constructor calls it; the fields are only written by the test
CPlayerSettings::~CPlayerSettings() {}
CPlayerSettings gPlayerSettings;

int32_t StatusPanelIteratorCallback(CThing*) { return 0; }
int32_t delme(void*) { return 0; }
int32_t ViewWindowCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace {

constexpr int kMaxLabels = 64;
uint64_t gLabelAddr[kMaxLabels];
int32_t gLabelId[kMaxLabels];
int gLabelCount = 0;
uint64_t gNewPtr[32];
int gNewCount = 0;

template <class T>
int64_t I(T value) {
    return static_cast<int64_t>(value);
}

int64_t LL(const void* p) {
    if (p == nullptr) return 0;
    const uint64_t a = reinterpret_cast<uintptr_t>(p);
    for (int i = 0; i < gLabelCount; ++i)
        if (gLabelAddr[i] == a) return gLabelId[i];
    // an object the code made (operator new is the arena's): numbered 100, 101 ... in the order the log first names it
    for (int i = 0; i < gNewCount; ++i)
        if (gNewPtr[i] == a) return 100 + i;
    if (gNewCount < 32) {
        gNewPtr[gNewCount] = a;
        return 100 + gNewCount++;
    }
    return -1;
}

void* LabelAddr(int64_t label) {
    if (label == 0) return nullptr;
    for (int i = 0; i < gLabelCount; ++i)
        if (gLabelId[i] == label) return reinterpret_cast<void*>(static_cast<uintptr_t>(gLabelAddr[i]));
    return nullptr;
}

int64_t Low32(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }

// ---- the fake window vtable: slot N of the InterfaceWindow class; Enable's slot is found by calling it once
constexpr int kWinSlots = 48;
void* gWinVt[kWinSlots];
bool gCalibrating = false;
int gEnableSlot = -1;
int gCalibSlot = -1;

template <int N>
int16_t WinSlot(void* self, int16_t on) {
    if (gCalibrating) {
        gCalibSlot = N;
    } else {
        ev("virt", {LL(self), N == gEnableSlot ? 0x20001c : 0x2f0000 + N, I(static_cast<uint16_t>(on))}, {});
    }
    return 0;
}
template <int... Is>
void FillWin(std::integer_sequence<int, Is...>) {
    ((gWinVt[Is] = reinterpret_cast<void*>(&WinSlot<Is>)), ...);
}

void Calibrate() {
    if (gEnableSlot >= 0) return;
    FillWin(std::make_integer_sequence<int, kWinSlots>{});
    void* fake[4] = {&gWinVt[0], nullptr, nullptr, nullptr};
    gCalibrating = true;
    static_cast<InterfaceWindow*>(static_cast<void*>(fake))->Enable(0);
    gCalibrating = false;
    gEnableSlot = gCalibSlot;
}

// ---- the panel's slots: one function per table offset 8 + 4 * N
constexpr int kPanelSlots = 16;
uint64_t gPanelBlock[kPanelSlots];

template <int N>
void PanelSlot(UIStatusPanel* panel) {
    ev("virt", {LL(panel), 0x100000 + 8 + 4 * N}, {});
}
template <int... Is>
void FillPanel(std::integer_sequence<int, Is...>) {
    (([&] {
         void** block = static_cast<void**>(tpw::ArenaAllocZeroed(8));
         *block = reinterpret_cast<void*>(&PanelSlot<Is>);
         gPanelBlock[Is] = reinterpret_cast<uintptr_t>(block);
     }()),
     ...);
}

int64_t FnId(const void* fn, const void* known) { return fn == nullptr ? 0 : (fn == known ? 1 : -1); }

}  // namespace

// ---- the fakes
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {I(id)}, {});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(ret("GetString")));
}
InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2, dlgcallback callback2,
                              dlgbutton button3, dlgcallback callback3, bool flag) {
    const void* known = reinterpret_cast<const void*>(&delme);
    ev("DialogBox",
       {Low32(params.mText), I(params.mRed), I(params.mGreen), I(params.mBlue), I(params.mValue8),
        LL(params.mValueC != 0 ? tpw::gArenaBase + static_cast<uint32_t>(params.mValueC) : nullptr), I(button1),
        FnId(reinterpret_cast<const void*>(callback1), known), I(button2), FnId(reinterpret_cast<const void*>(callback2), known),
        I(button3), FnId(reinterpret_cast<const void*>(callback3), known)},
       {});
    ev("DialogFlag", {I(flag)}, {});
    return nullptr;
}
void UI_SetSelectedObject(void* object) { ev("SetSelected", {LL(object)}, {}); }
void LLCM_SetCurrentObject(bool flag) { ev("SetCurrent", {I(flag)}, {}); }
void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("SetMode", {I(mode), I(flag)}, {}); }
void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) { ev("ButtonUp", {I(x), I(y), I(shift), I(flag)}, {}); }
int32_t UI_IterateThingList(UIIteratorTypes mask, int (*fn)(CThing*)) {
    ev("Iterate", {I(static_cast<int32_t>(mask)), FnId(reinterpret_cast<const void*>(fn), reinterpret_cast<const void*>(&StatusPanelIteratorCallback))}, {});
    gStatusPanelListCount = static_cast<int32_t>(ret("Iterate"));
    return 0;
}
CThing* UI_WrapIterateThingNext(UIIteratorTypes type, CThing* thing) {
    ev("WrapNext", {I(static_cast<int32_t>(type)), LL(thing)}, {});
    return static_cast<CThing*>(LabelAddr(ret("WrapNext")));
}
bool UIMain_CheckPanel() {
    ev("CheckPanel", {}, {});
    return false;
}
void UIMain_SetPanel(InterfaceWindow* window) { ev("SetPanel", {LL(window)}, {}); }
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {LL(this), Low32(description), Low32(reinterpret_cast<const void*>(callback))}, {});
    return static_cast<InterfaceWindow*>(LabelAddr(ret("Build")));
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {LL(this), I(id)}, {});
    return static_cast<InterfaceWindow*>(LabelAddr(ret("GetWindow")));
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallback", {LL(this), FnId(reinterpret_cast<const void*>(callback), reinterpret_cast<const void*>(&ViewWindowCallback))}, {});
}
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("ExtraParam", {LL(this), I(index), LL(value)}, {});
    return 0;
}
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {LL(this)}, {});
    return static_cast<RideInfo*>(LabelAddr(ret("GetDBInfo")));
}
int32_t LLCC_SystemConstructQueueClickStack(CObject* object) {
    ev("ConstructQ", {LL(object)}, {});
    return static_cast<int32_t>(ret("ConstructQ"));
}
void LLCC_SystemMakeClickStackCopy() { ev("MakeCopy", {}, {}); }
void LLCC_SystemSetRide(const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id, int32_t track) { ev("SetRide", {I(id.mIndex), I(track)}, {}); }
void LLCC_SetRideRotation(int32_t rotation, bool flag) { ev("Rotation", {I(rotation), I(flag)}, {}); }

CMMPathDraw::CMMPathDraw(CELL_MODES mode, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("PathDraw", {LL(this), I(static_cast<int32_t>(mode)), I(id.mIndex)}, {});
}
CMMPathDraw::~CMMPathDraw() {}
void CMMPathDraw::OnLeftClick(int32_t, int32_t) {}
void CMMPathDraw::OnLeftRelease(int32_t, int32_t) {}
void CMMPathDraw::OnRightClick(int32_t, int32_t) {}
void CMMPathDraw::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void CMMPathDraw::WhileLeftDown(int32_t, int32_t) {}
void CMMPathDraw::WhileRightDown(int32_t, int32_t) {}
int32_t CMMPathDraw::GetMode() { return 3; }
void CMMPathDraw::OnAssign() {}
CMMDefault::CMMDefault() { ev("Default", {LL(this)}, {}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("Assign", {LL(this), LL(mode)}, {}); }

extern "C" {

uint64_t tpw_ui_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gThing", &gStaffRoomPanelThing},       {"gCount", &gStatusPanelListCount},
        {"gMouse", &gMouse},                     {"gWord1E0", &gMouseWord1E0},
        {"gWord1E4", &gMouseWord1E4},            {"gDir", &gStatusPanelClickStackDir},
        {"gMoveObj", &gStatusPanelMoveObject},   {"gGame", &gGameWindow},
        {"gConfirm", &gPlayerSettings.mCurrent.mConfirmDeleteOn},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ui_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ui_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mMode));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF04));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF10));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF14));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF18));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    out[n++] = static_cast<int64_t>(sizeof(UIStatusPanelFields));
    out[n++] = static_cast<int64_t>(reinterpret_cast<const uint8_t*>(&gPlayerSettings.mCurrent.mConfirmDeleteOn) -
                                    reinterpret_cast<const uint8_t*>(&gPlayerSettings));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellY));
    return n;
}

void tpw_ui_begin() {
    gLabelCount = 0;
    gNewCount = 0;
    Calibrate();
    gPanelBlock[0] = 0;
}
void tpw_ui_label(int32_t label, uint64_t address) {
    if (gLabelCount < kMaxLabels) {
        gLabelAddr[gLabelCount] = address;
        gLabelId[gLabelCount++] = label;
    }
}
void tpw_ui_init_window(uint64_t address) { *reinterpret_cast<void**>(static_cast<uintptr_t>(address)) = &gWinVt[0]; }
uint64_t tpw_ui_slot_block(uint32_t offset) {
    // the blocks come from the arena (32-bit addresses), made once a case (the harness may clear the arena between cases)
    if (gPanelBlock[0] == 0) FillPanel(std::make_integer_sequence<int, kPanelSlots>{});
    return gPanelBlock[(offset - 8) / 4];
}
int32_t tpw_ui_new_count() { return gNewCount; }
uint64_t tpw_ui_new_ptr(int32_t i) { return gNewPtr[i]; }
uint64_t tpw_ui_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

static UIStatusPanel* P(uint64_t a) { return reinterpret_cast<UIStatusPanel*>(static_cast<uintptr_t>(a)); }

void tpw_ui_get_next(uint64_t panel) { P(panel)->GetNextObject(); }
int32_t tpw_ui_fill(uint64_t panel, uint64_t thing) {
    return P(panel)->FillObjectList(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing)));
}
void tpw_ui_erase(uint64_t panel) { P(panel)->EraseObject(); }
void tpw_ui_open(uint64_t panel, uint32_t description, uint32_t callback, int32_t a, int32_t b, int32_t c) {
    P(panel)->OpenWindow(reinterpret_cast<int16_t*>(static_cast<uintptr_t>(description)),
                         reinterpret_cast<InterfaceWindowCallback>(static_cast<uintptr_t>(callback)), a, b, c);
}
void tpw_ui_move(uint64_t panel) { P(panel)->Move(); }
// The label the log gave an address (the world's labels, then the objects the code made), -1 when it never named it.
int32_t tpw_ui_label_of(uint64_t address) {
    for (int i = 0; i < gLabelCount; ++i)
        if (gLabelAddr[i] == address) return gLabelId[i];
    for (int i = 0; i < gNewCount; ++i)
        if (gNewPtr[i] == address) return 100 + i;
    return -1;
}

}  // extern "C"

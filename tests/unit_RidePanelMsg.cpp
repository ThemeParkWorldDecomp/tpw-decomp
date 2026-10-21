// Test library for the ride list panel's message handler (unit v51005; libtpw_unit_RidePanelMsg.dylib, tests/test_RidePanelMsg.py), main app:
//   src/game/ui/ui_ridepanel.cpp   RidePanelMsg
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The windows
// are static blocks whose first word is a fake vtable (slot 10 is SetText); the globals of other files the unit reads are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "Mouse.h"
#include "ObjectControl.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_controls.h"
#include "ui_iterators.h"
#include "ui_main.h"

using namespace calllog;

// gText is raw storage here under the program's symbol (its constructors are not in this library); only the table slot is used.
alignas(16) uint8_t gTextStorage[0x800] __asm__("_gText");

// ---- globals of other files, defined here
int32_t gRideListType = 0;
InterfaceWindow* gRideListPanel = nullptr;
int32_t gRideListSortColumn = 1;
int32_t gRideListF90 = 0;
int16_t gRideListF8C = 0;
int16_t gRideListF8A = 0;
uint16_t gRideListPending = 0;
uint16_t gRideListHelpShown = 0;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouse* gMouse;
CWorld* gGameWorld;
CRideDatabase gRideDatabase;
char16_t gUIMainConvertBuffer[0x4000];

namespace {

struct Win {
    void* vptr;
    uint8_t pad[0x140];
};
alignas(16) Win gWinA, gWinB, gWinPanel;
alignas(16) uint8_t gWorldBlock[64];
alignas(16) uint8_t gBankBlock[64];
alignas(16) uint8_t gCtlBlock[64];
alignas(16) uint8_t gRideBlock[0x1000];
alignas(16) uint8_t gTableBlock[64];
CMouse gMouseBlock;
void* gFakeVt[16];

int32_t FakeSetText(InterfaceWindow* self, char16_t* text) {
    ev("SetText", {lab(self), text == gUIMainConvertBuffer ? 1 : 0});
    return 1;
}

void InitWindows() {
    if (gFakeVt[10] != nullptr) return;
    gFakeVt[10] = reinterpret_cast<void*>(&FakeSetText);
    gWinA.vptr = gFakeVt;
    gWinB.vptr = gFakeVt;
    gWinPanel.vptr = gFakeVt;
}

InterfaceWindow* WindowOf(int64_t k) {
    switch (k) {
    case 1: return reinterpret_cast<InterfaceWindow*>(&gWinA);
    case 2: return reinterpret_cast<InterfaceWindow*>(&gWinB);
    case 3: return reinterpret_cast<InterfaceWindow*>(&gWinPanel);
    default: return nullptr;
    }
}

int64_t WorldOffset(const void* p) {
    return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(gWorldBlock)));
}

}  // namespace

// ---- the fakes
void InitInfoWindow() { ev("InitInfoWindow", {}); }
void ChangeInfoWindow(UIObjType type) { ev("ChangeInfoWindow", {static_cast<int32_t>(type)}); }
void RemoveRideListPanel() { ev("RemoveRideListPanel", {}); }
void UIMain_DestroyRidePreview() { ev("UIMain_DestroyRidePreview", {}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage m) { ev("UIMain_CancelAdvisorMessage", {static_cast<int32_t>(m)}); }
void UIMain_AdvisorMessage(AdvisorMessage m) { ev("UIMain_AdvisorMessage", {static_cast<int32_t>(m)}); }
void UIMain_NotEnoughDough() { ev("UIMain_NotEnoughDough", {}); }
void UIMain_TickRidePreview() { ev("UIMain_TickRidePreview", {}); }
int32_t UIMain_DefaultPanelMsg(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    ev("UIMain_DefaultPanelMsg", {lab(w), m, a, b});
    return static_cast<int32_t>(ret("UIMain_DefaultPanelMsg"));
}
void UpdateHelpText(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id) {
    ev("UpdateHelpText", {id.mIndex, reinterpret_cast<void*>(&id) == reinterpret_cast<void*>(&gRideListF8A) ? 1 : 0});
}
void UpdateInfoWindow(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id) {
    ev("UpdateInfoWindow", {id.mIndex, reinterpret_cast<void*>(&id) == reinterpret_cast<void*>(&gRideListF8A) ? 1 : 0});
}
void UpdateViewWindow(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id) {
    ev("UpdateViewWindow", {id.mIndex, reinterpret_cast<void*>(&id) == reinterpret_cast<void*>(&gRideListF8C) ? 1 : 0});
}
void CreateHireListPanel(StaffType t) { ev("CreateHireListPanel", {static_cast<int32_t>(t)}); }
uint32_t IveGotTicks() {
    ev("IveGotTicks", {});
    return static_cast<uint32_t>(ret("IveGotTicks"));
}
CThing* UI_IterateThingNext(UIIteratorTypes type, CThing* thing) {
    ev("UI_IterateThingNext", {static_cast<int32_t>(type), lab(thing)});
    return reinterpret_cast<CThing*>(static_cast<uintptr_t>(ret("UI_IterateThingNext")));
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return WindowOf(ret("GetWindow"));
}
int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(this), id, ms});
    return static_cast<int16_t>(ret("setAnimTimer"));
}
int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {lab(this), which});
    return 0;
}
void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }
int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(ret("GetData"));
    return 0;
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(gBankBlock);
}
int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("GetBalance"));
}
ObjectControl* CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag) {
    ev("GetObjectControl", {WorldOffset(this), type.mIndex, flag});
    return reinterpret_cast<ObjectControl*>(gCtlBlock);
}
bool CGoldenTicketControl::HasBeenBought(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    ev("HasBeenBought", {WorldOffset(this), type.mIndex});
    return ret("HasBeenBought") != 0;
}
bool CGoldenTicketControl::CanIAfford(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    ev("CanIAfford", {WorldOffset(this), type.mIndex});
    return ret("CanIAfford") != 0;
}
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return reinterpret_cast<RideInfo*>(gRideBlock);
}

// ---- the mouse
CMMPathDraw::CMMPathDraw(CELL_MODES mode) {
    ev("CMMPathDraw1", {newlab(this), static_cast<int32_t>(mode)});
}
CMMPathDraw::CMMPathDraw(CELL_MODES mode, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("CMMPathDraw2", {newlab(this), static_cast<int32_t>(mode), id.mIndex});
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
CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

// ---- the string table
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list ap;
    va_start(ap, id);
    const ShowStatusTag* tag = va_arg(ap, const ShowStatusTag*);
    va_end(ap);
    ev("FormatString", {lab(this), out == gUIMainConvertBuffer ? 1 : 0, static_cast<int64_t>(size), static_cast<int64_t>(id),
                        static_cast<int64_t>(tag->mMagic), static_cast<int64_t>(tag->mId), static_cast<int64_t>(tag->mValue),
                        static_cast<int64_t>(tag->mFlag)});
    return 0;
}

extern "C" {

uint64_t tpw_rpm_addr(const char* name) {
    InitWindows();
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", gWorldBlock},
        {"wa", &gWinA},
        {"wb", &gWinB},
        {"panel", &gWinPanel},
        {"bank", gBankBlock},
        {"ctl", gCtlBlock},
        {"ride", gRideBlock},
        {"table", gTableBlock},
        {"mouse", &gMouseBlock},
        {"db", &gRideDatabase},
        {"gGameWorld", &gGameWorld},
        {"tableslot", &reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[6]},
        {"buf", gUIMainConvertBuffer},
        {"f8a", &gRideListF8A},
        {"f8c", &gRideListF8C},
        {"f90", &gRideListF90},
        {"type", &gRideListType},
        {"panelptr", &gRideListPanel},
        {"sort", &gRideListSortColumn},
        {"pending", &gRideListPending},
        {"help", &gRideListHelpShown},
        {"w1e0", &gMouseWord1E0},
        {"w1e4", &gMouseWord1E4},
        {"gMouse", &gMouse},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rpm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rpm_layout(int64_t* out) {
    int32_t n = 0;
    RideInfo* ride = reinterpret_cast<RideInfo*>(gRideBlock);
    out[n++] = reinterpret_cast<uint8_t*>(&ride->mBalance.mValues[kRideSlot_GoldenTicketCost]) - gRideBlock;
    out[n++] = reinterpret_cast<uint8_t*>(&ride->mBalance.mValues[kRideSlot_WhichUIType]) - gRideBlock;
    out[n++] = static_cast<int64_t>(offsetof(ObjectControl, mUpgradeCost));
    return n;
}

uint32_t tpw_rpm_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    InitWindows();
    return static_cast<uint32_t>(RidePanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}

}  // extern "C"

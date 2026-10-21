// Test library for the staff status panel (unit v5107; libtpw_unit_StaffStatusPanel_FireMe.dylib, tests/test_StaffStatusPanel_FireMe.py), main app:
//   src/StaffStatusPanel_FireMe.cpp   FireMe, Show, InitWindow, UpdateViewWindow
//
// Every callee outside the unit is a fake that records what the real one reads; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "AdvisorScoring.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "TPSettings.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
CPlayerSettings gPlayerSettings;
CThing* gStaffRoomPanelThing;
CThing* gStaffStatusThing;

void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Low32(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) & 0xffffffffu); }

char16_t gText[2] = {u'x', 0};
InterfaceWindow* gChild1;
InterfaceWindow* gChild2;

template <int K>
void Virt(StaffStatusPanel* panel) {
    ev("Virt", {lab(panel), K});
}

}  // namespace

// ---- the fakes
int32_t fireme(void*) { return 0; }
int32_t StaffStatusMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t StaffStatusViewWindowCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return gText;
}

InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2, dlgcallback callback2,
                              dlgbutton button3, dlgcallback callback3, bool flag) {
    ev("DialogBox", {params.mText == gText ? 1 : 0, params.mRed, params.mGreen, params.mBlue, params.mValue8,
                     static_cast<int64_t>(static_cast<uint32_t>(params.mValueC)), button1, callback1 == nullptr ? 1 : 0, button2,
                     callback2 == fireme ? 1 : 0, button3, (callback3 == nullptr ? 1 : 0) | (flag ? 2 : 0)});
    return nullptr;
}

void CStaff::SackMe() { ev("SackMe", {Low32(this)}); }

void UIMain_AdvisorMessage(AdvisorMessage message) { ev("Advisor", {static_cast<int64_t>(message)}); }

// The base class's members (not part of this unit): defined so that the vtables link; none is called.
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
CoolRect::CoolRect() { ev("CoolRectCtor", {0}); }
CoolRect::~CoolRect() { ev("CoolRectDtor", {-1}); }

int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {lab(this), lab(rect)});
    rect->mLeft = static_cast<int16_t>(ret("rl"));
    rect->mTop = static_cast<int16_t>(ret("rt"));
    rect->mRight = static_cast<int16_t>(ret("rr"));
    rect->mBottom = static_cast<int16_t>(ret("rb"));
    return 1;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    int64_t k = ret("GetWindow");
    return k == 1 ? gChild1 : (k == 2 ? gChild2 : nullptr);
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 1;
}

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallback", {lab(this), callback == StaffStatusViewWindowCallback ? 1 : 0});
}

void UIMain_ShowSpritePreview(CoolRect& rect, CPeep* peep, int32_t z, float scale) {
    ev("Preview", {lab(&rect), rect.mLeft, rect.mTop, rect.mRight, rect.mBottom, Low32(peep), z, CanonBits(scale)});
}

int32_t UIStatusPanel::FillObjectList(CThing* thing) {
    ev("Fill", {lab(this), Low32(thing)});
    return static_cast<int32_t>(ret("Fill"));
}

void UIStatusPanel::InitEditBox(int32_t id) { ev("InitEdit", {lab(this), id}); }

void UIStatusPanel::OpenWindow(int16_t* description, InterfaceWindowCallback callback, int32_t windowA, int32_t windowB,
                               int32_t windowC) {
    ev("OpenWindow", {lab(this), description == gStaffStatusScript ? 1 : 0, callback == StaffStatusMsg ? 1 : 0, windowA, windowB,
                      windowC});
}

extern "C" {

uint64_t tpw_ssp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"confirm", &gPlayerSettings.mCurrent.mConfirmDeleteOn},
        {"panelThing", &gStaffRoomPanelThing},
        {"statusThing", &gStaffStatusThing},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ssp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset of a block (what a Ptr32 field holds).
uint32_t tpw_ssp_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

// The function an entry block holds for table slot k (12 to 14).
uint64_t tpw_ssp_vfn(int32_t k) {
    void (*fn)(StaffStatusPanel*) = k == 12 ? Virt<12> : (k == 13 ? Virt<13> : Virt<14>);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn));
}

void tpw_ssp_setwins(uint64_t a, uint64_t b) {
    gChild1 = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(a));
    gChild2 = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(b));
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
int32_t tpw_ssp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mLeft));
    out[n++] = static_cast<int64_t>(offsetof(dlgboxparams, mValueC));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    return n;
}
#pragma clang diagnostic pop

void tpw_ssp_fire(uint64_t panel) { reinterpret_cast<StaffStatusPanel*>(static_cast<uintptr_t>(panel))->FireMe(); }
void tpw_ssp_show(uint64_t panel, uint32_t thing) {
    reinterpret_cast<StaffStatusPanel*>(static_cast<uintptr_t>(panel))->Show(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing)));
}
void tpw_ssp_init(uint64_t panel) { reinterpret_cast<StaffStatusPanel*>(static_cast<uintptr_t>(panel))->InitWindow(); }
void tpw_ssp_update(uint64_t panel) { reinterpret_cast<StaffStatusPanel*>(static_cast<uintptr_t>(panel))->UpdateViewWindow(); }

}  // extern "C"

// Test library for the status panel's handlers (unit v5910; libtpw_unit_DefaultStatusPanelMsg.dylib, tests/test_DefaultStatusPanelMsg.py), main app:
//   src/ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_ViewWindowCallback_DefaultStatusPanelMsg.cpp
//   UIStatusPanel::DisplayStatusText, DefaultStatusPanelMsg, UIStatusPanel::InitEditBox, TwoEditsMsgStatus (the Mac's TwoEditsMsg at 0x10193fac)
//
// Every callee outside the unit is a recording fake with the event name and arguments of the PowerPC stub. The objects (window, thing,
// panel, two more windows) are blocks the test allocates; the fakes answer with them.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
CThing* gStaffRoomPanelThing;
char16_t gEditSignBufA[15];
char16_t gEditSignBufB[15];

namespace {

void* gObj;
void* gPanel;
void* gWinA;
void* gWinB;

int64_t Ident(const void* p) { return p == gEditSignBufA ? 1 : p == gEditSignBufB ? 2 : 0; }

template <int Slot>
void SlotFake(void* self) {
    ev("slot", {Slot, lab(self)});
}

}  // namespace

// ---- the fakes
void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {lab(this), index});
    return index == 0 ? gObj : (index == 1 || index == 6) ? gPanel : nullptr;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(gWinB);
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), callback == StatusPanelTitleMsg ? 1 : 0});
}
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, lab(value)});
    return 0;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("default"));
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return 0;
}
void UIMain_EnableAllKeyTables() { ev("EnableAllKeyTables", {}); }
void UIMain_DecideKbCapture() { ev("DecideKbCapture", {}); }
void UIStatusPanel::EditNamePanelGone() { ev("EditNamePanelGone", {lab(this)}); }
void UIStatusPanel::GetNextObject() { ev("GetNextObject", {lab(this)}); }
void UIStatusPanel::GetPreviousObject() { ev("GetPreviousObject", {lab(this)}); }
void CObject::SetSignNameA(const char16_t* text) { ev("SetSignNameA", {lab(this), Ident(text)}); }
void CObject::SetSignNameB(const char16_t* text) { ev("SetSignNameB", {lab(this), Ident(text)}); }
void RegenerateRideSign(void* object, const char16_t* signA, const char16_t* signB) {
    ev("RegenerateRideSign", {lab(object), Ident(signA), Ident(signB)});
}
int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UIMain_DefaultPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("dpm"));
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("UIMain_SetTitleText", {lab(window), id, text == gStatusPanelEmptyText ? 1 : 0});
}
void UIMain_SetObjectStateText(InterfaceWindow* window, int32_t state) { ev("UIMain_SetObjectStateText", {lab(window), state}); }
int32_t StatusPanelTitleMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

extern "C" {

uint64_t tpw_dsp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffRoomPanelThing},
        {"bufA", gEditSignBufA},
        {"bufB", gEditSignBufB},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_dsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_dsp_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

void tpw_dsp_setobjs(uint64_t obj, uint64_t panel, uint64_t wina, uint64_t winb) {
    gObj = reinterpret_cast<void*>(static_cast<uintptr_t>(obj));
    gPanel = reinterpret_cast<void*>(static_cast<uintptr_t>(panel));
    gWinA = reinterpret_cast<void*>(static_cast<uintptr_t>(wina));
    gWinB = reinterpret_cast<void*>(static_cast<uintptr_t>(winb));
}

uint64_t tpw_dsp_slotfn(int32_t slot) {
    void (*fn)(void*) = nullptr;
    switch (slot) {
        case 0x44: fn = SlotFake<0x44>; break;
        case 0x24: fn = SlotFake<0x24>; break;
        case 0x28: fn = SlotFake<0x28>; break;
        case 0x10: fn = SlotFake<0x10>; break;
        case 0xc: fn = SlotFake<0xc>; break;
    }
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn));
}

int32_t tpw_dsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF04));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF18));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    return n;
}

void tpw_dsp_display(uint64_t panel) { reinterpret_cast<UIStatusPanel*>(static_cast<uintptr_t>(panel))->DisplayStatusText(); }
int32_t tpw_dsp_default(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return DefaultStatusPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), m, a, b);
}
void tpw_dsp_init(uint64_t panel, int32_t id) { reinterpret_cast<UIStatusPanel*>(static_cast<uintptr_t>(panel))->InitEditBox(id); }
int32_t tpw_dsp_two(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return TwoEditsMsgStatus(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), m, a, b);
}

}  // extern "C"

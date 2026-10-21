// Test library for the small members of the interface controls and panels (unit v51111; libtpw_unit_InterfaceKnob_GetValue.dylib,
// tests/test_InterfaceKnob_GetValue.py), main app:
//   src/game/ui/InterfaceBar.cpp, src/game/ui/InterfaceImage.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/InterfaceLightPanel.cpp, src/game/ui/InterfaceTextBox.cpp, src/game/ui/InterfaceVaryBox.cpp, src/game/ui/InterfaceWindow.cpp, src/game/ui/UI_Base.cpp, src/game/ui/ui_progressBar.cpp, src/game/ui/ui_upgradeStatus.cpp, src/game/ui/ui_yearEndPanel.cpp   the 22 functions of the unit
//
// The callees outside the unit are fakes that record what the real ones read: ChangeValue of the knob and of the vary box, the
// base destructor of the window, UIStatusPanel::CloseWindow, UI_SendMessage, InterfaceTimer::Stop and the window's default handler.
// The window's constructor and virtual members are empty fakes (the vtables of the classes whose destructor the unit defines refer
// to them); the test windows below override the virtuals the unit calls (WhatAmI, SetText). The globals of other files are
// defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files
InterfaceHelpText* gUIHelpText;
InterfaceWindow* gYearEndPanel;
UIKeyCapture gUIKeyCapture;
InterfaceWindow* gUIKeyboardWindow;
InterfaceWindow* gUIProgressBar;

// ---- the rectangle the window's members hold (the window's fake constructor builds them; nothing here runs in a test)
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

// ---- the window's constructor and virtual members (never run by a test, except the destructor and the overrides below)
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
InterfaceWindow::~InterfaceWindow() { ev("~InterfaceWindow", {lab(this), 0}); }
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

// ---- the controls' virtual members the vtables refer to (defined in files this library does not compile)
int32_t InterfaceKnob::WhatAmI() { return 0xb; }
int32_t InterfaceKnob::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceLightPanel::WhatAmI() { return 13; }
int32_t InterfaceLightPanel::SetValue(int32_t) { return 0; }
int32_t InterfaceVaryBox::WhatAmI() { return 12; }
InterfaceVaryBox::InterfaceVaryBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                   int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {}
InterfaceTextBox::~InterfaceTextBox() {}
int32_t InterfaceTextBox::WhatAmI() { return 10; }
void InterfaceTextBox::ClientDraw() {}
int32_t InterfaceTextBox::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceTextBox::SetText(char16_t*) { return 1; }

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
void InterfaceKnob::ChangeValue(int32_t value) {
    ev("KnobChange", {lab(this), value});
    mAngle = static_cast<int32_t>(ret("angle"));
}
int32_t InterfaceVaryBox::ChangeValue(int32_t value) {
    ev("VaryChange", {lab(this), value});
    return static_cast<int32_t>(ret("vc"));
}
void UIStatusPanel::CloseWindow() { ev("CloseWindow", {lab(this)}); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("SendMessage"));
}
void InterfaceTimer::Stop() { ev("TimerStop", {lab(this)}); }
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("Default", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("dflt"));
}

namespace {

// A vary box that answers its kind number from the case's queue (the virtual call in operator-).
struct TestVary : InterfaceVaryBox {
    TestVary() : InterfaceVaryBox(nullptr, 0, 0, 0, 0, 0, 0) {}
    int32_t WhatAmI() override {
        ev("WhatAmI", {lab(this)});
        return static_cast<int32_t>(ret("WhatAmI"));
    }
};

// A window that records the text it is given (the panel's UpdateWindow).
struct TestWindow : InterfaceWindow {
    TestWindow() : InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0) {}
    int32_t SetText(char16_t* text) override {
        ev("SetText", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(text))});
        return 1;
    }
};

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

// The panel's table at +0x20: at each byte offset the arena offset of a block whose first 8 bytes are the function.
void BuildPanelTable(UIStatusPanelFields* fields) {
    uint8_t* table = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(0x100));
    struct Entry {
        uint32_t offset;
        uint64_t (*fn)(void*);
    };
    const Entry entries[] = {{0x3c, Slot3c}, {0x20, Slot20}, {0x1c, Slot1c}};
    for (const Entry& entry : entries) {
        void** block = static_cast<void**>(tpw::ArenaAllocZeroed(16));
        *block = reinterpret_cast<void*>(entry.fn);
        *reinterpret_cast<uint32_t*>(table + entry.offset) = tpw::ArenaOffset(block);
    }
    fields->mVtable.set(reinterpret_cast<uint32_t*>(table));
}

template <class T>
T* P(int64_t value) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(value));
}
int64_t A(const void* pointer) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(pointer)); }

}  // namespace

extern "C" {

// A zeroed block (32-bit addressable like every Ptr32 the headers hold); every object the module uses is 0x400 bytes.
uint64_t tpw_ikgv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT); every host object it builds must fit its 0x400-byte block.
int32_t tpw_ikgv_layout(int64_t* out) {
    static_assert(sizeof(TestVary) <= 0x400 && sizeof(TestWindow) <= 0x400 && sizeof(InterfaceKnob) <= 0x400 &&
                      sizeof(InterfaceImage) <= 0x400 && sizeof(InterfaceLightPanel) <= 0x400 && sizeof(InterfaceBar) <= 0x400 &&
                      sizeof(InterfaceTextBox) <= 0x400,
                  "a host object outgrew the test blocks");
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    out[n++] = static_cast<int64_t>(sizeof(captureKeyList));
    return n;
}

// One entry for every function: `op` picks it, `a` holds its arguments (pointers as host addresses), `out` its results.
void tpw_ikgv_call(int32_t op, const int64_t* a, int64_t* out) {
    switch (op) {
        case 0: {   // InterfaceKnob::GetValue (a: object, value)
            auto* o = P<InterfaceKnob>(a[0]);
            o->mValue = static_cast<int32_t>(a[1]);
            out[0] = o->InterfaceKnob::GetValue();
            break;
        }
        case 1: {   // InterfaceImage::GetValue
            auto* o = P<InterfaceImage>(a[0]);
            o->mValue = static_cast<int32_t>(a[1]);
            out[0] = o->InterfaceImage::GetValue();
            break;
        }
        case 2: {   // InterfaceLightPanel::GetValue
            auto* o = P<InterfaceLightPanel>(a[0]);
            o->mValue = static_cast<int32_t>(a[1]);
            out[0] = o->InterfaceLightPanel::GetValue();
            break;
        }
        case 3: {   // InterfaceBar::GetValue
            auto* o = P<InterfaceBar>(a[0]);
            o->mValue = static_cast<int32_t>(a[1]);
            out[0] = o->InterfaceBar::GetValue();
            break;
        }
        case 4: {   // InterfaceVaryBox::GetValue
            auto* o = P<InterfaceVaryBox>(a[0]);
            o->mValue = static_cast<int32_t>(a[1]);
            out[0] = o->InterfaceVaryBox::GetValue();
            break;
        }
        case 5: {   // InterfaceKnob::SetValue (a: object, value, drag angle, angle)
            auto* o = P<InterfaceKnob>(a[0]);
            o->mDragAngle = static_cast<int32_t>(a[2]);
            o->mAngle = static_cast<int32_t>(a[3]);
            out[0] = o->InterfaceKnob::SetValue(static_cast<int32_t>(a[1]));
            out[1] = o->mDragAngle;
            out[2] = o->mAngle;
            break;
        }
        case 6:     // the destructors (a: object or 0, the hidden flag): the deleting half is the compiler's, so the wrapper
        case 7:     // records the delete the Mac's code calls when the flag (a short) is above 0
        case 8: {
            void* o = P<void>(a[0]);
            const int16_t flag = static_cast<int16_t>(a[1]);
            if (o != nullptr) {
                if (op == 6) static_cast<InterfaceKnob*>(o)->InterfaceKnob::~InterfaceKnob();
                if (op == 7) static_cast<InterfaceLightPanel*>(o)->InterfaceLightPanel::~InterfaceLightPanel();
                if (op == 8) static_cast<InterfaceVaryBox*>(o)->InterfaceVaryBox::~InterfaceVaryBox();
                if (flag > 0) ev("delete", {lab(o)});
            }
            out[0] = A(o) == a[0];
            break;
        }
        case 9: {   // InterfaceVaryBox::SetInc (a: object, increment, null flag, old increment)
            auto* o = P<InterfaceVaryBox>(a[0]);
            o->mIncrement = static_cast<int32_t>(a[3]);
            int32_t increment = static_cast<int32_t>(a[1]);
            o->InterfaceVaryBox::SetInc(a[2] != 0 ? nullptr : &increment);
            out[0] = o->mIncrement;
            break;
        }
        case 10: {  // InterfaceVaryBox::SetValue
            out[0] = P<InterfaceVaryBox>(a[0])->InterfaceVaryBox::SetValue(static_cast<int32_t>(a[1]));
            break;
        }
        case 11: {  // InterfaceVaryBox::operator- (a: this, other (may be the same), this value, other value)
            auto* x = new (P<void>(a[0])) TestVary;
            auto* y = a[0] == a[1] ? x : new (P<void>(a[1])) TestVary;
            x->mValue = static_cast<int32_t>(a[2]);
            y->mValue = static_cast<int32_t>(a[3]);
            out[0] = x->InterfaceVaryBox::operator-(*y);
            break;
        }
        case 12: {  // UpgradeStatusPanel::UpdateWindow (a: panel, window)
            auto* fields = P<UIStatusPanelFields>(a[0]);
            auto* window = new (P<void>(a[1])) TestWindow;
            fields->mF1C = tpw::ArenaOffset(window);
            BuildPanelTable(fields);
            P<UpgradeStatusPanel>(a[0])->UpdateWindow();
            break;
        }
        case 13:
            P<UpgradeStatusPanel>(a[0])->Remove();
            break;
        case 14:
            P<UpgradeStatusPanel>(a[0])->DeInitWindow();
            break;
        case 15:
            P<UpgradeStatusPanel>(a[0])->DeInitStatusWindow();
            break;
        case 16:
            P<InterfaceTextBox>(a[0])->InterfaceTextBox::DrawFunction();
            break;
        case 17:    // RemoveYearEndPanel (a: the panel or 0)
            gYearEndPanel = P<InterfaceWindow>(a[0]);
            RemoveYearEndPanel();
            out[0] = A(gYearEndPanel);
            break;
        case 18:    // UI_HelpTextEnabled (a: the handler, its enabled halfword)
            gUIHelpText = P<InterfaceHelpText>(a[0]);
            gUIHelpText->mEnabled = static_cast<int16_t>(a[1]);
            out[0] = UI_HelpTextEnabled();
            break;
        case 19:    // InterfaceWindow::CaptureKeys (a: window, keys, keyboard window, old keys, old window)
            gUIKeyboardWindow = P<InterfaceWindow>(a[2]);
            gUIKeyCapture.mKeys = P<captureKeyList>(a[3]);
            gUIKeyCapture.mWindow = P<InterfaceWindow>(a[4]);
            out[0] = P<InterfaceWindow>(a[0])->CaptureKeys(P<captureKeyList>(a[1]));
            out[1] = A(gUIKeyCapture.mKeys);
            out[2] = A(gUIKeyCapture.mWindow);
            break;
        case 20:    // ProgBarMsg (a: window, message, a, b, the progress bar)
            gUIProgressBar = P<InterfaceWindow>(a[4]);
            out[0] = ProgBarMsg(P<InterfaceWindow>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]),
                                static_cast<int32_t>(a[3]));
            out[1] = A(gUIProgressBar);
            break;
        case 21: {  // InterfaceWindow::stopAnimTimer (a: window, the four timers, id)
            auto* w = P<InterfaceWindow>(a[0]);
            for (int i = 0; i < 4; ++i) w->mAnimTimers[i] = P<InterfaceTimer>(a[1 + i]);
            out[0] = w->stopAnimTimer(static_cast<int32_t>(a[5]));
            break;
        }
        default:
            break;
    }
}

}  // extern "C"

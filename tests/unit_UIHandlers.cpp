// Test library for the default window handlers of five controls and their leaves (libtpw_unit_UIHandlers.dylib; see
// tests/test_UIHandlers.py): src/ui_handlers_small.cpp (UI_DefaultButtonCallback, UI_DefaultCheckButtonsCallback,
// UI_DefaultKnobCallback, UI_DefaultTextBoxCallback, UI_DefaultVaryBoxCallback), src/InterfaceWindow_focus.cpp (CheckFocus,
// CaptureDevice, UnCaptureDevice, cursorChange, GetID, GetParent), src/InterfaceKnob.cpp (DoKnobMove, ChangeAngle) and
// src/InterfaceVaryBox.cpp (DoVaryBoxButton), unit w37a. Under them, real: the button (src/InterfaceButton.cpp: the handler
// reaches CalcFrame, DoTextColour, SetState, HiLight, StopHeldTimer and WhatAmI), the controls' constructors
// (src/ui_controls_create.cpp, which build the test windows), the window's constructor (src/InterfaceWindow.cpp), GetWindow
// (src/InterfaceWindow_show.cpp) and the rectangles (src/UI_Base.cpp).
//
// Everything else the unit reaches is a fake that records through calllog: the message poster, the pointer update, the cursor,
// the UI clock (IveGotTicks, which answers a clock the test sets and moves on by a step each call), the window's default handler,
// GetAttr and GetChild, the help text handler's four members, the draw objects (Change records the fields its flags name), the
// window's own SetValue, the vary box's virtual SetValue and GetValue, and the untranslated members the controls' vtables need.
//
// The Python module drives this library through extern "C" operations on a world of SLOTS windows (a plain window, a button,
// check buttons, a text box, a knob or a vary box, all made by their constructors), three draw objects and the UI globals the
// handlers use. tpw_uih_state walks the windows reachable from the slots and writes each one's fields as (Mac offset, value)
// pairs, which the Python module reads from the original's memory by the same table.
#include <cstdint>
#include <cstring>
#include <new>
#include <typeinfo>

#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "calllog.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"

using namespace calllog;

namespace {
constexpr int kSlots = 12;
constexpr int kDraws = 3;
constexpr int kMaxWindows = 40;
constexpr int kWindowLabel = 10, kDrawLabel = 40, kHelpLabel = 50;   // (the windows take 10 to 21)
}  // namespace

// ---- the UI's globals the unit defines (the others live in files that are not linked)
bool gUIHebrew;
int32_t gUIAutoRedraw;
InterfaceHelpText* gUIHelpText;
InterfaceWindow* gUIFocusWindow;
InterfaceWindow* gUICaptureDevice;
UIButtonState gUIButtons[3];

namespace {
uint32_t gClock;       // what IveGotTicks answers next
uint32_t gClockStep;   // how far each call moves it on
}  // namespace

// ---- the senders, the pointer, the cursor and the clock
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("PostMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("PostMessage"));
}
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn");
    return 1;
}
void UI_SetCursor(int16_t cursor) { ev("SetCursor", {cursor}); }
uint32_t IveGotTicks() {
    const uint32_t now = gClock;
    gClock += gClockStep;
    ev("IveGotTicks", {now});
    return now;
}

// ---- the window's default handler (the handlers under test end in it)
int32_t UI_DefaultWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultHandler", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultHandler"));
}

// ---- the handlers of the controls whose own units are not this one: stored by the constructors, never called
int32_t UI_DefaultEditBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultListBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultListControl2Callback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace {
// Which handler a pointer is, as the Python module's table numbers them: 0 none, 1 the window's default, 2 the button's, 3
// check buttons, 4 edit box, 5 knob, 6 list box, 7 list control, 8 text box, 9 vary box, 99 any other.
int32_t CallbackCode(InterfaceWindowCallback callback) {
    if (callback == nullptr) return 0;
    if (callback == UI_DefaultWindowCallback) return 1;
    if (callback == UI_DefaultButtonCallback) return 2;
    if (callback == UI_DefaultCheckButtonsCallback) return 3;
    if (callback == UI_DefaultEditBoxCallback) return 4;
    if (callback == UI_DefaultKnobCallback) return 5;
    if (callback == UI_DefaultListBoxCallback) return 6;
    if (callback == UI_DefaultListControl2Callback) return 7;
    if (callback == UI_DefaultTextBoxCallback) return 8;
    if (callback == UI_DefaultVaryBoxCallback) return 9;
    return 99;
}
}  // namespace

// ---- the timers and the string heap (the edit box's constructor and the button's destructor; never reached)
InterfaceTimer::InterfaceTimer() {}
InterfaceTimer::~InterfaceTimer() {}
InterfaceTimer* UI_AddTimer(InterfaceWindow*, int32_t, int32_t) { return nullptr; }
void InterfaceStringHeap::ReleaseString(char16_t*) {}

// ---- the window's untranslated members. The accessors the handlers call record themselves, with the answer the original gives.
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {
    ev("CalcClientArea", {lab(this)});
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
// The vary box calls this one directly, not through the vtable.
int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("Window.SetValue", {lab(this), value});
    return 1;
}
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }
InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return mChild;
}
uint32_t InterfaceWindow::GetAttr() {
    ev("GetAttr", {lab(this)});
    return mAttributes;
}
// (the constructors of the edit box, the knob, the text box and the vary box store their handlers through these)
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), CallbackCode(callback)});
    if (callback != nullptr) mCallback = callback;
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
// (GetWindow and Show are the real ones; Show reaches this for a window it moves, which no operation does)
int32_t InterfaceWindow::SetPosition(CoolRect*) { return 1; }

// ---- the scroll bar (unit w30b's), as the list box and the list control constructors make one
InterfaceScrollBar::InterfaceScrollBar(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                       int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {}
void InterfaceScrollBar::SetMinMax(int32_t, int32_t) {}
void InterfaceScrollBar::DrawFunction() {}
void InterfaceScrollBar::CalcClientArea() { InterfaceWindow::CalcClientArea(); }
InterfaceScrollBar::~InterfaceScrollBar() {}
int32_t InterfaceScrollBar::WhatAmI() { return 3; }
void InterfaceScrollBar::ClientDraw() {}
int16_t InterfaceScrollBar::Enable(int16_t on) { return InterfaceWindow::Enable(on); }
int32_t InterfaceScrollBar::SetValue(int32_t value) { return InterfaceWindow::SetValue(value); }
int32_t InterfaceScrollBar::GetValue() { return mValue; }

// ---- the controls' untranslated virtual members (the answers are the originals': the originals run on the PowerPC side)
void InterfaceBar::DrawFunction() {}
InterfaceBar::~InterfaceBar() {}
int32_t InterfaceBar::WhatAmI() { return 9; }
int32_t InterfaceBar::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceBar::SetValue(int32_t) { return 1; }
int32_t InterfaceBar::GetValue() { return mValue; }

InterfaceCheckButtons::~InterfaceCheckButtons() {}
int32_t InterfaceCheckButtons::WhatAmI() { return 8; }

void InterfaceEditBox::DrawFunction() {}
void InterfaceEditBox::CalcClientArea() {
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
}
InterfaceEditBox::~InterfaceEditBox() {}
int32_t InterfaceEditBox::WhatAmI() { return 5; }
void InterfaceEditBox::ClientDraw() {}
int32_t InterfaceEditBox::SetText(char16_t*) { return 1; }

InterfaceImage::~InterfaceImage() {}
int32_t InterfaceImage::WhatAmI() { return 6; }
int32_t InterfaceImage::SetValue(int32_t) { return 1; }
int32_t InterfaceImage::GetValue() { return mValue; }

InterfaceKnob::~InterfaceKnob() {}
int32_t InterfaceKnob::WhatAmI() { return 0xb; }
int32_t InterfaceKnob::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceKnob::SetValue(int32_t) { return 1; }
int32_t InterfaceKnob::GetValue() { return mValue; }

InterfaceLightPanel::~InterfaceLightPanel() {}
int32_t InterfaceLightPanel::WhatAmI() { return 0xd; }
int32_t InterfaceLightPanel::SetValue(int32_t) { return 1; }
int32_t InterfaceLightPanel::GetValue() { return mValue; }

void InterfaceListBox::DrawFunction() {}
void InterfaceListBox::CalcClientArea() { InterfaceWindow::CalcClientArea(); }
InterfaceListBox::~InterfaceListBox() {}
int32_t InterfaceListBox::WhatAmI() { return 4; }
void InterfaceListBox::ClientDraw() {}

void InterfaceListControl2::CalcClientArea() { InterfaceWindow::CalcClientArea(); }
InterfaceListControl2::~InterfaceListControl2() {}
int32_t InterfaceListControl2::WhatAmI() { return 7; }
void InterfaceListControl2::ClientDraw() {}

void InterfaceTextBox::DrawFunction() {}
InterfaceTextBox::~InterfaceTextBox() {}
int32_t InterfaceTextBox::WhatAmI() { return 0xa; }
void InterfaceTextBox::ClientDraw() {}
int32_t InterfaceTextBox::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceTextBox::SetText(char16_t*) { return 1; }

// The vary box's virtual SetValue and GetValue are what the wheel handler calls: they record, and answer from the queues.
InterfaceVaryBox::~InterfaceVaryBox() {}
int32_t InterfaceVaryBox::WhatAmI() { return 0xc; }
int32_t InterfaceVaryBox::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceVaryBox::SetValue(int32_t value) {
    ev("VaryBox.SetValue", {lab(this), value});
    return static_cast<int32_t>(ret("VaryBox.SetValue"));
}
int32_t InterfaceVaryBox::GetValue() {
    ev("VaryBox.GetValue", {lab(this)});
    return static_cast<int32_t>(ret("VaryBox.GetValue"));
}

// ---- the help text handler: the four members the handlers call record themselves, the rest are never called
InterfaceHelpText::InterfaceHelpText() {}
void InterfaceHelpText::Init() {}
void InterfaceHelpText::DeInit() {}
void InterfaceHelpText::ShowHelpText() {}
void InterfaceHelpText::HideHelpText() {}
InterfaceHelpText::~InterfaceHelpText() {}
void InterfaceHelpText::Enable() {}
void InterfaceHelpText::Disable() {}
void InterfaceHelpText::Show() {}
void InterfaceHelpText::Hide() {}
void InterfaceHelpText::Update() {}
void InterfaceHelpText::OnMouseMoved(int16_t, int16_t) {}
void InterfaceHelpText::OnMouseMovedIn(InterfaceWindow*) {}
void InterfaceHelpText::OnMouseMovedOut() {}
void InterfaceHelpText::OnMouseDown() {}
void InterfaceHelpText::OnFrame() {}
void InterfaceHelpText::OnHideOrDelete(InterfaceWindow*) {}

namespace {
class FakeHelp : public InterfaceHelpText {
public:
    void Update() override { ev("Help.Update", {lab(this)}); }
    void OnMouseMoved(int16_t x, int16_t y) override { ev("Help.OnMouseMoved", {lab(this), x, y}); }
    void OnMouseMovedIn(InterfaceWindow* window) override { ev("Help.OnMouseMovedIn", {lab(this), lab(window)}); }
    void OnMouseMovedOut() override { ev("Help.OnMouseMovedOut", {lab(this)}); }
};
FakeHelp gHelp;
}  // namespace

// ---- the draw objects: the base class's members are never used, the fake records Change. A change with flag 1 records the frame,
// flag 4 the first short after the depth (the knob's angle) and the text style flags (0x4c000) the style's two offsets and its
// colour word (red in the high byte).
InterfaceDrawObject::InterfaceDrawObject(void*) {}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

namespace {
class FakeDraw : public InterfaceDrawObject {
public:
    FakeDraw() : InterfaceDrawObject(nullptr) {}
    using InterfaceDrawObject::Draw;
    void Change(uint32_t flags, DOChange* change) override {
        int64_t frame = 0, angle = 0, shiftX = 0, shiftY = 0, colour = 0;
        if (flags & 1) frame = change->mFrame;
        if (flags & 4) angle = change->mF08;
        if (flags & 0x4c000) {
            int16_t x, y;
            std::memcpy(&x, &change->mText->mF0A[0], sizeof x);
            std::memcpy(&y, &change->mText->mF0A[2], sizeof y);
            shiftX = x;
            shiftY = y;
            colour = (static_cast<uint32_t>(change->mText->mColour[0]) << 24) | (static_cast<uint32_t>(change->mText->mColour[1]) << 16) |
                     (static_cast<uint32_t>(change->mText->mColour[2]) << 8) | change->mText->mF13;
        }
        ev("Change", {lab(this), flags, frame, angle, shiftX, shiftY, colour});
    }
};
FakeDraw gDraws[kDraws];

InterfaceWindow* gWindows[kSlots];

int64_t Heap[4];

InterfaceWindow* Window(int64_t slot) { return slot >= 0 && slot < kSlots ? gWindows[slot] : nullptr; }
int64_t L(const void* object) { return lab(object); }

// ---- the kinds: the numbers CreateObject switches on (the unit makes 1, 2, 8, 10, 11 and 12). The host's object size and type.
std::size_t HostSize(int kind) {
    switch (kind) {
    case 1: return sizeof(InterfaceWindow);
    case 2: return sizeof(InterfaceButton);
    case 8: return sizeof(InterfaceCheckButtons);
    case 10: return sizeof(InterfaceTextBox);
    case 11: return sizeof(InterfaceKnob);
    case 12: return sizeof(InterfaceVaryBox);
    }
    return 0;
}

int KindOf(const InterfaceWindow* w) {
    const std::type_info& type = typeid(*w);
    if (type == typeid(InterfaceButton)) return 2;
    if (type == typeid(InterfaceCheckButtons)) return 8;
    if (type == typeid(InterfaceTextBox)) return 10;
    if (type == typeid(InterfaceKnob)) return 11;
    if (type == typeid(InterfaceVaryBox)) return 12;
    return 1;
}

// A constructor of the kind, into storage.
InterfaceWindow* Construct(int kind, void* block, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                           int16_t right, int16_t bottom) {
    switch (kind) {
    case 1: return new (block) InterfaceWindow(parent, attributes, id, left, top, right, bottom);
    case 2: return new (block) InterfaceButton(parent, attributes, id, left, top, right, bottom);
    case 8: return new (block) InterfaceCheckButtons(parent, attributes, id, left, top, right, bottom);
    case 10: return new (block) InterfaceTextBox(parent, attributes, id, left, top, right, bottom);
    case 11: return new (block) InterfaceKnob(parent, attributes, id, left, top, right, bottom);
    case 12: return new (block) InterfaceVaryBox(parent, attributes, id, left, top, right, bottom);
    }
    return nullptr;
}

// ---- the dumps: (Mac offset, value) pairs
struct Out {
    int64_t* p;
    int64_t n = 0;
    void key(int64_t offset, int64_t value) {
        *p++ = offset;
        *p++ = value;
        ++n;
    }
    // A CoolRect member: its vtable is right (always, on the host), then the four coordinates.
    void rect(int64_t base, const CoolRect& r) {
        key(base, 1);
        key(base + 4, r.mLeft);
        key(base + 6, r.mTop);
        key(base + 8, r.mRight);
        key(base + 0xa, r.mBottom);
    }
};

bool Untouched(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i)
        if (bytes[i] != 0xa5) return false;
    return true;
}
bool Zero(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i)
        if (bytes[i] != 0) return false;
    return true;
}
int64_t Colour(const UI_RGBA& c) {
    return (static_cast<int64_t>(c.mR) << 24) | (static_cast<int64_t>(c.mG) << 16) | (static_cast<int64_t>(c.mB) << 8) | c.mA;
}

void DumpBase(const InterfaceWindow* w, Out& o) {
    o.key(0x00, w->mDestroying);
    o.key(0x01, 1);   // (padding the Mac leaves untouched)
    o.rect(0x04, w->mRect);
    o.rect(0x10, w->mOriginalRect);
    o.rect(0x1c, w->mPreviousRect);
    o.rect(0x28, w->mClientRect);
    o.rect(0x34, w->mRect34);
    o.key(0x40, w->mRegion == nullptr ? 0 : w->mRegion == &w->mRect ? 1 : L(w->mRegion));
    o.key(0x44, static_cast<int32_t>(w->mAttributes));
    o.key(0x48, L(w->mFont));
    o.key(0x4c, w->mCursor);
    o.key(0x4e, 1);
    o.key(0x50, L(w->mNext));
    o.key(0x54, L(w->mPrevious));
    o.key(0x58, L(w->mChild));
    o.key(0x5c, L(w->mParent));
    o.key(0x60, L(w->mOwner));
    o.key(0x64, static_cast<int32_t>(w->mF64));
    o.key(0x68, Untouched(w->mF68, sizeof w->mF68));
    o.key(0x88, w->mId);
    o.key(0x8c, w->mHelpText);
    o.key(0x90, w->mF90);
    o.key(0x94, L(w->mDrawObject));
    o.key(0x98, L(w->mClientDrawObject));
    o.key(0x9c, Zero(w->mExtraParams, sizeof w->mExtraParams));
    o.key(0xdc, w->mZ);
    o.key(0xde, w->mZRot);
    o.key(0xe0, w->mPivotX);
    o.key(0xe2, w->mPivotY);
    o.key(0xe4, L(w->mAnim));
    o.key(0xe8, w->mAnimFrame);
    o.key(0xea, 1);
    o.key(0xec, w->mAnimTicks);
    o.key(0xf0, w->mAnimStart);
    for (int i = 0; i < 4; ++i) o.key(0xf4 + 2 * i, w->mAnimOffset[i]);
    o.key(0xfc, L(w->mSlideTimer));
    for (int i = 0; i < 4; ++i) o.key(0x100 + 2 * i, w->mSlideStep[i]);
    o.key(0x108, w->mSlideSteps);
    o.key(0x10a, Untouched(w->mF10A, sizeof w->mF10A));
    o.key(0x110, CallbackCode(w->mCallback));
    o.key(0x114, CallbackCode(w->mDefaultCallback));
    o.key(0x118, KindOf(w));
    o.key(0x11c, static_cast<int32_t>(w->mButtonsHeld));
    for (int i = 0; i < 4; ++i) o.key(0x120 + 4 * i, L(w->mAnimTimers[i]));
    o.key(0x130, static_cast<int32_t>(w->mF130));
    o.key(0x134, w->mF134);
    o.key(0x135, w->mAnimating);
    o.key(0x136, w->mSliding);
    o.key(0x137, 1);   // (the Mac's last padding byte has no member)
}

// The words the operations set or the handlers write are dumped as numbers (whatever they hold, including the 0xa5 fill); the
// padding as an "untouched" flag.
void DumpExtras(const InterfaceWindow* w, Out& o) {
    switch (KindOf(w)) {
    case 2: {
        const auto* b = static_cast<const InterfaceButton*>(w);
        o.key(0x138, b->mState);
        o.key(0x139, b->mHilight);
        o.key(0x13a, Untouched(b->mF13A, sizeof b->mF13A));
        o.key(0x13c, b->mRepeatDelay);
        o.key(0x140, b->mRepeatInterval);
        o.key(0x144, b->mHeldTick);
        o.key(0x148, L(b->mText));
        o.key(0x14c, Colour(b->mTextUpColour));
        o.key(0x150, Untouched(&b->mF150, sizeof b->mF150));
        o.key(0x154, Colour(b->mTextDownColour));
        o.key(0x158, b->mTextShiftX);
        o.key(0x159, b->mTextShiftY);
        o.key(0x15a, b->mHeldState);
        o.key(0x15b, Untouched(&b->mF15B, sizeof b->mF15B));
        break;
    }
    case 8:
        o.key(0x138, static_cast<const InterfaceCheckButtons*>(w)->mCurrent);
        break;
    case 10: {
        const auto* b = static_cast<const InterfaceTextBox*>(w);
        o.key(0x138, b->mText != nullptr);
        o.key(0x13c, b->mHorizontal);
        o.key(0x140, b->mVertical);
        o.key(0x144, b->mF144);
        o.key(0x148, b->mTextLength);
        o.key(0x14c, b->mTextX);
        o.key(0x14e, b->mTextY);
        break;
    }
    case 11: {
        const auto* b = static_cast<const InterfaceKnob*>(w);
        o.key(0x138, b->mMinAngle);
        o.key(0x13c, b->mMaxAngle);
        o.key(0x140, b->mAngleRange);
        o.key(0x144, b->mDragAngle);
        o.key(0x148, b->mAngle);
        o.key(0x14c, b->mMinValue);
        o.key(0x150, b->mMaxValue);
        o.key(0x154, b->mValueRange);
        o.key(0x158, b->mValue);
        o.key(0x15c, b->mOldValue);
        o.key(0x160, b->mLastY);
        o.key(0x162, b->mPreviousY);
        o.key(0x164, b->mDragging);
        o.key(0x166, Untouched(b->mF166, sizeof b->mF166));
        break;
    }
    case 12: {
        const auto* b = static_cast<const InterfaceVaryBox*>(w);
        o.key(0x138, b->mMinValue);
        o.key(0x13c, b->mMaxValue);
        o.key(0x140, b->mValue);
        o.key(0x144, b->mOldValue);
        o.key(0x148, b->mIncrement);
        o.key(0x14c, b->mRepeatCountdown);
        o.key(0x150, b->mRepeatStep);
        o.key(0x154, b->mForceUpdate);
        o.key(0x155, Untouched(b->mF155, sizeof b->mF155));
        break;
    }
    default:
        break;
    }
}

// The windows reachable from the slots: each slot's window, then its first child and the child's siblings, depth first.
int Discover(const InterfaceWindow** list) {
    int count = 0;
    auto visit = [&](auto&& self, const InterfaceWindow* w) -> void {
        if (w == nullptr || count >= kMaxWindows) return;
        for (int i = 0; i < count; ++i)
            if (list[i] == w) return;
        list[count++] = w;
        self(self, w->mChild);
        self(self, w->mNext);
    };
    for (int i = 0; i < kSlots; ++i) visit(visit, gWindows[i]);
    return count;
}

InterfaceDrawObject* Draw(int64_t index) { return index >= 0 && index < kDraws ? &gDraws[index] : nullptr; }
}  // namespace

extern "C" {

// Resets the world for a case: no windows, the globals as the Mac's data has them (zero), the clock stopped at 0.
void tpw_uih_reset() {
    for (auto& w : gWindows) w = nullptr;
    gUIHebrew = false;
    gUIAutoRedraw = 0;
    gUILastWindow = nullptr;
    gUIFocusWindow = nullptr;
    gUICaptureDevice = nullptr;
    gUIHelpText = &gHelp;
    std::memset(gUIButtons, 0, sizeof gUIButtons);
    gClock = 0;
    gClockStep = 0;
    std::memset(Heap, 0, sizeof Heap);
}

// Names the world's fixed objects for the call log (windows are named when made).
void tpw_uih_labels() {
    for (int i = 0; i < kDraws; ++i) tpw_cl_label(kDrawLabel + i, &gDraws[i]);
    tpw_cl_label(kHelpLabel, &gHelp);
}

// One operation of the script; a[] are its arguments, out[0] is the answer and out[1..4] more. The numbers are those of the
// Python module.
void tpw_uih_op(int32_t op, const int64_t* a, int64_t* out) {
    out[0] = out[1] = out[2] = out[3] = out[4] = 0;
    InterfaceWindow* w = Window(a[0]);
    int64_t before[4];
    for (int i = 0; i < 4; ++i) before[i] = tpw_cl_heap[i];
    switch (op) {
    case 1: {   // a control through its constructor: slot, kind, parent slot, attributes, id, left, top, right, bottom
        const int kind = static_cast<int>(a[1]);
        void* block = ::operator new(HostSize(kind));
        std::memset(block, 0xa5, HostSize(kind));
        tpw_cl_label(kWindowLabel + static_cast<int32_t>(a[0]), block);
        gWindows[a[0]] = Construct(kind, block, Window(a[2]), static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]),
                                   static_cast<int16_t>(a[5]), static_cast<int16_t>(a[6]), static_cast<int16_t>(a[7]),
                                   static_cast<int16_t>(a[8]));
        out[0] = L(gWindows[a[0]]);
        return;   // (the heap of the constructors is not counted)
    }
    case 2:   // links: slot, parent, previous, next, child
        w->mParent = Window(a[1]);
        w->mPrevious = Window(a[2]);
        w->mNext = Window(a[3]);
        w->mChild = Window(a[4]);
        return;
    case 3:   // fields: slot, attributes, cursor, buttons held, id, help text number
        w->mAttributes = static_cast<uint32_t>(a[1]);
        w->mCursor = static_cast<int16_t>(a[2]);
        w->mButtonsHeld = static_cast<uint32_t>(a[3]);
        w->mId = static_cast<int32_t>(a[4]);
        w->mHelpText = static_cast<int32_t>(a[5]);
        return;
    case 4:   // draw objects: slot, the window's (-1 none), the client's
        w->mDrawObject = Draw(a[1]);
        w->mClientDrawObject = Draw(a[2]);
        return;
    case 5: {   // a button's data: slot, state, highlight, held, delay, interval, held tick, up word, down word, shift x, shift y
        auto* button = static_cast<InterfaceButton*>(w);
        button->mState = static_cast<int8_t>(a[1]);
        button->mHilight = static_cast<int8_t>(a[2]);
        button->mHeldState = static_cast<uint8_t>(a[3]);
        button->mRepeatDelay = static_cast<int32_t>(a[4]);
        button->mRepeatInterval = static_cast<int32_t>(a[5]);
        button->mHeldTick = static_cast<uint32_t>(a[6]);
        button->mTextUpColour = {static_cast<uint8_t>(a[7] >> 24), static_cast<uint8_t>(a[7] >> 16), static_cast<uint8_t>(a[7] >> 8),
                                 static_cast<uint8_t>(a[7])};
        button->mTextDownColour = {static_cast<uint8_t>(a[8] >> 24), static_cast<uint8_t>(a[8] >> 16), static_cast<uint8_t>(a[8] >> 8),
                                   static_cast<uint8_t>(a[8])};
        button->mTextShiftX = static_cast<int8_t>(a[9]);
        button->mTextShiftY = static_cast<int8_t>(a[10]);
        return;
    }
    case 6: {   // a knob's data: slot, min angle, max angle, angle range, drag angle, angle, min value, max value, value range, value,
                // old value, last y, previous y, dragging
        auto* knob = static_cast<InterfaceKnob*>(w);
        knob->mMinAngle = static_cast<int32_t>(a[1]);
        knob->mMaxAngle = static_cast<int32_t>(a[2]);
        knob->mAngleRange = static_cast<int32_t>(a[3]);
        knob->mDragAngle = static_cast<int32_t>(a[4]);
        knob->mAngle = static_cast<int32_t>(a[5]);
        knob->mMinValue = static_cast<int32_t>(a[6]);
        knob->mMaxValue = static_cast<int32_t>(a[7]);
        knob->mValueRange = static_cast<int32_t>(a[8]);
        knob->mValue = static_cast<int32_t>(a[9]);
        knob->mOldValue = static_cast<int32_t>(a[10]);
        knob->mLastY = static_cast<int16_t>(a[11]);
        knob->mPreviousY = static_cast<int16_t>(a[12]);
        knob->mDragging = static_cast<int16_t>(a[13]);
        return;
    }
    case 7: {   // a vary box's data: slot, minimum, maximum, value, old value, increment, countdown, step, forced update
        auto* box = static_cast<InterfaceVaryBox*>(w);
        box->mMinValue = static_cast<int32_t>(a[1]);
        box->mMaxValue = static_cast<int32_t>(a[2]);
        box->mValue = static_cast<int32_t>(a[3]);
        box->mOldValue = static_cast<int32_t>(a[4]);
        box->mIncrement = static_cast<int32_t>(a[5]);
        box->mRepeatCountdown = static_cast<int32_t>(a[6]);
        box->mRepeatStep = static_cast<int32_t>(a[7]);
        box->mForceUpdate = static_cast<uint8_t>(a[8]);
        return;
    }
    case 8:   // check buttons: slot, the selection
        static_cast<InterfaceCheckButtons*>(w)->mCurrent = static_cast<int32_t>(a[1]);
        return;
    case 9:   // a global: which, value (1 auto redraw, 2 the focus window's slot or -1, 3 the capture's slot or -1, 4 the clock, 5 the step)
        switch (a[0]) {
        case 1: gUIAutoRedraw = static_cast<int32_t>(a[1]); break;
        case 2: gUIFocusWindow = Window(a[1]); break;
        case 3: gUICaptureDevice = Window(a[1]); break;
        case 4: gClock = static_cast<uint32_t>(a[1]); break;
        case 5: gClockStep = static_cast<uint32_t>(a[1]); break;
        }
        return;
    case 10:   // one of the pump's button states: index, state, x, y, tick
        gUIButtons[a[0]].mState = static_cast<int16_t>(a[1]);
        gUIButtons[a[0]].mX = static_cast<int16_t>(a[2]);
        gUIButtons[a[0]].mY = static_cast<int16_t>(a[3]);
        gUIButtons[a[0]].mTime = static_cast<int32_t>(a[4]);
        return;
    default:
        break;
    }
    // ---- the functions under test (op, slot in a[0])
    auto* check = static_cast<InterfaceCheckButtons*>(w);
    auto* knob = static_cast<InterfaceKnob*>(w);
    auto* box = static_cast<InterfaceVaryBox*>(w);
    switch (op) {
    case 20: out[0] = w->GetID(); break;
    case 21: out[0] = L(w->GetParent()); break;
    case 22: w->CheckFocus(); break;
    case 23: w->cursorChange(); break;
    case 24: out[0] = w->CaptureDevice(); break;
    case 25: out[0] = w->UnCaptureDevice(); break;
    case 30: out[0] = UI_DefaultButtonCallback(w, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3])); break;
    case 31: out[0] = UI_DefaultCheckButtonsCallback(check, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3])); break;
    case 32: out[0] = UI_DefaultKnobCallback(w, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3])); break;
    case 33: out[0] = UI_DefaultTextBoxCallback(w, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3])); break;
    case 34: out[0] = UI_DefaultVaryBoxCallback(w, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3])); break;
    case 40: knob->DoKnobMove(static_cast<int32_t>(a[1])); break;
    case 41: knob->ChangeAngle(static_cast<int32_t>(a[1])); break;
    case 42: box->DoVaryBoxButton(static_cast<int16_t>(a[1])); break;
    }
    for (int i = 0; i < 4; ++i) Heap[i] += tpw_cl_heap[i] - before[i];
}

// The state: the windows reachable from the slots, each as its label, the number of pairs and the pairs; answers the number
// of words written.
int32_t tpw_uih_state(int64_t* out) {
    const InterfaceWindow* list[kMaxWindows];
    const int count = Discover(list);
    int64_t* start = out;
    for (int i = 0; i < count; ++i) {
        int64_t* header = out;
        out += 2;
        Out o{out};
        DumpBase(list[i], o);
        DumpExtras(list[i], o);
        out = o.p;
        header[0] = L(list[i]);
        header[1] = o.n;
    }
    return static_cast<int32_t>(out - start);
}

// The globals: the auto redraw, the focus, the capture, the clock and its step, then the pump's three button states (state, x, y,
// tick).
void tpw_uih_globals(int64_t* out) {
    *out++ = gUIAutoRedraw;
    *out++ = L(gUIFocusWindow);
    *out++ = L(gUICaptureDevice);
    *out++ = gClock;
    *out++ = gClockStep;
    for (const UIButtonState& s : gUIButtons) {
        *out++ = s.mState;
        *out++ = s.mX;
        *out++ = s.mY;
        *out++ = s.mTime;
    }
}

void tpw_uih_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = Heap[i];
}

}  // extern "C"

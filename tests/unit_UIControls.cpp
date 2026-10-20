// Test library for the control factory, the control constructors and the button (libtpw_unit_UIControls.dylib; see
// tests/test_UIControls.py): src/ui_controls_create.cpp (InterfaceWindow::CreateObject and the constructors of the bar, check
// buttons, edit box, image, knob, light panel, list box, list control, text box and vary box), src/InterfaceButton.cpp (the
// button's constructor and 14 members), and under them the real window constructor (src/InterfaceWindow.cpp) and the real
// rectangles (src/UI_Base.cpp).
//
// Everything else the unit reaches is a fake that records through calllog: the message poster and the pointer update, the
// timer table's UI_AddTimer, the window's untranslated virtual members and its SetCallbackFunction, CaptureKb and Show, the
// string heap's ReleaseString, the scroll bar (its constructor, SetMinMax and, through the base class, Enable), the draw
// objects (Change records what it was given), the eight window handlers (never called) and the controls' untranslated virtual
// members (empty, except the three CalcClientArea overrides the constructors reach: they do what the originals do, so that
// the scroll bar's place is computed from the same rectangles).
//
// The memory of every object made with operator new while a function under test runs is filled with 0xa5 first and the
// object is given the next label, so a member the constructor does not set compares as untouched.
//
// The Python module drives this library through extern "C" operations on a world of SLOTS windows (any class), draw
// objects, timers and strings. tpw_ctl_state walks the windows reachable from the slots (children first, then siblings)
// and writes each one's fields as (Mac offset, value) pairs, which the Python module reads from the original's memory by
// the same table.
#include <cstdint>
#include <cstring>
#include <new>
#include <typeinfo>

namespace {
bool gObserve;
void ObserveAllocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#include "calllog.cpp"

#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"

using namespace calllog;

namespace {
constexpr int kSlots = 8;
constexpr int kDraws = 3;
constexpr int kTimers = 2;
constexpr int kStrings = 3;
constexpr int kMaxWindows = 40;
constexpr int kWindowLabel = 10, kDrawLabel = 20, kTimerLabel = 40, kStringLabel = 50;

void ObserveAllocation(std::size_t size, bool array, void* block) {
    if (!gObserve) return;
    std::memset(block, 0xa5, size);
    if (!array) newlab(block);
}
}  // namespace

// ---- the UI's globals the unit reads or defines (the others live in files that are not linked)
bool gUIHebrew;
int32_t gUIAutoRedraw;

// ---- the senders, the pointer and the timers
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("PostMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("PostMessage"));
}
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn");
    return 1;
}

namespace {
InterfaceTimer gTimers[kTimers];
}
InterfaceTimer::InterfaceTimer() {}
InterfaceTimer::~InterfaceTimer() {}
// The queue "AddTimer" says which timer comes back: 0 none, 1 and 2 the world's two.
InterfaceTimer* UI_AddTimer(InterfaceWindow* window, int32_t slice, int32_t id) {
    ev("AddTimer", {lab(window), slice, id});
    const int64_t k = ret("AddTimer");
    return k >= 1 && k <= kTimers ? &gTimers[k - 1] : nullptr;
}

// ---- the string heap
void InterfaceStringHeap::ReleaseString(char16_t* text) { ev("ReleaseString", {lab(text)}); }

// ---- the window handlers: never called, stored by the constructors
int32_t UI_DefaultWindowCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultButtonCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultCheckButtonsCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultEditBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultKnobCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultListBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultListControl2Callback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultTextBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultVaryBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

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

// ---- the window's untranslated members. The base CalcClientArea does what the original does (the window's constructor
// reaches it).
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {
    ev("CalcClientArea", {lab(this)});
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
}
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this)}); }
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t on) {
    ev("Window.Enable", {lab(this), on});
    return static_cast<int16_t>(ret("Window.Enable"));
}
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 1; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t* text) {
    ev("Window.SetText", {lab(this), lab(text)});
    return 1;
}
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Window.Show", {lab(this), on});
    return 1;
}
// (the original sets the handler when it is not NULL; the fakes of both sides do the same, so that the constructors' second
// store, to the default handler's word, is seen)
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), CallbackCode(callback)});
    if (callback != nullptr) mCallback = callback;
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }

// ---- the scroll bar (unit w30b's)
InterfaceScrollBar::InterfaceScrollBar(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                       int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    ev("ScrollBar.ctor", {lab(this), lab(parent), attributes, id, left, top, right, bottom});
}
void InterfaceScrollBar::SetMinMax(int32_t a, int32_t b) { ev("ScrollBar.SetMinMax", {lab(this), a, b}); }
// the scroll bar's virtual members (unit w30b's, translated in src/InterfaceScrollBar.cpp, which this library does not link). Each
// does what the window's fake above does, which is what the scroll bar had before its own class was declared (the Mac side of the
// test maps the scroll bar's Enable to the same "Window.Enable" event); they only give the vtable its definitions.
void InterfaceScrollBar::DrawFunction() { InterfaceWindow::DrawFunction(); }
void InterfaceScrollBar::CalcClientArea() { InterfaceWindow::CalcClientArea(); }
InterfaceScrollBar::~InterfaceScrollBar() {}
int32_t InterfaceScrollBar::WhatAmI() { return InterfaceWindow::WhatAmI(); }
void InterfaceScrollBar::ClientDraw() { InterfaceWindow::ClientDraw(); }
int16_t InterfaceScrollBar::Enable(int16_t on) { return InterfaceWindow::Enable(on); }
int32_t InterfaceScrollBar::SetValue(int32_t value) { return InterfaceWindow::SetValue(value); }
int32_t InterfaceScrollBar::GetValue() { return InterfaceWindow::GetValue(); }

// ---- the controls' untranslated virtual members
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
    ev("EditBox.CalcClientArea", {lab(this)});
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
// The client rectangle is the window's less 0x30 on the right, and the list rectangle is that inset by 4 on every side.
void InterfaceListBox::CalcClientArea() {
    ev("ListBox.CalcClientArea", {lab(this)});
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    mClientRect.mRight = static_cast<int16_t>(mClientRect.mRight - 0x30);
    mListRect.mLeft = mClientRect.mLeft;
    mListRect.mTop = mClientRect.mTop;
    mListRect.mRight = mClientRect.mRight;
    mListRect.mBottom = mClientRect.mBottom;
    mListRect.mLeft = static_cast<int16_t>(mListRect.mLeft + 4);
    mListRect.mRight = static_cast<int16_t>(mListRect.mRight - 4);
    mListRect.mTop = static_cast<int16_t>(mListRect.mTop + 4);
    mListRect.mBottom = static_cast<int16_t>(mListRect.mBottom - 4);
}
InterfaceListBox::~InterfaceListBox() {}
int32_t InterfaceListBox::WhatAmI() { return 4; }
void InterfaceListBox::ClientDraw() {}

// The client rectangle is the window's, and so is the list rectangle.
void InterfaceListControl2::CalcClientArea() {
    ev("ListControl2.CalcClientArea", {lab(this)});
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    mListRect.mLeft = mClientRect.mLeft;
    mListRect.mTop = mClientRect.mTop;
    mListRect.mRight = mClientRect.mRight;
    mListRect.mBottom = mClientRect.mBottom;
}
InterfaceListControl2::~InterfaceListControl2() {}
int32_t InterfaceListControl2::WhatAmI() { return 7; }
void InterfaceListControl2::ClientDraw() {}

void InterfaceTextBox::DrawFunction() {}
InterfaceTextBox::~InterfaceTextBox() {}
int32_t InterfaceTextBox::WhatAmI() { return 0xa; }
void InterfaceTextBox::ClientDraw() {}
int32_t InterfaceTextBox::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceTextBox::SetText(char16_t*) { return 1; }

InterfaceVaryBox::~InterfaceVaryBox() {}
int32_t InterfaceVaryBox::WhatAmI() { return 0xc; }
int32_t InterfaceVaryBox::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceVaryBox::SetValue(int32_t) { return 1; }
int32_t InterfaceVaryBox::GetValue() { return mValue; }

// ---- the draw objects: the base class's members are never used, the fake records Change. A change with the text style flags
// (0x4c000) records the style's two offsets and its colour word (red in the high byte).
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
        int64_t frame = 0, shiftX = 0, shiftY = 0, colour = 0;
        if (flags & 1) frame = change->mFrame;
        if (flags & 0x4c000) {
            int16_t x, y;
            std::memcpy(&x, &change->mText->mF0A[0], sizeof x);
            std::memcpy(&y, &change->mText->mF0A[2], sizeof y);
            shiftX = x;
            shiftY = y;
            colour = (static_cast<uint32_t>(change->mText->mColour[0]) << 24) | (static_cast<uint32_t>(change->mText->mColour[1]) << 16) |
                     (static_cast<uint32_t>(change->mText->mColour[2]) << 8) | change->mText->mF13;
        }
        ev("Change", {lab(this), flags, frame, shiftX, shiftY, colour});
    }
};
FakeDraw gDraws[kDraws];

char16_t gStrings[kStrings][4] = {{u'a', u'b', u'c', 0}, {u'x', 0, 0, 0}, {0, 0, 0, 0}};

InterfaceWindow* gWindows[kSlots];

int64_t Heap[4];

InterfaceWindow* Window(int64_t slot) { return slot >= 0 && slot < kSlots ? gWindows[slot] : nullptr; }
int64_t L(const void* object) { return lab(object); }
char16_t* String(int64_t index) { return index >= 0 && index < kStrings ? gStrings[index] : nullptr; }

// ---- the kinds: the numbers CreateObject switches on. The host's object size and dynamic type.
std::size_t HostSize(int kind) {
    switch (kind) {
    case 1: return sizeof(InterfaceWindow);
    case 2: return sizeof(InterfaceButton);
    case 3: return sizeof(InterfaceScrollBar);
    case 4: return sizeof(InterfaceListBox);
    case 5: return sizeof(InterfaceEditBox);
    case 6: return sizeof(InterfaceImage);
    case 7: return sizeof(InterfaceListControl2);
    case 8: return sizeof(InterfaceCheckButtons);
    case 9: return sizeof(InterfaceBar);
    case 10: return sizeof(InterfaceTextBox);
    case 11: return sizeof(InterfaceKnob);
    case 12: return sizeof(InterfaceVaryBox);
    case 13: return sizeof(InterfaceLightPanel);
    }
    return 0;
}

int KindOf(const InterfaceWindow* w) {
    const std::type_info& type = typeid(*w);
    if (type == typeid(InterfaceButton)) return 2;
    if (type == typeid(InterfaceScrollBar)) return 3;
    if (type == typeid(InterfaceListBox)) return 4;
    if (type == typeid(InterfaceEditBox)) return 5;
    if (type == typeid(InterfaceImage)) return 6;
    if (type == typeid(InterfaceListControl2)) return 7;
    if (type == typeid(InterfaceCheckButtons)) return 8;
    if (type == typeid(InterfaceBar)) return 9;
    if (type == typeid(InterfaceTextBox)) return 10;
    if (type == typeid(InterfaceKnob)) return 11;
    if (type == typeid(InterfaceVaryBox)) return 12;
    if (type == typeid(InterfaceLightPanel)) return 13;
    return 1;
}

// A constructor of the kind, into storage.
InterfaceWindow* Construct(int kind, void* block, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                           int16_t right, int16_t bottom) {
    switch (kind) {
    case 1: return new (block) InterfaceWindow(parent, attributes, id, left, top, right, bottom);
    case 2: return new (block) InterfaceButton(parent, attributes, id, left, top, right, bottom);
    case 3: return new (block) InterfaceScrollBar(parent, attributes, id, left, top, right, bottom);
    case 4: return new (block) InterfaceListBox(parent, attributes, id, left, top, right, bottom);
    case 5: return new (block) InterfaceEditBox(parent, attributes, id, left, top, right, bottom);
    case 6: return new (block) InterfaceImage(parent, attributes, id, left, top, right, bottom);
    case 7: return new (block) InterfaceListControl2(parent, attributes, id, left, top, right, bottom);
    case 8: return new (block) InterfaceCheckButtons(parent, attributes, id, left, top, right, bottom);
    case 9: return new (block) InterfaceBar(parent, attributes, id, left, top, right, bottom);
    case 10: return new (block) InterfaceTextBox(parent, attributes, id, left, top, right, bottom);
    case 11: return new (block) InterfaceKnob(parent, attributes, id, left, top, right, bottom);
    case 12: return new (block) InterfaceVaryBox(parent, attributes, id, left, top, right, bottom);
    case 13: return new (block) InterfaceLightPanel(parent, attributes, id, left, top, right, bottom);
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

void DumpExtras(const InterfaceWindow* w, Out& o) {
    switch (KindOf(w)) {
    case 2: {
        const auto* b = static_cast<const InterfaceButton*>(w);
        o.key(0x138, b->mState);
        o.key(0x139, b->mHilight);
        o.key(0x13a, Untouched(b->mF13A, sizeof b->mF13A));
        o.key(0x13c, b->mRepeatDelay);
        o.key(0x140, b->mRepeatInterval);
        o.key(0x144, Untouched(&b->mHeldTick, sizeof b->mHeldTick));
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
    case 3:
        o.key(0x138, 1);   // (the stand-in scroll bar has no members: the Mac's are all untouched)
        break;
    case 4: {
        const auto* b = static_cast<const InterfaceListBox*>(w);
        o.rect(0x138, b->mListRect);
        o.key(0x144, b->mF144);
        o.key(0x148, b->mF148);
        o.key(0x14c, b->mF14C);
        o.key(0x150, b->mF150);
        o.key(0x154, b->mF154);
        o.key(0x156, b->mF156);
        break;
    }
    case 5: {
        const auto* b = static_cast<const InterfaceEditBox*>(w);
        o.key(0x138, b->mBuffer != nullptr);
        o.key(0x2138, b->mBuffer != nullptr ? b->mBuffer[0] : 0);
        o.key(0x2139, b->mBuffer != nullptr ? Untouched(b->mBuffer + 1, 0x1fe) : 1);
        o.key(0x13c, b->mMaxLength);
        o.key(0x140, b->mF140);
        o.key(0x144, b->mF144);
        o.key(0x148, b->mF148);
        o.key(0x14a, b->mF14A);
        o.key(0x14c, b->mF14C);
        o.key(0x14e, b->mF14E);
        o.key(0x150, b->mF150);
        o.key(0x152, b->mF152);
        o.key(0x154, b->mF154);
        o.key(0x156, b->mF156);
        o.key(0x158, b->mF158);
        o.key(0x15a, Untouched(b->mF15A, sizeof b->mF15A));
        o.key(0x15c, b->mF15C);
        o.key(0x160, b->mF160);
        o.key(0x161, b->mF161);
        o.key(0x162, b->mRightToLeft);
        o.key(0x163, Untouched(&b->mF163, sizeof b->mF163));
        o.key(0x164, L(b->mTimer));
        break;
    }
    case 6: {
        const auto* b = static_cast<const InterfaceImage*>(w);
        o.key(0x138, Untouched(&b->mF138, sizeof b->mF138));
        o.key(0x13c, Untouched(&b->mValue, sizeof b->mValue));
        break;
    }
    case 7: {
        const auto* b = static_cast<const InterfaceListControl2*>(w);
        o.rect(0x138, b->mListRect);
        o.key(0x144, b->mF144);
        o.key(0x148, b->mF148);
        o.key(0x14c, b->mCurrentEntry);
        o.key(0x150, b->mItemCount);
        o.key(0x154, b->mTopEntry);
        o.key(0x158, b->mBlockWords);
        o.key(0x15c, Untouched(&b->mF15C, sizeof b->mF15C));
        o.key(0x160, b->mFirstEntry);
        o.key(0x164, b->mColumnCount);
        o.key(0x166, b->mF166);
        o.key(0x168, b->mSortColumn);
        o.key(0x16a, b->mSortDescending);
        o.key(0x16c, b->mColumns != nullptr);
        o.key(0x170, b->mRowHeight);
        o.key(0x172, b->mVisibleRows);
        o.key(0x174, b->mF174);
        break;
    }
    case 8:
        o.key(0x138, static_cast<const InterfaceCheckButtons*>(w)->mCurrent);
        break;
    case 9: {
        const auto* b = static_cast<const InterfaceBar*>(w);
        o.key(0x138, b->mF138);
        o.key(0x13c, b->mF13C);
        o.key(0x140, b->mValue);
        o.key(0x144, Untouched(&b->mF144, sizeof b->mF144));
        o.rect(0x148, b->mBarRect);
        break;
    }
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
        o.key(0x160, Untouched(&b->mLastY, sizeof b->mLastY + sizeof b->mPreviousY));
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
        o.key(0x14c, Untouched(&b->mRepeatCountdown, sizeof b->mRepeatCountdown));
        o.key(0x150, Untouched(&b->mRepeatStep, sizeof b->mRepeatStep));
        o.key(0x154, b->mForceUpdate);
        o.key(0x155, Untouched(b->mF155, sizeof b->mF155));
        break;
    }
    case 13:
        o.key(0x138, static_cast<const InterfaceLightPanel*>(w)->mValue);
        break;
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
}  // namespace

extern "C" {

// Resets the world for a case: no windows, the globals as the Mac's data has them (zero).
void tpw_ctl_reset() {
    for (auto& w : gWindows) w = nullptr;
    gUIHebrew = false;
    gUIAutoRedraw = 0;
    gUILastWindow = nullptr;
    gObserve = false;
    std::memset(Heap, 0, sizeof Heap);
}

// Names the world's fixed objects for the call log (windows are named when made).
void tpw_ctl_labels() {
    for (int i = 0; i < kDraws; ++i) tpw_cl_label(kDrawLabel + i, &gDraws[i]);
    for (int i = 0; i < kTimers; ++i) tpw_cl_label(kTimerLabel + i, &gTimers[i]);
    for (int i = 0; i < kStrings; ++i) tpw_cl_label(kStringLabel + i, gStrings[i]);
}

// One operation of the script; a[] are its arguments, out[0] is the answer and out[1..4] more. The numbers are those of the
// Python module.
void tpw_ctl_op(int32_t op, const int64_t* a, int64_t* out) {
    out[0] = out[1] = out[2] = out[3] = out[4] = 0;
    InterfaceWindow* w = Window(a[0]);
    auto* button = static_cast<InterfaceButton*>(w);
    int64_t before[4];
    auto begin = [&] {
        for (int i = 0; i < 4; ++i) before[i] = tpw_cl_heap[i];
        gObserve = true;
    };
    auto end = [&] {
        gObserve = false;
        for (int i = 0; i < 4; ++i) Heap[i] += tpw_cl_heap[i] - before[i];
    };
    switch (op) {
    case 1: {   // a plain window: slot, parent slot, attributes, id, left, top, right, bottom
        void* block = ::operator new(sizeof(InterfaceWindow));
        std::memset(block, 0xa5, sizeof(InterfaceWindow));
        tpw_cl_label(kWindowLabel + static_cast<int32_t>(a[0]), block);
        gWindows[a[0]] = new (block) InterfaceWindow(Window(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]),
                                                     static_cast<int16_t>(a[4]), static_cast<int16_t>(a[5]),
                                                     static_cast<int16_t>(a[6]), static_cast<int16_t>(a[7]));
        out[0] = L(gWindows[a[0]]);
        return;
    }
    case 2:   // links: slot, parent, previous, next, child
        w->mParent = Window(a[1]);
        w->mPrevious = Window(a[2]);
        w->mNext = Window(a[3]);
        w->mChild = Window(a[4]);
        return;
    case 3:   // fields: slot, attributes, help text number, second help text number
        w->mAttributes = static_cast<uint32_t>(a[1]);
        w->mHelpText = static_cast<int32_t>(a[2]);
        w->mF90 = static_cast<int32_t>(a[3]);
        return;
    case 4:   // draw objects: slot, the window's (-1 none), the client's
        w->mDrawObject = a[1] >= 0 ? &gDraws[a[1]] : nullptr;
        w->mClientDrawObject = a[2] >= 0 ? &gDraws[a[2]] : nullptr;
        return;
    case 5: {   // a control through its constructor: slot, kind, parent slot, attributes, id, left, top, right, bottom
        const int kind = static_cast<int>(a[1]);
        void* block = ::operator new(HostSize(kind));
        std::memset(block, 0xa5, HostSize(kind));
        tpw_cl_label(kWindowLabel + static_cast<int32_t>(a[0]), block);
        begin();
        gWindows[a[0]] = Construct(kind, block, Window(a[2]), static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]),
                                   static_cast<int16_t>(a[5]), static_cast<int16_t>(a[6]), static_cast<int16_t>(a[7]),
                                   static_cast<int16_t>(a[8]));
        end();
        out[0] = L(gWindows[a[0]]);
        return;
    }
    case 6: {   // CreateObject: result slot, parent slot, kind, attributes, id, left, top, right, bottom
        InterfaceWindow* parent = Window(a[1]);
        begin();
        InterfaceWindow* made = parent->CreateObject(static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]),
                                                     static_cast<int16_t>(a[5]), static_cast<int16_t>(a[6]), static_cast<int16_t>(a[7]),
                                                     static_cast<int16_t>(a[8]));
        end();
        if (made != nullptr) gWindows[a[0]] = made;
        out[0] = L(made);
        return;
    }
    case 7:   // a button's data: slot, state, highlight, held, delay, interval, text index (-1 none), up word, down word, shift x, shift y
        button->mState = static_cast<int8_t>(a[1]);
        button->mHilight = static_cast<int8_t>(a[2]);
        button->mHeldState = static_cast<uint8_t>(a[3]);
        button->mRepeatDelay = static_cast<int32_t>(a[4]);
        button->mRepeatInterval = static_cast<int32_t>(a[5]);
        button->mText = String(a[6]);
        button->mTextUpColour = {static_cast<uint8_t>(a[7] >> 24), static_cast<uint8_t>(a[7] >> 16), static_cast<uint8_t>(a[7] >> 8),
                                 static_cast<uint8_t>(a[7])};
        button->mTextDownColour = {static_cast<uint8_t>(a[8] >> 24), static_cast<uint8_t>(a[8] >> 16), static_cast<uint8_t>(a[8] >> 8),
                                   static_cast<uint8_t>(a[8])};
        button->mTextShiftX = static_cast<int8_t>(a[9]);
        button->mTextShiftY = static_cast<int8_t>(a[10]);
        return;
    case 8:   // globals: the UI's auto redraw, whether the UI is right to left
        gUIAutoRedraw = static_cast<int32_t>(a[0]);
        gUIHebrew = a[1] != 0;
        return;
    default:
        break;
    }
    // ---- the button's members (op, slot in a[0])
    UI_RGBA colour = {static_cast<uint8_t>(a[1] >> 24), static_cast<uint8_t>(a[1] >> 16), static_cast<uint8_t>(a[1] >> 8),
                      static_cast<uint8_t>(a[1])};
    begin();
    switch (op) {
    case 20: out[0] = button->CalcFrame(); break;
    case 21: out[0] = button->Enable(static_cast<int16_t>(a[1])); break;
    case 22: button->DoTextColour(); break;
    case 23: out[0] = button->SetState(static_cast<int8_t>(a[1]), static_cast<int8_t>(a[2])); break;
    case 24: out[0] = button->HiLight(static_cast<int8_t>(a[1]), static_cast<int8_t>(a[2])); break;
    case 25: out[0] = button->SetText(String(a[1])); break;
    case 26: out[0] = L(button->GetText()); break;
    case 27: out[0] = button->StopHeldTimer(); break;
    case 28: button->SetAutoRepeat(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])); break;
    case 29: button->SetTextUpColour(colour); break;
    case 30: button->SetTextDownColour(colour); break;
    case 31: out[0] = button->GetHelpText(); break;
    case 32: out[0] = button->GetState(); break;
    case 33: out[0] = button->WhatAmI(); break;
    case 34:   // the destructor: slot, flag (a positive flag deletes the object); the slot is empty after
        if (static_cast<int16_t>(a[1]) > 0) {
            delete button;
        } else {
            button->~InterfaceButton();
        }
        gWindows[a[0]] = nullptr;
        break;
    }
    end();
}

// The state: the windows reachable from the slots, each as its label, the number of pairs and the pairs; answers the number
// of words written.
int32_t tpw_ctl_state(int64_t* out) {
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

// The globals: the window the last constructor made, the auto redraw and the right to left flag.
void tpw_ctl_globals(int64_t* out) {
    out[0] = L(gUILastWindow);
    out[1] = gUIAutoRedraw;
    out[2] = gUIHebrew;
}

void tpw_ctl_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = Heap[i];
}

}  // extern "C"

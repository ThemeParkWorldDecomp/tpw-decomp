// Test library for the window drawing and the default window handler (libtpw_unit_UIWindowDraw.dylib; see
// tests/test_UIWindowDraw.py): src/InterfaceWindow_draw.cpp (the 13 functions of unit w30), under it the real window
// constructor (src/InterfaceWindow.cpp, which builds the test trees) and the real rectangles (src/UI_Base.cpp).
//
// Everything else the unit reaches is a fake that records through calllog: the message senders (UI_SendMessage and
// UI_PostMessage; unit w29's and w21's are not linked), UI_UpdatePointerIn, UI_SetCursor, the UI clock IveGotTicks, the
// window's untranslated virtual members (DrawFunction, CalcClientArea, the destructor, GetHelpText) and Show, the draw
// objects (Draw and Change record what they were given), the help text handler, the fake regions and the timers.
//
// The Python module drives this library through extern "C" operations on a small world: SLOTS windows made on the heap
// (so that message 4 can delete one), a few draw objects, two regions, two timers, three animation tables and a help text
// object. The dumps write the state in the order of the Python module's tables.
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"

using namespace calllog;

namespace {
constexpr int kSlots = 8;
constexpr int kDraws = 5;
constexpr int kRegions = 2;
constexpr int kTimers = 2;
constexpr int kTables = 3;
constexpr int kTableFrames = 64;   // frames past the ones a test sets are zero: amounts of 0
constexpr int kWindowLabel = 10, kDrawLabel = 20, kRegionLabel = 30, kTimerLabel = 40, kHelpLabel = 50, kTableLabel = 60;
}  // namespace

// ---- the senders, the pointer and the clock
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("SendMessage"));
}
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
    ev("IveGotTicks");
    return static_cast<uint32_t>(ret("IveGotTicks"));
}

// ---- the globals the unit does not define (the pump's and the help text's)
InterfaceWindow* gUIFocusWindow;
InterfaceHelpText* gUIHelpText;

// ---- the window's untranslated members
void InterfaceWindow::DrawFunction() { ev("DrawFunction", {lab(this)}); }
// (the real one copies the rectangle to the client rectangle; the constructor reaches it)
void InterfaceWindow::CalcClientArea() {
    ev("CalcClientArea", {lab(this)});
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
}
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this)}); }
int32_t InterfaceWindow::WhatAmI() { return 1; }
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() {
    ev("GetHelpText", {lab(this)});
    return static_cast<int32_t>(ret("GetHelpText"));
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Window.Show", {lab(this), on});
    return 1;
}

// ---- the timers (their constructor and destructor are not used; the table is plain storage)
InterfaceTimer::InterfaceTimer() {}
InterfaceTimer::~InterfaceTimer() {}

// ---- the help text handler: the base class is never called; the fake keeps the window like the real OnMouseMovedIn and
// OnMouseMovedOut do, so that sequences of messages see it
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
// The constructor and the two virtuals unit w36 added to the class (src/ui_helptext.cpp; the vtable must link here too).
InterfaceHelpText::InterfaceHelpText() {}
void InterfaceHelpText::Init() {}
void InterfaceHelpText::DeInit() {}

namespace {
class FakeHelp : public InterfaceHelpText {
public:
    void OnMouseMoved(int16_t x, int16_t y) override { ev("Help.OnMouseMoved", {x, y}); }
    void OnMouseMovedIn(InterfaceWindow* window) override {
        ev("Help.OnMouseMovedIn", {lab(window)});
        mWindow = window;
    }
    void OnMouseMovedOut() override {
        ev("Help.OnMouseMovedOut");
        mWindow = nullptr;
    }
    void OnFrame() override { ev("Help.OnFrame"); }
};
FakeHelp gHelp;

// A region that records Shift.
class FakeRegion : public CoolRegion {
public:
    void Shift(int16_t dx, int16_t dy) override { ev("Region.Shift", {lab(this), dx, dy}); }
};
FakeRegion gRegions[kRegions];

InterfaceTimer gTimers[kTimers];
UI_RectDelta gTables[kTables][kTableFrames];

InterfaceWindow* gWindows[kSlots];

// Which rectangle of which window a rectangle is, as the draw objects' events say: 10 * (slot + 1) + 1 the window's own
// rectangle, 2 the original, 3 the previous, 4 the client rectangle, 5 the one at 0x34; -1 for any other.
int32_t RectCode(const CoolRect* rect) {
    for (int slot = 0; slot < kSlots; ++slot) {
        const InterfaceWindow* w = gWindows[slot];
        if (w == nullptr) continue;
        if (rect == &w->mRect) return (slot + 1) * 10 + 1;
        if (rect == &w->mOriginalRect) return (slot + 1) * 10 + 2;
        if (rect == &w->mPreviousRect) return (slot + 1) * 10 + 3;
        if (rect == &w->mClientRect) return (slot + 1) * 10 + 4;
        if (rect == &w->mRect34) return (slot + 1) * 10 + 5;
    }
    return -1;
}
}  // namespace

// ---- the draw objects: the base class's members are never used, the fake records Draw and Change
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
    void Draw(int16_t a, CoolRect* rect, int16_t b) override {
        ev("Draw", {lab(this), a, RectCode(rect), rect->mLeft, rect->mTop, rect->mRight, rect->mBottom, b});
    }
    // The fields the flags name, in the order of the flags: 1 the frame, 4 the z rotation, 0x10 the rectangle, 0x40 the pivot,
    // 0x80 the value. Other flags are not used by the unit.
    void Change(uint32_t flags, DOChange* change) override {
        int64_t ints[12];
        int n = 0;
        ints[n++] = lab(this);
        ints[n++] = flags;
        if (flags & 1) ints[n++] = change->mFrame;
        if (flags & 4) ints[n++] = change->mF08;
        if (flags & 0x10) {
            ints[n++] = RectCode(change->mRect);
            ints[n++] = change->mRect->mLeft;
            ints[n++] = change->mRect->mTop;
            ints[n++] = change->mRect->mRight;
            ints[n++] = change->mRect->mBottom;
        }
        if (flags & 0x40) {
            ints[n++] = change->mF0C;
            ints[n++] = change->mF0E;
        }
        if (flags & 0x80) {
            int32_t value;
            std::memcpy(&value, change->mF00, sizeof value);
            ints[n++] = value;
        }
        ev("Change", {ints[0], ints[1], n > 2 ? ints[2] : 0, n > 3 ? ints[3] : 0, n > 4 ? ints[4] : 0, n > 5 ? ints[5] : 0,
                      n > 6 ? ints[6] : 0, n > 7 ? ints[7] : 0, n > 8 ? ints[8] : 0});
    }
};
FakeDraw gDraws[kDraws];

int64_t Heap[4];

InterfaceWindow* Window(int64_t slot) { return slot >= 0 && slot < kSlots ? gWindows[slot] : nullptr; }
int64_t L(const void* object) { return lab(object); }
void Put(int64_t*& out, int64_t value) { *out++ = value; }

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
}  // namespace

extern "C" {

// Resets the world for a case: no windows, the globals as the Mac's data has them (the double click time is the initialised
// word, the rest zero), the help text handler the fake.
void tpw_uiwd_reset() {
    for (int i = 0; i < kSlots; ++i) {
        if (gWindows[i] != nullptr) ::operator delete(gWindows[i]);
        gWindows[i] = nullptr;
    }
    gUIFocusWindow = nullptr;
    gUIHelpText = &gHelp;
    gHelp.mWindow = nullptr;
    std::memset(gUIButtons, 0, sizeof gUIButtons);
    gUIDoubleClickTime = 500;
    gUIAutoRedraw = 0;
    gUIHelpTextResult = 0;
    std::memset(gTables, 0, sizeof gTables);
    for (InterfaceTimer& timer : gTimers) timer.mRunning = 1;
    std::memset(Heap, 0, sizeof Heap);
}

// Names the world's fixed objects for the call log (windows are named when made).
void tpw_uiwd_labels() {
    for (int i = 0; i < kDraws; ++i) tpw_cl_label(kDrawLabel + i, &gDraws[i]);
    for (int i = 0; i < kRegions; ++i) tpw_cl_label(kRegionLabel + i, &gRegions[i]);
    for (int i = 0; i < kTimers; ++i) tpw_cl_label(kTimerLabel + i, &gTimers[i]);
    tpw_cl_label(kHelpLabel, &gHelp);
    for (int i = 0; i < kTables; ++i) tpw_cl_label(kTableLabel + i, gTables[i]);
}

// An animation table: `count` frames of four shorts.
void tpw_uiwd_table(int32_t k, int32_t count, const int16_t* words) {
    for (int i = 0; i < count; ++i) {
        gTables[k][i].mLeft = words[4 * i];
        gTables[k][i].mTop = words[4 * i + 1];
        gTables[k][i].mRight = words[4 * i + 2];
        gTables[k][i].mBottom = words[4 * i + 3];
    }
}

// One operation of the script; a[] are its arguments, out[0] is the answer and out[1..4] more. The numbers are those of the
// Python module.
void tpw_uiwd_op(int32_t op, const int64_t* a, int64_t* out) {
    out[0] = out[1] = out[2] = out[3] = out[4] = 0;
    InterfaceWindow* w = Window(a[0]);
    const int64_t heapBefore[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    switch (op) {
    case 1: {   // a window: slot, parent slot, attributes, id, left, top, right, bottom
        if (gWindows[a[0]] != nullptr) ::operator delete(gWindows[a[0]]);
        void* block = ::operator new(sizeof(InterfaceWindow));
        std::memset(block, 0xa5, sizeof(InterfaceWindow));
        tpw_cl_label(kWindowLabel + static_cast<int32_t>(a[0]), block);   // (before the constructor, which records an event)
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
    case 3:   // fields: slot, attributes, destroying, region (0 own rectangle, 1 and 2 a fake region), cursor
        w->mAttributes = static_cast<uint32_t>(a[1]);
        w->mDestroying = static_cast<uint8_t>(a[2]);
        w->mRegion = a[3] == 0 ? static_cast<CoolRegion*>(&w->mRect) : &gRegions[a[3] - 1];
        w->mCursor = static_cast<int16_t>(a[4]);
        return;
    case 4:   // draw objects: slot, the window's (-1 none), the client's
        w->mDrawObject = a[1] >= 0 ? &gDraws[a[1]] : nullptr;
        w->mClientDrawObject = a[2] >= 0 ? &gDraws[a[2]] : nullptr;
        return;
    case 5:   // animation: slot, table (-1 none), frame, ticks, start, animating, the four offsets
        w->mAnim = a[1] >= 0 ? gTables[a[1]] : nullptr;
        w->mAnimFrame = static_cast<int16_t>(a[2]);
        w->mAnimTicks = static_cast<int32_t>(a[3]);
        w->mAnimStart = static_cast<int32_t>(a[4]);
        w->mAnimating = static_cast<uint8_t>(a[5]);
        for (int i = 0; i < 4; ++i) w->mAnimOffset[i] = static_cast<int16_t>(a[6 + i]);
        return;
    case 6:   // slide: slot, timer (-1 none), the four steps, steps left, sliding, the target rectangle
        w->mSlideTimer = a[1] >= 0 ? &gTimers[a[1]] : nullptr;
        for (int i = 0; i < 4; ++i) w->mSlideStep[i] = static_cast<int16_t>(a[2 + i]);
        w->mSlideSteps = static_cast<int16_t>(a[6]);
        w->mSliding = static_cast<int8_t>(a[7]);
        w->mRect34.mLeft = static_cast<int16_t>(a[8]);
        w->mRect34.mTop = static_cast<int16_t>(a[9]);
        w->mRect34.mRight = static_cast<int16_t>(a[10]);
        w->mRect34.mBottom = static_cast<int16_t>(a[11]);
        return;
    case 8:   // depth and pivot: slot, z, z rotation, pivot x, pivot y, buttons held
        w->mZ = static_cast<int16_t>(a[1]);
        w->mZRot = static_cast<int16_t>(a[2]);
        w->mPivotX = static_cast<int16_t>(a[3]);
        w->mPivotY = static_cast<int16_t>(a[4]);
        w->mButtonsHeld = static_cast<uint32_t>(a[5]);
        return;
    case 7: {   // a global: which, values
        const int64_t v = a[1];
        switch (a[0]) {
        case 1: gUIFocusWindow = Window(v); break;
        case 2: gUIDoubleClickTime = static_cast<int32_t>(v); break;
        case 3: gUIAutoRedraw = static_cast<int32_t>(v); break;
        case 4: gHelp.mWindow = Window(v); break;
        case 5:   // a button: index, state, x, y, time
            gUIButtons[v].mState = static_cast<int16_t>(a[2]);
            gUIButtons[v].mX = static_cast<int16_t>(a[3]);
            gUIButtons[v].mY = static_cast<int16_t>(a[4]);
            gUIButtons[v].mTime = static_cast<int32_t>(a[5]);
            break;
        case 6: gTimers[v].mRunning = static_cast<int32_t>(a[2]); break;
        case 7: gUIHelpTextResult = static_cast<int32_t>(v); break;
        }
        return;
    }
    default:
        break;
    }
    // ---- the functions under test (op, slot in a[0])
    switch (op) {
    case 20: out[0] = w->DrawMe(); break;
    case 21: out[0] = w->DrawReverseOrder(); break;
    case 22: w->ClientDraw(); break;
    case 23: out[0] = w->ShiftPosition(static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2])); break;
    case 24: w->HandleScale(static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3]), static_cast<int16_t>(a[4])); break;
    case 25: {   // SetPosition: slot, left, top, right, bottom, null
        CoolRect rect(static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3]), static_cast<int16_t>(a[4]));
        out[0] = w->SetPosition(a[5] != 0 ? nullptr : &rect);
        out[1] = rect.mLeft;
        out[2] = rect.mTop;
        out[3] = rect.mRight;
        out[4] = rect.mBottom;
        break;
    }
    case 26: {   // GetPosition: slot, subtract, null; the rectangle starts as 7, 8, 9, 10
        CoolRect rect(7, 8, 9, 10);
        out[0] = w->GetPosition(a[2] != 0 ? nullptr : &rect, static_cast<int16_t>(a[1]));
        out[1] = rect.mLeft;
        out[2] = rect.mTop;
        out[3] = rect.mRight;
        out[4] = rect.mBottom;
        break;
    }
    case 27: out[0] = L(w->BringToTop()); break;
    case 28: out[0] = w->doAnimFrame(); break;
    case 29: w->setZRot(static_cast<int16_t>(a[1])); break;
    case 30: out[0] = w->SetValue(static_cast<int32_t>(a[1])); break;
    case 31: gTimers[a[0]].Stop(); break;
    case 32: {   // the default handler: slot, message, a, b. A window the handler deletes is gone from the world.
        const int32_t answer = UI_DefaultWindowCallback(w, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]));
        const int32_t helpAddress = static_cast<int32_t>(reinterpret_cast<intptr_t>(&gUIHelpTextResult));
        out[0] = (a[1] == 0xc && answer == helpAddress) ? -0x7001 : answer;
        if (a[1] == 4 && a[0] >= 0) gWindows[a[0]] = nullptr;
        break;
    }
    }
    for (int i = 0; i < 4; ++i) Heap[i] += tpw_cl_heap[i] - heapBefore[i];
}

// A window's state in the order of the Python module's dump_window: 57 words.
void tpw_uiwd_dump_window(int32_t slot, int64_t* out) {
    const InterfaceWindow* w = gWindows[slot];
    Put(out, w->mDestroying);
    for (const CoolRect* rect : {&w->mRect, &w->mOriginalRect, &w->mPreviousRect, &w->mClientRect, &w->mRect34}) {
        Put(out, rect->mLeft);
        Put(out, rect->mTop);
        Put(out, rect->mRight);
        Put(out, rect->mBottom);
    }
    Put(out, w->mRegion == nullptr ? 0 : w->mRegion == &w->mRect ? 1 : L(w->mRegion));
    Put(out, static_cast<int32_t>(w->mAttributes));
    Put(out, L(w->mFont));
    Put(out, w->mCursor);
    Put(out, L(w->mNext));
    Put(out, L(w->mPrevious));
    Put(out, L(w->mChild));
    Put(out, L(w->mParent));
    Put(out, static_cast<int32_t>(w->mF60));
    Put(out, static_cast<int32_t>(w->mF64));
    Put(out, Untouched(w->mF68, sizeof w->mF68));
    Put(out, w->mId);
    Put(out, w->mHelpText);
    Put(out, w->mF90);
    Put(out, L(w->mDrawObject));
    Put(out, L(w->mClientDrawObject));
    Put(out, Zero(w->mExtraParams, sizeof w->mExtraParams));
    Put(out, w->mZ);
    Put(out, w->mZRot);
    Put(out, w->mPivotX);
    Put(out, w->mPivotY);
    Put(out, L(w->mAnim));
    Put(out, w->mAnimFrame);
    Put(out, w->mAnimTicks);
    Put(out, w->mAnimStart);
    for (int i = 0; i < 4; ++i) Put(out, w->mAnimOffset[i]);
    Put(out, L(w->mSlideTimer));
    for (int i = 0; i < 4; ++i) Put(out, w->mSlideStep[i]);
    Put(out, w->mSlideSteps);
    Put(out, Untouched(w->mF10A, sizeof w->mF10A));
    Put(out, w->mCallback == nullptr ? 0 : w->mCallback == UI_DefaultWindowCallback ? 1 : 2);
    Put(out, w->mDefaultCallback == nullptr ? 0 : w->mDefaultCallback == UI_DefaultWindowCallback ? 1 : 2);
    Put(out, static_cast<int32_t>(w->mButtonsHeld));
    for (int i = 0; i < 4; ++i) Put(out, L(w->mAnimTimers[i]));
    Put(out, static_cast<int32_t>(w->mF130));
    Put(out, w->mF134);
    Put(out, w->mAnimating);
    Put(out, w->mSliding);
}

// The globals, the timers and the help text handler: 20 words.
void tpw_uiwd_dump_globals(int64_t* out) {
    Put(out, L(gUIFocusWindow));
    Put(out, gUIDoubleClickTime);
    Put(out, gUIAutoRedraw);
    Put(out, gUIHelpTextResult);
    Put(out, L(gHelp.mWindow));
    for (int i = 0; i < 3; ++i) {
        Put(out, gUIButtons[i].mState);
        Put(out, gUIButtons[i].mX);
        Put(out, gUIButtons[i].mY);
        Put(out, gUIButtons[i].mTime);
    }
    for (int i = 0; i < kTimers; ++i) Put(out, gTimers[i].mRunning);
    Put(out, L(gUIHelpText));
}

void tpw_uiwd_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) Put(out, Heap[i]);
}

}  // extern "C"

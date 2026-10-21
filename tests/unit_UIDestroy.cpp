// Test library for the window's destructor chain (libtpw_unit_UIDestroy.dylib; see tests/test_UIDestroy.py, unit w51):
// src/game/ui/InterfaceWindow.cpp (~InterfaceWindow, PropagateDestruction, UnCaptureKeys), src/game/ui/InterfaceTimer.cpp (UI_RemoveTimer,
// InterfaceTimer::FindTimer) and src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/UI_Base.cpp (ClearAllPointers, UI_InvalidateMessages, UI_InvalidateDrawMessages,
// UI_InvalidateDeferredMessages and the two queues' InvalidateMsgs). Under them, real: the window's constructor
// (src/game/ui/InterfaceWindow.cpp), UI_SendMessage and CallWindowHandler (src/game/ui/InterfaceWindow.cpp, src/game/ui/UI_Base.cpp) and the rectangles (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp).
//
// Everything else the unit calls is a fake that records through calllog: the two window handlers (which also do what the case's
// script says), the default handler, the message poster, the pointer update, the help text handler (OnHideOrDelete), the draw
// objects (Show and the destructor) and the regions (the destructor), the assertion. Objects of the world are deleted through the
// destructors the original calls (the windows' own, the fakes') and their storage is kept, so that the state can be read afterwards:
// a delete only records.
//
// Unit w54 adds to the same library: the base window's small virtuals (src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceWindow.cpp), the bar's and the text box's
// destructors (src/game/ui/InterfaceBar.cpp, src/game/ui/InterfaceTextBox.cpp), the timer's destructor (src/game/ui/InterfaceTimer.cpp), ReleaseString (src/game/rides/RidePlacement.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/InterfaceStringHeap.cpp) and
// the button (src/game/ui/InterfaceButton.cpp: its Enable, GetHelpText and destructor). A window of the world is a plain window, a bar, a text box
// or a button; the bar's and the text box's constructors and virtuals are fakes here (the originals run on the PowerPC side).
//
// The world: fourteen windows (made with the real constructor), six draw objects, three fake regions, a table of twelve timers, a
// general, a draw and a timed queue, a key list, the help text handler, three strings and a text block for each window. The Python module drives the library through extern "C"
// operations; after each one it dumps the state in the order of its tables.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"

#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "fake_arena_on_heap.h"

using namespace calllog;

namespace {
constexpr int kSlots = 14;
constexpr int kDraws = 6;
constexpr int kRegions = 3;
constexpr int kTimers = 12;
constexpr int kBigEntries = 0x8010;   // slots of a queue made big (the short count wraps)
constexpr int kTimedEntries = 16;
constexpr int kStrings = 3;           // strings of the string heap (unit w54): kStringChars characters each
constexpr int kStringChars = 6;
constexpr int kKinds = 4;             // 0 window, 1 bar, 2 text box, 3 button
constexpr std::size_t kWindowStore = 0x240;   // the biggest class (the button: 0x15c on the Mac, more on the host)
constexpr int32_t kBigQueue = 16;     // a queue of more slots than this is dumped as a checksum
// object labels (the Python module's LAB_*)
constexpr int kWindowLabel = 10, kQueueLabel = 30, kTimedLabel = 32, kDrawLabel = 40, kRegionLabel = 50, kTimerLabel = 60,
              kHelpLabel = 80, kKeyListLabel = 90, kStringLabel = 100, kTextLabel = 110;

// Deleting an object of the world only records it: the storage is the world's, and a window or a draw object may go on pointing at
// an object (the original's windows share draw objects). The destructor has put the base class's vtable pointer in the object by
// the time operator delete runs; it is put back, so that a later call or a second delete behaves as the PowerPC side's object, which
// the fake destructor does not change.
void* VtablePointerOf(const void* object) {
    void* pointer;
    std::memcpy(&pointer, object, sizeof pointer);
    return pointer;
}
void RecordDelete(void* object, void* vtablePointer) {
    if (vtablePointer != nullptr) std::memcpy(object, &vtablePointer, sizeof vtablePointer);
    ++tpw_cl_heap[2];
    if (calllog::on() && calllog::lab(object) > 0) {
        calllog::ev("delete", {calllog::lab(object)});
        calllog::forget(object);
    }
}

// A window of the world: the real constructor and destructor, and a delete that only records. The vtable pointer the destructor
// leaves (the base class's) is put back, as for the other objects, so that a window may be destroyed or deleted again. One class per
// kind (0 window, 1 bar, 2 text box, 3 button); the vtable pointer put back is the kind's.
void* gWindowVtablePointer[kKinds];
template <class Base, int Kind>
class Tracked : public Base {
public:
    using Base::Base;
    static void operator delete(void* object) { RecordDelete(object, gWindowVtablePointer[Kind]); }
};
using TestWindow = Tracked<InterfaceWindow, 0>;
using TestBar = Tracked<InterfaceBar, 1>;
using TestTextBox = Tracked<InterfaceTextBox, 2>;
using TestButton = Tracked<InterfaceButton, 3>;
static_assert(sizeof(TestWindow) <= kWindowStore && sizeof(TestBar) <= kWindowStore && sizeof(TestTextBox) <= kWindowStore &&
                  sizeof(TestButton) <= kWindowStore,
              "a window of the world must fit its storage");
int gKind[14];   // the kind of each slot's window

class FakeDraw : public InterfaceDrawObject {
public:
    FakeDraw() : InterfaceDrawObject(nullptr) {}
    static void operator delete(void* object);
};
FakeDraw gDraws[kDraws];
void* gDrawVtablePointer;
void FakeDraw::operator delete(void* object) { RecordDelete(object, gDrawVtablePointer); }

class FakeRegion : public CoolRegion {
public:
    ~FakeRegion() override { ev("FakeRegion.dtor", {lab(this)}); }
    static void operator delete(void* object);
};
FakeRegion gRegions[kRegions];
void* gRegionVtablePointer;
void FakeRegion::operator delete(void* object) { RecordDelete(object, gRegionVtablePointer); }

// The help text handler: the one member the destructor calls records.
class FakeHelp : public InterfaceHelpText {
public:
    void OnHideOrDelete(InterfaceWindow* window) override { ev("Help.OnHideOrDelete", {lab(this), lab(window)}); }
};
FakeHelp gHelp;

alignas(16) unsigned char gWindowStore[kSlots][kWindowStore];
char16_t gStrings[kStrings][kStringChars];
InterfaceWindow* gWindows[kSlots];
bool gMade[kSlots];
alignas(InterfaceMessageQueue) unsigned char gQueueStore[2][sizeof(InterfaceMessageQueue)];
InterfaceMessage gMessages[2][kBigEntries];
alignas(InterfaceTimedMessages) unsigned char gTimedStore[sizeof(InterfaceTimedMessages)];
InterfaceTimedMessage gTimedEntries[kTimedEntries];
InterfaceTimer gTimerTable[kTimers];
captureKeyList gKeys[] = {{0x41, 0}, {0, -1}};

InterfaceWindow* Window(int64_t slot) { return slot >= 0 && slot < kSlots && gMade[slot] ? gWindows[slot] : nullptr; }
InterfaceMessageQueue* QueueObject(int64_t k) { return reinterpret_cast<InterfaceMessageQueue*>(gQueueStore[k]); }
InterfaceTimedMessages* TimedObject() { return reinterpret_cast<InterfaceTimedMessages*>(gTimedStore); }
InterfaceTimer* Timer(int64_t index) { return index >= 0 && index < kTimers ? &gTimerTable[index] : nullptr; }
InterfaceDrawObject* Draw(int64_t index) { return index >= 0 && index < kDraws ? &gDraws[index] : nullptr; }

// ---- the window handlers: record, and do what the script says (the case's "act")
int32_t HandleMessage(int id, InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Handler", {id, lab(window), message, a, b});
    switch (ret("act", 0)) {
    case 1: window->mNext = nullptr; break;
    case 2: window->mChild = nullptr; break;
    case 3: gUIFocusWindow = window; break;
    case 4: gUIKeyboardWindow = window; break;
    case 5: window->mAttributes ^= 2; break;
    case 6: window->mCallback = nullptr; break;
    }
    return 0;
}
int32_t Handler1(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) { return HandleMessage(1, w, m, a, b); }
int32_t Handler2(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) { return HandleMessage(2, w, m, a, b); }
InterfaceWindowCallback const gHandlers[] = {nullptr, Handler1, Handler2};
}  // namespace

// ---- the UI's globals the unit defines (the others live in files that are not linked)
InterfaceWindow* gUIPointerIn;
InterfaceWindow* gUICaptureDevice;
InterfaceWindow* gUIFocusWindow;
InterfaceWindow* gUIKeyboardWindow;
UIKeyCapture gUIKeyCapture;
InterfaceMessageQueue* gUIMessageQueue;
InterfaceMessageQueue* gUIDrawQueue;
InterfaceTimedMessages* gUITimedMessages;
InterfaceTimer* gUITimers;
int32_t gUITimerCount;
InterfaceTimer* gUITimerList;
InterfaceWindow* gUIButtonWindow[3];
InterfaceHelpText* gUIHelpText;
int32_t (*gUIPeekCallBack)(InterfaceWindow*, int32_t, int32_t, int32_t);

// ---- the window's untranslated virtual members (its destructor, DrawFunction, Enable, operator-, GetValue, GetText and GetHelpText
// are the unit's). WhatAmI records (operator- calls it, through each window's own vtable).
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() {
    ev("WhatAmI", {lab(this)});
    return 1;
}
void InterfaceWindow::ClientDraw() {}
int32_t InterfaceWindow::SetValue(int32_t) { return 1; }
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
// The bar's and the text box's untranslated members (their constructors are the translated bodies, written out; the Mac's run on the
// PowerPC side). Their destructors are the unit's.
InterfaceBar::InterfaceBar(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mValue = 0;
    mF13C = 0;
    mF138 = 0;
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    mBarRect.mLeft = mClientRect.mLeft;
    mBarRect.mTop = mClientRect.mTop;
    mBarRect.mRight = mClientRect.mRight;
    mBarRect.mBottom = mClientRect.mBottom;
}
void InterfaceBar::DrawFunction() {}
int32_t InterfaceBar::WhatAmI() {
    ev("Bar.WhatAmI", {lab(this)});
    return 9;
}
int32_t InterfaceBar::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceBar::SetValue(int32_t) { return 1; }
int32_t InterfaceBar::GetValue() { return mValue; }
int32_t UI_DefaultTextBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
InterfaceTextBox::InterfaceTextBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                   int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mTextLength = 0;
    mText = nullptr;
    mHorizontal = 0;
    mVertical = 0;
    mF144 = 0;
    mTextX = 0;
    mTextY = 0;
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    mCallback = UI_DefaultTextBoxCallback;
}
void InterfaceTextBox::DrawFunction() {}
int32_t InterfaceTextBox::WhatAmI() {
    ev("TextBox.WhatAmI", {lab(this)});
    return 10;
}
void InterfaceTextBox::ClientDraw() {}
int32_t InterfaceTextBox::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceTextBox::SetText(char16_t*) { return 1; }
// The button's handler (untranslated, 2,316 bytes) and the UI's auto redraw flag.
int32_t UI_DefaultButtonCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t gUIAutoRedraw;
// The handler every window starts with (untranslated): records.
int32_t UI_DefaultWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Default", {lab(window), message, a, b});
    return 0;
}

// ---- the callees outside the unit
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("PostMessage", {lab(window), message, a, b});
    return 1;
}
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn");
    return 1;
}
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }

// A timer starts free, in no list and not running (its destructor is the unit's).
InterfaceTimer::InterfaceTimer() : mWindow(nullptr), mStart(0), mSlice(0), mId(0), mRunning(0), mFree(1), mNext(nullptr), mPrevious(nullptr) {}

// The help text handler's members (its vtable must link here).
InterfaceHelpText::InterfaceHelpText() {}
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
void InterfaceHelpText::Init() {}
void InterfaceHelpText::DeInit() {}

// ---- the draw objects: Show and the destructor record, the rest is never called
InterfaceDrawObject::InterfaceDrawObject(void*)
    : mData(nullptr), mShown(0), mFrameBase(0), mFrame(0), mFrameCount(0), mInLibrary(0), mResizable(0), mF28(0), mF2A(0), mF2C(0),
      mF2E(0), mF30(0), mF32(0), mDepth(0) {}
InterfaceDrawObject::~InterfaceDrawObject() { ev("Draw.dtor", {lab(this)}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t flags, DOChange* change) { ev("Draw.Change", {lab(this), static_cast<int64_t>(flags), change->mFrame}); }
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t shown) { ev("Draw.Show", {lab(this), shown}); }
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

namespace {
// A text block of the heap (out of line: the compiler may move a new-expression past the counter it changes).
[[gnu::noinline]] char16_t* MakeTextBlock() { return new char16_t[4]; }
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
int64_t L(const void* object) { return lab(object); }
void Put(int64_t*& out, int64_t value) { *out++ = value; }

// 0 none, 1 and 2 a handler, 5 the default callback, 6 any other.
int64_t CallbackCode(InterfaceWindowCallback callback) {
    if (callback == nullptr) return 0;
    for (int i = 1; i < 3; ++i)
        if (callback == gHandlers[i]) return i;
    return callback == UI_DefaultWindowCallback ? 5 : 6;
}
}  // namespace

extern "C" {

// Resets the world and the globals for a case, to what the Mac's data and BSS hold before the interface starts.
void tpw_uid_reset() {
    gUIPointerIn = nullptr;
    gUICaptureDevice = nullptr;
    gUIFocusWindow = nullptr;
    gUIKeyboardWindow = nullptr;
    gUIKeyCapture.mKeys = nullptr;
    gUIKeyCapture.mWindow = nullptr;
    gUIMessageQueue = nullptr;
    gUIDrawQueue = nullptr;
    gUITimedMessages = nullptr;
    gUITimers = nullptr;
    gUITimerCount = 0;
    gUITimerList = nullptr;
    for (InterfaceWindow*& window : gUIButtonWindow) window = nullptr;
    gUIHelpText = &gHelp;
    gUIPeekCallBack = nullptr;
    gUILastWindow = nullptr;
    gUIAutoRedraw = 0;
    for (int i = 0; i < kSlots; ++i) {
        gWindows[i] = nullptr;
        gMade[i] = false;
        gKind[i] = 0;
    }
    std::memset(gStrings, 0, sizeof gStrings);
    for (FakeDraw& draw : gDraws) new (&draw) FakeDraw;
    gDrawVtablePointer = VtablePointerOf(&gDraws[0]);
    for (FakeRegion& region : gRegions) new (&region) FakeRegion;
    gRegionVtablePointer = VtablePointerOf(&gRegions[0]);
    for (int i = 0; i < kTimers; ++i) new (&gTimerTable[i]) InterfaceTimer;
    std::memset(gQueueStore, 0, sizeof gQueueStore);
    std::memset(gMessages, 0, sizeof gMessages);
    std::memset(gTimedStore, 0, sizeof gTimedStore);
    std::memset(gTimedEntries, 0, sizeof gTimedEntries);
}

// Names the world's fixed objects for the call log (windows are named when made).
void tpw_uid_labels() {
    for (int i = 0; i < kSlots; ++i) tpw_cl_label(kWindowLabel + i, gWindowStore[i]);
    tpw_cl_label(kQueueLabel, gQueueStore[0]);
    tpw_cl_label(kQueueLabel + 1, gQueueStore[1]);
    tpw_cl_label(kTimedLabel, gTimedStore);
    for (int i = 0; i < kDraws; ++i) tpw_cl_label(kDrawLabel + i, &gDraws[i]);
    for (int i = 0; i < kRegions; ++i) tpw_cl_label(kRegionLabel + i, &gRegions[i]);
    for (int i = 0; i < kTimers; ++i) tpw_cl_label(kTimerLabel + i, &gTimerTable[i]);
    for (int i = 0; i < kStrings; ++i) tpw_cl_label(kStringLabel + i, gStrings[i]);
    tpw_cl_label(kHelpLabel, &gHelp);
    tpw_cl_label(kKeyListLabel, gKeys);
}

// A window made with the real constructor in storage filled with 0xa5. kind: 0 window, 1 bar, 2 text box, 3 button.
int64_t tpw_uid_make_window(int32_t slot, int32_t parent, int32_t attributes, int32_t id, int32_t left, int32_t top, int32_t right,
                            int32_t bottom, int32_t kind) {
    std::memset(gWindowStore[slot], 0xa5, sizeof gWindowStore[slot]);
    void* place = gWindowStore[slot];
    InterfaceWindow* parentWindow = Window(parent);
    const int16_t l = static_cast<int16_t>(left), t = static_cast<int16_t>(top), r = static_cast<int16_t>(right),
                  b = static_cast<int16_t>(bottom);
    switch (kind) {
    case 1: gWindows[slot] = new (place) TestBar(parentWindow, attributes, id, l, t, r, b); break;
    case 2: gWindows[slot] = new (place) TestTextBox(parentWindow, attributes, id, l, t, r, b); break;
    case 3: gWindows[slot] = new (place) TestButton(parentWindow, attributes, id, l, t, r, b); break;
    default: gWindows[slot] = new (place) TestWindow(parentWindow, attributes, id, l, t, r, b); kind = 0; break;
    }
    gKind[slot] = kind;
    gMade[slot] = true;
    gWindowVtablePointer[kind] = VtablePointerOf(gWindows[slot]);
    return L(gWindows[slot]);
}

// The queues' slots: every slot free (all zero), the words as an empty ring leaves them.
void tpw_uid_make_queues(int32_t general, int32_t draw, int32_t timed) {
    const int32_t counts[2] = {general, draw};
    for (int k = 0; k < 2; ++k) {
        InterfaceMessageQueue* q = QueueObject(k);
        q->mEntries = counts[k] > 0 ? gMessages[k] : nullptr;
        q->mCount = counts[k];
        q->mHead = counts[k] - 1;
        q->mTail = counts[k] - 1;
        q->mQueued = 0;
    }
    InterfaceTimedMessages* t = TimedObject();
    t->mEntries = timed > 0 ? gTimedEntries : nullptr;
    t->mCount = timed;
}

// One operation of the script; a[] are its arguments. Returns the answer (0 when it has none).
int64_t tpw_uid_op(int32_t op, const int64_t* a) {
    switch (op) {
    case 1: {   // a window's fields: slot, destroying byte, handler, region (0 its own rectangle, -1 none, n a fake region), draw, client draw
        InterfaceWindow* w = Window(a[0]);
        w->mDestroying = static_cast<uint8_t>(a[1]);
        w->mCallback = a[2] >= 1 && a[2] <= 2 ? gHandlers[a[2]] : a[2] == 5 ? UI_DefaultWindowCallback : nullptr;
        if (a[3] == 0) w->mRegion = &w->mRect;
        else if (a[3] < 0) w->mRegion = nullptr;
        else w->mRegion = &gRegions[a[3] - 1];
        w->mDrawObject = Draw(a[4]);
        w->mClientDrawObject = Draw(a[5]);
        return 0;
    }
    case 2: {   // links: slot, parent, previous, next, child
        InterfaceWindow* w = Window(a[0]);
        w->mParent = Window(a[1]);
        w->mPrevious = Window(a[2]);
        w->mNext = Window(a[3]);
        w->mChild = Window(a[4]);
        return 0;
    }
    case 3: {   // a global: which, value
        const int64_t v = a[1];
        switch (a[0]) {
        case 1: gUIPointerIn = Window(v); break;
        case 2: gUICaptureDevice = Window(v); break;
        case 3: gUIFocusWindow = Window(v); break;
        case 4: gUIKeyboardWindow = Window(v); break;
        case 5: gUIKeyCapture.mWindow = Window(v); break;
        case 6: gUIKeyCapture.mKeys = v == 0 ? nullptr : gKeys; break;
        case 7: gUIButtonWindow[0] = Window(v); break;
        case 8: gUIButtonWindow[1] = Window(v); break;
        case 9: gUIButtonWindow[2] = Window(v); break;
        case 10: gUITimerList = Timer(v); break;
        case 11:   // the timer table: v timers; negative: no table but a count of -v
            gUITimers = v >= 0 ? gTimerTable : nullptr;
            gUITimerCount = static_cast<int32_t>(v >= 0 ? v : -v);
            break;
        case 12: gUIMessageQueue = v != 0 ? QueueObject(0) : nullptr; break;
        case 13: gUIDrawQueue = v != 0 ? QueueObject(1) : nullptr; break;
        case 14: gUITimedMessages = v != 0 ? TimedObject() : nullptr; break;
        case 15: gUITimers = v >= 0 && v < kTimers ? &gTimerTable[v] : nullptr; break;   // the table starts at a timer of the world (the count stays)
        }
        return 0;
    }
    case 4: {   // a window's animation timers: slot, four timer indexes (-1 none)
        InterfaceWindow* w = Window(a[0]);
        for (int i = 0; i < 4; ++i) w->mAnimTimers[i] = Timer(a[1 + i]);
        return 0;
    }
    case 5: {   // a draw object: index, in library, users, shown
        gDraws[a[0]].mInLibrary = static_cast<int16_t>(a[1]);
        gDraws[a[0]].mResizable = static_cast<int16_t>(a[2]);
        gDraws[a[0]].mShown = static_cast<int16_t>(a[3]);
        return 0;
    }
    case 6: {   // a timer: index, window, id, free, next, previous, running, slice, start
        InterfaceTimer& t = gTimerTable[a[0]];
        t.mWindow = Window(a[1]);
        t.mId = static_cast<int32_t>(a[2]);
        t.mFree = static_cast<int32_t>(a[3]);
        t.mNext = Timer(a[4]);
        t.mPrevious = Timer(a[5]);
        t.mRunning = static_cast<int32_t>(a[6]);
        t.mSlice = static_cast<int32_t>(a[7]);
        t.mStart = static_cast<uint32_t>(a[8]);
        return 0;
    }
    case 7: {   // a queue's entry: queue, index, window, message, a, b
        InterfaceMessage& entry = gMessages[a[0]][a[1]];
        entry.mWindow = Window(a[2]);
        entry.mMessage = static_cast<int32_t>(a[3]);
        entry.mA = static_cast<int32_t>(a[4]);
        entry.mB = static_cast<int32_t>(a[5]);
        return 0;
    }
    case 8: {   // a timed message: index, window, message, a, b, time
        InterfaceTimedMessage& entry = gTimedEntries[a[0]];
        entry.mWindow = Window(a[1]);
        entry.mMessage = static_cast<int32_t>(a[2]);
        entry.mA = static_cast<int32_t>(a[3]);
        entry.mB = static_cast<int32_t>(a[4]);
        entry.mTime = static_cast<uint32_t>(a[5]);
        return 0;
    }
    case 9: {   // a queue's shape: queue (0 general, 1 draw, 2 timed), entries present, count
        if (a[0] == 2) {
            TimedObject()->mEntries = a[1] != 0 ? gTimedEntries : nullptr;
            TimedObject()->mCount = static_cast<int32_t>(a[2]);
        } else {
            QueueObject(a[0])->mEntries = a[1] != 0 ? gMessages[a[0]] : nullptr;
            QueueObject(a[0])->mCount = static_cast<int32_t>(a[2]);
        }
        return 0;
    }
    case 10: {   // many entries of one window in a queue: queue, count, window, message
        for (int64_t i = 0; i < a[1]; ++i) {
            gMessages[a[0]][i].mWindow = Window(a[2]);
            gMessages[a[0]][i].mMessage = static_cast<int32_t>(a[3]);
            gMessages[a[0]][i].mA = static_cast<int32_t>(i);
            gMessages[a[0]][i].mB = static_cast<int32_t>(-i);
        }
        return 0;
    }
    // ---- the functions under test
    case 20: {   // the destructor: slot, flag (0 the plain body, 1 the deleting form)
        InterfaceWindow* w = Window(a[0]);
        if (static_cast<int16_t>(a[1]) > 0) {
            delete w;
        } else {
            w->~InterfaceWindow();
            std::memcpy(static_cast<void*>(w), &gWindowVtablePointer[gKind[a[0]]], sizeof gWindowVtablePointer[0]);
        }
        return 0;
    }
    case 21: return UI_RemoveTimerResult(Window(a[0]), static_cast<int32_t>(a[1]));
    case 22: return L(Timer(a[0])->FindTimer(Window(a[1]), static_cast<int32_t>(a[2])));
    case 23: ClearAllPointers(Window(a[0])); return 0;
    case 24: return UI_InvalidateMessages(Window(a[0]));
    case 25: return UI_InvalidateDrawMessages(Window(a[0]));
    case 26: return UI_InvalidateDeferredMessages(Window(a[0]));
    case 27: return QueueObject(a[0])->InvalidateMsgs(Window(a[1]));
    case 28: return TimedObject()->InvalidateMsgs(Window(a[0]));
    case 29: Window(a[0])->PropagateDestruction(); return 0;
    case 30: return Window(a[0])->UnCaptureKeys();
    // ---- unit w54
    case 31: return Window(a[0])->Enable(static_cast<int16_t>(a[1]));
    case 32: return Window(a[0])->InterfaceWindow::operator-(*Window(a[1]));   // the base's, not the class's override
    case 33: return Window(a[0])->GetHelpText();
    case 34: return L(Window(a[0])->GetText());
    case 35: return Window(a[0])->GetValue();
    case 36: Window(a[0])->DrawFunction(); return 0;
    case 37: {   // a timer's destructor: index, flag (0 the plain body, 1 the deleting form: the class operator delete)
        InterfaceTimer* t = Timer(a[0]);
        if (static_cast<int16_t>(a[1]) > 0) delete t;
        else t->~InterfaceTimer();
        return 0;
    }
    case 38: InterfaceStringHeap::ReleaseString(a[0] >= 0 && a[0] < kStrings ? gStrings[a[0]] : nullptr); return 0;
    case 39: {   // a string's first characters: index, c0, c1, c2 (the others are 0x1000 + i)
        for (int i = 0; i < kStringChars; ++i) gStrings[a[0]][i] = static_cast<char16_t>(0x1000 + i);
        gStrings[a[0]][0] = static_cast<char16_t>(a[1]);
        gStrings[a[0]][1] = static_cast<char16_t>(a[2]);
        gStrings[a[0]][2] = static_cast<char16_t>(a[3]);
        return 0;
    }
    case 40: {   // a text box's text: slot, mode (0 none, 1 a block of the heap, labelled for the slot); a button's: mode 2 + string index
        InterfaceWindow* w = Window(a[0]);
        char16_t* text = nullptr;
        if (a[1] == 1) {
            const int32_t before = tpw_cl_heap[1];
            text = MakeTextBlock();
            tpw_cl_heap[1] = before;   // the PowerPC side makes its block outside the count
            tpw_cl_label(kTextLabel + static_cast<int32_t>(a[0]), text);
        } else if (a[1] >= 2) {
            text = gStrings[a[1] - 2];
        }
        if (gKind[a[0]] == 2) static_cast<InterfaceTextBox*>(w)->mText = text;
        else if (gKind[a[0]] == 3) static_cast<InterfaceButton*>(w)->mText = text;
        return 0;
    }
    case 41: Window(a[0])->mHelpText = static_cast<int32_t>(a[1]); return 0;   // the window's help text number
    case 42: Window(a[0])->mF90 = static_cast<int32_t>(a[1]); return 0;        // the second one (a held button's)
    case 43:   // a button's state byte
        if (gKind[a[0]] == 3) static_cast<InterfaceButton*>(Window(a[0]))->mState = static_cast<int8_t>(a[1]);
        return 0;
    }
    return -1;
}

// ---- dumps. The orders are the Python module's tables.

int32_t tpw_uid_dump_globals(int64_t* out) {
    const int64_t* start = out;
    Put(out, L(gUIPointerIn));
    Put(out, L(gUICaptureDevice));
    Put(out, L(gUIFocusWindow));
    Put(out, L(gUIKeyboardWindow));
    Put(out, gUIKeyCapture.mKeys == nullptr ? 0 : gUIKeyCapture.mKeys == gKeys ? 1 : 9);
    Put(out, L(gUIKeyCapture.mWindow));
    for (InterfaceWindow* window : gUIButtonWindow) Put(out, L(window));
    Put(out, L(gUIMessageQueue));
    Put(out, L(gUIDrawQueue));
    Put(out, L(gUITimedMessages));
    Put(out, gUITimers != nullptr);
    Put(out, gUITimerCount);
    Put(out, L(gUITimerList));
    Put(out, L(gUIHelpText));
    Put(out, L(gUILastWindow));
    return static_cast<int32_t>(out - start);
}

void tpw_uid_dump_window(int32_t slot, int64_t* out) {
    const InterfaceWindow* w = gWindows[slot];
    Put(out, 1);
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
    Put(out, Untouched(w->mSlideStep, sizeof w->mSlideStep) && Untouched(&w->mSlideSteps, sizeof w->mSlideSteps) &&
                 Untouched(w->mF10A, sizeof w->mF10A));
    Put(out, CallbackCode(w->mCallback));
    Put(out, CallbackCode(w->mDefaultCallback));
    Put(out, static_cast<int32_t>(w->mButtonsHeld));
    for (int i = 0; i < 4; ++i) Put(out, L(w->mAnimTimers[i]));
    Put(out, static_cast<int32_t>(w->mF130));
    Put(out, w->mF134);
    Put(out, w->mAnimating);
}

// A queue: its words, then every entry (a checksum when the ring is long).
int32_t tpw_uid_dump_queue(int32_t which, int64_t* out) {
    const int64_t* start = out;
    const InterfaceMessageQueue* q = QueueObject(which);
    Put(out, L(q));
    Put(out, q->mEntries != nullptr);
    Put(out, q->mCount);
    Put(out, q->mHead);
    Put(out, q->mTail);
    Put(out, q->mQueued);
    if (q->mEntries == nullptr || q->mCount <= 0) return static_cast<int32_t>(out - start);
    if (q->mCount > kBigQueue) {
        uint32_t sum = 0x811c9dc5u;
        for (int32_t i = 0; i < q->mCount; ++i) {
            const uint32_t words[4] = {static_cast<uint32_t>(L(q->mEntries[i].mWindow)), static_cast<uint32_t>(q->mEntries[i].mMessage),
                                       static_cast<uint32_t>(q->mEntries[i].mA), static_cast<uint32_t>(q->mEntries[i].mB)};
            for (uint32_t word : words) sum = (sum ^ word) * 0x01000193u;
        }
        Put(out, sum);
        return static_cast<int32_t>(out - start);
    }
    for (int32_t i = 0; i < q->mCount; ++i) {
        Put(out, L(q->mEntries[i].mWindow));
        Put(out, q->mEntries[i].mMessage);
        Put(out, q->mEntries[i].mA);
        Put(out, q->mEntries[i].mB);
    }
    return static_cast<int32_t>(out - start);
}

int32_t tpw_uid_dump_timed(int64_t* out) {
    const int64_t* start = out;
    const InterfaceTimedMessages* t = TimedObject();
    Put(out, t->mEntries != nullptr);
    Put(out, t->mCount);
    if (t->mEntries == nullptr) return static_cast<int32_t>(out - start);
    for (int32_t i = 0; i < t->mCount; ++i) {
        Put(out, L(t->mEntries[i].mWindow));
        Put(out, t->mEntries[i].mMessage);
        Put(out, t->mEntries[i].mA);
        Put(out, t->mEntries[i].mB);
        Put(out, static_cast<int32_t>(t->mEntries[i].mTime));
    }
    return static_cast<int32_t>(out - start);
}

int32_t tpw_uid_dump_timers(int64_t* out) {
    const int64_t* start = out;
    for (int i = 0; i < kTimers; ++i) {
        const InterfaceTimer& t = gTimerTable[i];
        Put(out, L(t.mWindow));
        Put(out, static_cast<int32_t>(t.mStart));
        Put(out, t.mSlice);
        Put(out, t.mId);
        Put(out, t.mRunning);
        Put(out, t.mFree);
        Put(out, L(t.mNext));
        Put(out, L(t.mPrevious));
    }
    return static_cast<int32_t>(out - start);
}

// The draw objects: in library, users, shown.
int32_t tpw_uid_dump_draws(int64_t* out) {
    const int64_t* start = out;
    for (const FakeDraw& draw : gDraws) {
        Put(out, draw.mInLibrary);
        Put(out, draw.mResizable);
        Put(out, draw.mShown);
    }
    return static_cast<int32_t>(out - start);
}

// What a subclass adds to a window (8 words): the bar's three words and rectangle, the text box's block (its label), five words, the
// button's bytes, its text and the repeat words. A plain window answers its kind and zeros.
void tpw_uid_dump_extras(int32_t slot, int64_t* out) {
    const InterfaceWindow* w = gWindows[slot];
    for (int i = 0; i < 8; ++i) out[i] = 0;
    out[0] = gKind[slot];
    if (gKind[slot] == 1) {
        const auto* bar = static_cast<const InterfaceBar*>(w);
        out[1] = bar->mF138;
        out[2] = bar->mF13C;
        out[3] = bar->mValue;
        out[4] = bar->mBarRect.mLeft;
        out[5] = bar->mBarRect.mTop;
        out[6] = bar->mBarRect.mRight;
        out[7] = bar->mBarRect.mBottom;
    } else if (gKind[slot] == 2) {
        const auto* box = static_cast<const InterfaceTextBox*>(w);
        out[1] = L(box->mText);
        out[2] = box->mHorizontal;
        out[3] = box->mVertical;
        out[4] = box->mF144;
        out[5] = box->mTextLength;
        out[6] = box->mTextX;
        out[7] = box->mTextY;
    } else if (gKind[slot] == 3) {
        const auto* button = static_cast<const InterfaceButton*>(w);
        out[1] = button->mState;
        out[2] = button->mHilight;
        out[3] = button->mRepeatDelay;
        out[4] = button->mRepeatInterval;
        out[5] = L(button->mText);
        out[6] = button->mTextShiftX;
        out[7] = button->mHeldState;
    }
}

// The strings of the heap: every character of each.
int32_t tpw_uid_dump_strings(int64_t* out) {
    for (int i = 0; i < kStrings; ++i)
        for (int j = 0; j < kStringChars; ++j) out[i * kStringChars + j] = gStrings[i][j];
    return kStrings * kStringChars;
}

void tpw_uid_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = tpw_cl_heap[i];
}
void tpw_uid_heap_clear() {
    for (int i = 0; i < 4; ++i) tpw_cl_heap[i] = 0;
}

}  // extern "C"

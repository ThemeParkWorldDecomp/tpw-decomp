// Test library for the interface's message pump (libtpw_unit_UIPump.dylib; see tests/test_UIPump.py): src/UI_pump.cpp
// (UIInterface::Process, ProcessUI, the dispatch, onframe, the queues' Post and Peek, the timers, the window accessors,
// TickUIErrors), and under it the real src/UI_init.cpp (the queues' Add and Flush, UI_PostMessage, UI_FlushAllMessages),
// src/InterfaceWindow.cpp (the window constructor), src/UI_Base.cpp (rectangles), src/UI_focus.cpp (the globals) and
// src/ui_DrawObject.cpp.
//
// Everything else the unit calls is a fake that records through calllog: the window handlers (four of them, which also do
// what the case's script says: destroy a window, unlink one, post, flush), the peek callbacks, the clock, highestZ,
// the error dialog and the strings it shows, the mutex, the postcard, the assertion. Windows are made with the real
// constructor in storage filled with 0xa5.
//
// The Python module drives this library through extern "C" operations on a small world: twelve windows in fixed storage,
// a general and a draw queue and a timed queue, a table of six timers, one clock, handlers and peek callbacks, key lists,
// three string tables with their texts, and the error queue. After each operation the dump writes the state in the order
// of the Python module's tables.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"

#include "InterfaceWindow.h"
#include "TbITable.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

// UIInterface::UnPause shares the pump source; this module only exercises Process.
void UnPauseUI() {}

namespace {
constexpr int kSlots = 12;
constexpr int kTimers = 6;
constexpr int32_t kBigQueue = 16;   // a queue of more slots than this is dumped as a checksum
constexpr int kTables = 3;
constexpr int kTexts = 3;
constexpr int kRects = 2;
// object labels (the Python module's LAB_*)
constexpr int kWindowLabel = 10, kQueueLabel = 30, kTimedLabel = 32, kClockLabel = 41, kTimerLabel = 50, kTableLabel = 60,
              kTextLabel = 70, kWideLabel = 75, kRectLabel = 80, kMutexLabel = 85, kFunctionLabel = 90, kKeyListLabel = 100;

alignas(InterfaceWindow) unsigned char gWindowStore[kSlots][sizeof(InterfaceWindow)];
InterfaceWindow* gWindows[kSlots];
bool gMade[kSlots];
alignas(InterfaceMessageQueue) unsigned char gQueueStore[2][sizeof(InterfaceMessageQueue)];
InterfaceMessageQueue* gQueues[2];
alignas(InterfaceTimedMessages) unsigned char gTimedStore[sizeof(InterfaceTimedMessages)];
InterfaceTimedMessages* gTimed;
InterfaceTimer gTimerTable[kTimers];
alignas(16) unsigned char gTableStore[kTables][32];
unsigned char gText[kTexts][8];
char16_t gWide[kTexts][8];
CoolRect gRects[kRects];

// Key lists (the entries end with the modifier -1).
captureKeyList gKeys1[] = {{0x41, 0}, {0x42, 0x10}, {0x41, 0x20}, {0, 0}, {0x1234, 0xff}, {0, -1}};
captureKeyList gKeys2[] = {{0, -1}};
captureKeyList gKeys3[] = {{-1, 0xff}, {0x7fff, 0x80}, {0x41, 0x100}, {0x43, -2}, {0x44, 0x7f}, {0, -1}};
captureKeyList* const gKeyLists[] = {nullptr, gKeys1, gKeys2, gKeys3};

InterfaceWindow* Window(int64_t slot) { return slot >= 0 && slot < kSlots && gMade[slot] ? gWindows[slot] : nullptr; }

// False when posting the message would write through a queue that does not exist or has no slots (the original would crash):
// the draw queue takes messages 1 to 3, the general one the rest; 0x1000a to 0x1000c go to it whatever the window.
bool PostSafe(InterfaceWindow* window, int32_t message) {
    const uint32_t u = static_cast<uint32_t>(message);
    InterfaceMessageQueue* target = u - 1 <= 2 ? gUIDrawQueue : gUIMessageQueue;
    const bool unchecked = u - 0x1000a <= 2;
    const bool adds = unchecked || (target != nullptr && window != nullptr && window->mDestroying == 0);
    return !adds || (target != nullptr && target->mEntries != nullptr && target->mCount > 0);
}

// ---- the window handlers: record, do what the script says, answer
int32_t HandleMessage(int id, InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Handler", {id, lab(window), message, a, b});
    const int64_t action = ret("act", 0);
    switch (action) {
    case 1: window->mDestroying = 1; break;
    case 2:
        if (window->mNext != nullptr) window->mNext = window->mNext->mNext;
        break;
    case 3: {
        InterfaceWindow* target = Window(ret("actarg", -1));
        const int32_t m = static_cast<int32_t>(ret("actarg", 0));
        const int32_t x = static_cast<int32_t>(ret("actarg", 0));
        const int32_t y = static_cast<int32_t>(ret("actarg", 0));
        if (PostSafe(target, m)) UI_PostMessage(target, m, x, y);
        break;
    }
    case 4: UI_FlushAllMessages(); break;
    case 5: gUIPeekCallBack = nullptr; break;
    case 6: window->mChild = nullptr; break;
    case 7: gUIKeyCapture.mKeys = nullptr; break;
    case 8:
        gUIKeyboardWindow = nullptr;
        gUIFocusWindow = nullptr;
        break;
    case 9: window->mNext = nullptr; break;
    case 10: window->mCallback = nullptr; break;
    }
    char key[8];
    std::snprintf(key, sizeof key, "H%d", id);
    return static_cast<int32_t>(ret(key, 0));
}
int32_t Handler1(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) { return HandleMessage(1, w, m, a, b); }
int32_t Handler2(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) { return HandleMessage(2, w, m, a, b); }
int32_t Handler3(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) { return HandleMessage(3, w, m, a, b); }
int32_t Handler4(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) { return HandleMessage(4, w, m, a, b); }
InterfaceWindowCallback const gHandlers[] = {nullptr, Handler1, Handler2, Handler3, Handler4};
int32_t OtherCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- the peek callbacks: record, and maybe change the peek callback (the script's "pact": 1 clears it, 2 swaps A and B)
int32_t PeekA(InterfaceWindow* w, int32_t m, int32_t a, int32_t b);
int32_t PeekB(InterfaceWindow* w, int32_t m, int32_t a, int32_t b);
int32_t PeekCommon(int id, InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    ev("Peek", {id, lab(w), m, a, b});
    const int64_t action = ret("pact", 0);
    if (action == 1) gUIPeekCallBack = nullptr;
    else if (action == 2) gUIPeekCallBack = id == 1 ? PeekB : PeekA;
    return 0;
}
int32_t PeekA(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) { return PeekCommon(1, w, m, a, b); }
int32_t PeekB(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) { return PeekCommon(2, w, m, a, b); }

// The label of a function pointer the unit knows (the dialog's callbacks and the window handlers).
int64_t FunctionLabel(const void* function) {
    if (function == nullptr) return 0;
    if (function == reinterpret_cast<const void*>(&ErrorDlgCancel)) return kFunctionLabel + 6;
    if (function == reinterpret_cast<const void*>(&ErrorMessageCallback)) return kFunctionLabel + 7;
    for (int i = 1; i < 5; ++i)
        if (function == reinterpret_cast<const void*>(gHandlers[i])) return kFunctionLabel + i;
    return -1;
}

class FakeClock : public clockTickFunction {
public:
    uint32_t tickyticky() override {
        ev("tick", {lab(this)});
        return static_cast<uint32_t>(ret("tick", 0));
    }
};
FakeClock gClock;
}  // namespace

// ---- the window's untranslated virtual members and the other classes the unit's files need
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 1; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }
// The handler every window starts with (untranslated): records and answers "Hdef".
int32_t UI_DefaultWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Default", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("Hdef", 0));
}
void activateUI(InterfaceWindow*, int32_t) {}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) { ev("SetCallback", {lab(this), FunctionLabel(reinterpret_cast<const void*>(callback))}); }

// A timer starts free, in no list and not running.
InterfaceTimer::InterfaceTimer() : mWindow(nullptr), mStart(0), mSlice(0), mId(0), mRunning(0), mFree(1), mNext(nullptr), mPrevious(nullptr) {}
InterfaceTimer::~InterfaceTimer() {}

uint32_t clockTickFunction::tickyticky() { return 0; }
void InterfaceHelpText::ShowHelpText() {}
void InterfaceHelpText::HideHelpText() {}
InterfaceHelpText::~InterfaceHelpText() {}
void InterfaceHelpText::Enable() {}
void InterfaceHelpText::Disable() {}
// The virtuals unit w30 added to the class (its vtable must link here too).
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

// ---- the cursor classes UI_init.cpp's set-up reaches (never called here)
CursorTable gCursorTable;
UIInterface gUIInterface;
int32_t gScreenWidth;
void CreateEngineMesh(tag_sMesh*) {}
void DestroyEngineMesh(tag_sMesh*) {}
void DestroyMeshInstance(tag_sMesh*, sHeapInfo*, uint32_t) {}
void EnableMeshDraw(tag_sMesh*, sMatrix*, int32_t) {}
void DisableMeshDraw(tag_sMesh*) {}
void InterfaceDrawEngineMesh::Change(uint32_t, DOChange*) {}
InterfaceDrawObject* InterfaceDrawEngineMesh::InstanceMe() { return this; }
void InterfaceDrawEngineResizableFrame::Change(uint32_t, DOChange*) {}
InterfaceDrawObject* InterfaceDrawEngineResizableFrame::InstanceMe() { return this; }
void InterfaceDrawEngineResizableSlider::Change(uint32_t, DOChange*) {}
InterfaceDrawObject* InterfaceDrawEngineResizableSlider::InstanceMe() { return this; }

// ---- the unit's callees
int32_t highestZ(InterfaceWindow* window, CoolRect* rect) {
    ev("highestZ", {lab(window), lab(rect)});
    return static_cast<int32_t>(ret("highestZ", 0));
}
void UI_ActuallyDoPostcard() {
    ev("Postcard");
    if (ret("pc.clear", 1) != 0) gPostcardWanted = 0;
}
void bthread_mutex::lock() { ev("lock", {lab(this)}); }
void bthread_mutex::unlock() { ev("unlock", {lab(this)}); }
const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {lab(this), id});
    return gText[ret("text", 0) % kTexts];
}
char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    ev("ConvertMulti", {lab(text)});
    return gWide[ret("wide", 0) % kTexts];
}
// The dialog: records every word of the parameters and every argument; answers one of the world's windows (or NULL).
InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2,
                                    dlgcallback callback2, dlgbutton button3, dlgcallback callback3, bool flag) {
    ev("DialogBox", {lab(params.mText), (params.mRed << 16) | (params.mGreen << 8) | params.mBlue, params.mValue8, params.mValueC,
                     button1, FunctionLabel(reinterpret_cast<const void*>(callback1)), button2,
                     FunctionLabel(reinterpret_cast<const void*>(callback2)), button3,
                     FunctionLabel(reinterpret_cast<const void*>(callback3)), flag});
    return Window(ret("dialog", -1));
}
int32_t ErrorDlgCancel(void*) { return 0; }
int32_t ErrorMessageCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }

namespace {
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

// 0 none, 1 to 4 a handler, 5 the default callback, 6 any other.
int64_t CallbackCode(InterfaceWindowCallback callback) {
    if (callback == nullptr) return 0;
    for (int i = 1; i < 5; ++i)
        if (callback == gHandlers[i]) return i;
    return callback == UI_DefaultWindowCallback ? 5 : 6;
}

// The error queue and the dialog and the table state are not heap traffic the original has: the host's deque allocates
// blocks that the Mac's (the C++ runtime's) does not count here.
struct HeapGuard {
    int32_t saved[4];
    HeapGuard() { for (int i = 0; i < 4; ++i) saved[i] = tpw_cl_heap[i]; }
    ~HeapGuard() { for (int i = 0; i < 4; ++i) tpw_cl_heap[i] = saved[i]; }
};
}  // namespace

extern "C" {

// Resets the world and the globals for a case, to what the Mac's data and BSS hold before the interface starts.
void tpw_pump_reset() {
    gUIRootWindow = nullptr;
    gUIActive = 1;
    gUIButtonFlags = 0;
    gUIPointerIn = nullptr;
    gUICaptureDevice = nullptr;
    gUIKeyCapture.mKeys = nullptr;
    gUIKeyCapture.mWindow = nullptr;
    gUIDrawQueue = nullptr;
    gUIMessageQueue = nullptr;
    gUITimedMessages = nullptr;
    gUIPeekCallBack = nullptr;
    gUIStarted = 0;
    gUIHebrew = false;
    gUIPointerX = 0;
    gUIPointerY = 0;
    gUICursor = nullptr;
    gUICursorTable = nullptr;
    gUIStandardCursor = 0;
    gUICursorNumber = -1;
    gUIHelpText = nullptr;
    gUIClock = &gClock;
    gUITimers = nullptr;
    gUITimerCount = 0;
    gUITimerList = nullptr;
    gUITimersPaused = 0;
    gUILastWindow = nullptr;
    gUIKeyboardWindow = nullptr;
    gUIFocusWindow = nullptr;
    gUIOnFrameCount = 0;
    gUIGeneralQueued = 0;
    gUIDrawQueued = 0;
    gPostcardDelay = 0;
    gPostcardWanted = 0;
    gUIErrorDialog = nullptr;
    gUIErrorQueue.clear();
    for (int i = 0; i < kSlots; ++i) {
        gWindows[i] = nullptr;
        gMade[i] = false;
    }
    gQueues[0] = gQueues[1] = nullptr;
    gTimed = nullptr;
    for (int i = 0; i < kTimers; ++i) new (&gTimerTable[i]) InterfaceTimer;
    for (int t = 0; t < kTexts; ++t) {
        for (int i = 0; i < 8; ++i) {
            gText[t][i] = static_cast<unsigned char>('a' + t);
            gWide[t][i] = static_cast<char16_t>('A' + t);
        }
    }
}

// Names the world's fixed objects for the call log.
void tpw_pump_labels() {
    for (int i = 0; i < kSlots; ++i) tpw_cl_label(kWindowLabel + i, gWindowStore[i]);
    tpw_cl_label(kQueueLabel, gQueueStore[0]);
    tpw_cl_label(kQueueLabel + 1, gQueueStore[1]);
    tpw_cl_label(kTimedLabel, gTimedStore);
    tpw_cl_label(kClockLabel, &gClock);
    for (int i = 0; i < kTimers; ++i) tpw_cl_label(kTimerLabel + i, &gTimerTable[i]);
    for (int i = 0; i < kTables; ++i) tpw_cl_label(kTableLabel + i, gTableStore[i]);
    for (int i = 0; i < kTexts; ++i) {
        tpw_cl_label(kTextLabel + i, gText[i]);
        tpw_cl_label(kWideLabel + i, gWide[i]);
    }
    for (int i = 0; i < kRects; ++i) tpw_cl_label(kRectLabel + i, &gRects[i]);
    tpw_cl_label(kMutexLabel, &gUIErrorMutex);
    for (int i = 1; i < 4; ++i) tpw_cl_label(kKeyListLabel + i, gKeyLists[i]);
}

// A window made with the real constructor in storage filled with 0xa5.
int64_t tpw_pump_make_window(int32_t slot, int32_t parent, int32_t attributes, int32_t id, int32_t left, int32_t top, int32_t right,
                             int32_t bottom) {
    std::memset(gWindowStore[slot], 0xa5, sizeof gWindowStore[slot]);
    gWindows[slot] = new (gWindowStore[slot]) InterfaceWindow(Window(parent), attributes, id, static_cast<int16_t>(left),
                                                              static_cast<int16_t>(top), static_cast<int16_t>(right),
                                                              static_cast<int16_t>(bottom));
    gMade[slot] = true;
    return L(gWindows[slot]);
}

// The queues and the timed queue, made and sized with the real members.
void tpw_pump_make_queues(int32_t general, int32_t draw, int32_t timed) {
    std::memset(gQueueStore, 0xa5, sizeof gQueueStore);
    std::memset(gTimedStore, 0xa5, sizeof gTimedStore);
    gQueues[0] = new (gQueueStore[0]) InterfaceMessageQueue;
    gQueues[1] = new (gQueueStore[1]) InterfaceMessageQueue;
    if (general > 0) gQueues[0]->Initialise(general);
    if (draw > 0) gQueues[1]->Initialise(draw);
    gTimed = new (gTimedStore) InterfaceTimedMessages;
    if (timed >= 0) {
        gTimed->Initialise(timed);
        // the entries start all zero (the PowerPC side's start as the allocation was, so both are cleared)
        if (gTimed->mEntries != nullptr) std::memset(static_cast<void*>(gTimed->mEntries), 0, sizeof(InterfaceTimedMessage) * timed);
    }
}

namespace {
InterfaceMessageQueue* QueueOf(int64_t k) { return k >= 0 && k < 2 ? gQueues[k] : nullptr; }
InterfaceMessageQueue* QueueGlobal(int64_t v) { return v == 1 ? gQueues[0] : v == 2 ? gQueues[1] : nullptr; }
}  // namespace

// One operation of the script; a[] are its arguments. Returns the answer (0 when it has none).
int64_t tpw_pump_op(int32_t op, const int64_t* a) {
    switch (op) {
    case 1: {   // a window's fields: slot, attributes, destroying byte, handler
        InterfaceWindow* w = Window(a[0]);
        w->mAttributes = static_cast<uint32_t>(a[1]);
        w->mDestroying = static_cast<uint8_t>(a[2]);
        w->mCallback = a[3] >= 1 && a[3] <= 4 ? gHandlers[a[3]] : a[3] == 5 ? UI_DefaultWindowCallback : a[3] == 6 ? OtherCallback : nullptr;
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
        case 1: gUIRootWindow = Window(v); break;
        case 2: gUIPointerIn = Window(v); break;
        case 3: gUICaptureDevice = Window(v); break;
        case 4: gUIKeyboardWindow = Window(v); break;
        case 5: gUIFocusWindow = Window(v); break;
        case 6: gUIKeyCapture.mWindow = Window(v); break;
        case 7: gUIKeyCapture.mKeys = gKeyLists[v]; break;
        case 8: gUIActive = static_cast<int32_t>(v); break;
        case 9: gUITimersPaused = static_cast<int32_t>(v); break;
        case 10: gUIPeekCallBack = v == 0 ? nullptr : v == 1 ? PeekA : PeekB; break;
        case 11: gUIMessageQueue = QueueGlobal(v); break;
        case 12: gUIDrawQueue = QueueGlobal(v); break;
        case 13: gUITimedMessages = v == 0 ? nullptr : gTimed; break;
        case 15:   // the timer table: 0 none (count 0), n the table with n timers
            gUITimers = v == 0 ? nullptr : gTimerTable;
            gUITimerCount = static_cast<int32_t>(v);
            break;
        case 16: gUITimerList = v >= 0 && v < kTimers ? &gTimerTable[v] : nullptr; break;
        case 17: gPostcardDelay = static_cast<int32_t>(v); break;
        case 18: gPostcardWanted = static_cast<int32_t>(v); break;
        case 19: gUIErrorDialog = Window(v); break;
        case 20: gUIOnFrameCount = static_cast<int32_t>(v); break;
        }
        return 0;
    }
    case 4: return QueueOf(a[0])->Add(Window(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]));
    case 5: {   // a queue's entry: queue, index, field (0 window, 1 message, 2 and 3 the arguments), value
        InterfaceMessage& entry = QueueOf(a[0])->mEntries[a[1]];
        if (a[2] == 0) entry.mWindow = Window(a[3]);
        else if (a[2] == 1) entry.mMessage = static_cast<int32_t>(a[3]);
        else if (a[2] == 2) entry.mA = static_cast<int32_t>(a[3]);
        else entry.mB = static_cast<int32_t>(a[3]);
        return 0;
    }
    case 6: {   // a queue's words: queue, head, tail, queued
        InterfaceMessageQueue* q = QueueOf(a[0]);
        q->mHead = static_cast<int32_t>(a[1]);
        q->mTail = static_cast<int32_t>(a[2]);
        q->mQueued = static_cast<int32_t>(a[3]);
        return 0;
    }
    case 7: {   // a timed message: index, window, message, a, b, time
        InterfaceTimedMessage& entry = gTimed->mEntries[a[0]];
        entry.mWindow = Window(a[1]);
        entry.mMessage = static_cast<int32_t>(a[2]);
        entry.mA = static_cast<int32_t>(a[3]);
        entry.mB = static_cast<int32_t>(a[4]);
        entry.mTime = static_cast<uint32_t>(a[5]);
        return 0;
    }
    case 8: {   // a timer: index, window, start, slice, id, running, free, next, previous
        InterfaceTimer& t = gTimerTable[a[0]];
        t.mWindow = Window(a[1]);
        t.mStart = static_cast<uint32_t>(a[2]);
        t.mSlice = static_cast<int32_t>(a[3]);
        t.mId = static_cast<int32_t>(a[4]);
        t.mRunning = static_cast<int32_t>(a[5]);
        t.mFree = static_cast<int32_t>(a[6]);
        t.mNext = a[7] >= 0 && a[7] < kTimers ? &gTimerTable[a[7]] : nullptr;
        t.mPrevious = a[8] >= 0 && a[8] < kTimers ? &gTimerTable[a[8]] : nullptr;
        return 0;
    }
    case 9: {   // an error: table, id
        HeapGuard guard;
        gUIErrorQueue.push_back(errorthingy{reinterpret_cast<TbITable*>(gTableStore[a[0]]), static_cast<uint32_t>(a[1])});
        return 0;
    }
    case 10: {
        HeapGuard guard;
        gUIErrorQueue.clear();
        return 0;
    }
    case 11: {   // many messages added to a queue: queue, count, window, message, first argument (the second counts up)
        int64_t added = 0;
        for (int64_t i = 0; i < a[1]; ++i)
            added += QueueOf(a[0])->Add(Window(a[2]), static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4] + i), static_cast<int32_t>(i));
        return added;
    }
    // ---- the functions under test
    case 20: {
        HeapGuard guard;
        gUIInterface.Process();
        return 0;
    }
    case 21: {
        HeapGuard guard;
        ProcessUI();
        return 0;
    }
    case 22: return UI_DispatchMessages(static_cast<int32_t>(a[0]));
    case 23: return UI_DispatchDrawMessages(static_cast<int32_t>(a[0]));
    case 24: return UI_DispatchDeferredMessages();
    case 25: return UI_SendOnFrameMessages();
    case 26: onframe(Window(a[0])); return 0;
    case 27: return UI_SendMessage(Window(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]));
    case 28: UI_ServiceAllTimers(); return 0;
    case 29: return L(UI_AddTimer(Window(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])));
    case 30: return IveGotTicks();
    case 31: return QueueOf(a[0])->Post();
    case 32: {
        InterfaceMessage* message = QueueOf(a[0])->Peek();
        if (message == nullptr) return -1;
        return message - QueueOf(a[0])->mEntries;
    }
    case 33: return QueueOf(a[0])->GetNumQd();
    case 34: return gTimed->Process();
    case 35: return Window(a[0])->CallWindowHandler(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]));
    case 36: {   // an accessor: slot, which (0 AmIGettingDestroyed, 1 GetChild, 2 GetNext, 3 GetAttr)
        InterfaceWindow* w = Window(a[0]);
        if (a[1] == 0) return w->AmIGettingDestroyed();
        if (a[1] == 1) return L(w->GetChild(static_cast<int32_t>(a[2])));
        if (a[1] == 2) return L(w->GetNext());
        return w->GetAttr();
    }
    case 37: return UI_GetHighestZ(Window(a[0]), a[1] >= 1 ? &gRects[a[1] - 1] : nullptr);
    case 38: return UI_PostcardPending() ? 1 : 0;
    case 39: {
        HeapGuard guard;
        TickUIErrors();
        return 0;
    }
    }
    return -1;
}

// ---- dumps. The orders are the Python module's tables.

int32_t tpw_pump_dump_globals(int64_t* out) {
    const int64_t* start = out;
    Put(out, L(gUIRootWindow));
    Put(out, L(gUIPointerIn));
    Put(out, L(gUICaptureDevice));
    Put(out, gUIKeyCapture.mKeys == nullptr ? 0 : gUIKeyCapture.mKeys == gKeys1 ? 1 : gUIKeyCapture.mKeys == gKeys2 ? 2 : gUIKeyCapture.mKeys == gKeys3 ? 3 : 9);
    Put(out, L(gUIKeyCapture.mWindow));
    Put(out, L(gUIDrawQueue));
    Put(out, L(gUIMessageQueue));
    Put(out, L(gUITimedMessages));
    Put(out, gUIPeekCallBack == nullptr ? 0 : gUIPeekCallBack == PeekA ? 1 : gUIPeekCallBack == PeekB ? 2 : 3);
    Put(out, gUIStarted);
    Put(out, gUIHebrew);
    Put(out, gUIPointerX);
    Put(out, gUIPointerY);
    Put(out, L(gUICursor));
    Put(out, L(gUICursorTable));
    Put(out, gUIStandardCursor);
    Put(out, gUICursorNumber);
    Put(out, L(gUIHelpText));
    Put(out, L(gUIClock));
    Put(out, gUITimers != nullptr);
    Put(out, gUITimerCount);
    Put(out, L(gUITimerList));
    Put(out, gUITimersPaused);
    Put(out, L(gUILastWindow));
    Put(out, gUIButtonFlags);
    Put(out, gUIActive);
    // the pump's own
    Put(out, L(gUIKeyboardWindow));
    Put(out, L(gUIFocusWindow));
    Put(out, gUIOnFrameCount);
    Put(out, gUIGeneralQueued);
    Put(out, gUIDrawQueued);
    Put(out, gPostcardDelay);
    Put(out, gPostcardWanted);
    Put(out, L(gUIErrorDialog));
    Put(out, static_cast<int64_t>(gUIErrorQueue.size()));
    return static_cast<int32_t>(out - start);
}

void tpw_pump_dump_window(int32_t slot, int64_t* out) {
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

// A queue: its words, then every entry.
int32_t tpw_pump_dump_queue(int32_t which, int64_t* out) {
    const int64_t* start = out;
    const InterfaceMessageQueue* q = gQueues[which];
    Put(out, L(q));
    Put(out, q->mEntries != nullptr);
    Put(out, q->mCount);
    Put(out, q->mHead);
    Put(out, q->mTail);
    Put(out, q->mQueued);
    if (q->mCount > kBigQueue) {   // a long ring: a checksum of its entries stands for them
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

int32_t tpw_pump_dump_timed(int64_t* out) {
    const int64_t* start = out;
    Put(out, L(gTimed));
    Put(out, gTimed->mEntries != nullptr);
    Put(out, gTimed->mCount);
    for (int32_t i = 0; i < gTimed->mCount; ++i) {
        Put(out, L(gTimed->mEntries[i].mWindow));
        Put(out, gTimed->mEntries[i].mMessage);
        Put(out, gTimed->mEntries[i].mA);
        Put(out, gTimed->mEntries[i].mB);
        Put(out, static_cast<int32_t>(gTimed->mEntries[i].mTime));
    }
    return static_cast<int32_t>(out - start);
}

int32_t tpw_pump_dump_timers(int64_t* out) {
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

// The errors waiting, oldest first: the table's label and the id.
int32_t tpw_pump_dump_errors(int64_t* out) {
    const int64_t* start = out;
    for (const errorthingy& e : gUIErrorQueue) {
        Put(out, L(e.mTable));
        Put(out, static_cast<int32_t>(e.mId));
    }
    return static_cast<int32_t>(out - start);
}

void tpw_pump_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = tpw_cl_heap[i];
}
void tpw_pump_heap_clear() {
    for (int i = 0; i < 4; ++i) tpw_cl_heap[i] = 0;
}

}  // extern "C"

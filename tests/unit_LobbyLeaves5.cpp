// Test library for the lobby's small leaves part 5 (unit w64; libtpw_unit_LobbyLeaves5.dylib, tests/test_LobbyLeaves5.py):
//   src/ui_mainpanel2_pause.cpp  HidePauseWindow 0x101a8744
//   src/lobby_start_leaves.cpp   InterfaceStringHeap::DeAllocate 0x101d7978
//   src/UI_close.cpp             UI_CloseSystem 0x101c8c2c, UI_CloseTimerSystem 0x101d8c2c,
//                                InterfaceMessageQueue::~InterfaceMessageQueue 0x101c7b24, InterfaceTimedMessages::~InterfaceTimedMessages 0x101c8214
//
// Real: the four sources above and the timer's destructor (src/UI_timers_remove.cpp). Fakes that record the same event on both sides:
// UI_SendMessage, UIMain_DecideKbCapture, DrawObjectLibrary::RemoveAll, CursorLibrary::RemoveAll and the window's destructor (a
// recording TestWindow whose class operator delete records too). The three heap counters and which globals are set at the time are part
// of the events of the last four, so the ORDER of the frees and of the clearing shows in the log.
//
// A case is a world (the root, focus, pointer and pause windows, the three queue objects with their slot arrays, the string heap's
// tables, the timer table with its links and three timers outside it) and an operation; tpw_l5_run builds the world, runs the operation
// with the call log on and dumps everything the operation could change. The world is built over objects made with operator new, so a
// deleted object is told by its label going away. Mac sizes are never used here: the PowerPC side builds the same world in emulator
// memory at the Mac's sizes (tests/test_LobbyLeaves5.py).
#include <cstdint>
#include <cstring>
#include <new>

#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "calllog.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "fake_arena_on_heap.h"

using namespace calllog;

// ---- globals of other files that the unit owns here
InterfaceWindow* gUIRootWindow;
InterfaceWindow* gUIFocusWindow;
InterfaceWindow* gUIPointerIn;
InterfaceMessageQueue* gUIMessageQueue;
InterfaceMessageQueue* gUIDrawQueue;
InterfaceTimedMessages* gUITimedMessages;
InterfaceTimer* gUITimers;
int32_t gUITimerCount;
InterfaceTimer* gUITimerList;

// The string heap's globals (src/lobby_start_leaves.cpp defines them without a header).
extern uint8_t* gStringHeapTableB;
extern uint8_t* gStringHeapTableA;
extern int32_t gStringHeapRowsB;
extern int32_t gStringHeapWidthB;
extern int32_t gStringHeapRowsA;
extern int32_t gStringHeapWidthA;

namespace {

enum Label : int32_t {
    kWindow = 10,        // 10 .. 13
    kMessageQueue = 20, kMessageSlots = 21, kDrawQueue = 22, kDrawSlots = 23, kTimed = 24, kTimedSlots = 25,
    kTableA = 30, kTableB = 31,
};
constexpr int kWindows = 4;
constexpr int kOutside = 3;
constexpr int kMaxTimers = 32;

// A window of the world: deleting one records (and counts the block, as the PowerPC side's fake destructor does).
class TestWindow : public InterfaceWindow {
public:
    TestWindow() : InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0) {}
    static void operator delete(void* object) {
        ++tpw_cl_heap[2];
        ev("delete", {lab(object)});
        forget(object);
    }
};

TestWindow* gWindow[kWindows];
InterfaceMessageQueue* gMessageQueueObject;
InterfaceMessageQueue* gDrawQueueObject;
InterfaceTimedMessages* gTimedObject;
InterfaceMessage* gMessageSlots;
InterfaceMessage* gDrawSlots;
InterfaceTimedMessage* gTimedSlots;
uint8_t* gTableABlock;
uint8_t* gTableBBlock;
InterfaceTimer* gTable;
int32_t gTableCount;
InterfaceTimer gOutside[kOutside];


// What is set at this moment and how many blocks have been given back: the events of the fakes that run in the middle of
// UI_CloseSystem carry it.
int64_t Snap(int i) {
    switch (i) {
    case 0: return gUIRootWindow != nullptr;
    case 1: return gUIMessageQueue != nullptr;
    case 2: return gUIDrawQueue != nullptr;
    case 3: return gUITimedMessages != nullptr;
    case 4: return gUIFocusWindow != nullptr;
    case 5: return gUIPointerIn != nullptr;
    case 6: return gUITimerList != nullptr;
    case 7: return gPauseWindow != nullptr;
    case 8: return tpw_cl_heap[2];
    default: return tpw_cl_heap[3];
    }
}
#define SNAP Snap(0), Snap(1), Snap(2), Snap(3), Snap(4), Snap(5), Snap(6), Snap(7), Snap(8), Snap(9)

}  // namespace

// ---- the window: its constructor, destructor and the other virtuals are the unit's (the vtable is emitted with DrawFunction)
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this), SNAP}); }
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { return 0; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

// ---- the classes' constructors (src/UI_init.cpp is not linked)
InterfaceMessageQueue::InterfaceMessageQueue() : mEntries(nullptr), mCount(0), mHead(0), mTail(0), mQueued(0) {}
InterfaceTimedMessages::InterfaceTimedMessages() : mCount(0), mEntries(nullptr) {}
InterfaceTimer::InterfaceTimer() {}

// ---- fakes
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b, Snap(7)});
    return 0;
}
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {Snap(7)}); }
void DrawObjectLibrary::RemoveAll(int16_t destroyObjects) { ev("DrawObjectLibrary.RemoveAll", {destroyObjects, SNAP}); }
void CursorLibrary::RemoveAll() { ev("CursorLibrary.RemoveAll", {SNAP}); }
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }

namespace {

// A description is a flat list of numbers, read in order.
struct Reader {
    const int64_t* data;
    int32_t at;
    int64_t next() { return data[at++]; }
    int32_t i32() { return static_cast<int32_t>(next()); }
    uint32_t u32() { return static_cast<uint32_t>(next()); }
};

// The place a window pointer has in the world: 0 for NULL, 1 to 4 for the four windows, -1 otherwise.
int64_t WindowRef(const InterfaceWindow* window) {
    if (window == nullptr) return 0;
    for (int i = 0; i < kWindows; ++i)
        if (window == gWindow[i]) return i + 1;
    return -1;
}
InterfaceWindow* WindowFromRef(int64_t ref) { return ref >= 1 && ref <= kWindows ? gWindow[ref - 1] : nullptr; }

// A timer pointer: 0 for NULL, 1 to the table's count for the table's timers, 101 to 103 for the outside ones, -1 otherwise.
int64_t TimerRef(const InterfaceTimer* timer) {
    if (timer == nullptr) return 0;
    for (int i = 0; i < gTableCount; ++i)
        if (timer == &gTable[i]) return i + 1;
    for (int i = 0; i < kOutside; ++i)
        if (timer == &gOutside[i]) return 101 + i;
    return -1;
}
InterfaceTimer* TimerFromRef(int64_t ref) {
    if (ref >= 1 && ref <= gTableCount) return &gTable[ref - 1];
    if (ref >= 101 && ref < 101 + kOutside) return &gOutside[ref - 101];
    return nullptr;
}

int64_t QueueRef(const void* queue) {
    if (queue == nullptr) return 0;
    if (queue == gMessageQueueObject) return 1;
    if (queue == gDrawQueueObject) return 2;
    if (queue == gTimedObject) return 3;
    return -1;
}

bool Alive(const void* object) { return object != nullptr && lab(object) > 0; }

int64_t* gOut;
int32_t gCount;
void Put(int64_t value) { gOut[gCount++] = value; }

void ReadTimer(Reader& in, InterfaceTimer* timer, int64_t* links) {
    timer->mWindow = WindowFromRef(in.next());
    timer->mStart = in.u32();
    timer->mSlice = in.i32();
    timer->mId = in.i32();
    timer->mRunning = in.i32();
    timer->mFree = in.i32();
    links[0] = in.next();   // next
    links[1] = in.next();   // previous
}

void PutTimer(const InterfaceTimer* timer) {
    Put(WindowRef(timer->mWindow));
    Put(timer->mStart);
    Put(timer->mSlice);
    Put(timer->mId);
    Put(timer->mRunning);
    Put(timer->mFree);
    Put(TimerRef(timer->mNext));
    Put(TimerRef(timer->mPrevious));
}

// A queue's four words and its slots, from the description: kind 0 none, 1 an object with n slots, 2 an object without.
InterfaceMessageQueue* ReadQueue(Reader& in, int32_t objectLabel, int32_t slotsLabel, InterfaceMessage** slots) {
    const int32_t kind = in.i32();
    const int32_t n = in.i32();
    const int32_t count = in.i32();
    const int32_t head = in.i32();
    const int32_t tail = in.i32();
    const int32_t queued = in.i32();
    *slots = nullptr;
    if (kind == 0) return nullptr;
    InterfaceMessageQueue* queue = new InterfaceMessageQueue;
    if (kind == 1) {
        *slots = new InterfaceMessage[n];
        tpw_cl_label(slotsLabel, *slots);
        queue->mEntries = *slots;
    }
    queue->mCount = count;
    queue->mHead = head;
    queue->mTail = tail;
    queue->mQueued = queued;
    tpw_cl_label(objectLabel, queue);
    return queue;
}

void PutQueue(const InterfaceMessageQueue* queue, const InterfaceMessage* slots) {
    const bool alive = Alive(queue);
    Put(alive);
    Put(alive ? (queue->mEntries == nullptr ? 0 : queue->mEntries == slots ? 1 : -1) : 0);
    Put(alive ? queue->mCount : 0);
    Put(alive ? queue->mHead : 0);
    Put(alive ? queue->mTail : 0);
    Put(alive ? queue->mQueued : 0);
}

}  // namespace

extern "C" {

// The description, in order:
//   op, target, the root, focus, pointer and pause windows (0 none, else the window 1 to 4);
//   the general queue, the draw queue (kind, slots, the four words of each);
//   the timed messages (kind, slots, the count);
//   the string heap: the size of table A and of table B (0: none), then the four words rows B, width B, rows A, width A;
//   the timer table: its timers (0: no table), the global count, the list head (a timer ref), three outside timers and the table's,
//   each (window, start, slice, id, running, free, next, previous).
//   ops: 1 HidePauseWindow  2 InterfaceStringHeap::DeAllocate  3 ~InterfaceMessageQueue (target 0 general, 1 draw)  4 delete of that queue
//        5 ~InterfaceTimedMessages  6 delete of it  7 UI_CloseTimerSystem  8 UI_CloseSystem
// out: the answer, the globals as places, which objects are alive, the queues' words, the outside timers, the table's timers (not for
// ops 7 and 8, which free the table) and the two delete counters. Answers the number of words.
int32_t tpw_l5_run(const int64_t* description, int64_t* out) {
    gOut = out;
    gCount = 0;
    Reader in{description, 0};
    const int32_t op = in.i32();
    const int32_t target = in.i32();
    const int64_t root = in.next();
    const int64_t focus = in.next();
    const int64_t pointer = in.next();
    const int64_t pause = in.next();

    tpw_cl_on = 0;
    for (int i = 0; i < kWindows; ++i) {
        gWindow[i] = new TestWindow;
        tpw_cl_label(kWindow + i, gWindow[i]);
    }
    gMessageQueueObject = ReadQueue(in, kMessageQueue, kMessageSlots, &gMessageSlots);
    gDrawQueueObject = ReadQueue(in, kDrawQueue, kDrawSlots, &gDrawSlots);
    gTimedObject = nullptr;
    gTimedSlots = nullptr;
    {
        const int32_t kind = in.i32();
        const int32_t n = in.i32();
        const int32_t count = in.i32();
        if (kind != 0) {
            gTimedObject = new InterfaceTimedMessages;
            if (kind == 1) {
                gTimedSlots = new InterfaceTimedMessage[n];
                tpw_cl_label(kTimedSlots, gTimedSlots);
                gTimedObject->mEntries = gTimedSlots;
            }
            gTimedObject->mCount = count;
            tpw_cl_label(kTimed, gTimedObject);
        }
    }
    const int32_t sizeA = in.i32();
    const int32_t sizeB = in.i32();
    gTableABlock = sizeA > 0 ? new uint8_t[sizeA] : nullptr;
    gTableBBlock = sizeB > 0 ? new uint8_t[sizeB] : nullptr;
    tpw_cl_label(kTableA, gTableABlock);
    tpw_cl_label(kTableB, gTableBBlock);
    gStringHeapTableA = gTableABlock;
    gStringHeapTableB = gTableBBlock;
    gStringHeapRowsB = in.i32();
    gStringHeapWidthB = in.i32();
    gStringHeapRowsA = in.i32();
    gStringHeapWidthA = in.i32();

    gTableCount = in.i32();
    gTable = gTableCount > 0 ? new InterfaceTimer[gTableCount] : nullptr;
    const int32_t timerCount = in.i32();
    const int64_t listHead = in.next();
    int64_t links[kOutside + kMaxTimers][2];
    for (int i = 0; i < kOutside; ++i) ReadTimer(in, &gOutside[i], links[i]);
    for (int i = 0; i < gTableCount; ++i) ReadTimer(in, &gTable[i], links[kOutside + i]);
    for (int i = 0; i < kOutside; ++i) {
        gOutside[i].mNext = TimerFromRef(links[i][0]);
        gOutside[i].mPrevious = TimerFromRef(links[i][1]);
    }
    for (int i = 0; i < gTableCount; ++i) {
        gTable[i].mNext = TimerFromRef(links[kOutside + i][0]);
        gTable[i].mPrevious = TimerFromRef(links[kOutside + i][1]);
    }

    gUIRootWindow = WindowFromRef(root);
    gUIFocusWindow = WindowFromRef(focus);
    gUIPointerIn = WindowFromRef(pointer);
    gPauseWindow = WindowFromRef(pause);
    gUIMessageQueue = gMessageQueueObject;
    gUIDrawQueue = gDrawQueueObject;
    gUITimedMessages = gTimedObject;
    gUITimers = gTable;
    gUITimerCount = timerCount;
    gUITimerList = TimerFromRef(listHead);

    // Places before the operation (the operation frees things and clears globals).
    const int64_t rootRef = root, tableWas = gTable != nullptr;
    InterfaceMessageQueue* queue = target == 0 ? gMessageQueueObject : gDrawQueueObject;
    tpw_cl_heap[0] = tpw_cl_heap[1] = tpw_cl_heap[2] = tpw_cl_heap[3] = 0;
    tpw_cl_on = 1;
    int64_t result = 0;
    switch (op) {
    case 1: HidePauseWindow(); break;
    case 2: InterfaceStringHeap::DeAllocate(); break;
    case 3: queue->~InterfaceMessageQueue(); break;
    case 4: delete queue; break;
    case 5: gTimedObject->~InterfaceTimedMessages(); break;
    case 6: delete gTimedObject; break;
    case 7: UI_CloseTimerSystem(); break;
    case 8: result = UI_CloseSystem(); break;
    }
    tpw_cl_on = 0;

    // The dump.
    Put(result);
    Put(WindowRef(gUIRootWindow));
    Put(WindowRef(gUIFocusWindow));
    Put(WindowRef(gUIPointerIn));
    Put(WindowRef(gPauseWindow));
    Put(QueueRef(gUIMessageQueue));
    Put(QueueRef(gUIDrawQueue));
    Put(QueueRef(gUITimedMessages));
    Put(gUITimers == nullptr ? 0 : gUITimers == gTable ? 1 : -1);
    Put(gUITimerCount);
    Put(TimerRef(gUITimerList));
    Put(gStringHeapTableA == nullptr ? 0 : gStringHeapTableA == gTableABlock ? 1 : -1);
    Put(gStringHeapTableB == nullptr ? 0 : gStringHeapTableB == gTableBBlock ? 1 : -1);
    Put(gStringHeapRowsB);
    Put(gStringHeapWidthB);
    Put(gStringHeapRowsA);
    Put(gStringHeapWidthA);
    for (int i = 0; i < kWindows; ++i) Put(Alive(gWindow[i]));
    PutQueue(gMessageQueueObject, gMessageSlots);
    PutQueue(gDrawQueueObject, gDrawSlots);
    {
        const bool alive = Alive(gTimedObject);
        Put(alive);
        Put(alive ? gTimedObject->mCount : 0);
        Put(alive ? (gTimedObject->mEntries == nullptr ? 0 : gTimedObject->mEntries == gTimedSlots ? 1 : -1) : 0);
    }
    Put(Alive(gMessageSlots));
    Put(Alive(gDrawSlots));
    Put(Alive(gTimedSlots));
    Put(Alive(gTableABlock));
    Put(Alive(gTableBBlock));
    for (int i = 0; i < kOutside; ++i) PutTimer(&gOutside[i]);
    const bool tableFreed = op == 7 || (op == 8 && rootRef != 0);
    if (!tableFreed && tableWas)
        for (int i = 0; i < gTableCount; ++i) PutTimer(&gTable[i]);
    Put(tpw_cl_heap[2]);
    Put(tpw_cl_heap[3]);
    const int32_t words = gCount;

    // Clean up what is still alive.
    for (int i = 0; i < kWindows; ++i)
        if (Alive(gWindow[i])) delete gWindow[i];
    if (Alive(gMessageSlots)) delete[] gMessageSlots;
    if (Alive(gDrawSlots)) delete[] gDrawSlots;
    if (Alive(gTimedSlots)) delete[] gTimedSlots;
    // (the slots are gone or freed above; the objects must not free them again)
    if (Alive(gMessageQueueObject)) {
        gMessageQueueObject->mEntries = nullptr;
        delete gMessageQueueObject;
    }
    if (Alive(gDrawQueueObject)) {
        gDrawQueueObject->mEntries = nullptr;
        delete gDrawQueueObject;
    }
    if (Alive(gTimedObject)) {
        gTimedObject->mEntries = nullptr;
        delete gTimedObject;
    }
    if (Alive(gTableABlock)) delete[] gTableABlock;
    if (Alive(gTableBBlock)) delete[] gTableBBlock;
    if (!tableFreed) delete[] gTable;
    for (int i = 0; i < kWindows; ++i) gWindow[i] = nullptr;
    gMessageQueueObject = gDrawQueueObject = nullptr;
    gTimedObject = nullptr;
    gTable = nullptr;
    gUIRootWindow = gUIFocusWindow = gUIPointerIn = gPauseWindow = nullptr;
    gUIMessageQueue = gUIDrawQueue = nullptr;
    gUITimedMessages = nullptr;
    gUITimers = gUITimerList = nullptr;
    gStringHeapTableA = gStringHeapTableB = nullptr;
    return words;
}

}  // extern "C"

// InterfaceTimer.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceTimer code, named after it.

#include "Game.h"
#include "helpers/InterfaceTimerHelpers.h"
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "lobby.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "TbITable.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

// The interface's help text handler (main app, UI toolkit 0x101cf874..0x101cfdb0, unit w36): the base class of the object that shows a
// window's help text (the tooltip), its default instance, UI_SetHelpTextHandler, and (it is in the same part of the toolkit) the
// constructor of the timers' table entries.
//
// The base class does nothing visible: it keeps the pointer position and the window the pointer is in, and tells itself to hide
// (HideHelpText, a virtual member that the game's own handler class overrides) when the pointer leaves, a button goes down or the
// window goes. Show, Hide, Update, OnFrame, Init and DeInit are empty. The default instance (gUIDefaultHelpText, src/UI_init.cpp) is
// made by the constructor, which leaves it ENABLED; before this unit the host's was zero-initialised, i.e. disabled.
//
// The Mac's object is 0x18 bytes: +0 enabled (short), +2 wanted (short), +4 and +6 the pointer position, +8 a short that the
// constructor clears and the base class only tests, +0xc the tick the pointer entered a window at, +0x10 the vtable pointer, +0x14 the
// window. The virtual members are called through the vtable (0x102afa88): the slot at 0xc, HideHelpText, from Disable, OnMouseMovedOut,
// OnMouseDown and OnHideOrDelete.

// 0x101d819c (100 bytes): a free timer that has not started: no window, slice, id or neighbours, the tick it was made at.
InterfaceTimer::InterfaceTimer() {
    mWindow = nullptr;
    mSlice = 0;
    mId = 0;
    mNext = nullptr;
    mPrevious = nullptr;
    mStart = IveGotTicks();
    mRunning = 0;
    mFree = 1;
}

// InterfaceWindow (main app, UI toolkit 0x101da564..0x101dfe84, ClientDraw 0x1018a2a8 and InterfaceTimer::Stop
// 0x101d8428): the drawing walk, the positions, the z rotation, the frame animation, and UI_DefaultWindowCallback, the
// handler every window starts with. The draw objects a window owns are reached through their virtual members
// (include/ui_DrawObject.h): Draw for the client rectangle, Change for everything else, with the flags below.
//
// Change flags this file uses (DOChange fields): 1 mFrame, 4 mF08 (the z rotation), 0x10 mRect, 0x40 mF0C and mF0E (the
// pivot), 0x80 the first word (a value). The Mac builds the record on the stack with only the fields it sets; the host's
// is zeroed first, which nothing can see.

void InterfaceTimer::Stop() { mRunning = 0; }

// InterfaceTimer::Start, main 0x101d8460 (60 bytes), unit w37c.

void InterfaceTimer::Start() {
    mRunning = 1;
    mStart = IveGotTicks();
}

// ui_cityLobbyController.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: InterfaceWindow::FinishWithClientObjectSurface
// (0x101dbd1c, 56 bytes) and InterfaceTimer::SetSlice (0x101d84c8, 8 bytes). Unit v5L16: InterfaceWindow::GetClientObjectSurface
// (0x101dbc98, 64 bytes).

void InterfaceTimer::SetSlice(int32_t slice) { mSlice = slice; }

// The UI timers' removal (main app, UI toolkit 0x101d8500 and 0x101d8a00; unit w51): InterfaceTimer::FindTimer and UI_RemoveTimer.
// The table, the list and AddTimer are src/UI_pump.cpp and src/UI_init.cpp.

// 0x101d8500 (384 bytes). The timer in this one's list (this and those after it along mNext) with this window and id; when several
// match the LAST one wins, because the Mac's search is a recursion that asks the rest of the list first and tests the timer itself on the
// way back (it unrolls the first five levels and calls itself from the sixth). The id is compared as a signed number.
InterfaceTimer* InterfaceTimer::FindTimer(InterfaceWindow* window, int32_t id) {
    InterfaceTimer* found = nullptr;
    if (mNext != nullptr) found = mNext->FindTimer(window, id);
    if (found == nullptr && mId == id && mWindow == window) found = this;
    return found;
}

// UI_Base.cpp's start-up, message queues, pointer and cursors (main app 0x101c7ad8..0x101cabe0
// and the timer table's maker 0x101d86c4): UI_InitSystem and what it makes, the queues and their
// posting, the pointer's position and window, the cursors, the help text switches. The rest of
// UI_Base.cpp (the buttons, keys, wheel and the dispatch of the queues) is untranslated.

InterfaceTimer* gUITimers;
int32_t gUITimerCount;
InterfaceTimer* gUITimerList;
int32_t gUITimersPaused;

// ---- the timer table

void UI_InitTimerSystem(int32_t count) {
    gUITimerList = nullptr;
    gUITimers = new InterfaceTimer[count];
    gUITimerCount = count;
    gUITimersPaused = 0;
}

// The interface's message pump (main app): what UIInterface::Process does once a pass. ProcessUI (0x101866c4, ui_main.cpp)
// runs the error display, the timers, the online postcard, the three message queues and the on-frame messages; this file has
// it and everything it reaches that was not translated before: the queues' Post, Peek and GetNumQd and the timed queue's
// Process (UI_Base.cpp's range), the dispatch, onframe and UI_SendMessage, the timers' AddTimer and ServiceAllTimers, the
// window members they use (CallWindowHandler, GetChild, GetNext, GetAttr, AmIGettingDestroyed), UI_GetHighestZ, the postcard
// test and TickUIErrors. The set-up is in UI_init.cpp and InterfaceWindow.cpp; the window handlers, the panel's drawing and
// the dialog are other units' (UI_DialogBox and UI_ActuallyDoPostcard are generated stand-ins in the launcher).

uint32_t IveGotTicks();

// ---- the timers

InterfaceTimer* UI_AddTimer(InterfaceWindow* window, int32_t slice, int32_t id) {
    InterfaceTimer* timer = nullptr;
    if (gUITimers == nullptr) msl_assertion_failed("pUITimerFreeList", "ui_timers.cpp", 0x37);
    for (int32_t i = 0; i < gUITimerCount && timer == nullptr; ++i) {
        InterfaceTimer* candidate = &gUITimers[i];
        if (candidate->mFree != 0) {
            candidate->mFree = 0;
            timer = candidate;
        }
    }
    if (timer == nullptr) return nullptr;
    timer->mWindow = window;
    timer->mSlice = slice;
    timer->mId = id;
    timer->mNext = nullptr;
    timer->mPrevious = nullptr;
    timer->mStart = IveGotTicks();
    timer->mRunning = 0;
    timer->mFree = 0;
    InterfaceTimer* last = gUITimerList;
    if (last == nullptr) {
        gUITimerList = timer;
    } else {
        while (last->mNext != nullptr) last = last->mNext;
        last->mNext = timer;
        timer->mPrevious = last;
    }
    return timer;
}

// For each timer in use, unless the timers are paused: when its slice has gone by (the elapsed ticks as a signed number), it
// starts again from the clock's now, and every slice that fits in the elapsed time is taken off; a running timer with a
// window posts message 0x10 for each (while the UI is active), with the timer's id and the number posted so far this call.
// What is left over is put back, by moving the start tick back by it.
void UI_ServiceAllTimers() {
    if (gUITimersPaused != 0) return;
    for (InterfaceTimer* timer = gUITimerList; timer != nullptr; timer = timer->mNext) {
        int32_t elapsed = static_cast<int32_t>(IveGotTicks() - timer->mStart);
        int32_t posted = 0;
        if (elapsed >= timer->mSlice) {
            timer->mStart = IveGotTicks();
            while (elapsed >= timer->mSlice) {
                elapsed = elapsed - timer->mSlice;
                if (timer->mRunning != 0 && timer->mWindow != nullptr && gUIActive != 0) {
                    UI_PostMessage(timer->mWindow, 0x10, timer->mId, posted);
                    posted = posted + 1;
                }
            }
            timer->mStart = timer->mStart - elapsed;
        }
    }
}

// 0x101d8a00 (300 bytes). Takes the window's timer with this id out of the list of timers in use and frees its slot; 1, or 0 when the
// list is empty or has no such timer. The check that the timer lies in the table (gUITimers, gUITimerCount of them) is an assertion
// that goes on when it fails (the message strings are the Mac's); the slot is freed either way. The timer's own links are left as they
// were. (The declaration in UI_Base.h answers nothing, as the callers' unit libraries fake it.)
int32_t UI_RemoveTimerResult(InterfaceWindow* window, int32_t id) {
    if (gUITimerList == nullptr) return 0;
    InterfaceTimer* timer = gUITimerList->FindTimer(window, id);
    if (timer == nullptr) return 0;
    if (timer == gUITimerList) gUITimerList = timer->mNext;
    if (timer->mPrevious != nullptr) timer->mPrevious->mNext = timer->mNext;
    if (timer->mNext != nullptr) timer->mNext->mPrevious = timer->mPrevious;
    bool inTable = false;
    if (gUITimers != nullptr) {
        const uintptr_t at = reinterpret_cast<uintptr_t>(timer);
        const uintptr_t first = reinterpret_cast<uintptr_t>(gUITimers);
        if (at >= first && at < first + static_cast<intptr_t>(gUITimerCount) * static_cast<intptr_t>(sizeof(InterfaceTimer))) inTable = true;
    }
    if (!inTable)
        msl_assertion_failed("p_mem && pUITimerFreeList && (p_mem >= pUITimerFreeList) && (p_mem < pUITimerFreeList+UITimerFreeListSize)",
                             "ui_timers.cpp", 0x48);
    timer->mFree = 1;
    return 1;
}

void UI_RemoveTimer(InterfaceWindow* window, int32_t id) { UI_RemoveTimerResult(window, id); }

// The deleting form's tail of 0x101d830c: no memory is freed (the timers live in the table UI_InitTimerSystem makes) and the slot is made
// free again. The check that the timer lies in the table is an assertion that goes on when it fails (the strings are the Mac's, as in
// UI_RemoveTimerResult). The destructor has run by now, so the timer's own links are what it left.
void InterfaceTimer::operator delete(void* object) {
    InterfaceTimer* timer = static_cast<InterfaceTimer*>(object);
    if (!InTimerTable(timer))
        msl_assertion_failed("p_mem && pUITimerFreeList && (p_mem >= pUITimerFreeList) && (p_mem < pUITimerFreeList+UITimerFreeListSize)",
                             "ui_timers.cpp", 0x48);
    timer->mFree = 1;
}
// 0x101d830c (240 bytes). The body is the unlink: the previous timer's next link and the next timer's previous link are set to skip this
// one (in that order; this timer's own links are not cleared). The Mac does the rest only when the deleting flag (the SHORT in r4) is
// positive, and that is the class operator delete below on the host. The Mac stores no vtable here (the class has none). A NULL `this`
// is skipped on the Mac; the host never has one.
InterfaceTimer::~InterfaceTimer() {
    if (mPrevious != nullptr) mPrevious->mNext = mNext;
    if (mNext != nullptr) mNext->mPrevious = mPrevious;
}

// ui_ErrorDisplay.cpp (main app), unit w43: UI_DialogBox 0x101f1800, 1008 bytes.

// 0x101d8b64, 16 bytes.
void UI_PauseTimerSystem() { gUITimersPaused = 1; }

// The dialog box's message functions (main app, SimTheme Park 2.04b3; the inventory files them under ui_ErrorDisplay.cpp), and the two
// small functions they call that undo UI_DialogBox's pause and key table switch-off. Unit w47: DialogBoxCallback 0x101f1520 (676 bytes),
// DefaultDialogCallback 0x101f1448 (152), UIMain_EnableAllKeyTables 0x10182304 (48) and UI_UnPauseTimerSystem 0x101d8ba0 (96; the
// inventory files it under ui_cityLobbyController.cpp).
//
// UI_DialogBox (src/ui_dialogbox.cpp) keeps in the dialog's extra parameters 0 to 2 the buttons' callbacks (NULL for none), 3 their
// argument, 4 the key table bookkeeping byte it found (-1: it left the tables alone) and 5 whether it paused the game.

// 0x101d8ba0: undoes UI_PauseTimerSystem. Only when the timers are paused: they are let run, and every timer in use (the list from
// gUITimerList) is restarted from the clock's tick now, so that no time passed in the pause counts (the tick is asked once per timer).
void UI_UnPauseTimerSystem() {
    if (gUITimersPaused != 0) {
        gUITimersPaused = 0;
        for (InterfaceTimer* timer = gUITimerList; timer != nullptr; timer = timer->mNext) timer->mStart = IveGotTicks();
    }
}

// UI_Base.cpp (main app, SimTheme Park 2.04b3): the end of the UI system UI_InitSystem (src/UI_init.cpp) started.
//   0x101c8c2c UI_CloseSystem (236)
//   0x101c7b24 InterfaceMessageQueue::~InterfaceMessageQueue (108)
//   0x101c8214 InterfaceTimedMessages::~InterfaceTimedMessages (100)
//   0x101d8c2c UI_CloseTimerSystem (60; the inventory files it under ui_cityLobbyController.cpp)
// The Mac's two destructors are non-virtual with the deleting flag in r4 (a positive flag frees the object): on the host the
// destructor is the first part and `delete queue` is the deleting call. Both classes have no vtable.

// 0x101d8c2c. Destroys the timer table: the Mac's __destroy_new_array runs ~InterfaceTimer on each timer from the last to the first
// (each unlinks itself from the list of timers in use; the deleting half of the destructor is not run) and frees the array, which
// delete[] does here. The table's pointer and count are left as they were; the list of timers in use is forgotten.
void UI_CloseTimerSystem() {
    delete[] gUITimers;
    gUITimerList = nullptr;
}

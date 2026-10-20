// The UI timers' removal (main app, UI toolkit 0x101d8500 and 0x101d8a00; unit w51): InterfaceTimer::FindTimer and UI_RemoveTimer.
// The table, the list and AddTimer are src/UI_pump.cpp and src/UI_init.cpp.
#include <cstdint>

#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "platform/msl.h"

// 0x101d8500 (384 bytes). The timer in this one's list (this and those after it along mNext) with this window and id; when several
// match the LAST one wins, because the Mac's search is a recursion that asks the rest of the list first and tests the timer itself on the
// way back (it unrolls the first five levels and calls itself from the sixth). The id is compared as a signed number.
InterfaceTimer* InterfaceTimer::FindTimer(InterfaceWindow* window, int32_t id) {
    InterfaceTimer* found = nullptr;
    if (mNext != nullptr) found = mNext->FindTimer(window, id);
    if (found == nullptr && mId == id && mWindow == window) found = this;
    return found;
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

// ---- unit w54: the timer's destructor

namespace {
// The table check of the destructor (the same test as UI_RemoveTimerResult's): the timer is non-NULL, the table exists, and the timer
// is at or after the table's start and before its end (unsigned compares, the host's stride is sizeof(InterfaceTimer)).
bool InTimerTable(const InterfaceTimer* timer) {
    if (timer == nullptr || gUITimers == nullptr) return false;
    const uintptr_t at = reinterpret_cast<uintptr_t>(timer);
    const uintptr_t first = reinterpret_cast<uintptr_t>(gUITimers);
    return at >= first && at < first + static_cast<intptr_t>(gUITimerCount) * static_cast<intptr_t>(sizeof(InterfaceTimer));
}
}  // namespace

// 0x101d830c (240 bytes). The body is the unlink: the previous timer's next link and the next timer's previous link are set to skip this
// one (in that order; this timer's own links are not cleared). The Mac does the rest only when the deleting flag (the SHORT in r4) is
// positive, and that is the class operator delete below on the host. The Mac stores no vtable here (the class has none). A NULL `this`
// is skipped on the Mac; the host never has one.
InterfaceTimer::~InterfaceTimer() {
    if (mPrevious != nullptr) mPrevious->mNext = mNext;
    if (mNext != nullptr) mNext->mPrevious = mPrevious;
}

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

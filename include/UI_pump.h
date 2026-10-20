// The rest of what the interface does once a pass (src/UI_pump.cpp): ProcessUI (ui_main.cpp), the error display's tick
// (ui_ErrorDisplay.cpp) and the postcard test (ui_Postcard.cpp). The pass itself is UIInterface::Process (include/UIInterface.h).
// The message queues, the timers and the dispatch are in include/UI_Base.h; the window members in include/InterfaceWindow.h.
#pragma once
#include <cstdint>
#include <deque>

#include "ui_ErrorDisplay.h"

class InterfaceWindow;
class TbITable;

// One pass of the interface: the error display, the timers, the two queues' lengths (kept for a debug view), the online
// postcard, the general queue (up to 0x400 messages), the timed messages, the draw message to the root window, the on-frame
// messages and the draw queue (up to 0x1000 messages), in that order.
void ProcessUI();   // 0x101866c4

extern int32_t gUIGeneralQueued;   // 0x1038cc4c, the general queue's length as ProcessUI saw it (original name unknown; nothing reads it)
extern int32_t gUIDrawQueued;      // 0x1038cc48, the draw queue's

// ---- the postcard (ui_Postcard.cpp; the online postcards are untranslated)
// Counts gPostcardDelay down and answers 1 once it is 0 while gPostcardWanted is set. UI_ActuallyDoPostcard takes the
// picture and shows the panel (it clears gPostcardWanted).
bool UI_PostcardPending();        // 0x101b22bc
void UI_ActuallyDoPostcard();     // 0x101b20b4 (untranslated)
extern int32_t gPostcardDelay;    // 0x10394b04, passes to wait (original name unknown)
extern int32_t gPostcardWanted;   // 0x103956f0, set when a postcard is to be made (original name unknown)

// ---- the error display (ui_ErrorDisplay.cpp)
// An error waiting to be shown: the string `mId` of the table `mTable`. Mac 8 bytes (table +0, id +4).
struct errorthingy {
    TbITable* mTable;
    uint32_t mId;
};

// The bullfrog thread library's mutex (the Mac's is 16 bytes at 0x10396444). Only its handle word is used here. lock and unlock are
// translated in src/email.cpp (unit w62; the inventory files them there).
class bthread_mutex {
public:
    // lock enters the critical section whose handle is the object's first word (NS_MacDoze::EnterCriticalSection(&mHandle)), unlock
    // leaves it. The Mac's two functions also load 0 into r3 (nothing reads it); void here.
    void lock();     // 0x10200d8c
    void unlock();   // 0x10200de0
    void* mHandle;   // 0x00  the critical section's handle (the object is the handle: the Mac's mutex is 16 bytes)
    // ~bthread_mutex (0x10200d04, unit v5120). The Mac's destructor takes the hidden "delete storage" flag (r4, a short): when this is
    // not NULL it deletes the critical section, and frees the object when the flag is above 0. Written as a plain function so that
    // the globals of this class do not get a static destructor. Answers this.
    static bthread_mutex* Destroy(bthread_mutex* self, int16_t deleteFlag);
    // Unit v51056: bthread_mutex::bthread_mutex 0x10200ca4 (52): InitializeCriticalSection(this); answers this.
    static bthread_mutex* Construct(bthread_mutex* self);
};

// The errors waiting (the Mac's std::deque<errorthingy>, 0x10396454, is the C++ runtime's: this is the host's), the mutex that
// guards them and the dialog showing one (NULL when there is none; ErrorMessageCallback clears it when the dialog closes).
// Original names unknown. Defined in src/UI_pump.cpp until ui_ErrorDisplay.cpp is translated.
extern std::deque<errorthingy> gUIErrorQueue;   // 0x10396454
extern bthread_mutex gUIErrorMutex;             // 0x10396444
extern InterfaceWindow* gUIErrorDialog;         // 0x1039647c

// Under the mutex: when an error waits, no error dialog is up and the interface's windows are at depth 0x22 or less, takes the
// oldest error and shows it in a dialog with one button (UI_DialogBox) whose handler is ErrorMessageCallback. At most one
// error a pass.
void TickUIErrors();   // 0x101f1e90

// The dialog's button (answers a bool on the Mac) and window handler (answers nothing there). Untranslated.
int32_t ErrorDlgCancel(void* unused);                                                             // 0x101f1cd0
int32_t ErrorMessageCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);     // 0x101f1c54

// unit w43: UI_DialogBox is declared with its real return type in ui_ErrorDisplay.h.

// Unit v5134: ui_ErrorDisplay.cpp's static initialiser (0x101f499c). It builds the error queue (a temporary deque copied into
// the global, then torn down), registers its destructor, constructs the mutex and registers that. The Mac's deque and mutex
// constructors are the callees below (fakes in the test).
void __sinit_ui_ErrorDisplay_cpp();   // 0x101f499c
namespace errsinit_callee {
void DequeCtor(void* self, void* allocator);        // 0x101f5158 std::deque<errorthingy>::deque(const allocator&)
void DequeCopy(void* self, const void* other);      // 0x101f4d14 std::deque<errorthingy>::deque(const deque&)
void* BthreadMutexCtor(void* self);                 // 0x10200ca4 bthread_mutex::bthread_mutex(); answers its r3
}  // namespace errsinit_callee

// Unit v51076 (src/UI_pump_std___cdeque_pop_front.cpp).
extern uint32_t gMslErrorDequeMapSize;   // 0x102b1428 (initial word 0x40), original name unknown: the first word of a new deque
bool UI_HelpTextEnabled();               // 0x101cab08 (stand-in)
void CoasterButtonsDown(uint32_t mask);  // 0x1019cd00 (64 bytes, stand-in)
// std::__cdeque<errorthingy*>::resize 0x101f5044 and reserve 0x101f50d4: forward to the const void* deque's (self, n[, value]).
void ErrorCdequeResize(void* self, uint32_t n, errorthingy* value);
void ErrorCdequeReserve(void* self, uint32_t n);

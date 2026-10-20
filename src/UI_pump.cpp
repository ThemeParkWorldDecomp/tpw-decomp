// The interface's message pump (main app): what UIInterface::Process does once a pass. ProcessUI (0x101866c4, ui_main.cpp)
// runs the error display, the timers, the online postcard, the three message queues and the on-frame messages; this file has
// it and everything it reaches that was not translated before: the queues' Post, Peek and GetNumQd and the timed queue's
// Process (UI_Base.cpp's range), the dispatch, onframe and UI_SendMessage, the timers' AddTimer and ServiceAllTimers, the
// window members they use (CallWindowHandler, GetChild, GetNext, GetAttr, AmIGettingDestroyed), UI_GetHighestZ, the postcard
// test and TickUIErrors. The set-up is in UI_init.cpp and InterfaceWindow.cpp; the window handlers, the panel's drawing and
// the dialog are other units' (UI_DialogBox and UI_ActuallyDoPostcard are generated stand-ins in the launcher).
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "platform/msl.h"
#include "ui_main.h"

// ---- the pump's state (the original names are unknown; the addresses are in include/UI_Base.h and UI_pump.h)
InterfaceWindow* gUIKeyboardWindow;
InterfaceWindow* gUIFocusWindow;
int32_t gUIOnFrameCount;
int32_t gUIGeneralQueued;
int32_t gUIDrawQueued;
int32_t gPostcardDelay;
int32_t gPostcardWanted;
std::deque<errorthingy> gUIErrorQueue;
bthread_mutex gUIErrorMutex;
InterfaceWindow* gUIErrorDialog;

namespace {
// What InvalidateMsgs writes over the three words of a queued message whose window has gone.
constexpr int32_t kInvalidated = static_cast<int32_t>(0x80808080u);
}  // namespace

// ---- the window members

int8_t InterfaceWindow::AmIGettingDestroyed() { return static_cast<int8_t>(mDestroying); }

InterfaceWindow* InterfaceWindow::GetChild(int32_t) { return mChild; }

InterfaceWindow* InterfaceWindow::GetNext() { return mNext; }

uint32_t InterfaceWindow::GetAttr() { return mAttributes; }

// ---- the queues

// Delivers the message at mHead and frees its slot. A message with 0x1000a, 0x1000b or 0x1000c (a key going down, going up
// or typed: the first argument is the key, bits 16 to 23 of the second the modifiers) goes to the window that asked for that
// key (the key capture's, when its list holds the key and the modifiers), else to the keyboard window, else to the focus
// window, whichever of them has attribute 1 (and none of them when none has). The queued window is not asked: UI_Keydown,
// UI_Keyup and UI_KeyChar queue these with 1 for a window. Any other message goes to its own window, which is not tested for
// being destroyed (InvalidateMsgs is what removes the messages of a window that went). The counters are moved after the
// handler has run, so a handler that flushes the queue leaves them one short.
int16_t InterfaceMessageQueue::Post() {
    if (mEntries == nullptr) return 0;
    InterfaceMessage* slot = &mEntries[mHead];
    if (slot->mWindow == nullptr) return 0;
    const int32_t message = slot->mMessage;
    const int32_t a = slot->mA;
    const int32_t b = slot->mB;
    if (message != kInvalidated && a != kInvalidated && b != kInvalidated) {
        if (message == 0x1000b || message == 0x1000a || message == 0x1000c) {
            bool wanted = false;
            if (gUIKeyCapture.mKeys != nullptr && gUIKeyCapture.mWindow != nullptr &&
                (gUIKeyCapture.mWindow->GetAttr() & 1) != 0) {
                const int16_t key = static_cast<int16_t>(a);
                const int32_t modifiers = (b >> 16) & 0xff;
                for (const captureKeyList* entry = gUIKeyCapture.mKeys; entry->mModifiers != -1; ++entry) {
                    if (entry->mKey == key && entry->mModifiers == modifiers) {
                        wanted = true;
                        break;
                    }
                }
            }
            if (wanted) {
                gUIKeyCapture.mWindow->CallWindowHandler(message, a, b);
            } else if (gUIKeyboardWindow != nullptr && (gUIKeyboardWindow->GetAttr() & 1) != 0) {
                gUIKeyboardWindow->CallWindowHandler(message, a, b);
            } else if (gUIFocusWindow != nullptr && (gUIFocusWindow->GetAttr() & 1) != 0) {
                gUIFocusWindow->CallWindowHandler(message, a, b);
            }
        } else {
            slot->mWindow->CallWindowHandler(message, a, b);
        }
    }
    slot->mWindow = nullptr;
    mQueued = mQueued - 1;
    mHead = mHead - 1;
    if (mHead < 0) mHead = mCount - 1;
    return 1;
}

InterfaceMessage* InterfaceMessageQueue::Peek() {
    if (mEntries != nullptr) {
        InterfaceMessage* slot = &mEntries[mHead];
        if (slot->mWindow != nullptr && slot->mMessage != kInvalidated && slot->mA != kInvalidated && slot->mB != kInvalidated)
            return slot;
    }
    return nullptr;
}

int32_t InterfaceMessageQueue::GetNumQd() { return mQueued; }

// The slots are looked at in order, and a slot with a time (not 0) that is not after the clock's now is delivered and freed.
// The window is not tested at all (a NULL one would be a crash); InvalidateMsgs frees the slots of a window that went.
int16_t InterfaceTimedMessages::Process() {
    int32_t delivered = 0;
    if (mEntries != nullptr) {
        for (int32_t i = 0; i < mCount; ++i) {
            if (mEntries[i].mTime != 0) {
                const uint32_t now = IveGotTicks();
                if (mEntries[i].mTime <= now) {
                    InterfaceTimedMessage& entry = mEntries[i];
                    entry.mWindow->CallWindowHandler(entry.mMessage, entry.mA, entry.mB);
                    delivered = delivered + 1;
                    mEntries[i].mTime = 0;
                }
            }
        }
    }
    return static_cast<int16_t>(delivered);
}

// ---- the messages

// With a peek callback the limit is exact (and tested before the first message). Without one, and in the draw queue, the limit
// is tested after a message has gone out and the count goes up only then, so a queue longer than the limit sends one message
// more than the count says, and one goes out whatever the limit if the queue has any.
int32_t UI_DispatchMessages(int32_t limit) {
    int32_t delivered = 0;
    int16_t posted = 1;
    if (gUIMessageQueue != nullptr) {
        if (gUIPeekCallBack == nullptr) {
            for (;;) {
                posted = gUIMessageQueue->Post();
                if (posted == 0 || !(delivered < limit)) break;
                delivered = delivered + 1;
            }
        } else {
            while (posted != 0 && delivered < limit) {
                if (gUIPeekCallBack != nullptr) {
                    InterfaceMessage* message = gUIMessageQueue->Peek();
                    if (message != nullptr) gUIPeekCallBack(message->mWindow, message->mMessage, message->mA, message->mB);
                }
                posted = gUIMessageQueue->Post();
                if (posted != 0) delivered = delivered + 1;
            }
        }
    }
    return delivered;
}

int32_t UI_DispatchDrawMessages(int32_t limit) {
    int32_t delivered = 0;
    if (gUIDrawQueue != nullptr) {
        for (;;) {
            if (gUIDrawQueue->Post() == 0 || !(delivered < limit)) break;
            delivered = delivered + 1;
        }
    }
    return delivered;
}

int32_t UI_DispatchDeferredMessages() {
    if (gUITimedMessages == nullptr) return -1;
    return gUITimedMessages->Process();
}

// The Mac unrolls the first five levels of the tree and calls itself below them (the fifth level goes through
// UI_SendMessage); the order is the same all the way down: the window's handler, the count, then its children, each read
// after the one before has been done with (a handler that unlinks a window changes where the walk goes).
void onframe(InterfaceWindow* window) {
    if (window != nullptr && window->mDestroying == 0) {
        if (gUIPeekCallBack != nullptr) gUIPeekCallBack(window, 0x1e, 0, 0);
        window->CallWindowHandler(0x1e, 0, 0);
    }
    gUIOnFrameCount = gUIOnFrameCount + 1;
    for (InterfaceWindow* child = window->GetChild(0); child != nullptr; child = child->GetNext()) onframe(child);
}

int32_t UI_SendOnFrameMessages() {
    gUIOnFrameCount = 0;
    if (gUIRootWindow != nullptr) onframe(gUIRootWindow);
    return gUIOnFrameCount;
}

uint32_t IveGotTicks() { return gUIClock->tickyticky(); }

int32_t UI_GetHighestZ(InterfaceWindow* window, CoolRect* rect) {
    if (window == nullptr) window = gUIRootWindow;
    if (window != nullptr) return highestZ(window, rect);
    return -1;
}

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

// ---- the postcard

bool UI_PostcardPending() {
    if (gPostcardDelay > 0) gPostcardDelay = gPostcardDelay - 1;
    bool pending = false;
    if (gPostcardDelay == 0 && gPostcardWanted != 0) pending = true;
    return pending;
}

// ---- the error display

// The mutex and the queue are the C++ runtime's on the Mac (a CodeWarrior std::deque of 8-byte records): the host's
// std::deque stands in for the queue. The record is copied before it is popped, and its string is looked up after.
void TickUIErrors() {
    gUIErrorMutex.lock();
    if (!gUIErrorQueue.empty() && gUIErrorDialog == nullptr) {
        if (UI_GetHighestZ(nullptr, nullptr) < 0x23) {
            const errorthingy error = gUIErrorQueue.front();
            gUIErrorQueue.pop_front();
            const unsigned char* text = error.mTable->GetConstString(error.mId);
            dlgboxparams params;
            params.mText = UIMain_ConvertMulti(text);
            params.mRed = 0xff;
            params.mGreen = 0xff;
            params.mBlue = 0xff;
            params.mValue8 = 4;
            params.mValueC = 0;
            gUIErrorDialog = UI_DialogBox(params, dlgbutton(2), ErrorDlgCancel, dlgbutton(0), nullptr, dlgbutton(0), nullptr, false);
            gUIErrorDialog->SetCallbackFunction(ErrorMessageCallback);
        }
    }
    gUIErrorMutex.unlock();
}

// ---- the pass

void ProcessUI() {
    TickUIErrors();
    UI_ServiceAllTimers();
    gUIGeneralQueued = gUIMessageQueue->GetNumQd();
    gUIDrawQueued = gUIDrawQueue->GetNumQd();
    if (UI_PostcardPending()) UI_ActuallyDoPostcard();
    UI_DispatchMessages(0x400);
    UI_DispatchDeferredMessages();
    UI_PostMessage(gUIRootWindow, 2, 0, 0);
    UI_SendOnFrameMessages();
    UI_DispatchDrawMessages(0x1000);
}

void UIInterface::Process() { ProcessUI(); }

void UIInterface::UnPause() { UnPauseUI(); } // 0x101ef8f0

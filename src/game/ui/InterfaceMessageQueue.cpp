// InterfaceMessageQueue.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceMessageQueue code, named after it.

#include "email.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "platform/msl.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include "World.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>
#include <string>

// UI_Base.cpp's start-up, message queues, pointer and cursors (main app 0x101c7ad8..0x101cabe0
// and the timer table's maker 0x101d86c4): UI_InitSystem and what it makes, the queues and their
// posting, the pointer's position and window, the cursors, the help text switches. The rest of
// UI_Base.cpp (the buttons, keys, wheel and the dispatch of the queues) is untranslated.

InterfaceHelpText gUIDefaultHelpText;
// ---- the message queues

// (mQueued is not set here: Initialise does it.)
InterfaceMessageQueue::InterfaceMessageQueue() : mEntries(nullptr), mCount(0), mHead(0), mTail(0) {}

// UI_Base.cpp (main app, SimTheme Park 2.04b3): the end of the UI system UI_InitSystem (src/UI_init.cpp) started.
//   0x101c8c2c UI_CloseSystem (236)
//   0x101c7b24 InterfaceMessageQueue::~InterfaceMessageQueue (108)
//   0x101c8214 InterfaceTimedMessages::~InterfaceTimedMessages (100)
//   0x101d8c2c UI_CloseTimerSystem (60; the inventory files it under ui_cityLobbyController.cpp)
// The Mac's two destructors are non-virtual with the deleting flag in r4 (a positive flag frees the object): on the host the
// destructor is the first part and `delete queue` is the deleting call. Both classes have no vtable.

// 0x101c7b24. Gives the slots back (an array made with new[]) and forgets them.
InterfaceMessageQueue::~InterfaceMessageQueue() {
    if (mEntries != nullptr) {
        delete[] mEntries;
        mEntries = nullptr;
    }
}

int16_t InterfaceMessageQueue::Initialise(int32_t count) {
    if (mEntries == nullptr) {
        mEntries = new InterfaceMessage[count];
        if (mEntries != nullptr) {
            mHead = count - 1;
            mTail = count - 1;
            mCount = count;
            mQueued = 0;
            std::memset(mEntries, 0, count * sizeof(InterfaceMessage));
            return 1;
        }
    }
    return 0;
}

int16_t InterfaceMessageQueue::Add(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    InterfaceMessage* slot = &mEntries[mTail];
    if (slot->mWindow == nullptr && window != nullptr) {
        slot->mWindow = window;
        mEntries[mTail].mMessage = message;
        mEntries[mTail].mA = a;
        mEntries[mTail].mB = b;
        mQueued = mQueued + 1;
        mTail = mTail - 1;
        if (mTail < 0) mTail = mCount - 1;
        return 1;
    }
    return 0;
}

// The interface's message pump (main app): what UIInterface::Process does once a pass. ProcessUI (0x101866c4, ui_main.cpp)
// runs the error display, the timers, the online postcard, the three message queues and the on-frame messages; this file has
// it and everything it reaches that was not translated before: the queues' Post, Peek and GetNumQd and the timed queue's
// Process (UI_Base.cpp's range), the dispatch, onframe and UI_SendMessage, the timers' AddTimer and ServiceAllTimers, the
// window members they use (CallWindowHandler, GetChild, GetNext, GetAttr, AmIGettingDestroyed), UI_GetHighestZ, the postcard
// test and TickUIErrors. The set-up is in UI_init.cpp and InterfaceWindow.cpp; the window handlers, the panel's drawing and
// the dialog are other units' (UI_DialogBox and UI_ActuallyDoPostcard are generated stand-ins in the launcher).

// ---- the pump's state (the original names are unknown; the addresses are in include/UI_Base.h and UI_pump.h)
InterfaceWindow* gUIKeyboardWindow;
InterfaceWindow* gUIFocusWindow;
namespace {
// What InvalidateMsgs writes over the three words of a queued message whose window has gone.
constexpr int32_t kInvalidated = static_cast<int32_t>(0x80808080u);
}  // namespace

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

// Forgetting a window that is going (main app, UI toolkit 0x101c8038..0x101ca254; unit w51): ClearAllPointers, the three
// UI_Invalidate*Messages and the two queues' InvalidateMsgs.

// 0x101c8038 (116 bytes). -1 when the queue has no slots; otherwise every slot whose window is this one has its message and both
// arguments set to 0x80808080 (the window stays, so the slot is not free: Post drops the message and frees it), and the answer is how
// many there were, as a SHORT (a queue of more than 32767 of them answers a negative number).
int32_t InterfaceMessageQueue::InvalidateMsgs(InterfaceWindow* window) {
    if (mEntries == nullptr) return -1;
    int32_t count = 0;
    for (int32_t i = 0; i < mCount; ++i) {
        if (mEntries[i].mWindow == window) {
            mEntries[i].mMessage = kInvalidated;
            mEntries[i].mA = kInvalidated;
            mEntries[i].mB = kInvalidated;
            ++count;
        }
    }
    return static_cast<int16_t>(count);
}

void InterfaceMessageQueue::Flush() {
    if (mEntries != nullptr) {
        mHead = mCount - 1;
        mTail = mCount - 1;
        mQueued = 0;
        std::memset(mEntries, 0, mCount * sizeof(InterfaceMessage));
    }
}

int32_t InterfaceMessageQueue::GetNumQd() { return mQueued; }

InterfaceTimedMessages::InterfaceTimedMessages() : mCount(0), mEntries(nullptr) {}

// 0x101c8214. Gives the slots back; the pointer is left as it was.
InterfaceTimedMessages::~InterfaceTimedMessages() {
    if (mEntries != nullptr) delete[] mEntries;
}

// 0x101c82ac: stores the message in the first free slot (time 0); answers 1 when it did. A time of 0 or of 60000 and more is refused.
int16_t InterfaceTimedMessages::Add(InterfaceWindow* window, int32_t message, int32_t a, int32_t b, uint32_t time) {
    int16_t added = 0;
    if (mEntries != nullptr && time != 0 && time < 60000) {
        for (int32_t i = 0; i < mCount && added == 0; i++) {
            if (mEntries[i].mTime == 0) {
                mEntries[i].mTime = time + IveGotTicks();
                added = 1;
                mEntries[i].mWindow = window;
                mEntries[i].mMessage = message;
                mEntries[i].mA = a;
                mEntries[i].mB = b;
            }
        }
    }
    return added;
}

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

uint32_t IveGotTicks();

// 0x101c84d8 (88 bytes). -1 when there are no slots; otherwise every slot of this window gets time 0 (free) and the answer is how many,
// as a short.
int32_t InterfaceTimedMessages::InvalidateMsgs(InterfaceWindow* window) {
    if (mEntries == nullptr) return -1;
    int32_t count = 0;
    for (int32_t i = 0; i < mCount; ++i) {
        if (mEntries[i].mWindow == window) {
            mEntries[i].mTime = 0;
            ++count;
        }
    }
    return static_cast<int16_t>(count);
}

void InterfaceTimedMessages::Flush() {
    if (mEntries == nullptr) return;
    for (int32_t i = 0; i < mCount; ++i) mEntries[i].mTime = 0;
}
int16_t InterfaceTimedMessages::Initialise(int32_t count) {
    if (mEntries == nullptr && count < 0x200) {
        mEntries = new InterfaceTimedMessage[count];
        if (mEntries != nullptr) {
            mCount = count;
            for (int32_t i = 0; i < count; ++i) mEntries[i].mTime = 0;
            return 1;
        }
    }
    return 0;
}

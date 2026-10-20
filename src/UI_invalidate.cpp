// Forgetting a window that is going (main app, UI toolkit 0x101c8038..0x101ca254; unit w51): ClearAllPointers, the three
// UI_Invalidate*Messages and the two queues' InvalidateMsgs.
#include <cstdint>

#include "InterfaceWindow.h"
#include "UI_Base.h"

namespace {
// What InvalidateMsgs writes over the three words of a queued message whose window has gone (Post drops such a message).
constexpr int32_t kInvalidated = static_cast<int32_t>(0x80808080u);
}  // namespace

// 0x101ca254 (128 bytes). Every global that holds this window is cleared: the window the pointer is in, the focus window, the window that
// captured the device, and the window each of the three mouse buttons went down in.
void ClearAllPointers(InterfaceWindow* window) {
    if (gUIPointerIn == window) gUIPointerIn = nullptr;
    if (gUIFocusWindow == window) gUIFocusWindow = nullptr;
    if (gUICaptureDevice == window) gUICaptureDevice = nullptr;
    for (int32_t i = 0; i < 3; ++i) {
        if (gUIButtonWindow[i] == window) gUIButtonWindow[i] = nullptr;
    }
}

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

// 0x101c9ca4, 0x101c9d2c and 0x101c9db8 (72 bytes each). -1 when the queue does not exist or the window is NULL, else the queue's answer.
int32_t UI_InvalidateMessages(InterfaceWindow* window) {
    if (gUIMessageQueue == nullptr || window == nullptr) return -1;
    return gUIMessageQueue->InvalidateMsgs(window);
}

int32_t UI_InvalidateDrawMessages(InterfaceWindow* window) {
    if (gUIDrawQueue == nullptr || window == nullptr) return -1;
    return gUIDrawQueue->InvalidateMsgs(window);
}

int32_t UI_InvalidateDeferredMessages(InterfaceWindow* window) {
    if (gUITimedMessages == nullptr || window == nullptr) return -1;
    return gUITimedMessages->InvalidateMsgs(window);
}

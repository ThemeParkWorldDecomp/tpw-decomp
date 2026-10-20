// UI_Base.cpp's start-up, message queues, pointer and cursors (main app 0x101c7ad8..0x101cabe0
// and the timer table's maker 0x101d86c4): UI_InitSystem and what it makes, the queues and their
// posting, the pointer's position and window, the cursors, the help text switches. The rest of
// UI_Base.cpp (the buttons, keys, wheel and the dispatch of the queues) is untranslated.
#include <cstring>

#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"

// ---- the UI's state (the original names are unknown; the addresses are in include/UI_Base.h)
InterfaceWindow* gUIPointerIn;
InterfaceWindow* gUIButtonWindow[3];
InterfaceWindow* gUICaptureDevice;
UIKeyCapture gUIKeyCapture;
InterfaceMessageQueue* gUIDrawQueue;
InterfaceMessageQueue* gUIMessageQueue;
InterfaceTimedMessages* gUITimedMessages;
int32_t (*gUIPeekCallBack)(InterfaceWindow*, int32_t, int32_t, int32_t);
int32_t gUIStarted;
bool gUIHebrew;
int16_t gUIPointerX;
int16_t gUIPointerY;
InterfaceCursor* gUICursor;
CursorTable* gUICursorTable;
uint16_t gUIStandardCursor;
int16_t gUICursorNumber = -1;
InterfaceHelpText gUIDefaultHelpText;
InterfaceHelpText* gUIHelpText = &gUIDefaultHelpText;
clockTickFunction gUIDefaultClock;
clockTickFunction* gUIClock = &gUIDefaultClock;
InterfaceTimer* gUITimers;
int32_t gUITimerCount;
InterfaceTimer* gUITimerList;
int32_t gUITimersPaused;

// ---- the message queues

// (mQueued is not set here: Initialise does it.)
InterfaceMessageQueue::InterfaceMessageQueue() : mEntries(nullptr), mCount(0), mHead(0), mTail(0) {}

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

void InterfaceMessageQueue::Flush() {
    if (mEntries != nullptr) {
        mHead = mCount - 1;
        mTail = mCount - 1;
        mQueued = 0;
        std::memset(mEntries, 0, mCount * sizeof(InterfaceMessage));
    }
}

InterfaceTimedMessages::InterfaceTimedMessages() : mCount(0), mEntries(nullptr) {}

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

void InterfaceTimedMessages::Flush() {
    if (mEntries == nullptr) return;
    for (int32_t i = 0; i < mCount; ++i) mEntries[i].mTime = 0;
}

// ---- the timer table

void UI_InitTimerSystem(int32_t count) {
    gUITimerList = nullptr;
    gUITimers = new InterfaceTimer[count];
    gUITimerCount = count;
    gUITimersPaused = 0;
}

// ---- the system

// Makes the root window (2048 x 1536, attributes 0xf0000001, id -0x29a: the constructor's
// own UI_UpdatePointerIn therefore runs before the root is set, on the previous root if
// there was one) and resets the pointer, key capture and button state; then the two message
// queues (the general one first, both of `queueSize` messages), the timer table (0x100) and
// the timed messages (0x40). Every step after the first is skipped if one before it failed.
void UI_InitSystem(int32_t, int32_t queueSize, bool hebrew) {
    InterfaceWindow* root = new InterfaceWindow(nullptr, static_cast<int32_t>(0xf0000001u), -0x29a, 0, 0, 0x7ff, 0x5ff);
    gUIRootWindow = root;
    if (root == nullptr) return;
    gUIStarted = 1;
    gUIPointerIn = root;
    gUICursor = nullptr;
    gUIKeyCapture.mWindow = nullptr;
    gUIKeyCapture.mKeys = nullptr;
    gUIButtonFlags = 0;
    gUIHebrew = hebrew;
    gUIMessageQueue = new InterfaceMessageQueue;
    if (gUIMessageQueue == nullptr || gUIMessageQueue->Initialise(queueSize) == 0) return;
    gUIDrawQueue = new InterfaceMessageQueue;
    if (gUIDrawQueue == nullptr || gUIDrawQueue->Initialise(queueSize) == 0) return;
    UI_InitTimerSystem(0x100);
    gUITimedMessages = new InterfaceTimedMessages;
    if (gUITimedMessages == nullptr || gUITimedMessages->Initialise(0x40) == 0) return;
}

// ---- posting

void UI_FlushAllMessages() {
    if (gUITimedMessages != nullptr) gUITimedMessages->Flush();
    if (gUIDrawQueue != nullptr) gUIDrawQueue->Flush();
    if (gUIMessageQueue != nullptr) gUIMessageQueue->Flush();
}

// ---- the pointer

// The pointer moved to (x, y) (UI window coordinates). Nothing happens while the UI is not
// active. The position is kept; with a root window the message 0x10003 (moved) goes to the
// window that captured the device, or else to the window the pointer was in when that is
// still the window under it, or else 0x10002 (left) goes to the old window and 0x10001
// (entered) to the new one. The messages carry x in the high half and y in the low half,
// but y is sign-extended into the word, so a negative y fills the high half with ones.
void UI_DevicePointerMove(int16_t x, int16_t y) {
    if (gUIActive == 0) return;
    gUIPointerX = x;
    gUIPointerY = y;
    if (gUIRootWindow == nullptr) return;
    const int32_t position = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint16_t>(x)) << 16) | static_cast<int32_t>(y);
    if (gUICaptureDevice == nullptr) {
        InterfaceWindow* under = gUIRootWindow->PointIn(x, y);
        InterfaceWindow* previous = gUIPointerIn;
        if (under == previous) {
            UI_PostMessage(previous, 0x10003, position, 0);
        } else {
            if (previous != nullptr) UI_PostMessage(previous, 0x10002, 0, 0);
            gUIPointerIn = under;
            UI_PostMessage(gUIPointerIn, 0x10001, position, 0);
        }
    } else {
        UI_PostMessage(gUICaptureDevice, 0x10003, position, 0);
    }
}

// The same enter and leave messages for the last pointer position, after a window moved,
// appeared or went. 0 when there is no root window or the device is captured.
int32_t UI_UpdatePointerIn() {
    const int16_t x = gUIPointerX;
    const int16_t y = gUIPointerY;
    if (gUIRootWindow == nullptr) return 0;
    if (gUICaptureDevice != nullptr) return 0;
    InterfaceWindow* under = gUIRootWindow->PointIn(x, y);
    InterfaceWindow* previous = gUIPointerIn;
    if (under != previous) {
        if (previous != nullptr) UI_PostMessage(previous, 0x10002, 0, 0);
        gUIPointerIn = under;
        const int32_t position = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint16_t>(x)) << 16) | static_cast<int32_t>(y);
        UI_PostMessage(gUIPointerIn, 0x10001, position, 0);
    }
    return 1;
}

// ---- callbacks, clock

void UI_SetPeekCallBack(int32_t (*callback)(InterfaceWindow*, int32_t, int32_t, int32_t)) { gUIPeekCallBack = callback; }

void UI_SetClockTickFunction(clockTickFunction* clock) { gUIClock = clock != nullptr ? clock : &gUIDefaultClock; }

// ---- cursors

void UI_SetStandardCursor(uint16_t cursor) { gUIStandardCursor = cursor; }

// Switches to cursor number `cursor` (-1 is the standard one) when a table is in use and
// the number is not the one in use: the cursor in use is told Use(0), the new one is taken
// from the table (or the standard one when the table has none under that number) and told
// Use(1). The number kept is the one asked for, even when the standard cursor stood in
// for it.
void UI_SetCursor(int16_t cursor) {
    if (gUICursorTable != nullptr && cursor != gUICursorNumber) {
        if (cursor == -1) cursor = static_cast<int16_t>(gUIStandardCursor);
        InterfaceCursor* previous = gUICursorTable->GetEntry(static_cast<uint16_t>(gUICursorNumber));
        if (previous != nullptr) previous->Use(0);
        InterfaceCursor* next = gUICursorTable->GetEntry(static_cast<uint16_t>(cursor));
        if (next == nullptr) next = gUICursorTable->GetEntry(gUIStandardCursor);
        gUICursor = next;
        gUICursorNumber = cursor;
        if (next != nullptr) next->Use(1);
    }
}

void UI_HideCursor() {
    if (gUICursor != nullptr) gUICursor->Hide();
}

void UI_ShowCursor() {
    if (gUICursor != nullptr) gUICursor->Show();
}

// Switches to another table: the cursor in use is told Use(0), and the entry of the table
// for the number in use (whatever it is, even -1 read as 0xffff) becomes the cursor in use
// and is told Use(1). A NULL table or the one in use changes nothing.
void UI_UseCursorTable(CursorTable* table) {
    if (table != nullptr && gUICursorTable != table) {
        if (gUICursor != nullptr) gUICursor->Use(0);
        gUICursorTable = table;
        InterfaceCursor* entry = table->GetEntry(static_cast<uint16_t>(gUICursorNumber));
        if (entry != nullptr) entry->Use(1);
        gUICursor = entry;
    }
}

// ---- help text

void UI_DisableHelpText() { gUIHelpText->Disable(); }

void UI_EnableHelpText() { gUIHelpText->Enable(); }

// ---- unit w46: the mouse buttons

// 1 << n as the Mac's slw computes it: the low six bits of n are the shift, and 32 or more gives 0.
static uint32_t ShiftedBit(int16_t n) {
    const uint32_t count = static_cast<uint32_t>(n) & 0x3f;
    return count >= 32 ? 0 : (1u << count);
}

// 0x101c9034: button `button` (0 left, 1 right, 2 middle) went down at (x, y). Nothing happens while the UI is not
// active. Message 0x10005 (button down) goes to the window the pointer is in, with the button and the position (x in
// the high half, y sign-extended into the word as in UI_DevicePointerMove); that window is kept for the button, and
// the button's bit is set in gUIButtonFlags. The button is not checked: the Mac indexes the table of three words
// with any value. The Mac answers gUIActive != 0 in r3.
void UI_DeviceButtonDown(int16_t button, int16_t x, int16_t y) {
    if (gUIActive == 0) return;
    const int32_t position = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint16_t>(x)) << 16) | static_cast<int32_t>(y);
    UI_PostMessage(gUIPointerIn, 0x10005, button, position);
    gUIButtonWindow[button] = gUIPointerIn;
    gUIButtonFlags = static_cast<int16_t>(static_cast<uint16_t>(gUIButtonFlags) | static_cast<uint16_t>(ShiftedBit(button)));
}

// 0x101c9100: the button went up at (x, y). Message 0x10004 (button up) goes to the window the button went down in
// (NULL when it was never down: UI_PostMessage then drops it), that window is forgotten and the button's bit is
// cleared. Nothing happens while the UI is not active.
void UI_DeviceButtonUp(int16_t button, int16_t x, int16_t y) {
    if (gUIActive == 0) return;
    InterfaceWindow** slot = &gUIButtonWindow[button];
    const int32_t position = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint16_t>(x)) << 16) | static_cast<int32_t>(y);
    UI_PostMessage(*slot, 0x10004, button, position);
    *slot = nullptr;
    gUIButtonFlags = static_cast<int16_t>(static_cast<uint16_t>(gUIButtonFlags) & ~static_cast<uint16_t>(ShiftedBit(button)));
}

// 0x101c91cc (lead 2026-10-14): the wheel turned by delta at (x, y). Message 0x1000d goes with the delta and the position
// (packed as for the buttons) to the pointer's capture window, or to the window the pointer is in when nothing has the
// capture. The modifiers are not used. Nothing happens while the UI is not active. The Mac answers gUIActive != 0 in r3.
void UI_DeviceMouseWheel(int32_t delta, int16_t x, int16_t y, int16_t modifiers) {
    (void)modifiers;
    if (gUIActive == 0) return;
    const int32_t position = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint16_t>(x)) << 16) | static_cast<int32_t>(y);
    UI_PostMessage(gUICaptureDevice != nullptr ? gUICaptureDevice : gUIPointerIn, 0x1000d, delta, position);
}

// ---- unit w50: the keys

// 0x101c929c, 0x101c9314 and 0x101c9388: while the UI is active, a key going down (0x1000a), going up (0x1000b) or typed
// (0x1000c) is queued with the key and the flags (the modifiers in bits 16 to 23). The window argument is the constant 1,
// not a window: UI_PostMessage queues these three messages without looking at the window and the pump
// (InterfaceMessageQueue::Post) sends them to the key capture's, the keyboard or the focus window. The key is a 16-bit value
// sign-extended into the word. The Mac answers gUIActive != 0 in r3.
bool UI_Keydown(int16_t key, int32_t flags) {
    if (gUIActive == 0) return false;
    UI_PostMessage(reinterpret_cast<InterfaceWindow*>(1), 0x1000a, key, flags);
    return true;
}

bool UI_Keyup(int16_t key, int32_t flags) {
    if (gUIActive == 0) return false;
    UI_PostMessage(reinterpret_cast<InterfaceWindow*>(1), 0x1000b, key, flags);
    return true;
}

bool UI_KeyChar(int16_t character, int32_t flags) {
    if (gUIActive == 0) return false;
    UI_PostMessage(reinterpret_cast<InterfaceWindow*>(1), 0x1000c, character, flags);
    return true;
}

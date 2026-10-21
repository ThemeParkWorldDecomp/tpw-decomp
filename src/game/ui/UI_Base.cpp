// UI_Base.cpp: name proven by __sinit_UI_Base_cpp in the main application.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "CFixableTime.h"
#include "CFixedTime.h"
#include "Clock.h"
#include "CPausableFixedTime.h"
#include "CPausableTime.h"
#include "CSaveableFixedTime.h"
#include "CSaveableTime.h"
#include "CVariableTime.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "LbFile.h"
#include "LbTime.h"
#include "ParkTime.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "TbDynamicStringTemplate.h"
#include "TbIRLE4bitFont.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "TPSound.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_menu.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include "WorldThings.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// UI_Base.cpp's start-up, message queues, pointer and cursors (main app 0x101c7ad8..0x101cabe0
// and the timer table's maker 0x101d86c4): UI_InitSystem and what it makes, the queues and their
// posting, the pointer's position and window, the cursors, the help text switches. The rest of
// UI_Base.cpp (the buttons, keys, wheel and the dispatch of the queues) is untranslated.

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
InterfaceHelpText* gUIHelpText = &gUIDefaultHelpText;
clockTickFunction gUIDefaultClock;
clockTickFunction* gUIClock = &gUIDefaultClock;
extern InterfaceTimer* gUITimers;

extern int32_t gUITimerCount;

extern InterfaceTimer* gUITimerList;

extern int32_t gUITimersPaused;

void UI_InitTimerSystem(int32_t count);

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

// UI_Base.cpp (main app, SimTheme Park 2.04b3): the end of the UI system UI_InitSystem (src/UI_init.cpp) started.
//   0x101c8c2c UI_CloseSystem (236)
//   0x101c7b24 InterfaceMessageQueue::~InterfaceMessageQueue (108)
//   0x101c8214 InterfaceTimedMessages::~InterfaceTimedMessages (100)
//   0x101d8c2c UI_CloseTimerSystem (60; the inventory files it under ui_cityLobbyController.cpp)
// The Mac's two destructors are non-virtual with the deleting flag in r4 (a positive flag frees the object): on the host the
// destructor is the first part and `delete queue` is the deleting call. Both classes have no vtable.

void UI_CloseTimerSystem();

// 0x101c8c2c. Answers 0 when there is no root window (nothing else is done); else deletes the root window (the deleting form of its
// virtual destructor, slot 4 of the table the window's word at +0x118 points to; the global is NOT cleared yet, so the destructor
// still finds itself there), the two message queues and the timed messages (each only when set), clears those three, closes the timer
// system, removes all draw objects (and destroys them) and all cursors, and only then clears the root window, the focus window and
// the pointer's window. Answers 1. The queues are deleted in the order general, draw, timed.
int32_t UI_CloseSystem() {
    if (gUIRootWindow == nullptr) return 0;
    delete gUIRootWindow;
    if (gUIMessageQueue != nullptr) delete gUIMessageQueue;
    if (gUIDrawQueue != nullptr) delete gUIDrawQueue;
    if (gUITimedMessages != nullptr) delete gUITimedMessages;
    gUIMessageQueue = nullptr;
    gUIDrawQueue = nullptr;
    gUITimedMessages = nullptr;
    UI_CloseTimerSystem();
    DrawObjectLibrary::RemoveAll(1);
    CursorLibrary::RemoveAll();
    gUIRootWindow = nullptr;
    gUIFocusWindow = nullptr;
    gUIPointerIn = nullptr;
    return 1;
}

// UI_Base.cpp (main app of SimTheme Park 2.04b3). PARTIAL: UI_SetRealScreenRez
// (0x101c9ed8), the rectangle classes and the name hash so far.

int16_t gUIRealScreenWidth;
int16_t gUIRealScreenHeight;


// The UI's hash of a name: each character (as a signed char) is mixed in and the whole
// multiplied, so the last characters weigh least.
uint32_t UI_hash_cakes(const char* text, uint32_t multiplier) {
    uint32_t hash = 0;
    for (; *text != 0; ++text) hash = multiplier * (static_cast<uint32_t>(static_cast<int32_t>(static_cast<signed char>(*text))) ^ hash);
    return hash;
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

// UI_PostMessage, main 0x101c9400 (220 bytes); existing body separated for unit w43.

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (static_cast<uint32_t>(message) - 1 <= 2) {
        if (gUIDrawQueue != nullptr && window != nullptr && window->mDestroying == 0) {
            gUIDrawQueue->Add(window, message, a, b);
            return 1;
        }
    } else if (static_cast<uint32_t>(message) - 0x1000a <= 2) {
        gUIMessageQueue->Add(window, message, a, b);
    } else if (gUIMessageQueue != nullptr && window != nullptr && window->mDestroying == 0) {
        gUIMessageQueue->Add(window, message, a, b);
        return 1;
    }
    return 0;
}

// UI_SendMessage 0x101c9518 (164 bytes) and CallWindowHandler 0x101de384 (56); existing bodies, main, unit w43.

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (window == nullptr || window->mDestroying != 0) return -1;
    if (gUIPeekCallBack != nullptr) gUIPeekCallBack(window, message, a, b);
    return window->CallWindowHandler(message, a, b);
}

// Unit v51048: eight small functions of the main app (Game.cpp, ActionRecorder.cpp, ui_visitorStatus.cpp's list instance, Peep.cpp's park time,
// UI_Base.cpp's bar and deferred message).

// 0x101c95f8 (120 bytes): messages 1 to 3 (unsigned compare of message - 1) are refused.
int32_t UI_PutDeferredMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b, uint32_t time) {
    if (gUITimedMessages == nullptr || window == nullptr || static_cast<uint32_t>(message - 1) <= 2) return 0;
    gUITimedMessages->Add(window, message, a, b, time);
    return 1;
}

// The interface's message pump (main app): what UIInterface::Process does once a pass. ProcessUI (0x101866c4, ui_main.cpp)
// runs the error display, the timers, the online postcard, the three message queues and the on-frame messages; this file has
// it and everything it reaches that was not translated before: the queues' Post, Peek and GetNumQd and the timed queue's
// Process (UI_Base.cpp's range), the dispatch, onframe and UI_SendMessage, the timers' AddTimer and ServiceAllTimers, the
// window members they use (CallWindowHandler, GetChild, GetNext, GetAttr, AmIGettingDestroyed), UI_GetHighestZ, the postcard
// test and TickUIErrors. The set-up is in UI_init.cpp and InterfaceWindow.cpp; the window handlers, the panel's drawing and
// the dialog are other units' (UI_DialogBox and UI_ActuallyDoPostcard are generated stand-ins in the launcher).

int32_t gUIOnFrameCount;
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

// ---- the window members

int8_t InterfaceWindow::AmIGettingDestroyed() { return static_cast<int8_t>(mDestroying); }

int32_t UI_SendOnFrameMessages() {
    gUIOnFrameCount = 0;
    if (gUIRootWindow != nullptr) onframe(gUIRootWindow);
    return gUIOnFrameCount;
}

// Forgetting a window that is going (main app, UI toolkit 0x101c8038..0x101ca254; unit w51): ClearAllPointers, the three
// UI_Invalidate*Messages and the two queues' InvalidateMsgs.

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

// ---- posting

void UI_FlushAllMessages() {
    if (gUITimedMessages != nullptr) gUITimedMessages->Flush();
    if (gUIDrawQueue != nullptr) gUIDrawQueue->Flush();
    if (gUIMessageQueue != nullptr) gUIMessageQueue->Flush();
}

void UI_SetRealScreenRez(int16_t width, int16_t height) {
    gUIRealScreenWidth = width;
    gUIRealScreenHeight = height;
}

// ---- callbacks, clock

void UI_SetPeekCallBack(int32_t (*callback)(InterfaceWindow*, int32_t, int32_t, int32_t)) { gUIPeekCallBack = callback; }

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

// ---- cursors

void UI_SetStandardCursor(uint16_t cursor) { gUIStandardCursor = cursor; }

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

// activateUI (main app, SimTheme Park 2.04b3, UI_Base.cpp; unit w81). UI_Activate and UI_DeActivate (src/UI_focus.cpp) call it.

namespace {

// The message a window gets: 0x21 when the interface becomes active, 0x22 when it does not (any non-zero `active` is "active").
void Notify(InterfaceWindow* window, int32_t active) { UI_PostMessage(window, active != 0 ? 0x21 : 0x22, 0, 0); }

}  // namespace

// 0x101ca30c (672 bytes). Posts the message to the window and then, depth first, to its descendants: the window's children, theirs, and so
// on for FIVE levels below the window, written out as six nested loops as the Mac has them (the sixth level recurses into activateUI for
// its children instead, which then starts a new six levels). Each loop posts to a window, walks that window's children, and only then
// asks the window for its next sibling (the sibling is read AFTER the subtree, so a handler that unlinked it is honoured).
void activateUI(InterfaceWindow* window, int32_t active) {
    Notify(window, active);
    for (InterfaceWindow* level1 = window->GetChild(0); level1 != nullptr; level1 = level1->GetNext()) {
        Notify(level1, active);
        for (InterfaceWindow* level2 = level1->GetChild(0); level2 != nullptr; level2 = level2->GetNext()) {
            Notify(level2, active);
            for (InterfaceWindow* level3 = level2->GetChild(0); level3 != nullptr; level3 = level3->GetNext()) {
                Notify(level3, active);
                for (InterfaceWindow* level4 = level3->GetChild(0); level4 != nullptr; level4 = level4->GetNext()) {
                    Notify(level4, active);
                    for (InterfaceWindow* level5 = level4->GetChild(0); level5 != nullptr; level5 = level5->GetNext()) {
                        Notify(level5, active);
                        for (InterfaceWindow* level6 = level5->GetChild(0); level6 != nullptr; level6 = level6->GetNext()) {
                            activateUI(level6, active);
                        }
                    }
                }
            }
        }
    }
}

// UI_Base.cpp's window activation and held mouse buttons (0x101ca5e0,
// 0x101ca658, 0x101ca944). Names of the shared globals are inferred.

InterfaceWindow* gUIRootWindow;   // 0x10395c24, filled by UI_InitSystem
int32_t gUIActive = 1;           // 0x102af924, original initialised data
int16_t gUIButtonFlags;          // 0x10395bbe

void UI_Activate() {
    if (gUIRootWindow != nullptr && gUIActive == 0) {
        activateUI(gUIRootWindow, 1);
        gUIActive = 1;
    }
}

void UI_DeActivate() {
    if (gUIRootWindow != nullptr && gUIActive != 0) {
        activateUI(gUIRootWindow, 0);
        gUIActive = 0;
    }
    // Clear held mouse buttons even when there is no root window or it was inactive.
    gUIButtonFlags = 0;
}

// highestZ, main 0x101ca6dc (360 bytes), unit w50: the depth a new dialog needs to land above every visible window that
// overlaps it. UI_GetHighestZ (src/UI_pump.cpp) is the caller; Dialog::PlaceDepth reaches it through that.

// The three static words the original keeps (original names unknown; declared in include/UI_Base.h so the test can set and read
// them). Only highestZ uses them.
int32_t gUIHighestZ = 0;        // 0x10395b80  the running answer: set from the root's Z, raised by each child that is higher
int32_t gUIHighestZAttr = 0;    // 0x10395b78  the attribute word of the child last looked at
int32_t gUIHighestZChildZ = 0;  // 0x10395b7c  the Z of the child last looked at

// Walks the window's children in order. A child counts when attribute bit 1 is set and bit 2 is clear (its descendants are
// visited only then too). Its Z is the first item's for a list control (the type query answers 7) and its own otherwise.
// When that is above the answer so far: with no rectangle the answer becomes it plus 1; with a rectangle, only if the
// child's absolute position (GetPosition with 1) intersects it. The recursion's own answer is ignored; the static word
// carries it. Only the root resets the answer (to its own Z), so any other window starts from what the last call left,
// including from a different tree (UI_GetHighestZ passes the root when given NULL).
int32_t highestZ(InterfaceWindow* window, CoolRect* rect) {
    if (window == gUIRootWindow) gUIHighestZ = window->mZ;
    for (InterfaceWindow* child = window->GetChild(0); child != nullptr; child = child->GetNext()) {
        gUIHighestZAttr = static_cast<int32_t>(child->GetAttr());
        if ((gUIHighestZAttr & 1) != 0 && (gUIHighestZAttr & 2) == 0) {
            if (child->WhatAmI() == kUIKindListControl) {
                gUIHighestZChildZ = static_cast<InterfaceListControl2*>(child)->GetZOfEntries();
            } else {
                gUIHighestZChildZ = child->mZ;
            }
            if (gUIHighestZ < gUIHighestZChildZ) {
                if (rect == nullptr) {
                    gUIHighestZ = gUIHighestZChildZ + 1;
                } else {
                    CoolRect position;
                    child->GetPosition(&position, 1);
                    if (static_cast<int16_t>(position.RectIntersects(rect)) != 0) gUIHighestZ = gUIHighestZChildZ + 1;
                }
            }
            highestZ(child, rect);
        }
    }
    return gUIHighestZ;
}

int32_t UI_GetHighestZ(InterfaceWindow* window, CoolRect* rect) {
    if (window == nullptr) window = gUIRootWindow;
    if (window != nullptr) return highestZ(window, rect);
    return -1;
}

// UI_Base.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: UI_GetMouseXY (0x101ca900, 28 bytes).

// The last pointer position UI_DevicePointerMove stored.
void UI_GetMouseXY(int16_t& x, int16_t& y) {
    x = gUIPointerX;
    y = gUIPointerY;
}

int16_t UI_GetButtonFlags() { return gUIButtonFlags; }

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

// 0x101ca978 (24 bytes): NULL puts the default instance back.
void UI_SetHelpTextHandler(InterfaceHelpText* handler) { gUIHelpText = handler != nullptr ? handler : &gUIDefaultHelpText; }

// UI_Base.cpp's static initialiser and UI_ToggleHelpText (unit v5295).

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940

char gUIDefaultHelpTextNode[16];   // 0x10395ba4

// The destructor TVector at 0x10268238 (D1 with the hidden delete flag).
void UI_DestroyDefaultHelpText(InterfaceHelpText* object, int16_t deleteFlag) {
    (void)deleteFlag;
    object->~InterfaceHelpText();
}

// 0x101ca9d0
void UI_ToggleHelpText() {
    InterfaceHelpText* handler = gUIHelpText;
    if (handler->mEnabled != 0)
        handler->Disable();   // vtable +0x18
    else
        handler->Enable();    // vtable +0x14
}

// ---- help text

void UI_DisableHelpText() { gUIHelpText->Disable(); }

void UI_EnableHelpText() { gUIHelpText->Enable(); }

// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

// ---- the help text: the first halfword of the handler (its enabled flag)
bool UI_HelpTextEnabled() { return gUIHelpText->mEnabled != 0; }

// The game clock (SimTheme Park 2.04b3): the time classes CClock (src/Clock.cpp) is built
// from, 28 functions in the inventory's Game.cpp, RideDB.cpp, SaveLoad.cpp and
// FS_MasterSave.cpp ranges, and the user interface's timeGetTime clock.
//
//   CFixedTime -> CPausableFixedTime -> CSaveableFixedTime      (ReadFixed)
//   CVariableTime -> CPausableTime -> CFixableTime -> CSaveableTime      (Read)
//
// Both chains count milliseconds of LbTime_GetClock, which is a 32-bit number that wraps.
// A reading adds the unsigned difference to the previous reading to a double and hands out
// that double as an unsigned long (CodeWarrior's saturating conversion), so a clock that
// goes backwards adds almost 2^32 milliseconds. Pausing keeps the reading it paused at and
// the time spent paused; the fixed-step mode holds the reading until NextFrame moves it.

// ---- the user interface's clock

// 0x101cab58: the platform timer's microseconds as milliseconds, the low word of an
// unsigned 64-bit divide by 1000 (the same as LbTime_GetClock).
uint32_t timeGetTimeClockTickFunction::tickyticky() {
    return static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsolute() / 1000);
}

void UI_SetClockTickFunction(clockTickFunction* clock) { gUIClock = clock != nullptr ? clock : &gUIDefaultClock; }

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

uint32_t IveGotTicks() { return gUIClock->tickyticky(); }

// 0x101cac94
void __sinit_UI_Base_cpp() {
    new (&gUIDefaultHelpText) InterfaceHelpText();
    __register_global_object(&gUIDefaultHelpText, reinterpret_cast<void*>(&UI_DestroyDefaultHelpText), gUIDefaultHelpTextNode);
    // The Mac stores the vtable of 0x102a7ae4, then the one of 0x102af94c, in the clock object (same slot).
    new (&gUIDefaultClock) clockTickFunction();
}

// UIMenu::Init (main app 0x1018ad20, 260 bytes) and UIMenu::AddItem (0x1018ae78, 572 bytes; the inventory files them under ui_utils.cpp;
// unit v5163): Init makes the menu's full-screen window and hooks the handlers; AddItem adds a text button.
//
// The word at 0 (mF00) holds the item handler (a function pointer on the Mac). The host's pointers are wider than the word, so the
// word holds a handle that UIMenuHandlerHandle makes of the pointer (0 for NULL), see the end of this file.
// The callees that the host cannot call as the Mac does are the menu_callee functions below (weak: the test library defines its own).

// The callees the host cannot call as the Mac does: the program's versions here (weak), the test library's fakes in
// tests/unit_UIMenu_AddItem.cpp replace them.
namespace menu_callee {
// 0x101c7448: CoolRect::CoolRect() on the storage.
__attribute__((weak)) void RectCtor(void* mem) { new (mem) CoolRect(); }
// 0x101c74e0: CoolRect::~CoolRect() with the delete flag -1 (nothing freed).
__attribute__((weak)) void RectDtor(void* mem) { static_cast<CoolRect*>(mem)->~CoolRect(); }
}  // namespace menu_callee

// UI_Base.cpp (main app, SimTheme Park 2.04b3): the end of the UI system UI_InitSystem (src/UI_init.cpp) started.
//   0x101c8c2c UI_CloseSystem (236)
//   0x101c7b24 InterfaceMessageQueue::~InterfaceMessageQueue (108)
//   0x101c8214 InterfaceTimedMessages::~InterfaceTimedMessages (100)
//   0x101d8c2c UI_CloseTimerSystem (60; the inventory files it under ui_cityLobbyController.cpp)
// The Mac's two destructors are non-virtual with the deleting flag in r4 (a positive flag frees the object): on the host the
// destructor is the first part and `delete queue` is the deleting call. Both classes have no vtable.
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"

// 0x101c7b24. Gives the slots back (an array made with new[]) and forgets them.
InterfaceMessageQueue::~InterfaceMessageQueue() {
    if (mEntries != nullptr) {
        delete[] mEntries;
        mEntries = nullptr;
    }
}

// 0x101c8214. Gives the slots back; the pointer is left as it was.
InterfaceTimedMessages::~InterfaceTimedMessages() {
    if (mEntries != nullptr) delete[] mEntries;
}

// 0x101d8c2c. Destroys the timer table: the Mac's __destroy_new_array runs ~InterfaceTimer on each timer from the last to the first
// (each unlinks itself from the list of timers in use; the deleting half of the destructor is not run) and frees the array, which
// delete[] does here. The table's pointer and count are left as they were; the list of timers in use is forgotten.
void UI_CloseTimerSystem() {
    delete[] gUITimers;
    gUITimerList = nullptr;
}

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

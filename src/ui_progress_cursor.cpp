// Two small leaves of the UI (main app, SimTheme Park 2.04b3; unit w75; the inventory files both under ui_cityLobbyController.cpp):
//   0x101cc8ec CursorLibrary::RemoveAll (120)
//   0x101e0294 FuckOffUIProgressBar (64)
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"

CursorLibrary::Node* gCursorLibrary;   // 0x10395c2c
InterfaceWindow* gUIProgressBar;       // 0x10395c6c

// 0x101cc8ec. Walks the list: each node's next is read first, then the cursor (when there is one) is deleted through its virtual
// destructor with the deleting flag (the cursor's vtable word is at +0x10, slot +8), then the node is freed. The head is not cleared:
// UI_CloseSystem leaves it pointing at the freed first node.
void CursorLibrary::RemoveAll() {
    Node* node = gCursorLibrary;
    while (node != nullptr) {
        Node* next = node->mNext;
        if (node->mCursor != nullptr) delete node->mCursor;
        delete node;
        node = next;
    }
}

// 0x101e0294. Tells the progress bar's window message 4 (close), when there is one. The global is left as it was.
void FuckOffUIProgressBar() {
    if (gUIProgressBar != nullptr) UI_SendMessage(gUIProgressBar, 4, 0, 0);
}

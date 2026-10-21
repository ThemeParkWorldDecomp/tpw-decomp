// InterfaceCursor.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceCursor code, named after it.

#include "InterfaceWindow.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include <cstring>

// The UI's draw objects and their library (main app of SimTheme Park 2.04b3): the cursor
// base class and the cursor table (0x101cc6a0..0x101ccc8c), the draw object base class and
// DrawObjectLibrary (0x101cccc0..0x101cd42c), the engine mesh objects'
// constructors, destructors and Show (0x10187a28..0x10188c04), and the base's three empty
// members that the compiler put with them (0x1018a1b0..0x1018a234).
// PARTIAL: see ui_DrawObject.h.

// ---- InterfaceCursor

InterfaceCursor::InterfaceCursor() {
    mHotX = 0;
    mHotY = 0;
    mVisible = 1;
    mF0C = 0;
}
InterfaceCursor::~InterfaceCursor() {}
void InterfaceCursor::SetData(void*, char) {}
void InterfaceCursor::SetHot(char x, char y) {
    mHotX = static_cast<int8_t>(x);
    mHotY = static_cast<int8_t>(y);
}

void InterfaceCursor::Show() { mVisible = 1; }
void InterfaceCursor::Hide() { mVisible = 0; }
uint8_t InterfaceCursor::GetVisible() { return mVisible; }
void InterfaceCursor::Draw(int16_t, int16_t) {}
void InterfaceCursor::Use(int16_t) {}

// Two small leaves of the UI (main app, SimTheme Park 2.04b3; unit w75; the inventory files both under ui_cityLobbyController.cpp):
//   0x101cc8ec CursorLibrary::RemoveAll (120)
//   0x101e0294 FuckOffUIProgressBar (64)

CursorLibrary::Node* gCursorLibrary;   // 0x10395c2c
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

// ---- CursorTable

CursorTable::CursorTable() : mEntries(nullptr), mCount(0) {}
CursorTable::~CursorTable() {}

// Destroys the cursors and frees the table; the pointer to the freed table is left.
void CursorTable::DeAllocate() {
    if (mEntries != nullptr) {
        for (int32_t i = 0; i < mCount; ++i) {
            if (mEntries[i] != nullptr) delete mEntries[i];
        }
        delete[] mEntries;
    }
    mCount = 0;
}

int32_t CursorTable::Allocate(uint16_t count) {
    mEntries = new InterfaceCursor*[count];
    if (mEntries == nullptr) return 0;
    for (int32_t i = 0; i < count; ++i) mEntries[i] = nullptr;
    mCount = count;
    return 1;
}

// Only an empty place takes a cursor.
int32_t CursorTable::SetEntry(uint16_t index, InterfaceCursor* cursor) {
    if (index < mCount && cursor != nullptr && mEntries[index] == nullptr) {
        mEntries[index] = cursor;
        return 1;
    }
    return 0;
}

InterfaceCursor* CursorTable::GetEntry(uint16_t index) { return index < mCount ? mEntries[index] : nullptr; }
void InterfaceCursor::Animate(int32_t) {}

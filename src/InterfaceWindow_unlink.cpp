// InterfaceWindow::unlink (main 0x101de084, 124 B): remove from the sibling list, then update the pointer.
#include "InterfaceWindow.h"
void InterfaceWindow::unlink() {
    if (mParent != nullptr && mPrevious == nullptr) mParent->mChild = mNext;
    if (mPrevious != nullptr) mPrevious->mNext = mNext;
    if (mNext != nullptr) mNext->mPrevious = mPrevious;
    mNext = nullptr;
    mPrevious = nullptr;
    mParent = nullptr;
    UI_UpdatePointerIn();
}

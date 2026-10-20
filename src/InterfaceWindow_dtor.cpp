// InterfaceWindow's destructor and the two members it uses (main app, UI toolkit 0x101d9ba4, 0x101d9ee8 and 0x101dcebc; unit w51):
// ~InterfaceWindow, PropagateDestruction and UnCaptureKeys.
//
// The Mac's destructor takes the compiler's deleting flag in r4 (the child windows are destroyed through the vtable with 1, the CoolRect
// members with -1) and calls operator delete when it is positive; on the host the deleting form is the compiler's, so this is the plain
// body. The vtable pointer store at the top (+0x118 on the Mac) is the compiler's too.
#include "InterfaceWindow.h"

#include "ui_DrawObject.h"

namespace {
// A draw object the window lets go of: it loses a user and is told Show(0); true when it is not in the library and has no user left
// (the same test as SetDrawObject's). The caller then deletes it if the window still holds it (the Mac reads the window's member again
// after the Show).
bool GivesUp(InterfaceDrawObject* object) {
    object->mResizable = static_cast<int16_t>(object->mResizable - 1);
    object->Show(0);
    return static_cast<int16_t>(object->mResizable - (1 - object->mInLibrary)) == -1;
}
}  // namespace

// 0x101d9ba4 (736 bytes). In order: tells the window's subtree it is going (unless that has begun), tells the help text handler
// (OnHideOrDelete), lets go of the draw object and then of the client draw object, removes the animation timers that are left (ids
// 0x80080 to 0x80083), deletes a region that is not the window's own rectangle, clears every global that names the window, poisons its
// queued messages in the three queues, unlinks it from its siblings and parent, deletes its children one at a time (each unlinks itself
// from mChild; a child that did not would be deleted for ever), works out the window under the pointer again and, last, the rectangle
// members are destroyed (the compiler's, in the reverse of their order: +0x34, +0x28, +0x1c, +0x10, +4). QUIRK: the window's own
// next and previous links are left as they were (only the parent link is cleared).
InterfaceWindow::~InterfaceWindow() {
    if (mDestroying == 0) PropagateDestruction();
    gUIHelpText->OnHideOrDelete(this);
    if (mDrawObject != nullptr) {
        InterfaceDrawObject* object = mDrawObject;
        if (GivesUp(object) && mDrawObject != nullptr) delete mDrawObject;
    }
    if (mClientDrawObject != nullptr) {
        InterfaceDrawObject* object = mClientDrawObject;
        if (GivesUp(object) && mClientDrawObject != nullptr) delete mClientDrawObject;
    }
    for (int32_t i = 0; i < 4; ++i) {
        if (mAnimTimers[i] != nullptr) UI_RemoveTimer(this, 0x80080 + i);
    }
    if (mRegion != nullptr && mRegion != &mRect) delete mRegion;
    ClearAllPointers(this);
    UI_InvalidateMessages(this);
    UI_InvalidateDrawMessages(this);
    UI_InvalidateDeferredMessages(this);
    if (mParent != nullptr && mPrevious == nullptr) mParent->mChild = mNext;
    if (mPrevious != nullptr) mPrevious->mNext = mNext;
    if (mNext != nullptr) mNext->mPrevious = mPrevious;
    mParent = nullptr;
    while (mChild != nullptr) delete mChild;
    UI_UpdatePointerIn();
}

// 0x101d9ee8 (500 bytes). Tells the window message 0x14 (destroy), marks it, does the same for each child in turn along mNext, and
// when a window's subtree is done takes it out of the key capture, the keyboard window and the focus window. The Mac unrolls the first
// four levels and calls itself for the fifth and deeper; the order of the sends and of the clears is the recursion's. A window that is
// marked already is not sent the message (UI_SendMessage refuses it) but is walked all the same.
void InterfaceWindow::PropagateDestruction() {
    UI_SendMessage(this, 0x14, 0, 0);
    mDestroying = 1;
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) child->PropagateDestruction();
    UnCaptureKeys();
    if (gUIKeyboardWindow == this) gUIKeyboardWindow = nullptr;
    if (gUIFocusWindow == this) gUIFocusWindow = nullptr;
}

// 0x101dcebc (48 bytes). When this window is the one that captured the keys, both words of the capture are cleared and the answer is
// 1; otherwise nothing changes and the answer is 0. (Nothing reads the answer.)
int32_t InterfaceWindow::UnCaptureKeys() {
    if (gUIKeyCapture.mWindow != this) return 0;
    gUIKeyCapture.mWindow = nullptr;
    gUIKeyCapture.mKeys = nullptr;
    return 1;
}

// InterfaceWindow (main app, UI toolkit 0x101db1b0..0x101dc2f0): the window's show and position leaves that the drawing unit
// (InterfaceWindow_draw.cpp) calls and does not define: Show, ShowAllDRAWOBJS, GetWindow and two of the three SetPosition overloads.
#include "InterfaceWindow.h"

#include "ui_DrawObject.h"

// The first child with this id, or NULL. (The original takes one more step along the list after finding it; that reads nothing that
// matters.)
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) {
        if (child->mId == id) return child;
    }
    return nullptr;
}

// A window takes part when its attribute bit 1 (visible) is set; a request to hide takes every window, whatever its bit says. A
// part is a visit to both draw objects (Show), the handler (message 0x11 with `shown`, called directly) and then every child, which is
// the same walk. (The Mac unrolls the first three levels of the tree; the order is the same.)
void InterfaceWindow::ShowAllDRAWOBJS(int16_t shown) {
    if (((mAttributes & 1) != 0 && shown != 0) || shown == 0) {
        if (mDrawObject != nullptr) mDrawObject->Show(shown);
        if (mClientDrawObject != nullptr) mClientDrawObject->Show(shown);
        if (mCallback != nullptr) mCallback(this, 0x11, shown, 0);
        for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) child->ShowAllDRAWOBJS(shown);
    }
}

// Any nonzero `on` is 1. A window in the state asked for is left alone. A window that is shown gets its bit set first, and the draw
// objects and the handler hear of it only when this window and all its parents are visible (otherwise it is shown in name only). A
// hidden one is announced to the help text handler after the pointer's window is worked out again.
int32_t InterfaceWindow::Show(int16_t on) {
    const int16_t shown = on != 0 ? 1 : 0;
    const int16_t visible = (mAttributes & 1) == 1 ? 1 : 0;
    if (shown == visible) return 1;
    bool announce = true;
    if (shown == 0) {
        mAttributes &= ~1u;
    } else {
        mAttributes |= 1;
        for (const InterfaceWindow* window = this; window != nullptr && announce; window = window->mParent) {
            if ((window->mAttributes & 1) == 0) announce = false;
        }
    }
    if (announce) ShowAllDRAWOBJS(shown);
    UI_UpdatePointerIn();
    if (shown == 0) gUIHelpText->OnHideOrDelete(this);
    return 1;
}

// The window is moved to (*x, *y) with the size it has (a NULL keeps that coordinate). The right and bottom are 16-bit sums.
int32_t InterfaceWindow::SetPosition(int16_t* x, int16_t* y) {
    const int32_t width = mRect.mRight - mRect.mLeft;
    const int32_t height = mRect.mBottom - mRect.mTop;
    const int32_t left = x != nullptr ? *x : mRect.mLeft;
    const int32_t top = y != nullptr ? *y : mRect.mTop;
    CoolRect rect(static_cast<int16_t>(left), static_cast<int16_t>(top), static_cast<int16_t>(left + width),
                  static_cast<int16_t>(top + height));
    SetPosition(&rect);
    return 1;
}

int32_t InterfaceWindow::SetPosition(int32_t left, int32_t top, int32_t right, int32_t bottom) {
    CoolRect rect(static_cast<int16_t>(left), static_cast<int16_t>(top), static_cast<int16_t>(right), static_cast<int16_t>(bottom));
    SetPosition(&rect);
    return 1;
}

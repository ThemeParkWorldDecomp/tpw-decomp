// InterfaceWindow::SetOriginalPosition 0x101db284 (48 bytes; Mac file ui_cityLobbyController.cpp; unit w118).
#include "InterfaceWindow.h"

// The four shorts of the rectangle (read with lha, written with sth) go to the rectangle the window was made with, in the order left, top,
// right, bottom. A NULL rectangle writes nothing. The answer is 1 either way (GetOriginalPosition answers 0 for NULL).
int32_t InterfaceWindow::SetOriginalPosition(CoolRect* rect) {
    if (rect != nullptr) {
        mOriginalRect.mLeft = rect->mLeft;
        mOriginalRect.mTop = rect->mTop;
        mOriginalRect.mRight = rect->mRight;
        mOriginalRect.mBottom = rect->mBottom;
    }
    return 1;
}

// InterfaceWindow::SetFrame (SimTheme Park 2.04b3, 0x101db7dc; the inventory files it under ui_cityLobbyController.cpp): the
// window's picture is chosen by telling its draw objects to show another frame.
#include "InterfaceWindow.h"

#include "ui_DrawObject.h"

// Change's flag 1 carries the frame (a 16-bit number); the window's draw object and its client draw object are each told, when
// the window has them.
void InterfaceWindow::SetFrame(int32_t frame) {
    DOChange change{};
    change.mFrame = static_cast<int16_t>(frame);
    if (mDrawObject != nullptr) mDrawObject->Change(1, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(1, &change);
}

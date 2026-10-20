// InterfaceWindow::CheckClientMouse, main 0x101dc5d0 (160 bytes), unit w37c.
#include "InterfaceWindow.h"
void InterfaceWindow::CheckClientMouse(int32_t message, int32_t button, int32_t point) {
    if (mClientRect.PointIn(static_cast<int16_t>(static_cast<uint32_t>(point) >> 16), static_cast<int16_t>(point))) {
        const UIButtonState& state = gUIButtons[button];
        UI_PostMessage(this, message | 0x1000, button,
                       static_cast<int32_t>(static_cast<uint32_t>(state.mX) << 16) | state.mY);
    }
}

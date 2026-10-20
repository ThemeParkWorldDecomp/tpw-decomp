// UI_SendMessage 0x101c9518 (164 bytes) and CallWindowHandler 0x101de384 (56); existing bodies, main, unit w43.
#include "InterfaceWindow.h"
#include "UI_Base.h"

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (window == nullptr || window->mDestroying != 0) return -1;
    if (gUIPeekCallBack != nullptr) gUIPeekCallBack(window, message, a, b);
    return window->CallWindowHandler(message, a, b);
}

int32_t InterfaceWindow::CallWindowHandler(int32_t message, int32_t a, int32_t b) {
    if (mCallback != nullptr) return mCallback(this, message, a, b);
    return -1;
}

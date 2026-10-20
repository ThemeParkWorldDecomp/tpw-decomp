// UI_PostMessage, main 0x101c9400 (220 bytes); existing body separated for unit w43.
#include "InterfaceWindow.h"
#include "UI_Base.h"

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (static_cast<uint32_t>(message) - 1 <= 2) {
        if (gUIDrawQueue != nullptr && window != nullptr && window->mDestroying == 0) {
            gUIDrawQueue->Add(window, message, a, b);
            return 1;
        }
    } else if (static_cast<uint32_t>(message) - 0x1000a <= 2) {
        gUIMessageQueue->Add(window, message, a, b);
    } else if (gUIMessageQueue != nullptr && window != nullptr && window->mDestroying == 0) {
        gUIMessageQueue->Add(window, message, a, b);
        return 1;
    }
    return 0;
}

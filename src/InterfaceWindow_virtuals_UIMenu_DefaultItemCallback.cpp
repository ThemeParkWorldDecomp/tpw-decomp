#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

// 0x1018aa48 (232 bytes): a menu item's window handler. 0x14 (destroy) releases the string in extra parameter 1;
// 0x10006 plays the sound and tells the parent the item's id (message 0x100001); the rest goes to the default handler.
int32_t UIMenu_DefaultItemCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        UIMain_UISound(0xc1);
        int32_t id = window->GetID();
        UI_SendMessage(window->GetParent(), 0x100001, id, 0);
        return 0;
    }
    if (message < 0x10006 && message == 0x14) {
        InterfaceStringHeap::ReleaseString(static_cast<char16_t*>(window->getExtraParam(1)));
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x1018ab78 (136 bytes): on destroy (0x14) clears word 0x10 of extra parameter 0 (no NULL check), then the default handler.
int32_t UIMenu_DefaultCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) {
        *reinterpret_cast<int32_t*>(static_cast<char*>(window->getExtraParam(0)) + 0x10) = 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

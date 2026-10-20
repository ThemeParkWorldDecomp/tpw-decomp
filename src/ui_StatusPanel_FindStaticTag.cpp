// FindStaticTag 0x10191cf4 and AddTag 0x101921f4 (unit v5249).
#include "InterfaceWindow.h"
#include "ui_main.h"

// The index of the first static tag that holds the window, -1 for none. The index is a 16-bit counter.
int32_t FindStaticTag(InterfaceWindow* window) {
    int32_t result = -1;
    int16_t found = 0;
    for (int16_t i = 0; i < 10 && found == 0; ++i) {
        if (gStaticTags[i].mWindow == window) {
            found = 1;
            result = i;
        }
    }
    return result;
}

bool AddTag(CTagParams& params) {
    if (params.mShow == 1) {
        InterfaceWindow* window = AddATag(params);
        if (window == nullptr) return false;
        ShowTagPanel(window);
        return true;
    }
    return AddATag(params) != nullptr;
}

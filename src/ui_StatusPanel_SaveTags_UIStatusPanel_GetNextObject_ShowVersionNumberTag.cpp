// ShowVersionNumberTag 0x101928dc and delme 0x10193084 (ui_StatusPanel.cpp).
#include <cstdint>

#include "LLCellSystem.h"
#include "online.h"
#include "platform/msl.h"
#include "ui_main.h"

// 0x1024dde8: the version tag's format string (wide); starts "Ve" in the original, the rest is not known here.
char16_t gVersionNumberFormat[32] = u"Version %d.%d";

// Makes a tag from the format string with 2 and 0 and shows its panel.
void ShowVersionNumberTag() {
    char16_t text[16];
    CTagParams params = {};
    params.mId = 0x17e;
    params.mF4 = 0x17e;
    swprintf(text, 0x10, gVersionNumberFormat, 2, 0);
    params.mText = text;
    params.mTime = 0;
    params.mShow = 1;
    if (params.mShow == 1) {
        InterfaceWindow* window = AddATag(params);
        if (window != nullptr) ShowTagPanel(window);
    } else {
        AddATag(params);
    }
}

// The confirm handler of the dialog: selects the object, then clicks on its cell.
int32_t delme(void* thing) {
    const uint8_t* bytes = static_cast<const uint8_t*>(thing);
    uint16_t cell = static_cast<uint16_t>(bytes[4] + (static_cast<uint32_t>(bytes[6]) << 7) + 1);
    UI_SetSelectedObject(thing);
    LLCM_SetCurrentObject(true);
    LLCC_SystemSetMode(0x33, true);
    uint32_t v = static_cast<uint32_t>(cell) - 1;
    LLCM_LeftMouseButtonUp(static_cast<int32_t>(v & 0x7f), static_cast<int32_t>(v >> 7), 0, 0);
    return 1;
}

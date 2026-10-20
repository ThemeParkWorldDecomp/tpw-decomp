// LinkSlideoutPanel 0x101a82ec and RemoveSlideoutPanel 0x101a83d8: the main panel's slide-out slot (child 0x21).
#include <cstdint>

#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_main.h"

// 0x101a82ec (176 bytes). With the main panel and a panel given and the slot (child 0x21) found: when the slot's extra parameter 2
// is non-zero the answer is NULL; else the slot is sent 0xb (0, 0) and posted 0xa with the panel as its first argument. Every other
// path answers the panel itself. (The second argument is never read.)
void LinkSlideoutPanel(InterfaceWindow* panel, int32_t n) {
    LinkSlideoutPanelResult(panel, n);
}

// The same function with r3 kept: the answer the Mac leaves (the panel, or NULL). The header declares LinkSlideoutPanel as void because
// another unit's fake (tests/unit_CoasterEditMode.cpp) defines it so; callers ignore the answer.
InterfaceWindow* LinkSlideoutPanelResult(InterfaceWindow* panel, int32_t n) {
    (void)n;
    if (gMainPanel != nullptr && panel != nullptr) {
        InterfaceWindow* slot = gMainPanel->GetWindow(0x21);
        if (slot != nullptr) {
            if (slot->getExtraParam(2) != nullptr) return nullptr;
            UI_SendMessage(slot, 0xb, 0, 0);
            UI_PostMessage(slot, 0xa, static_cast<int32_t>(reinterpret_cast<intptr_t>(panel)), 0);
        }
    }
    return panel;
}

// 0x101a83d8 (256 bytes). Needs the game window and the main panel. The slot (child 0x21): when its byte at +0x134 (signed) is 3 or 4,
// extra parameter 2 is cleared if set; else it is sent 0xb (0, 0). Then, for a non-zero `which`, child 0x27 of child 0x25 of the main
// panel is set to state 0, redraw 1 (no NULL checks, as on the Mac).
void RemoveSlideoutPanel(int16_t which) {
    if (gGameWindow == nullptr) return;
    if (gMainPanel == nullptr) return;
    InterfaceWindow* slot = gMainPanel->GetWindow(0x21);
    if (slot != nullptr) {
        const int8_t kind = static_cast<int8_t>(slot->mF134);
        if (kind == 3 || kind == 4) {
            if (slot->getExtraParam(2) != nullptr) slot->setExtraParam(2, nullptr);
        } else {
            UI_SendMessage(slot, 0xb, 0, 0);
        }
    }
    if (which != 0) {
        InterfaceWindow* a = gMainPanel->GetWindow(0x25);
        InterfaceButton* button = static_cast<InterfaceButton*>(a->GetWindow(0x27));
        button->SetState(0, 1);
    }
}

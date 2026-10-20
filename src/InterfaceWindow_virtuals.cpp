// The base window's small virtual members (main app, UI toolkit; unit w54): DrawFunction, Enable, operator-, GetValue, GetText and
// GetHelpText. Every window exposes them to the frame through its vtable (0x102a7d94); the button (src/InterfaceButton.cpp) and the
// other controls override some and call these.
#include "InterfaceWindow.h"
#include "UI_Base.h"

// 0x1018a270 (4 bytes): nothing (the Mac's is a bare blr).
void InterfaceWindow::DrawFunction() {}

// 0x101dc320 (152 bytes). Bit 2 of the attributes is "disabled": Enable(0) sets it, any other value clears it. The answer is 1 when
// the bit changed, else 0 (nothing else is done then). When it changed the window's message function is called DIRECTLY (not through
// UI_SendMessage) with the enable message 0x17 and the short `on` sign-extended, if there is one, and the window under the pointer
// is worked out again.
int16_t InterfaceWindow::Enable(int16_t on) {
    int16_t changed = 0;
    if (on == 0) {
        if ((mAttributes & 2) == 0) {
            changed = 1;
            mAttributes |= 2;
        }
    } else if ((mAttributes & 2) != 0) {
        changed = 1;
        mAttributes &= ~static_cast<uint32_t>(2);
    }
    if (changed != 0) {
        if (mCallback != nullptr) mCallback(this, 0x17, on, 0);
        UI_UpdatePointerIn();
    }
    return changed;
}

// 0x101dcf24 (108 bytes). Asks both windows what they are (WhatAmI, vtable slot +0x14, through each one's own vtable: this one first)
// and answers 0. The Mac compares the two answers, but both of its branches answer 0, so the comparison does nothing. (An earlier reading took
// the slot as ClientDraw; the vtable has it at +0x18.)
int32_t InterfaceWindow::operator-(InterfaceWindow& other) {
    WhatAmI();
    other.WhatAmI();
    return 0;
}

// 0x101dff2c (8 bytes): 0.
int32_t InterfaceWindow::GetValue() { return 0; }

// 0x1018a9d4 (8 bytes): no text.
char16_t* InterfaceWindow::GetText() { return nullptr; }

// 0x1018aa0c (8 bytes): the help text's number (mHelpText, -1 from the constructor).
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }

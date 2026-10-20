// InterfaceTextBox::operator- (0x101d8034), main app.
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_main.h"

// Compares two windows: 0 when their kinds (WhatAmI, slot +0x14) differ, else wcscmp of the two text blocks (the other one is read
// at +0x138 whatever it is, as the original does).
int32_t InterfaceTextBox::operator-(InterfaceWindow& other) {
    int32_t mine = WhatAmI();
    int32_t theirs = other.WhatAmI();
    if (theirs == mine) return wcscmp(mText, static_cast<InterfaceTextBox&>(other).mText);
    return 0;
}

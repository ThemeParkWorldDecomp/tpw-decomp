// The cursor made from a picture file, InterfaceBFCursor (main app 2.04b3): its destructor and four virtual members
// (0x101fe27c, 0x101fe338, 0x101fe390, 0x101fe404, 0x101fe478), which talk to the platform's cursor (Sam's Utilities'
// UCursor). The constructor and LoadData, which makes that cursor from the picture, are in src/ui_cursors.cpp; the base
// class InterfaceCursor and CursorTable (0x101cc6a0..0x101ccc8c) are in src/ui_DrawObject.cpp, where unit w21 put them.
#include "platform/samsutils.h"
#include "ui_DrawObject.h"

using namespace SamsUtilities;

// Lets the platform's cursor go (a UCursorOpenGL: its virtual destructor with the delete flag), then the base class'
// destructor runs. mCursor is not cleared.
InterfaceBFCursor::~InterfaceBFCursor() {
    if (mCursor != nullptr) delete static_cast<UCursorOpenGL*>(mCursor);
}

// Whether the platform's cursor is shown. The library's IsVisible is static: it does not look at this cursor, and a
// cursor without a picture answers too.
uint8_t InterfaceBFCursor::GetVisible() { return UCursor::IsVisible(); }

// Show and Hide tell the library to show or hide the cursor (they nest: it keeps a count) and keep the count it
// answers, as a byte, in the visible flag. A cursor without a picture does nothing.
void InterfaceBFCursor::Show() {
    if (mCursor != nullptr) mVisible = static_cast<uint8_t>(UCursor::Show());
}

void InterfaceBFCursor::Hide() {
    if (mCursor != nullptr) mVisible = static_cast<uint8_t>(UCursor::Hide());
}

// Takes the cursor in use (a non-zero argument: its hot spot, the two signed bytes, goes to the platform's cursor
// first) or gives it up (0: the host's arrow comes back).
void InterfaceBFCursor::Use(int16_t use) {
    if (mCursor == nullptr) return;
    if (use != 0) {
        UPoint hotSpot;
        hotSpot.x = mHotX;
        hotSpot.y = mHotY;
        static_cast<UCursorOpenGL*>(mCursor)->SetHotspot(hotSpot);
        mCursor->Set();
    } else {
        mCursor->Reset();
    }
}

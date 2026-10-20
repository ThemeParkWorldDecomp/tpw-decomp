// InterfaceButton (main app, UI toolkit 0x101cad20..0x101cbf58): the control the lobby's menu is made of. The constructor
// and the 14 members below; the window handler it stores (UI_DefaultButtonCallback, 0x101cb5f0, 2,316 bytes) is not
// translated yet.
//
// The button draws itself with two draw objects (the window's and the client's): a frame number picks the picture
// (CalcFrame) and the client object also gets the text's colour and offset (DoTextColour). Every change goes to the draw
// objects as a DOChange record built on the stack.
#include <cstring>

#include "ui_DrawObject.h"
#include "ui_controls.h"

// The window handler is stored as the window's message function and as its default one.
InterfaceButton::InterfaceButton(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                 int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mState = 0;
    mHilight = 0;
    mRepeatDelay = 500;
    mRepeatInterval = 125;
    mCallback = UI_DefaultButtonCallback;
    mDefaultCallback = UI_DefaultButtonCallback;
    mHeldState = 0;
    mText = nullptr;
    mTextUpColour = {0xff, 0xff, 0xff, 0xff};
    mTextDownColour = {0, 0, 0, 0xff};
    mTextShiftX = 1;
    mTextShiftY = 1;
}

// The destructor gives the text back to the string heap. (The Mac's skips everything for a NULL `this`; the base class's
// destructor and the delete after it are the compiler's here.)
InterfaceButton::~InterfaceButton() {
    if (mText != nullptr) InterfaceStringHeap::ReleaseString(mText);
}

int32_t InterfaceButton::WhatAmI() { return 2; }

// The picture to show: 1 when the button is disabled (attribute 2); with the highlight on, 3 (state bit 0, the button is
// down) or 2; else by the state: bit 1 gives 4, bit 0 gives 5.
int16_t InterfaceButton::CalcFrame() {
    if ((mAttributes & 2) != 0) return 1;
    if (mHilight != 0) return (mState & 1) != 0 ? 3 : 2;
    if ((mState & 2) != 0) return 4;
    if ((mState & 1) != 0) return 5;
    return 0;
}

// The base class's Enable answers whether the disabled bit changed. When it did, the draw objects are told the new frame
// (flag 1 of a DOChange), the window's first.
int16_t InterfaceButton::Enable(int16_t on) {
    if (InterfaceWindow::Enable(on) == 0) return 0;
    DOChange change;
    change.mFrame = CalcFrame();
    if (mDrawObject != nullptr) mDrawObject->Change(1, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(1, &change);
    return 1;
}

int8_t InterfaceButton::GetState() { return mState; }

// Tells the client draw object (nothing without one) the text style of the frame: flags 0x4c000 with the text offset (two
// shorts) and the colour. Frames 3 and 5 (down) take the down colour and the offsets (mTextShiftX, mTextShiftY); frame 4
// takes the up colour and the offsets with x one more; frames 0 and 2 the up colour with no offset. Frame 1 (disabled) and
// any other number leave the style alone and the flags 0: the object is told Change(0) with a record nothing was written to.
void InterfaceButton::DoTextColour() {
    if (mClientDrawObject == nullptr) return;
    DOTextStyle style;
    DOChange change;
    change.mText = &style;
    uint32_t flags = 0;
    int16_t shiftX = 0;
    int16_t shiftY = 0;
    UI_RGBA colour = mTextUpColour;
    const int16_t frame = CalcFrame();
    if (frame == 3 || frame == 5) {
        flags = 0x4c000;
        shiftX = mTextShiftX;
        shiftY = mTextShiftY;
        colour = mTextDownColour;
    } else if (frame == 4) {
        flags = 0x4c000;
        shiftX = static_cast<int16_t>(mTextShiftX + 1);
        shiftY = mTextShiftY;
    } else if (frame == 0 || frame == 2) {
        flags = 0x4c000;
    }
    if (flags != 0) {
        // (ui_DrawObject.h names no member for the style's two offsets yet: they are the shorts at +0x0a and +0x0c)
        std::memcpy(&style.mF0A[0], &shiftX, sizeof shiftX);
        std::memcpy(&style.mF0A[2], &shiftY, sizeof shiftY);
        style.mColour[0] = colour.mR;
        style.mColour[1] = colour.mG;
        style.mColour[2] = colour.mB;
        style.mF13 = colour.mA;
    }
    mClientDrawObject->Change(flags, &change);
}

// A new state (the byte is stored as given, compared as a signed byte): the window is posted a draw message when `redraw`
// and the UI's auto redraw are set, the window's draw object is told the frame and the text style is redone.
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    if (state == mState) return 0;
    mState = state;
    if (redraw != 0 && gUIAutoRedraw != 0) UI_PostMessage(this, 1, 0, 0);
    DOChange change;
    change.mFrame = CalcFrame();
    if (mDrawObject != nullptr) mDrawObject->Change(1, &change);
    DoTextColour();
    return 1;
}

// The same for the highlight, except that both draw objects are told the frame and the text style is not redone.
int32_t InterfaceButton::HiLight(int8_t on, int8_t redraw) {
    if (on == mHilight) return 0;
    mHilight = on;
    if (redraw != 0 && gUIAutoRedraw != 0) UI_PostMessage(this, 1, 0, 0);
    DOChange change;
    change.mFrame = CalcFrame();
    if (mDrawObject != nullptr) mDrawObject->Change(1, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(1, &change);
    return 1;
}

// The text style is redone first, then the window class keeps the text.
int32_t InterfaceButton::SetText(char16_t* text) {
    DoTextColour();
    InterfaceWindow::SetText(text);
    return 1;
}

char16_t* InterfaceButton::GetText() { return mText; }

int32_t InterfaceButton::StopHeldTimer() {
    const int32_t answer = mHeldState == 2 ? 0 : 1;
    mHeldState = 0;
    return answer;
}

void InterfaceButton::SetAutoRepeat(int32_t delay, int32_t interval) {
    mRepeatDelay = delay;
    mRepeatInterval = interval;
}

void InterfaceButton::SetTextUpColour(UI_RGBA colour) {
    mTextUpColour = colour;
    DoTextColour();
}

void InterfaceButton::SetTextDownColour(UI_RGBA colour) {
    mTextDownColour = colour;
    DoTextColour();
}

// A button held down answers the second help text number (the word at +0x90), any other state the first (+0x8c).
int32_t InterfaceButton::GetHelpText() { return static_cast<uint8_t>(mState) == 1 ? mF90 : mHelpText; }

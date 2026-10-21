// InterfaceButton.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceButton code, named after it.

#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include <cstring>

// InterfaceButton (main app, UI toolkit 0x101cad20..0x101cbf58): the control the lobby's menu is made of. The constructor
// and the 14 members below; the window handler it stores (UI_DefaultButtonCallback, 0x101cb5f0, 2,316 bytes) is not
// translated yet.
//
// The button draws itself with two draw objects (the window's and the client's): a frame number picks the picture
// (CalcFrame) and the client object also gets the text's colour and offset (DoTextColour). Every change goes to the draw
// objects as a DOChange record built on the stack.

// The picture to show: 1 when the button is disabled (attribute 2); with the highlight on, 3 (state bit 0, the button is
// down) or 2; else by the state: bit 1 gives 4, bit 0 gives 5.
int16_t InterfaceButton::CalcFrame() {
    if ((mAttributes & 2) != 0) return 1;
    if (mHilight != 0) return (mState & 1) != 0 ? 3 : 2;
    if ((mState & 2) != 0) return 4;
    if ((mState & 1) != 0) return 5;
    return 0;
}

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

int32_t InterfaceButton::WhatAmI() { return 2; }

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

// The window handlers of five controls (main app, UI toolkit): the button (0x101cb5f0), the check buttons (0x101cc27c), the knob
// (0x101d07ac), the text box (0x101d8134) and the vary box (0x101d9674). The constructors store them as the window's message
// function (and, for the button and the check buttons, as its default one). Each handles a few messages and passes the rest to the
// window's own handler, UI_DefaultWindowCallback, whose answer it gives back.
//
// The messages the pump sends (FINDINGS 2026-10-11 w21/w29/w30): 0x10001 the pointer came into the window, 0x10002 it left,
// 0x10003 it moved (x in the high half of `a`, y in the low), 0x10004 mouse button `a` went up, 0x10005 went down (`b` is the
// point), 0x1e a frame, 0x15 the window was made. Messages 0x100 (a click), 0x10008 and 0x10009 are the ones a button sends to
// its parent (see InterfaceButton), 0x800 the value of a control changed.

namespace {

// The attribute of a button that makes it a toggle (it stays down after a click until it is clicked again) and the one that makes it
// repeat while held; for a group of check buttons 0x10 means that one button is always selected.
constexpr uint32_t kToggle = 0x10;
constexpr uint32_t kAutoRepeat = 0x20;

// The button's state bits: 1 down, 2 down with the pointer over it.
constexpr int8_t kStateDown = 1;
constexpr int8_t kStateOver = 2;

// The first of the three mouse buttons is the one a button reacts to.
constexpr int32_t kLeftButton = 0;

// The state the left button has in the pump's table moves with the pointer: out of the window it is 1 (held, pressed here) -> 2
// (held, moved away); back in it is 2 -> 1.
void ChangeHeldButtons(int16_t from, int16_t to) {
    for (int32_t i = 0; i < 3; ++i) {
        if (gUIButtons[i].mState == from) gUIButtons[i].mState = to;
    }
}

}  // namespace

// ---- the button

// Pointer in (0x10001): the cursor of the window is shown and the help text told. With a mouse button held (the window caught its
// press) the held buttons that were moved away count as pressed here again, the button shows itself down with the pointer over it
// (state bit 2) and the window is posted 0x10008 (its id and the pointer's position). That message goes to the window ITSELF here
// (the parent only gets 0x10008 from the on-frame repeat below). Without a held button the button lights up.
// Pointer out (0x10002): the help text is told; a button held goes to "moved away" (state bit 2 off), otherwise the light goes out.
// Pointer moved (0x10003): only the help text is told (the point, as two shorts).
// The left button's release (0x10004) and press (0x10005) are the button's own business and are not passed on.
int32_t UI_DefaultButtonCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    InterfaceButton* button = static_cast<InterfaceButton*>(window);
    switch (message) {
    case 0x1e:
        // A button with the auto-repeat attribute that is held: after the delay (mRepeatDelay) it posts its parent 0x10009 (its id) once
        // and goes on to the interval (mRepeatInterval), after which every interval it posts 0x10008. Both only while the
        // pointer is over it (state bit 2). The ticks are read again to store the time of a step, after the test.
        if (window->mButtonsHeld != 0 && (window->mAttributes & kAutoRepeat) != 0) {
            if (button->mHeldState == 1) {
                const int32_t elapsed = static_cast<int32_t>(IveGotTicks() - button->mHeldTick);
                if (elapsed >= button->mRepeatDelay) {
                    button->mHeldState = 2;
                    button->mHeldTick = IveGotTicks();
                    if ((button->mState & kStateOver) != 0) UI_PostMessage(window->mParent, 0x10009, window->mId, 0);
                }
            } else if (button->mHeldState == 2) {
                const int32_t elapsed = static_cast<int32_t>(IveGotTicks() - button->mHeldTick);
                if (elapsed > button->mRepeatInterval) {
                    if ((button->mState & kStateOver) != 0) UI_PostMessage(window->mParent, 0x10008, window->mId, 0);
                    button->mHeldTick = IveGotTicks();
                }
            }
        }
        break;
    case 0x10001:
        window->cursorChange();
        gUIHelpText->OnMouseMovedIn(window);
        if (window->mButtonsHeld != 0) {
            const int8_t state = button->mState;
            ChangeHeldButtons(2, 1);
            button->SetState(static_cast<int8_t>(state | kStateOver), 1);
            UI_PostMessage(window, 0x10008, window->GetID(), a);
        } else {
            button->HiLight(1, 1);
        }
        return 0;
    case 0x10002:
        gUIHelpText->OnMouseMovedOut();
        if (window->mButtonsHeld != 0) {
            const int8_t state = button->mState;
            ChangeHeldButtons(1, 2);
            button->SetState(static_cast<int8_t>(state & ~kStateOver), 1);
        } else {
            button->HiLight(0, 1);
        }
        return 0;
    case 0x10003:
        gUIHelpText->OnMouseMoved(static_cast<int16_t>(a >> 16), static_cast<int16_t>(a));
        return 0;
    case 0x10004:
        // The left button goes up. When it was pressed here (the pump's table says 1) and the hold's timer was not at its second stage
        // (StopHeldTimer answers 1: not an auto-repeating hold that has fired) the button counts as clicked. A toggle button flips
        // (down with the pointer over it goes up, up with the pointer over it goes down) and tells the help text handler's Update (the
        // help text number depends on the state); any other button goes up. The window and the table forget the press, the light
        // comes on, and a click posts the PARENT 0x100 (the button's id, its state as a signed short).
        if (a == kLeftButton) {
            bool clicked = false;
            if (gUIButtons[kLeftButton].mState == 1 && button->StopHeldTimer() != 0) clicked = true;
            const int8_t state = button->mState;
            if ((window->GetAttr() & kToggle) != 0) {
                if ((state & kStateOver) != 0) {
                    if ((state & kStateDown) != 0) {
                        if (button->mState != 0) button->SetState(0, 1);
                    } else if (button->mState != 1) {
                        button->SetState(1, 1);
                    }
                }
                gUIHelpText->Update();
            } else if (button->mState != 0) {
                button->SetState(0, 1);
            }
            window->mButtonsHeld &= ~(1u << kLeftButton);
            gUIButtons[kLeftButton].mState = 0;
            gUIButtons[kLeftButton].mTime = 0;
            button->HiLight(1, 1);
            if (clicked) {
                const int16_t current = static_cast<int16_t>(button->mState);
                const int32_t id = window->GetID();
                UI_PostMessage(window->GetParent(), 0x100, id, current);
            }
            return 0;
        }
        break;
    case 0x10005:
        // The left button goes down: the window takes the focus; when the pump's table has no press for it yet it is recorded (time,
        // point, state 1), the window counts it as held, the light goes out and the button shows itself down (bit 2 set), and an
        // auto-repeating button starts its hold (the tick it began, held state 1). A press already recorded changes nothing.
        if (a == kLeftButton) {
            window->CheckFocus();
            IveGotTicks();
            UIButtonState& left = gUIButtons[kLeftButton];
            if (left.mState == 0) {
                left.mTime = static_cast<int32_t>(IveGotTicks());
                left.mX = static_cast<int16_t>(b >> 16);
                left.mY = static_cast<int16_t>(b);
                left.mState = 1;
                window->mButtonsHeld |= 1u << kLeftButton;
                button->HiLight(0, 0);
                button->SetState(static_cast<int8_t>(button->mState | kStateOver), 1);
                if ((window->mAttributes & kAutoRepeat) != 0) {
                    button->mHeldTick = IveGotTicks();
                    button->mHeldState = 1;
                }
            }
            return 0;
        }
        break;
    default:
        break;
    }
    return UI_DefaultWindowCallback(window, message, a, b);
}

// A button held down answers the second help text number (the word at +0x90), any other state the first (+0x8c).
int32_t InterfaceButton::GetHelpText() { return static_cast<uint8_t>(mState) == 1 ? mF90 : mHelpText; }

// The control factory and the controls' constructors (main app, UI toolkit): InterfaceWindow::CreateObject (0x101dc750) and
// the constructors of the bar, check buttons, edit box, image, knob, light panel, list box, list control, text box and
// vary box. The button's constructor and members are in src/InterfaceButton.cpp; the scroll bar's belongs to another unit.
//
// Every constructor runs the window's constructor first, stores its own vtable (the compiler's here) and its members, and
// for most of them a window handler. The destructors, the other virtual members and the handlers are untranslated.

InterfaceCheckButtons::InterfaceCheckButtons(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left,
                                             int16_t top, int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mCurrent = -1;
    mCallback = UI_DefaultCheckButtonsCallback;
    mDefaultCallback = UI_DefaultCheckButtonsCallback;
}

// InterfaceCheckButtons::~InterfaceCheckButtons (main app, SimTheme Park 2.04b3, 0x101cc02c, 104 bytes; the inventory files it under
// ui_cityLobbyController.cpp). Unit w91.

// The Mac's destructor stores the class's vtable pointer (0x102af9c8, at +0x118), calls ~InterfaceWindow with the deleting flag 0 and
// then, when its own flag (r4) is positive, operator delete. On the host the vtable store, the call of the base destructor and the
// deleting form are the compiler's, so the body is empty: the destructor is the base's (src/InterfaceWindow_dtor.cpp).
InterfaceCheckButtons::~InterfaceCheckButtons() {}

// The window classes' WhatAmI members, the base window's CalcClientArea and CallDefaultWindowHandler (main app, UI toolkit; unit w36).
// WhatAmI is the interface's run time type: each class answers its own kind number, the one InterfaceWindow::CreateObject
// (src/ui_controls_create.cpp) takes to make it and the window scripts' records test with (src/InterfaceWindow_script.cpp, whose
// records 6 to 16 assert on the answer). The button's and the scroll bar's are in their own files (InterfaceButton.cpp,
// InterfaceScrollBar.cpp); the kinds are the enum in include/ui_controls.h.

int32_t InterfaceCheckButtons::WhatAmI() { return kUIKindCheckButtons; }  // 0x101cc0c8

// InterfaceCheckButtons::SetCurrent (main app, SimTheme Park 2.04b3, 0x101cc148, 252 bytes; the inventory files it under
// ui_cityLobbyController.cpp): selects one of the check buttons. Unit w43b.

namespace {

constexpr int8_t kOff = 0;
constexpr int8_t kOn = 1;

}  // namespace

// unit w47. 0x101cc108, 8 bytes: the word at 0x138, with no test of any kind (the check buttons' current id, -1 for none).
int32_t InterfaceCheckButtons::GetCurrent() { return mCurrent; }
// The buttons are the window's children, found by id. A button that is not there is skipped (the id -1 can be a child's too). QUIRKS: the
// answer is the old id cut to a short; with no child of the new id nothing changes, not even for the old button.
int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    if (id != mCurrent) {
        if (id == -1) {
            InterfaceButton* old = static_cast<InterfaceButton*>(GetWindow(mCurrent));
            if (old != nullptr) old->SetState(kOff, 1);
            mCurrent = -1;
        } else if (GetWindow(id) != nullptr) {
            const int32_t previous = mCurrent;
            InterfaceButton* old = static_cast<InterfaceButton*>(GetWindow(previous));
            if (old != nullptr) old->SetState(kOff, 1);
            mCurrent = id;
            InterfaceButton* current = static_cast<InterfaceButton*>(GetWindow(mCurrent));
            if (current != nullptr) current->SetState(kOn, 1);
            return static_cast<int16_t>(previous);
        }
    }
    return -1;
}

// ---- the check buttons

namespace {

// Makes the button with this id the group's selection (-1 no selection): nothing when it is the selection already. The old selection's
// button is put up (SetState 0) and the new one down (1), each only if the group has a child with that id; an id that no child has
// leaves the selection alone. (The original does this inline four times.)
void Select(InterfaceCheckButtons* group, int32_t id) {
    if (id == group->mCurrent) return;
    if (id == -1) {
        InterfaceWindow* old = group->GetWindow(group->mCurrent);
        if (old != nullptr) static_cast<InterfaceButton*>(old)->SetState(0, 1);
        group->mCurrent = -1;
    } else if (group->GetWindow(id) != nullptr) {
        InterfaceWindow* old = group->GetWindow(group->mCurrent);
        if (old != nullptr) static_cast<InterfaceButton*>(old)->SetState(0, 1);
        group->mCurrent = id;
        InterfaceWindow* now = group->GetWindow(group->mCurrent);
        if (now != nullptr) static_cast<InterfaceButton*>(now)->SetState(1, 1);
    }
}

}  // namespace

// Window made (0x15): a group with attribute 0x10 and no selection selects its first child when that is a button.
// A button of the group was clicked (0x100: `a` its id, `b` its new state, 1 down): with a button that went DOWN the old selection is
// told to the parent as deselected (0x101, id, 0) first, then the new one becomes the selection and the parent hears (0x101, id, 1).
// With one that went UP in a group with attribute 0x10 (one is always selected) the click is undone: the selection is made that id
// and the button put down again; in any other group the selection is cleared and the parent hears (0x101, id, 0). Everything goes on
// to the default handler.
int32_t UI_DefaultCheckButtonsCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    InterfaceCheckButtons* group = static_cast<InterfaceCheckButtons*>(window);
    if (message == 0x100) {
        InterfaceWindow* clicked = window->GetWindow(a);
        if (clicked != nullptr && window != nullptr) {
            if (b == 1) {
                const int32_t previous = group->mCurrent;
                if (previous != -1) UI_PostMessage(window->GetParent(), 0x101, previous, 0);
                Select(group, a);
                UI_PostMessage(window->GetParent(), 0x101, a, 1);
            } else if ((window->GetAttr() & kToggle) == 0) {
                if (group->mCurrent != -1) {
                    InterfaceWindow* old = window->GetWindow(group->mCurrent);
                    if (old != nullptr) static_cast<InterfaceButton*>(old)->SetState(0, 1);
                    group->mCurrent = -1;
                }
                UI_PostMessage(window->GetParent(), 0x101, a, 0);
            } else {
                Select(group, a);
                static_cast<InterfaceButton*>(clicked)->SetState(1, 1);
            }
        }
    } else if (message == 0x15 && (window->GetAttr() & kToggle) != 0 && group->mCurrent == -1) {
        InterfaceWindow* first = window->GetChild(0);
        if (first != nullptr && first->WhatAmI() == 2) Select(group, first->GetID());
    }
    return UI_DefaultWindowCallback(window, message, a, b);
}

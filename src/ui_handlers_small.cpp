// The window handlers of five controls (main app, UI toolkit): the button (0x101cb5f0), the check buttons (0x101cc27c), the knob
// (0x101d07ac), the text box (0x101d8134) and the vary box (0x101d9674). The constructors store them as the window's message
// function (and, for the button and the check buttons, as its default one). Each handles a few messages and passes the rest to the
// window's own handler, UI_DefaultWindowCallback, whose answer it gives back.
//
// The messages the pump sends (FINDINGS 2026-10-11 w21/w29/w30): 0x10001 the pointer came into the window, 0x10002 it left,
// 0x10003 it moved (x in the high half of `a`, y in the low), 0x10004 mouse button `a` went up, 0x10005 went down (`b` is the
// point), 0x1e a frame, 0x15 the window was made. Messages 0x100 (a click), 0x10008 and 0x10009 are the ones a button sends to
// its parent (see InterfaceButton), 0x800 the value of a control changed.
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"

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

// ---- the knob

// The left button's press takes the focus whatever the button (0x10005) and, for the left one, starts the drag: the pointer is
// captured and its y (the low half of `b`) kept twice. The release (0x10004) of the left button ends the drag and lets go of the
// pointer. Both answer 0 and neither reaches the default handler. A move (0x10003) turns the knob (DoKnobMove), then goes on.
int32_t UI_DefaultKnobCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    InterfaceKnob* knob = static_cast<InterfaceKnob*>(window);
    if (message == 0x10004) {
        if (a == 0) {
            knob->mDragging = 0;
            window->UnCaptureDevice();
        }
        return 0;
    }
    if (message == 0x10005) {
        window->CheckFocus();
        if (a == 0) {
            knob->mDragging = 1;
            knob->mLastY = static_cast<int16_t>(b);
            knob->mPreviousY = knob->mLastY;
            window->CaptureDevice();
        }
        return 0;
    }
    if (message == 0x10003) knob->DoKnobMove(a);
    return UI_DefaultWindowCallback(window, message, a, b);
}

// ---- the text box

int32_t UI_DefaultTextBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    return UI_DefaultWindowCallback(window, message, a, b);
}

// ---- the vary box

// The box has two buttons, children with the ids 1 (the increment, the upper) and 2 (the decrement); they send the box (their parent)
// 0x100 (a click, the id in `a`), 0x10009 (held long enough) and 0x10008 (a repeat step). DoVaryBoxButton takes the number of what to do:
// click 0 decrement, 1 increment; hold 2 decrement, 3 increment; repeat 4 decrement, 5 increment. The wheel (0x1000d) adds `a` to
// the box's value through its virtual GetValue and SetValue. The creation message (0x15) gives both buttons (and, if a child with that
// id is not a button, corrupts it) the auto-repeat 200 and 50, written straight into their words. Everything goes on to the default
// handler.
int32_t UI_DefaultVaryBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    InterfaceVaryBox* box = static_cast<InterfaceVaryBox*>(window);
    if (message == 0x10008) {
        if (a == 2) {
            box->DoVaryBoxButton(4);
        } else if (a == 1) {
            box->DoVaryBoxButton(5);
        }
    } else if (message < 0x10008) {
        if (message == 0x100) {
            if (a == 2) {
                box->DoVaryBoxButton(0);
            } else if (a == 1) {
                box->DoVaryBoxButton(1);
            }
        } else if (message == 0x15) {
            for (int32_t id = 1; id <= 2; ++id) {
                InterfaceButton* button = static_cast<InterfaceButton*>(window->GetWindow(id));
                if (button != nullptr) {
                    button->mRepeatInterval = 0x32;
                    button->mRepeatDelay = 200;
                }
            }
        }
    } else if (message == 0x1000d) {
        const int32_t current = window->GetValue();
        window->SetValue(a + current);
    } else if (message == 0x10009) {
        if (a == 2) {
            box->DoVaryBoxButton(2);
        } else if (a == 1) {
            box->DoVaryBoxButton(3);
        }
    }
    return UI_DefaultWindowCallback(window, message, a, b);
}

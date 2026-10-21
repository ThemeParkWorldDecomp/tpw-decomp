// InterfaceVaryBox.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceVaryBox code, named after it.

#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "UI_pump.h"
#include <cstring>

// The control factory and the controls' constructors (main app, UI toolkit): InterfaceWindow::CreateObject (0x101dc750) and
// the constructors of the bar, check buttons, edit box, image, knob, light panel, list box, list control, text box and
// vary box. The button's constructor and members are in src/InterfaceButton.cpp; the scroll bar's belongs to another unit.
//
// Every constructor runs the window's constructor first, stores its own vtable (the compiler's here) and its members, and
// for most of them a window handler. The destructors, the other virtual members and the handlers are untranslated.

InterfaceVaryBox::InterfaceVaryBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                   int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mMinValue = 0;
    mMaxValue = 1;
    mValue = 0x9708cd;
    mOldValue = 0;
    mIncrement = 1;
    mForceUpdate = 1;
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    SetCallbackFunction(UI_DefaultVaryBoxCallback);
}

// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

// ---- the vary box
InterfaceVaryBox::~InterfaceVaryBox() {}   // key function of the vary box's vtable

// The window classes' WhatAmI members, the base window's CalcClientArea and CallDefaultWindowHandler (main app, UI toolkit; unit w36).
// WhatAmI is the interface's run time type: each class answers its own kind number, the one InterfaceWindow::CreateObject
// (src/ui_controls_create.cpp) takes to make it and the window scripts' records test with (src/InterfaceWindow_script.cpp, whose
// records 6 to 16 assert on the answer). The button's and the scroll bar's are in their own files (InterfaceButton.cpp,
// InterfaceScrollBar.cpp); the kinds are the enum in include/ui_controls.h.

int32_t InterfaceVaryBox::WhatAmI() { return kUIKindVaryBox; }            // 0x101d8e0c

// InterfaceVaryBox::ChangeValue and SetMinMaxValues (unit v5266).

// 0x101d8e44: either end may be NULL (kept); an inverted range collapses to the maximum; the value is cut into the range.
void InterfaceVaryBox::SetMinMaxValues(int32_t* minimum, int32_t* maximum) {
    if (minimum != nullptr) mMinValue = *minimum;
    if (maximum != nullptr) mMaxValue = *maximum;
    if (mMinValue > mMaxValue) {
        mMinValue = mMaxValue;
        mMaxValue = mMinValue;
    }
    int32_t value = mValue;
    if (value < mMinValue) {
        mValue = mMinValue;
        return;
    }
    if (value <= mMaxValue) return;
    mValue = mMaxValue;
}

void InterfaceVaryBox::SetInc(int32_t* increment) {
    if (increment == nullptr) return;
    mIncrement = *increment;
}

int32_t InterfaceVaryBox::SetValue(int32_t value) { return ChangeValue(value); }

int32_t InterfaceVaryBox::GetValue() { return mValue; }

// The difference of the values when both are vary boxes (the same kind number), else 0.
int32_t InterfaceVaryBox::operator-(InterfaceWindow& other) {
    const int32_t mine = WhatAmI();
    const int32_t theirs = other.WhatAmI();
    if (theirs != mine) return 0;
    return mValue - static_cast<InterfaceVaryBox&>(other).mValue;
}

// 0x101d9080
int32_t InterfaceVaryBox::ChangeValue(int32_t value) {
    if (value > mMaxValue) {
        value = mMaxValue;
    } else if (value < mMinValue) {
        value = mMinValue;
    }
    if (value == mValue && mForceUpdate == 0) return 0;
    mForceUpdate = 0;
    mOldValue = mValue;
    mValue = value;
    InterfaceWindow::SetValue(value);
    UI_PostMessage(mParent, 0x800, mId, mValue);
    return 1;
}

// InterfaceVaryBox (main app, UI toolkit 0x101d915c): the member that moves the value when one of the box's two buttons is used. The
// handler that drives it (UI_DefaultVaryBoxCallback, 0x101d9674) is in src/ui_handlers_small.cpp.
//
// A button held down repeats: the handler starts the hold (DoVaryBoxButton 2 or 3) and then steps it (4 or 5) for each repeat the
// button sends. With an increment of 1 the step grows: it is 1 for the first 20 repeats, then 10, then 100 and so on, a tenfold
// every 20. With any other increment the value moves by the increment once every 5 repeats.

namespace {

// The value is cut to the ends, the maximum first (with the ends the wrong way round the maximum wins). The box takes it when it is
// not the value it has or the update is forced (mForceUpdate, which then clears): the old value is kept, the draw objects are told
// the new one by the WINDOW's SetValue (a direct call, not the box's own virtual one) and the parent hears 0x800 with the box's id
// and the value.
void TakeValue(InterfaceVaryBox* box, int32_t value) {
    if (value > box->mMaxValue) {
        value = box->mMaxValue;
    } else if (value < box->mMinValue) {
        value = box->mMinValue;
    }
    if (value == box->mValue && box->mForceUpdate == 0) return;
    box->mForceUpdate = 0;
    box->mOldValue = box->mValue;
    box->mValue = value;
    box->InterfaceWindow::SetValue(value);
    UI_PostMessage(box->mParent, 0x800, box->mId, box->mValue);
}

// The hold begins: an increment of 1 waits 20 repeats and then steps by 1 (and grows), any other waits 5.
void StartHold(InterfaceVaryBox* box) {
    if (box->mIncrement == 1) {
        box->mRepeatCountdown = 0x14;
        box->mRepeatStep = 1;
    } else {
        box->mRepeatCountdown = 5;
    }
}

// One repeat of a held button: `direction` is -1 for the decrement button, 1 for the increment. With an increment of 1 every repeat
// moves the value by the step (mRepeatStep) and counts the 20 down, after which the step is ten times as big; with another
// increment nothing happens until the countdown of 5 runs out, and then the value moves by the increment.
void Repeat(InterfaceVaryBox* box, int32_t direction) {
    if (box->mIncrement == 1) {
        TakeValue(box, box->mValue + direction * box->mRepeatStep);
        box->mRepeatCountdown -= 1;
        if (box->mRepeatCountdown == 0) {
            box->mRepeatStep *= 10;
            box->mRepeatCountdown = 0x14;
        }
    } else {
        box->mRepeatCountdown -= 1;
        if (box->mRepeatCountdown == 0) {
            TakeValue(box, box->mValue + direction * box->mIncrement);
            box->mRepeatCountdown = 5;
        }
    }
}

}  // namespace

// QUIRK: the start of a hold on the decrement button (2) only sets the counters; the increment button's start (3) also takes one
// increment at once.
void InterfaceVaryBox::DoVaryBoxButton(int16_t which) {
    switch (which) {
    case 0:
        TakeValue(this, mValue - mIncrement);
        break;
    case 1:
        TakeValue(this, mValue + mIncrement);
        break;
    case 2:
        StartHold(this);
        break;
    case 3:
        StartHold(this);
        TakeValue(this, mValue + mIncrement);
        break;
    case 4:
        Repeat(this, -1);
        break;
    case 5:
        Repeat(this, 1);
        break;
    default:
        break;
    }
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

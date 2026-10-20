// InterfaceVaryBox (main app, UI toolkit 0x101d915c): the member that moves the value when one of the box's two buttons is used. The
// handler that drives it (UI_DefaultVaryBoxCallback, 0x101d9674) is in src/ui_handlers_small.cpp.
//
// A button held down repeats: the handler starts the hold (DoVaryBoxButton 2 or 3) and then steps it (4 or 5) for each repeat the
// button sends. With an increment of 1 the step grows: it is 1 for the first 20 repeats, then 10, then 100 and so on, a tenfold
// every 20. With any other increment the value moves by the increment once every 5 repeats.
#include "ui_controls.h"

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

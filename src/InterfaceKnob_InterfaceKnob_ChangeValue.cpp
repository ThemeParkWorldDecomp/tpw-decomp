#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "UI_Base.h"

// Sets the value: cut to the value range (the largest first), nothing more when it is the one the knob has. Otherwise the old value
// is kept, the displayed angle follows from the value (ends to ends, else the smallest angle plus the angle range times the value
// above the smallest over the value range, a 32-bit signed division), the client draw object (if any) is told the angle with change
// flag 4 and the parent hears 0x800 with the knob's id and the value.
void InterfaceKnob::ChangeValue(int32_t value) {
    if (value > mMaxValue) {
        value = mMaxValue;
    } else if (value < mMinValue) {
        value = mMinValue;
    }
    if (value == mValue) return;
    mOldValue = mValue;
    mValue = value;
    int32_t shown;
    if (mValue == mMaxValue) {
        shown = mMaxAngle;
    } else if (mValue == mMinValue) {
        shown = mMinAngle;
    } else {
        shown = mMinAngle + (mAngleRange * (mValue - mMinValue)) / mValueRange;
    }
    mAngle = shown;
    if (mClientDrawObject != nullptr) {
        DOChange change;
        change.mF08 = static_cast<int16_t>(mAngle);
        mClientDrawObject->Change(4, &change);
    }
    UI_PostMessage(mParent, 0x800, mId, mValue);
}

// InterfaceVaryBox::ChangeValue and SetMinMaxValues (unit v5266).
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"

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

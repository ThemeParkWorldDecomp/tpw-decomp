// InterfaceKnob (main app, UI toolkit 0x101d0574..0x101d0718): the two members that turn the knob. The handler that drives them
// (UI_DefaultKnobCallback, 0x101d07ac) is in src/ui_handlers_small.cpp.
//
// The knob keeps two angles: mDragAngle, which the pointer moves (8 units for each unit it goes up the screen) between the smallest
// and the largest angle, and mAngle, which is the angle the value maps back to (so the displayed angle snaps to the values). The
// value is the drag angle's share of the value range.
#include "ui_DrawObject.h"
#include "ui_controls.h"

// The angle is cut to the range (the smallest first), and nothing more happens when it is the one the knob has. The value is the
// minimum value for the smallest angle, the maximum for the largest and between them the minimum plus the value range times the
// ANGLE (not the angle above the smallest: the original does not take the smallest away, which only matters for a smallest angle
// other than 0) over the angle range; it is cut to the value range too. When the value changes, the old one is kept, the displayed
// angle is worked out from the value the same way and the client draw object (if any) is told it with change flag 4 (the first
// short after the frame and the depth, +8 of the record), and the parent hears 0x800 with the knob's id and the value.
// The divisions are the original's 32-bit signed ones (no test for a range of 0; the ends are tested first, so a range of 0 is
// only reached when the range words disagree with the ends, which the setters do not allow).
void InterfaceKnob::ChangeAngle(int32_t angle) {
    if (angle < mMinAngle) {
        angle = mMinAngle;
    } else if (angle > mMaxAngle) {
        angle = mMaxAngle;
    }
    if (angle == mDragAngle) return;
    mDragAngle = angle;
    int32_t value;
    if (mDragAngle == mMaxAngle) {
        value = mMaxValue;
    } else if (mDragAngle == mMinAngle) {
        value = mMinValue;
    } else {
        value = mMinValue + (mValueRange * mDragAngle) / mAngleRange;
    }
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

// While the knob is held, a move to another y (the low half of the message's word, compared as an unsigned 16-bit number against
// the signed short kept: a negative y never matches) keeps the old y and turns the knob by the distance moved up, times 8.
void InterfaceKnob::DoKnobMove(int32_t y) {
    if (mDragging != 0 && static_cast<int32_t>(y & 0xffff) != static_cast<int32_t>(mLastY)) {
        mPreviousY = mLastY;
        mLastY = static_cast<int16_t>(y);
        ChangeAngle(mDragAngle + (static_cast<int32_t>(mPreviousY) - static_cast<int32_t>(mLastY)) * 8);
    }
}

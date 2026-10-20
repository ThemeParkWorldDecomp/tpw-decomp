// The interface's control classes: the setters the window script calls (main app, unit w31a). InterfaceKnob::SetMinMaxAngles,
// InterfaceListBox::SetListRect, InterfaceListControl2's SetListRect, SetColumns and SetColumnPos, and the constructors of the
// three region shapes a window can be given (CoolCircle, CoolEllipse, CoolPoly; src/ui_cool_shapes.cpp has the rest of them).
#include "ui_controls.h"

// ---- the knob

void InterfaceKnob::SetMinMaxAngles(int32_t* minimum, int32_t* maximum) {
    if (minimum != nullptr) mMinAngle = *minimum;
    if (maximum != nullptr) mMaxAngle = *maximum;
    if (mMinAngle > mMaxAngle) {
        mMinAngle = mMaxAngle;
        mMaxAngle = mMinAngle;
    }
    mAngleRange = mMaxAngle - mMinAngle;
}

// ---- the lists

void InterfaceListBox::SetListRect(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    mListRect.Set(left, top, right, bottom);
}

void InterfaceListControl2::SetListRect(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    mListRect.Set(left, top, right, bottom);
}

void InterfaceListControl2::SetColumns(int16_t count) {
    if (mColumnCount != 0) return;
    mColumns = new CoolRect[count];
    for (int32_t i = 0; i < count; ++i)
        mColumns[i].Set(static_cast<int16_t>(i << 5), 0, static_cast<int16_t>((i + 1) << 5), 1);
    mColumnCount = count;
}

void InterfaceListControl2::SetColumnPos(int16_t column, int16_t left, int16_t right) {
    if (column < mColumnCount && mColumns != nullptr) mColumns[column].Set(left, 0, right, 1);
}

// ---- the regions

CoolCircle::CoolCircle(int16_t x, int16_t y, int16_t radius) : mX(x), mY(y), mRadius(radius) {}

CoolEllipse::CoolEllipse(int16_t x, int16_t y, int16_t radiusX, int16_t radiusY)
    : mX(x), mY(y), mRadiusX(radiusX), mRadiusY(radiusY) {}

// Copies the points into an array of its own.
CoolPoly::CoolPoly(int16_t count, const int16_t* points) : mCount(count) {
    mPoints = new int16_t[static_cast<uint32_t>(static_cast<int32_t>(mCount)) * 2];
    for (int32_t i = 0; static_cast<int16_t>(i) < count; ++i) {
        mPoints[2 * i] = points[2 * i];
        mPoints[2 * i + 1] = points[2 * i + 1];
    }
}

// ---- unit w50

// The Z of the first item window (the word at +0x148 points at an array of window pointers; the first is read), 0 without one.
int16_t InterfaceListControl2::GetZOfEntries() {
    if (mItemWindows != nullptr && *mItemWindows != nullptr) return (*mItemWindows)->mZ;
    return 0;
}

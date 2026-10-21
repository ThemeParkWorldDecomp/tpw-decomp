// CoolRegion.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of CoolRegion code, named after it.

#include "TbIRLE4bitFont.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include <algorithm>

// UI_Base.cpp (main app of SimTheme Park 2.04b3). PARTIAL: UI_SetRealScreenRez
// (0x101c9ed8), the rectangle classes and the name hash so far.

// ---- CoolRegion (0x101d5a3c..0x101d5b74): the base of the UI's regions
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }

// The interface's control classes: the setters the window script calls (main app, unit w31a). InterfaceKnob::SetMinMaxAngles,
// InterfaceListBox::SetListRect, InterfaceListControl2's SetListRect, SetColumns and SetColumnPos, and the constructors of the
// three region shapes a window can be given (CoolCircle, CoolEllipse, CoolPoly; src/ui_cool_shapes.cpp has the rest of them).

// ---- the regions

CoolCircle::CoolCircle(int16_t x, int16_t y, int16_t radius) : mX(x), mY(y), mRadius(radius) {}

// The three region shapes a window can be given by its script (main app, UI toolkit 0x101d5bb0..0x101d6200; unit w31a): the
// virtual members of CoolCircle, CoolEllipse and CoolPoly. Their constructors are in src/ui_controls_small.cpp. Each overrides
// CoolRegion's PointIn, Shift, Adjust and destructor; Middle and operator== stay the base's.

// ---- CoolCircle (0x101d5bfc..0x101d6200): x, y and radius

// Inside when the squared distance from the centre is below the squared radius (32-bit products, wrapping; signed compare).
bool CoolCircle::PointIn(int16_t x, int16_t y) {
    const int32_t dx = x - mX;
    const int32_t dy = y - mY;
    const uint32_t distance = static_cast<uint32_t>(dx) * static_cast<uint32_t>(dx) + static_cast<uint32_t>(dy) * static_cast<uint32_t>(dy);
    const uint32_t radius = static_cast<uint32_t>(static_cast<int32_t>(mRadius)) * static_cast<uint32_t>(static_cast<int32_t>(mRadius));
    return static_cast<int32_t>(distance) < static_cast<int32_t>(radius);
}

void CoolCircle::Shift(int16_t dx, int16_t dy) {
    mX = static_cast<int16_t>(mX + dx);
    mY = static_cast<int16_t>(mY + dy);
}

void CoolCircle::Adjust(int16_t, int16_t, int16_t, int16_t) {}

CoolEllipse::CoolEllipse(int16_t x, int16_t y, int16_t radiusX, int16_t radiusY)
    : mX(x), mY(y), mRadiusX(radiusX), mRadiusY(radiusY) {}

// ---- CoolEllipse (0x101d5d34..0x101d6200): x, y and the two radii

// Inside when ((x - mX) * mRadiusY / mRadiusX)^2 + (y - mY)^2 is below mRadiusY^2 (the division truncates; a radius of 0 is a
// division by zero, undefined in the original, answered as 0 here).
bool CoolEllipse::PointIn(int16_t x, int16_t y) {
    const int32_t dx = x - mX;
    const int32_t dy = y - mY;
    const int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(mRadiusY)) * static_cast<uint32_t>(dx));
    const int32_t quotient = mRadiusX != 0 ? scaled / mRadiusX : 0;
    const uint32_t distance = static_cast<uint32_t>(quotient) * static_cast<uint32_t>(quotient) + static_cast<uint32_t>(dy) * static_cast<uint32_t>(dy);
    const uint32_t radius = static_cast<uint32_t>(static_cast<int32_t>(mRadiusY)) * static_cast<uint32_t>(static_cast<int32_t>(mRadiusY));
    return static_cast<int32_t>(distance) < static_cast<int32_t>(radius);
}

void CoolEllipse::Shift(int16_t dx, int16_t dy) {
    mX = static_cast<int16_t>(mX + dx);
    mY = static_cast<int16_t>(mY + dy);
}

void CoolEllipse::Adjust(int16_t, int16_t, int16_t, int16_t) {}

// Copies the points into an array of its own.
CoolPoly::CoolPoly(int16_t count, const int16_t* points) : mCount(count) {
    mPoints = new int16_t[static_cast<uint32_t>(static_cast<int32_t>(mCount)) * 2];
    for (int32_t i = 0; static_cast<int16_t>(i) < count; ++i) {
        mPoints[2 * i] = points[2 * i];
        mPoints[2 * i + 1] = points[2 * i + 1];
    }
}

CoolPoly::~CoolPoly() {
    if (mPoints != nullptr) delete[] mPoints;
}
// ---- CoolPoly (0x101d5f04..0x101d6150): the points

// The even-odd rule: a ray from the point crosses the edge from the previous point to this one when the two ends are on different
// sides of the point's y (a point is above when its y is not below) and the point is on the near side of the edge (the products
// are 32 bits, wrapping). A polygon with no points, or with a count and no array, contains nothing.
bool CoolPoly::PointIn(int16_t x, int16_t y) {
    if (mCount == 0 || mPoints == nullptr) return false;
    bool inside = false;
    const int16_t* previous = mPoints + 2 * (mCount - 1);
    bool previousAbove = previous[1] >= y;
    const int16_t* point = mPoints;
    for (int32_t remaining = mCount; remaining != 0; --remaining) {
        const bool above = point[1] >= y;
        if (previousAbove != above) {
            const uint32_t a = static_cast<uint32_t>(point[1] - y) * static_cast<uint32_t>(previous[0] - point[0]);
            const uint32_t b = static_cast<uint32_t>(point[0] - x) * static_cast<uint32_t>(previous[1] - point[1]);
            if (above == (static_cast<int32_t>(a) >= static_cast<int32_t>(b))) inside = !inside;
        }
        previous = point;
        previousAbove = above;
        point += 2;
    }
    return inside;
}

void CoolPoly::Shift(int16_t dx, int16_t dy) {
    if (mCount == 0 || mPoints == nullptr) return;
    for (int32_t i = 0; i < mCount; ++i) {
        mPoints[2 * i] = static_cast<int16_t>(mPoints[2 * i] + dx);
        mPoints[2 * i + 1] = static_cast<int16_t>(mPoints[2 * i + 1] + dy);
    }
}

void CoolPoly::Adjust(int16_t, int16_t, int16_t, int16_t) {}

CoolEllipse::~CoolEllipse() {}

CoolCircle::~CoolCircle() {}

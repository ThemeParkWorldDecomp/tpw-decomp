// CoolRect.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of CoolRect code, named after it.

#include "TbIRLE4bitFont.h"
#include "UI_Base.h"
#include <algorithm>

// UI_Base.cpp (main app of SimTheme Park 2.04b3). PARTIAL: UI_SetRealScreenRez
// (0x101c9ed8), the rectangle classes and the name hash so far.

// ---- CoolRect (0x101c7448..0x101c7a78)
CoolRect::CoolRect() : mLeft(0), mTop(0), mRight(0), mBottom(0) {}
CoolRect::CoolRect(int16_t left, int16_t top, int16_t right, int16_t bottom)
    : mLeft(left), mTop(top), mRight(right), mBottom(bottom) {}
CoolRect::~CoolRect() {}

void CoolRect::Set(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    mLeft = left;
    mTop = top;
    mRight = right;
    mBottom = bottom;
}

void CoolRect::Get(int16_t* left, int16_t* top, int16_t* right, int16_t* bottom) {
    if (left != nullptr) *left = mLeft;
    if (top != nullptr) *top = mTop;
    if (right != nullptr) *right = mRight;
    if (bottom != nullptr) *bottom = mBottom;
}

void CoolRect::GetDims(int16_t* width, int16_t* height) {
    if (width != nullptr) *width = static_cast<int16_t>(mRight - mLeft);
    if (height != nullptr) *height = static_cast<int16_t>(mBottom - mTop);
}

bool CoolRect::PointIn(int16_t x, int16_t y) { return mLeft <= x && mTop <= y && x < mRight && y < mBottom; }

void CoolRect::Middle(int16_t* x, int16_t* y) {
    if (x != nullptr) *x = static_cast<int16_t>(mLeft + ((mRight - mLeft) >> 1));
    if (y != nullptr) *y = static_cast<int16_t>(mTop + ((mBottom - mTop) >> 1));
}

// How `other` lies on this rectangle, as bits: 0x200 the same rectangle (the test compares
// the other's bottom with this one's TOP, as the original does); 0x100 this one is inside
// the other; 1, 2, 8, 4 the other's top left, top right, bottom right, bottom left corner
// is inside this one, each with the bits of the two edges that meet there (0x10 top, 0x20
// bottom, 0x40 left, 0x80 right); with no corner inside, the edges of the other that
// cross this one.
uint32_t CoolRect::RectIntersects(CoolRect* other) {
    uint32_t result = 0;
    if (other->mLeft == mLeft && other->mTop == mTop && other->mRight == mRight && other->mBottom == mTop) return 0x200;
    if (other->PointIn(static_cast<int16_t>(mLeft + 1), static_cast<int16_t>(mTop + 1)) &&
        other->PointIn(static_cast<int16_t>(mRight - 1), static_cast<int16_t>(mBottom - 1))) {
        return 0x100;
    }
    if (PointIn(other->mLeft, other->mTop)) result = 0x51;
    if (PointIn(other->mRight, other->mTop)) result |= 0x92;
    if (PointIn(other->mRight, other->mBottom)) result |= 0xa8;
    if (PointIn(other->mLeft, other->mBottom)) result |= 0x64;
    if ((result & 0xf) == 0) {
        const int16_t otherLeft = other->mLeft;
        const int16_t left = mLeft;
        if (otherLeft <= left && mRight <= other->mRight) {
            if (mTop < other->mTop && other->mTop < mBottom) result |= 0x10;
            if (mTop < other->mBottom && other->mBottom < mBottom) result |= 0x20;
        }
        if (other->mTop <= mTop && mBottom <= other->mBottom) {
            if (left < otherLeft && otherLeft < mRight) result |= 0x40;
            if (left < other->mRight && other->mRight < mRight) result |= 0x80;
        }
    }
    return result;
}

void CoolRect::Shift(int16_t dx, int16_t dy) {
    mLeft = static_cast<int16_t>(mLeft + dx);
    mRight = static_cast<int16_t>(mRight + dx);
    mTop = static_cast<int16_t>(mTop + dy);
    mBottom = static_cast<int16_t>(mBottom + dy);
}

void CoolRect::Adjust(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    mLeft = static_cast<int16_t>(mLeft + left);
    mRight = static_cast<int16_t>(mRight + right);
    mTop = static_cast<int16_t>(mTop + top);
    mBottom = static_cast<int16_t>(mBottom + bottom);
}

// InterfaceScrollBar.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceScrollBar code, named after it.

#include "InterfaceScrollBar.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include <cstring>

// InterfaceScrollBar (main app, 0x101d6280..0x101d72fc and the two empty draw members at 0x1018a328/0x1018a364): the scroll bar control
// and its message function. See include/InterfaceScrollBar.h for the class and for the coordinates; the thumb is the child with id 3,
// the buttons those with ids 1 and 2.

namespace {
constexpr int32_t kThumbId = 3;
constexpr int16_t kHorizontal = 2;   // mStyle: the buttons are at the left and right ends (a vertical bar, 1, has them at top and bottom)
constexpr int16_t kNoButtons = 4;
constexpr int32_t kMessageValueChanged = 0x800;   // sent to the parent: the bar's id and its value
}  // namespace

// The client area is the rectangle less the two square buttons (the side of a button is the bar's width, or its height for a
// horizontal bar); with no buttons it is the whole rectangle. The sums are 16-bit.
void InterfaceScrollBar::CalcClientArea() {
    if ((mStyle & kNoButtons) != 0) {
        mClientRect.mLeft = mRect.mLeft;
        mClientRect.mTop = mRect.mTop;
        mClientRect.mRight = mRect.mRight;
        mClientRect.mBottom = mRect.mBottom;
        return;
    }
    if ((mStyle & kHorizontal) != 0) {
        mClientRect.mTop = mRect.mTop;
        mClientRect.mBottom = mRect.mBottom;
        const int16_t side = static_cast<int16_t>(mClientRect.mBottom - mClientRect.mTop);
        mClientRect.mLeft = static_cast<int16_t>(mRect.mLeft + side);
        mClientRect.mRight = static_cast<int16_t>(mRect.mRight - side);
        return;
    }
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mRight = mRect.mRight;
    const int16_t side = static_cast<int16_t>(mClientRect.mRight - mClientRect.mLeft);
    mClientRect.mTop = static_cast<int16_t>(mRect.mTop + side);
    mClientRect.mBottom = static_cast<int16_t>(mRect.mBottom - side);
}

// Makes the buttons (unless the style has none) and the thumb when the attributes ask for them, and takes over the handler. The
// base constructor has run (and has called the base's CalcClientArea); this class's own then runs, once the style is known.
InterfaceScrollBar::InterfaceScrollBar(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                       int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mMax = 0;
    mMin = 0;
    mValue = 0;
    mStep = 1;
    mStyle = (attributes & 0x10) != 0 ? 1 : 2;
    if ((attributes & 0x40) != 0) mStyle = static_cast<int16_t>(mStyle | kNoButtons);
    mPage = 1;
    CalcClientArea();
    if ((mStyle & kNoButtons) == 0 && (attributes & 0x80) != 0) {
        AddIncButton();
        AddDecButton();
    }
    if ((attributes & 0x80) != 0) AddThumbTrack();
    mCallback = UI_DefaultScrollBarCallback;
    mDefaultCallback = UI_DefaultScrollBarCallback;
}

InterfaceScrollBar::~InterfaceScrollBar() {}

int32_t InterfaceScrollBar::WhatAmI() { return 3; }
// Changing the state of the window means the same for every child; a request for the state it is in does nothing.
int16_t InterfaceScrollBar::Enable(int16_t on) {
    const bool disabled = (mAttributes & 2) != 0;
    if ((on != 0) != disabled) return 0;
    InterfaceWindow::Enable(on);
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->GetNext()) child->Enable(on);
    if (gUIAutoRedraw != 0) UI_PostMessage(this, 1, 0, 0);
    return 1;
}

// The value from the thumb's place: the share of the track the thumb is along, of the range. (The original divides in 32 bits and
// does not look for a track of length 0.)
void InterfaceScrollBar::ReCalcValue() {
    CoolRect rect;
    InterfaceWindow* thumb = GetWindow(kThumbId);
    thumb->GetPosition(&rect, 1);
    const int32_t oldValue = mValue;
    const int32_t range = mMax - mMin;
    if ((mStyle & kHorizontal) != 0) {
        const int32_t thumbWidth = rect.mRight - rect.mLeft;
        const int32_t track = (mClientRect.mRight - thumbWidth) - mClientRect.mLeft;
        mValue = mMin + ((rect.mLeft - mClientRect.mLeft) * range) / track;
    } else {
        const int32_t thumbHeight = rect.mBottom - rect.mTop;
        const int32_t track = (mClientRect.mBottom - thumbHeight) - mClientRect.mTop;
        mValue = mMin + ((mClientRect.mBottom - rect.mBottom) * range) / track;
    }
    if (oldValue != mValue) UI_PostMessage(mParent, kMessageValueChanged, mId, mValue);
}

int16_t InterfaceScrollBar::ReCalcThumbPos(int32_t mode, int32_t value) {
    CoolRect rect;
    InterfaceWindow* thumb = GetWindow(kThumbId);
    thumb->GetPosition(&rect, 1);
    const int32_t min = mMin;
    const int32_t max = mMax;
    const int32_t oldValue = mValue;
    const int32_t range = max - min;
    int32_t target = oldValue;
    if (mode == 1) {
        target = oldValue + mStep;
    } else if (mode == 0) {
        target = oldValue - mStep;
    } else if (mode == 2) {
        target = oldValue + mPage;
    } else if (mode == 3) {
        target = oldValue - mPage;
    } else if (mode == -1) {
        target = value;
    }
    // (any other mode but -2 leaves the caller's register in the original)
    if (mStep > 0) {
        if (target > max) target = max;
        if (target < min) target = min;
    } else {
        if (target < max) target = max;
        if (target > min) target = min;
    }
    if (target == oldValue && mode != -2) return 0;
    mValue = target;
    if ((mStyle & kHorizontal) != 0) {
        const int32_t thumbWidth = rect.mRight - rect.mLeft;
        const int32_t track = (mClientRect.mRight - thumbWidth) - mClientRect.mLeft;
        int16_t left = mClientRect.mLeft;
        if (range != 0) left = static_cast<int16_t>(mClientRect.mLeft + (track * (mValue - mMin)) / range);
        rect.mLeft = left;
        rect.mRight = static_cast<int16_t>(left + thumbWidth);
    } else {
        const int32_t thumbHeight = rect.mBottom - rect.mTop;
        const int32_t track = (mClientRect.mBottom - thumbHeight) - mClientRect.mTop;
        int16_t top = mClientRect.mTop;
        if (range != 0) top = static_cast<int16_t>(mClientRect.mTop + (track * (mMax - mValue)) / range);
        rect.mTop = top;
        rect.mBottom = static_cast<int16_t>(top + thumbHeight);
    }
    thumb->SetPosition(&rect);
    const int32_t span = mMax - mMin;
    if (span != 0) {
        // the draw objects are told how far along the thumb is, 0 to 0x400 (a short in the word)
        const int32_t along = static_cast<int16_t>(((mValue - mMin) * 0x400) / span);
        DOChange change{};
        std::memcpy(change.mF00, &along, sizeof along);
        if (mDrawObject != nullptr) mDrawObject->Change(8, &change);
        if (mClientDrawObject != nullptr) mClientDrawObject->Change(8, &change);
    }
    InterfaceWindow::SetValue(mValue);
    if (oldValue != mValue) UI_SendMessage(mParent, kMessageValueChanged, mId, mValue);
    return 1;
}

int32_t InterfaceScrollBar::GetValue() { return mValue; }
// A maximum below the minimum makes the step negative (and back).
void InterfaceScrollBar::SetMinMax(int32_t min, int32_t max) {
    if (max == mMax && min == mMin) return;
    if (max < min) {
        if (mStep > 0) mStep = -mStep;
    } else if (mStep < 0) {
        mStep = -mStep;
    }
    mMin = min;
    mMax = max;
}

int32_t InterfaceScrollBar::SetValue(int32_t value) { return ReCalcThumbPos(-1, value); }

void InterfaceScrollBar::SetPageValue(int32_t page) { mPage = page; }

// The increment button is at the maximum end (the top, or the right), the decrement button at the other.
void InterfaceScrollBar::AddIncButton() {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
    if ((mStyle & kHorizontal) != 0) {
        left = mClientRect.mRight;
        top = mClientRect.mTop;
        right = mRect.mRight;
        bottom = mClientRect.mBottom;
    } else {
        left = mClientRect.mLeft;
        top = mRect.mTop;
        right = mClientRect.mRight;
        bottom = mClientRect.mTop;
    }
    InterfaceWindow* button = CreateObject(2, 0, 1, left, top, right, bottom);
    if (button != nullptr) button->Show(1);
}

void InterfaceScrollBar::AddDecButton() {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
    if ((mStyle & kHorizontal) != 0) {
        left = mRect.mLeft;
        top = mClientRect.mTop;
        right = mClientRect.mLeft;
        bottom = mClientRect.mBottom;
    } else {
        left = mClientRect.mLeft;
        top = mClientRect.mBottom;
        right = mClientRect.mRight;
        bottom = mRect.mBottom;
    }
    InterfaceWindow* button = CreateObject(2, 0, 2, left, top, right, bottom);
    if (button != nullptr) button->Show(1);
}

// The thumb starts at the minimum end of the client area, 0x40 long (the sum is cut to 16 bits).
void InterfaceScrollBar::AddThumbTrack() {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
    if ((mStyle & kHorizontal) != 0) {
        left = mClientRect.mLeft;
        top = mClientRect.mTop;
        right = static_cast<int16_t>(mClientRect.mLeft + 0x40);
        bottom = mClientRect.mBottom;
    } else {
        left = mClientRect.mLeft;
        top = mClientRect.mTop;
        right = mClientRect.mRight;
        bottom = static_cast<int16_t>(mClientRect.mTop + 0x40);
    }
    InterfaceWindow* thumb = CreateObject(2, 0, kThumbId, left, top, right, bottom);
    if (thumb != nullptr) {
        thumb->Show(1);
        thumb->SetCallbackFunction(UI_DefaultThumbtrackCallback);
    }
}

// The scroll bar's thumb handler and position leaves (main app, unit w37b).

// 0x101d6e9c. Every stored coordinate wraps to a short; the extent does not.
void CalcThumbtrack(CoolRect* thumb, CoolRect track, int32_t dx, int32_t dy, int32_t horizontal) {
    int16_t& first = horizontal != 0 ? thumb->mLeft : thumb->mTop;
    int16_t& last = horizontal != 0 ? thumb->mRight : thumb->mBottom;
    const int16_t minimum = horizontal != 0 ? track.mLeft : track.mTop;
    const int16_t maximum = horizontal != 0 ? track.mRight : track.mBottom;
    const int32_t delta = horizontal != 0 ? dx : dy;
    first = static_cast<int16_t>(first + delta);
    last = static_cast<int16_t>(last + delta);
    const int32_t extent = last - first;
    if (first < minimum) {
        first = minimum;
        last = static_cast<int16_t>(first + extent);
    }
    if (last > maximum) {
        last = maximum;
        first = static_cast<int16_t>(last - extent);
    }
}

// How far the point is past the thumb's far edge (16-bit difference): a click before it moves a page one way, a click after it the
// other, whichever way the step runs.
void InterfaceScrollBar::ClientClick(int16_t x, int16_t y) {
    InterfaceWindow* thumb = GetWindow(kThumbId);
    if (thumb == nullptr) return;
    CoolRect rect;
    thumb->GetPosition(&rect, 1);
    int16_t distance;
    if ((mStyle & kHorizontal) != 0) {
        distance = static_cast<int16_t>(x - rect.mRight);
    } else {
        distance = static_cast<int16_t>(rect.mBottom - y);
    }
    int32_t mode = distance < 0 ? 2 : 3;
    if (mStep > 0) mode = mode == 2 ? 3 : 2;
    ReCalcThumbPos(mode, 0);
}

int32_t UI_DefaultScrollBarCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    InterfaceScrollBar* bar = static_cast<InterfaceScrollBar*>(window);
    switch (message) {
    case 3: {
        window->ClientDraw();
        InterfaceWindow* thumb = window->GetWindow(kThumbId);
        if (thumb != nullptr) thumb->DrawMe();
        return 0;
    }
    case -1:
        if (gUIAutoRedraw != 0) {
            UI_PostMessage(window->GetWindow(kThumbId), 1, 0, 0);
            window->ClientDraw();
        }
        break;
    case 0x100:
    case 0x10008:
    case 0x10009:
        // (the first argument is which button: 2 the decrement, 1 the increment)
        if (a == 2) {
            bar->ReCalcThumbPos(0, 0);
        } else if (a == 1) {
            bar->ReCalcThumbPos(1, 0);
        }
        break;
    case 0x1000d: {
        const int32_t current = window->GetValue();
        window->SetValue(current - a);
        break;
    }
    case 0x11006:
        bar->ClientClick(static_cast<int16_t>(b >> 16), static_cast<int16_t>(b));
        break;
    default:
        break;
    }
    return UI_DefaultWindowCallback(window, message, a, b);
}

// 0x101d7348. The original casts the thumb window to a button for its visual state.
int32_t UI_DefaultThumbtrackCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    auto* bar = static_cast<InterfaceScrollBar*>(window->GetParent());
    auto* button = static_cast<InterfaceButton*>(window);
    switch (message) {
    case 0xc:
        return UI_SendMessage(window->GetParent(), message, a, b);
    case 0x13:
        if (gUIAutoRedraw != 0) UI_PostMessage(bar, 3, 0, 0);
        break;
    case 0x15:
        window->UpdatePreviousPosition();
        bar->ReCalcThumbPos(-2, 0);
        break;
    case 0x10001:
        gUIHelpText->OnMouseMovedIn(window->GetParent());
        if (window->mButtonsHeld == 0) button->HiLight(1, 1);
        return 0;
    case 0x10002:
        gUIHelpText->OnMouseMovedOut();
        if (window->mButtonsHeld == 0) button->HiLight(0, 1);
        return 0;
    case 0x10003: {
        const int16_t x = static_cast<int16_t>(static_cast<uint32_t>(a) >> 16);
        const int16_t y = static_cast<int16_t>(a);
        gUIHelpText->OnMouseMoved(x, y);
        if ((window->mButtonsHeld & 1) != 0) {
            CoolRect rect;
            CoolRect track;
            window->GetPreviousPosition(&rect);
            track.mLeft = bar->mClientRect.mLeft;
            track.mTop = bar->mClientRect.mTop;
            track.mRight = bar->mClientRect.mRight;
            track.mBottom = bar->mClientRect.mBottom;
            const int16_t dx = static_cast<int16_t>(x - gUIButtons[0].mX);
            const int16_t dy = static_cast<int16_t>(y - gUIButtons[0].mY);
            bar->GetValue();
            CalcThumbtrack(&rect, track, dx, dy, bar->mStyle & 2);
            window->SetPosition(&rect);
            bar->ReCalcValue();
        }
        return 0;
    }
    case 0x10004:
        button->SetState(0, 1);
        window->mButtonsHeld &= ~(uint32_t(1) << a);
        gUIButtons[a].mState = 0;
        window->UnCaptureDevice();
        bar->ReCalcThumbPos(-2, 0);
        window->UpdatePreviousPosition();
        UI_SendMessage(bar->mParent, 0x802, bar->mId, bar->mValue);
        return 0;
    case 0x10005:
        window->CheckFocus();
        if (gUIButtons[a].mState == 0) {
            gUIButtons[a].mX = static_cast<int16_t>(static_cast<uint32_t>(b) >> 16);
            gUIButtons[a].mY = static_cast<int16_t>(b);
            gUIButtons[a].mState = 1;
            window->mButtonsHeld |= uint32_t(1) << a;
            button->HiLight(0, 0);
            button->SetState(1, 1);
            window->UpdatePreviousPosition();
            window->CaptureDevice();
            UI_SendMessage(bar->mParent, 0x801, bar->mId, bar->mValue);
        }
        return 0;
    case 0x1000d: {
        InterfaceWindow* parent = window->GetParent();
        const int32_t value = parent->GetValue();
        parent->SetValue(value - a);
        break;
    }
    default:
        break;
    }
    return UI_DefaultWindowCallback(window, message, a, b);
}

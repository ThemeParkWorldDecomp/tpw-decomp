// The scroll bar's thumb handler and position leaves (main app, unit w37b).
#include "InterfaceScrollBar.h"
#include "ui_controls.h"

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

// 0x101db2f8, 0x101db954. Copy coordinates only, preserving the destination's vtable.
int32_t InterfaceWindow::UpdatePreviousPosition() {
    mPreviousRect.mLeft = mRect.mLeft;
    mPreviousRect.mTop = mRect.mTop;
    mPreviousRect.mRight = mRect.mRight;
    mPreviousRect.mBottom = mRect.mBottom;
    return 1;
}

int32_t InterfaceWindow::GetPreviousPosition(CoolRect* rect) {
    if (rect == nullptr) return 0;
    rect->mLeft = mPreviousRect.mLeft;
    rect->mTop = mPreviousRect.mTop;
    rect->mRight = mPreviousRect.mRight;
    rect->mBottom = mPreviousRect.mBottom;
    return 1;
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

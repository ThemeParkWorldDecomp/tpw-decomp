// InterfaceWindow leaves of unit v5119: GetSizeOfClientDrawObject 0x101dbba8, AdjustPosition 0x101db0e8, UnCaptureKb 0x101dcdb0,
// stopAnim 0x101dd1e8, setAnimTimer 0x101dd5e0, SetWindowDrawText 0x101de1bc.
#include <cstdint>

#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"

// 0x101dbba8 (164 bytes).
void InterfaceWindow::GetSizeOfClientDrawObject(CoolRect* rect) {
    InterfaceDrawObject* object = mClientDrawObject;
    if (object != nullptr) {
        rect->mLeft = object->mRect2.mLeft;
        rect->mTop = object->mRect2.mTop;
        rect->mRight = object->mRect2.mRight;
        rect->mBottom = object->mRect2.mBottom;
    } else {
        CoolRect empty(0, 0, 0, 0);
        rect->mLeft = empty.mLeft;
        rect->mTop = empty.mTop;
        rect->mRight = empty.mRight;
        rect->mBottom = empty.mBottom;
    }
}

// 0x101db0e8 (140 bytes).
void InterfaceWindow::AdjustPosition(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    CoolRect rect;
    rect.mLeft = mRect.mLeft;
    rect.mTop = mRect.mTop;
    rect.mRight = mRect.mRight;
    rect.mBottom = mRect.mBottom;
    rect.Adjust(left, top, right, bottom);
    SetPosition(&rect);
}

// 0x101dcdb0 (104 bytes).
void InterfaceWindow::UnCaptureKb(int32_t on) {
    InterfaceWindow* holder = gUIKeyboardWindow;
    if (holder != this) return;
    UI_PostMessage(holder, 0x20, on, 0);
    gUIKeyboardWindow = nullptr;
    UI_UpdatePointerIn();
}

// 0x101dd1e8 (236 bytes).
bool InterfaceWindow::stopAnim() {
    if (mAnimating == 0) return false;
    mAnimating = 0;
    mAnimFrame = 0;
    mAnim = nullptr;
    CoolRect rect;
    rect.mLeft = mRect.mLeft;
    rect.mTop = mRect.mTop;
    rect.mRight = mRect.mRight;
    rect.mBottom = mRect.mBottom;
    rect.Adjust(static_cast<int16_t>(-mAnimOffset[0]), static_cast<int16_t>(-mAnimOffset[1]),
                static_cast<int16_t>(-mAnimOffset[2]), static_cast<int16_t>(-mAnimOffset[3]));
    SetPosition(&rect);
    mAnimOffset[3] = 0;
    mAnimOffset[2] = 0;
    mAnimOffset[1] = 0;
    mAnimOffset[0] = 0;
    return true;
}

// 0x101dd5e0 (160 bytes). No bounds check on the index, like the original.
int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    if ((id & 0x80080) == 0x80080) id -= 0x80080;
    InterfaceTimer* timer = mAnimTimers[id];
    if (timer != nullptr) {
        timer->SetSlice(ms);
        return 1;
    }
    timer = UI_AddTimer(this, ms, id + 0x80080);
    mAnimTimers[id] = timer;
    return timer != nullptr ? 1 : 0;
}

// 0x101de1bc (80 bytes).
void InterfaceWindow::SetWindowDrawText(char16_t* text) {
    if (mDrawObject != nullptr) {
        DOTextStyle style{};
        style.mText = text;
        DOChange change{};
        change.mText = &style;
        mDrawObject->Change(0x100, &change);
    }
}

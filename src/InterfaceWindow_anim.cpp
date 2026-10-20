// InterfaceWindow::setPivot (main app, SimTheme Park 2.04b3, 0x101ddd34, 440 bytes), startAnim (0x101dd08c, 284 bytes) and
// startAnimTimer (0x101dd6b8, 100 bytes); the inventory files them under ui_cityLobbyController.cpp. Unit w91.
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"

namespace {

constexpr uint32_t kChangePivot = 0x40;          // the draw object's Change flag that carries the pivot pair (mF0C, mF0E)
constexpr int32_t kAnimTimerId = 0x80080;        // the ids of the four animation timers are 0x80080 to 0x80083

// Stores the pivot in the window and hands the pair to its draw object and then to its client draw object (each only when it has
// one). The Mac reads the pair back from the window into the change record, so the record holds what the window now holds.
void PlacePivot(InterfaceWindow* window, int16_t x, int16_t y) {
    window->mPivotX = x;
    window->mPivotY = y;
    DOChange change{};
    change.mF0C = window->mPivotX;
    change.mF0E = window->mPivotY;
    if (window->mDrawObject != nullptr) window->mDrawObject->Change(kChangePivot, &change);
    if (window->mClientDrawObject != nullptr) window->mClientDrawObject->Change(kChangePivot, &change);
}

}  // namespace

// The Mac unrolls three levels (the window, its children, theirs) and calls setPivot for the windows below those, each of which starts
// the same walk again. The order of the stores and of the Change calls is that of a plain recursion over the children.
void InterfaceWindow::setPivot(int16_t x, int16_t y) {
    PlacePivot(this, x, y);
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) {
        PlacePivot(child, x, y);
        for (InterfaceWindow* grandchild = child->mChild; grandchild != nullptr; grandchild = grandchild->mNext) {
            PlacePivot(grandchild, x, y);
            for (InterfaceWindow* deeper = grandchild->mChild; deeper != nullptr; deeper = deeper->mNext) deeper->setPivot(x, y);
        }
    }
}

// A running animation is first undone: the window is put back where it rests (its rectangle less the animation's offsets, through
// SetPosition) and the offsets are cleared. Then, unless `anim` is NULL, the frames start: frame 0, the clock now. The answer is
// whether an animation was started. QUIRK: with a NULL `anim` a running animation is still undone and mAnimating stays 1.
int32_t InterfaceWindow::startAnim(UI_RectDelta* anim, int32_t ticks) {
    if (mAnimating != 0) {
        CoolRect rect(mRect.mLeft, mRect.mTop, mRect.mRight, mRect.mBottom);
        rect.Adjust(static_cast<int16_t>(-mAnimOffset[0]), static_cast<int16_t>(-mAnimOffset[1]),
                    static_cast<int16_t>(-mAnimOffset[2]), static_cast<int16_t>(-mAnimOffset[3]));
        SetPosition(&rect);
        mAnimOffset[3] = 0;
        mAnimOffset[2] = 0;
        mAnimOffset[1] = 0;
        mAnimOffset[0] = 0;
    }
    if (anim != nullptr) {
        mAnimating = 1;
        mAnimFrame = 0;
        mAnim = anim;
        mAnimTicks = ticks;
        mAnimStart = static_cast<int32_t>(IveGotTicks());
    }
    return anim != nullptr;
}

// Starts the animation timer with this number: 0 to 3, or its timer id 0x80080 to 0x80083 (the id has the two bits of 0x80080 set and
// loses that value). QUIRK: no bounds check, any other number reads past the array. The answer is whether the timer exists.
int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    if ((which & kAnimTimerId) == kAnimTimerId) which -= kAnimTimerId;
    InterfaceTimer* timer = mAnimTimers[which];
    if (timer != nullptr) timer->Start();
    return timer != nullptr;
}

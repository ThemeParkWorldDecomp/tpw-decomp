// InterfaceWindow (main app, UI toolkit 0x101da564..0x101dfe84, ClientDraw 0x1018a2a8 and InterfaceTimer::Stop
// 0x101d8428): the drawing walk, the positions, the z rotation, the frame animation, and UI_DefaultWindowCallback, the
// handler every window starts with. The draw objects a window owns are reached through their virtual members
// (include/ui_DrawObject.h): Draw for the client rectangle, Change for everything else, with the flags below.
//
// Change flags this file uses (DOChange fields): 1 mFrame, 4 mF08 (the z rotation), 0x10 mRect, 0x40 mF0C and mF0E (the
// pivot), 0x80 the first word (a value). The Mac builds the record on the stack with only the fields it sets; the host's
// is zeroed first, which nothing can see.
#include "InterfaceWindow.h"

#include <cstdlib>
#include <cstring>

#include "ui_DrawObject.h"

UIButtonState gUIButtons[3];
int32_t gUIDoubleClickTime = 500;
int32_t gUIAutoRedraw;
int32_t gUIHelpTextResult;

void InterfaceTimer::Stop() { mRunning = 0; }

// ---- drawing

// The window's client draw object draws the client rectangle, one level above the window.
void InterfaceWindow::ClientDraw() {
    if (mClientDrawObject != nullptr) mClientDrawObject->Draw(0, &mClientRect, static_cast<int16_t>(mZ + 1));
}

// A window is drawn only while its attribute bit 1 is set, and a window of one of the kinds in 0xf0000000 draws nothing
// itself but still has its children drawn. (The Mac unrolls the first five levels of the tree; the order is the same.)
int32_t InterfaceWindow::DrawMe() {
    if ((mAttributes & 1) != 0) {
        if ((mAttributes & 0xf0000000) == 0) {
            DrawFunction();
            UI_SendMessage(this, 3, 0, 0);
        }
        for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) child->DrawMe();
    }
    return 1;
}

// The children last to first, each before its parent; a window's message 3 comes before its DrawFunction here (after it in
// DrawMe). The kind bits are read again after the children, as the original does.
int32_t InterfaceWindow::DrawReverseOrder() {
    if ((mAttributes & 1) != 0) {
        InterfaceWindow* child = mChild;
        if (child != nullptr) {
            while (child->mNext != nullptr) child = child->mNext;
            for (; child != nullptr; child = child->mPrevious) child->DrawReverseOrder();
        }
        if ((mAttributes & 0xf0000000) == 0) {
            UI_SendMessage(this, 3, 0, 0);
            DrawFunction();
        }
    }
    return 1;
}

// ---- positions

// Moves the rectangle, the region when it is not the rectangle, the client rectangle and the pivot, tells the draw objects
// the new rectangle and pivot (the window's own object gets the rectangle, the client's the client rectangle) and the
// handler message 0x13, then moves the children the same way.
int32_t InterfaceWindow::ShiftPosition(int16_t dx, int16_t dy) {
    if (dx == 0 && dy == 0) return 0;
    mRect.Shift(dx, dy);
    if (mRegion != &mRect) mRegion->Shift(dx, dy);
    mClientRect.Shift(dx, dy);
    mPivotX = static_cast<int16_t>(mPivotX + dx);
    mPivotY = static_cast<int16_t>(mPivotY + dy);
    DOChange change{};
    change.mF0C = mPivotX;
    change.mF0E = mPivotY;
    change.mRect = &mRect;
    if (mDrawObject != nullptr) mDrawObject->Change(0x50, &change);
    change.mRect = &mClientRect;
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(0x50, &change);
    UI_SendMessage(this, 0x13, dx, dy);
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) child->ShiftPosition(dx, dy);
    return 1;
}

// The window's edges moved by these amounts (the client rectangle's edges first); each child answers by its attribute bits,
// 0x10000 for the left, 0x20000 the right, 0x40000 the top, 0x80000 the bottom edge following the parent's. A child that
// follows both edges of an axis is stretched (SetPosition), one that follows one is shifted. All four bits: stretched both
// ways and not shifted. The rectangle SetPosition is given is the child's own less its animation offsets.
void InterfaceWindow::HandleScale(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    InterfaceWindow* child = mChild;
    CoolRect rect;
    mClientRect.Adjust(left, top, right, bottom);
    for (; child != nullptr; child = child->mNext) {
        uint32_t attributes = child->mAttributes;
        child->GetPosition(&rect, 1);
        if ((attributes & 0xf0000) == 0xf0000) {
            if (left != 0 || right != 0 || top != 0 || bottom != 0) {
                rect.mLeft = static_cast<int16_t>(rect.mLeft + left);
                rect.mRight = static_cast<int16_t>(rect.mRight + right);
                rect.mTop = static_cast<int16_t>(rect.mTop + top);
                rect.mBottom = static_cast<int16_t>(rect.mBottom + bottom);
                child->SetPosition(&rect);
            }
            continue;
        }
        if ((attributes & 0x30000) == 0x30000) {
            if (left != 0 || right != 0) {
                rect.mLeft = static_cast<int16_t>(rect.mLeft + left);
                rect.mRight = static_cast<int16_t>(rect.mRight + right);
                child->SetPosition(&rect);
            }
            attributes &= ~0x30000u;
        } else if ((attributes & 0xc0000) == 0xc0000) {
            if (top != 0 || bottom != 0) {
                rect.mTop = static_cast<int16_t>(rect.mTop + top);
                rect.mBottom = static_cast<int16_t>(rect.mBottom + bottom);
                child->SetPosition(&rect);
            }
            attributes &= ~0xc0000u;
        }
        if ((attributes & 0x10000) != 0 && left != 0) child->ShiftPosition(left, 0);
        if ((attributes & 0x20000) != 0 && right != 0) child->ShiftPosition(right, 0);
        if ((attributes & 0x40000) != 0 && top != 0) child->ShiftPosition(0, top);
        if ((attributes & 0x80000) != 0 && bottom != 0) child->ShiftPosition(0, bottom);
    }
}

// Nothing for NULL or for the rectangle the window has. A rectangle of the same size only shifts the window; any other
// resizes it: the old rectangle is kept as the previous one, the draw objects are told the new rectangle (flag 0x10, the
// client rectangle for the client object), the children are scaled by how each edge moved, and the handler gets message
// 0x12 with the new width and height. Both ways end in UI_UpdatePointerIn. Always answers 1.
int32_t InterfaceWindow::SetPosition(CoolRect* rect) {
    if (rect == nullptr) return 1;
    if (rect->mLeft == mRect.mLeft && rect->mTop == mRect.mTop && rect->mRight == mRect.mRight && rect->mBottom == mRect.mBottom)
        return 1;
    int16_t newWidth;
    int16_t newHeight;
    int16_t oldWidth;
    int16_t oldHeight;
    rect->GetDims(&newWidth, &newHeight);
    mRect.GetDims(&oldWidth, &oldHeight);
    if (newWidth == oldWidth && newHeight == oldHeight) {
        ShiftPosition(static_cast<int16_t>(rect->mLeft - mRect.mLeft), static_cast<int16_t>(rect->mTop - mRect.mTop));
        UI_UpdatePointerIn();
        return 1;
    }
    mPreviousRect.mLeft = mRect.mLeft;
    mPreviousRect.mTop = mRect.mTop;
    mPreviousRect.mRight = mRect.mRight;
    mPreviousRect.mBottom = mRect.mBottom;
    mRect.mLeft = rect->mLeft;
    mRect.mTop = rect->mTop;
    mRect.mRight = rect->mRight;
    mRect.mBottom = rect->mBottom;
    HandleScale(static_cast<int16_t>(mRect.mLeft - mPreviousRect.mLeft), static_cast<int16_t>(mRect.mTop - mPreviousRect.mTop),
                static_cast<int16_t>(mRect.mRight - mPreviousRect.mRight),
                static_cast<int16_t>(mRect.mBottom - mPreviousRect.mBottom));
    DOChange change{};
    change.mRect = &mRect;
    if (mDrawObject != nullptr) mDrawObject->Change(0x10, &change);
    change.mRect = &mClientRect;
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(0x10, &change);
    UI_SendMessage(this, 0x12, newWidth, newHeight);
    UI_UpdatePointerIn();
    return 1;
}

// The window's rectangle, with the animation's offsets taken off again when `subtract` is not 0 (so that an animated window
// is placed from where it is meant to rest). NULL: 0.
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    if (rect == nullptr) return 0;
    rect->mLeft = mRect.mLeft;
    rect->mTop = mRect.mTop;
    rect->mRight = mRect.mRight;
    rect->mBottom = mRect.mBottom;
    if (subtract != 0) {
        rect->Adjust(static_cast<int16_t>(-mAnimOffset[0]), static_cast<int16_t>(-mAnimOffset[1]),
                     static_cast<int16_t>(-mAnimOffset[2]), static_cast<int16_t>(-mAnimOffset[3]));
    }
    return 1;
}

// 0x101db9d0 (56 bytes): the rectangle the window was made with (the original rectangle, +0x14 to +0x1a). NULL: 0.
int32_t InterfaceWindow::GetOriginalPosition(CoolRect* rect) {
    if (rect == nullptr) return 0;
    rect->mLeft = mOriginalRect.mLeft;
    rect->mTop = mOriginalRect.mTop;
    rect->mRight = mOriginalRect.mRight;
    rect->mBottom = mOriginalRect.mBottom;
    return 1;
}

// The window goes to the end of its parent's list, and the window that was last is told 0x19 (and answered); this one 0x18.
// A window that is last already (or alone) is left alone and the answer is NULL. A first child makes its old next sibling the
// parent's first child.
InterfaceWindow* InterfaceWindow::BringToTop() {
    InterfaceWindow* next = mNext;
    if (next == nullptr) return nullptr;
    InterfaceWindow* previous = mPrevious;
    InterfaceWindow* last = this;
    while (last->mNext != nullptr) last = last->mNext;
    mNext = nullptr;
    last->mNext = this;
    mPrevious = last;
    if (previous != nullptr) previous->mNext = next;
    next->mPrevious = previous;
    if (previous == nullptr) next->mParent->mChild = next;
    UI_PostMessage(this, 0x18, 0, 0);
    UI_PostMessage(last, 0x19, 0, 0);
    return last;
}

// ---- z rotation

// The rotation is set on the window, told to its two draw objects (flag 4) and handed down to every descendant, each
// window before its children.
void InterfaceWindow::setZRot(int16_t rotation) {
    mZRot = rotation;
    DOChange change{};
    change.mF08 = mZRot;
    if (mDrawObject != nullptr) mDrawObject->Change(4, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(4, &change);
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) child->setZRot(rotation);
}

// ---- animation

// Takes one step of the animation (mAnim, from startAnim) when mAnimating is exactly 1 and more than mAnimTicks went by
// since the last step: the time of the step is the clock's again (a second reading). The frame mAnimFrame points at is
// either a command (see UI_RectDelta) or amounts added to the rectangle and to the animation's offsets; the window is then
// put in the new rectangle by SetPosition, which sends its own messages. Answers the frame the animation is at afterwards
// (0 after the end command, which also moves the window back by the offsets), or -1 when nothing was done. A goto command
// continues at once with the next step: the answer is whatever frame that leaves.
int32_t InterfaceWindow::doAnimFrame() {
    CoolRect rect;
    if (mAnimating != 1 || mAnim == nullptr) return -1;
    if (static_cast<int32_t>(IveGotTicks() - static_cast<uint32_t>(mAnimStart)) <= mAnimTicks) return -1;
    mAnimStart = static_cast<int32_t>(IveGotTicks());
    const UI_RectDelta* frame = &mAnim[mAnimFrame];
    if (frame->mLeft == kAnimEnd) {
        mAnimating = 0;
        mAnimFrame = 0;
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
    } else if (frame->mLeft == kAnimZRot) {
        setZRot(frame->mTop);
        mAnimFrame = static_cast<int16_t>(mAnimFrame + 1);
    } else if (frame->mLeft == kAnimGoto) {
        mAnimFrame = frame->mTop;
        doAnimFrame();
    } else if (frame->mLeft == kAnimFrame) {
        DOChange change{};
        change.mFrame = frame->mTop;
        if (mDrawObject != nullptr) mDrawObject->Change(1, &change);
        if (mClientDrawObject != nullptr) mClientDrawObject->Change(1, &change);
        mAnimFrame = static_cast<int16_t>(mAnimFrame + 1);
    } else {
        rect.mLeft = static_cast<int16_t>(mRect.mLeft + frame->mLeft);
        rect.mTop = static_cast<int16_t>(mRect.mTop + frame->mTop);
        rect.mRight = static_cast<int16_t>(mRect.mRight + frame->mRight);
        rect.mBottom = static_cast<int16_t>(mRect.mBottom + frame->mBottom);
        mAnimOffset[0] = static_cast<int16_t>(mAnimOffset[0] + frame->mLeft);
        mAnimOffset[1] = static_cast<int16_t>(mAnimOffset[1] + frame->mTop);
        mAnimOffset[2] = static_cast<int16_t>(mAnimOffset[2] + frame->mRight);
        mAnimOffset[3] = static_cast<int16_t>(mAnimOffset[3] + frame->mBottom);
        SetPosition(&rect);
        mAnimFrame = static_cast<int16_t>(mAnimFrame + 1);
    }
    return mAnimFrame;
}

// ---- value

// Hands the value to the window's two draw objects (change flag 0x80 reads it from the first word of the record). 1.
int32_t InterfaceWindow::SetValue(int32_t value) {
    DOChange change{};
    std::memcpy(change.mF00, &value, sizeof value);
    if (mDrawObject != nullptr) mDrawObject->Change(0x80, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(0x80, &change);
    return 1;
}

// ---- the default handler

namespace {

// The position a button went down at, as the next message carries it: x in the high half, y in the low half, with y
// sign-extended into the word (a negative y fills the high half with ones).
int32_t DownPosition(int32_t button) {
    const UIButtonState& state = gUIButtons[button];
    return static_cast<int32_t>(static_cast<uint32_t>(state.mX) << 16) | state.mY;
}

// The press, release and click messages (0x10004 to 0x10007) are repeated as 0x11004 to 0x11007 to a window whose client
// rectangle holds the point in `b` (x high, y low).
void PostIfInClient(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (window->mClientRect.PointIn(static_cast<int16_t>(b >> 16), static_cast<int16_t>(b)))
        UI_PostMessage(window, message | 0x1000, a, DownPosition(a));
}

// Message 0x10 with 0x80085: one step of a slide. While mSliding is nonzero each step moves the rectangle by mSlideStep and
// counts mSlideSteps down; when they are used up the timer is stopped, the window is put in mRect34 (the target) and told
// 0x1d.
void SlideStep(InterfaceWindow* window) {
    CoolRect rect;
    if (window->mSliding == 0) return;
    if (window->mSlideSteps == 0) {
        window->mSlideTimer->Stop();
        window->mSliding = 0;
        window->SetPosition(&window->mRect34);
        UI_PostMessage(window, 0x1d, 0, 0);
    } else {
        rect.mLeft = window->mRect.mLeft;
        rect.mTop = window->mRect.mTop;
        rect.mRight = window->mRect.mRight;
        rect.mBottom = window->mRect.mBottom;
        rect.Adjust(window->mSlideStep[0], window->mSlideStep[1], window->mSlideStep[2], window->mSlideStep[3]);
        window->SetPosition(&rect);
        window->mSlideSteps = static_cast<int16_t>(window->mSlideSteps - 1);
    }
}

}  // namespace

// The messages a window answers by itself (the others, and negative ones, are ignored; the answer is 0 unless said):
//   1 DrawMe, 2 DrawReverseOrder, 3 ClientDraw, 4 deletes the window, 5 posts 4 to it, 6 Show(a), 7 ShiftPosition(a, b),
//   8 SetPosition to the rectangle (a high, a low, b high, b low: left, top, right, bottom), 9 BringToTop and, when it
//   moved and a is not 0 and gUIAutoRedraw is set, a draw message,
//   0xc stores GetHelpText in gUIHelpTextResult and answers its ADDRESS (the host's int32_t answer is the low word of it),
//   0x10 with a = 0x80084 doAnimFrame, 0x80085 one step of a slide, 0x1e (on frame) tells the help text handler when the
//   pointer is in this window and steps the animation,
//   0x10001 pointer entered (cursor, help text), 0x10002 left, 0x10003 moved (x high, y low): the help text is told, and a
//   button held more than 6 away from where it went down stops counting as a click,
//   0x10004 button a released, 0x10005 pressed (and takes the focus), 0x10006 click, 0x10007 second press: the point is in
//   b; see the cases. All of these keep the position the button went down at in gUIButtons.
int32_t UI_DefaultWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message < 0) return 0;
    switch (message) {
    case 1:
        window->DrawMe();
        break;
    case 2:
        window->DrawReverseOrder();
        break;
    case 3:
        window->ClientDraw();
        break;
    case 4:
        if (window != nullptr) delete window;
        break;
    case 5:
        UI_PostMessage(window, 4, 0, 0);
        break;
    case 6:
        window->Show(static_cast<int16_t>(a));
        break;
    case 7:
        window->ShiftPosition(static_cast<int16_t>(a), static_cast<int16_t>(b));
        break;
    case 8: {
        CoolRect rect(static_cast<int16_t>(a >> 16), static_cast<int16_t>(a), static_cast<int16_t>(b >> 16), static_cast<int16_t>(b));
        window->SetPosition(&rect);
        break;
    }
    case 9:
        if (window->BringToTop() != nullptr && a != 0 && gUIAutoRedraw != 0) UI_PostMessage(window, 1, 0, 0);
        break;
    case 0xc:
        gUIHelpTextResult = window->GetHelpText();
        return static_cast<int32_t>(reinterpret_cast<intptr_t>(&gUIHelpTextResult));
    case 0x10:
        if (a == 0x80084) window->doAnimFrame();
        else if (a == 0x80085) SlideStep(window);
        break;
    case 0x1e:
        if (gUIHelpText->mWindow == window) gUIHelpText->OnFrame();
        window->doAnimFrame();
        break;
    case 0x10001:
        UI_SetCursor(window->mCursor);
        gUIHelpText->OnMouseMovedIn(window);
        break;
    case 0x10002:
        gUIHelpText->OnMouseMovedOut();
        break;
    case 0x10003: {
        const int16_t x = static_cast<int16_t>(a >> 16);
        const int16_t y = static_cast<int16_t>(a);
        gUIHelpText->OnMouseMoved(x, y);
        if (window->mButtonsHeld != 0) {
            for (int32_t i = 0; i < 3; ++i) {
                UIButtonState& button = gUIButtons[i];
                if (button.mState == 1 && (std::abs(button.mX - x) > 6 || std::abs(button.mY - y) > 6)) button.mState = 2;
            }
        }
        break;
    }
    case 0x10004: {
        PostIfInClient(window, message, a, b);
        UIButtonState& button = gUIButtons[a];
        if (button.mState == 1 && (window->mButtonsHeld & (1u << a)) != 0) {
            const uint32_t now = IveGotTicks();
            if (static_cast<int32_t>(now - static_cast<uint32_t>(button.mTime)) < gUIDoubleClickTime)
                UI_PostMessage(window, 0x10006, a, DownPosition(a));
            button.mTime = static_cast<int32_t>(now);
        } else {
            button.mTime = 0;
        }
        window->mButtonsHeld &= ~(1u << a);
        button.mState = 0;
        break;
    }
    case 0x10005: {
        if (gUIFocusWindow != window) {
            UI_PostMessage(gUIFocusWindow, 0x1b, 0, 0);
            gUIFocusWindow = window;
            UI_PostMessage(gUIFocusWindow, 0x1a, 0, 0);
        }
        const int16_t x = static_cast<int16_t>(b >> 16);
        const int16_t y = static_cast<int16_t>(b);
        PostIfInClient(window, message, a, b);
        const uint32_t now = IveGotTicks();
        UIButtonState& button = gUIButtons[a];
        if (button.mState == 0) {
            window->mButtonsHeld |= 1u << a;
            button.mX = x;
            button.mY = y;
            button.mState = 1;
            if ((window->mAttributes & 8) == 0 && static_cast<int32_t>(now - static_cast<uint32_t>(button.mTime)) < gUIDoubleClickTime) {
                UI_PostMessage(window, 0x10007, a, b);
                button.mState = 2;
            }
            button.mTime = static_cast<int32_t>(IveGotTicks());
        }
        break;
    }
    case 0x10006:
    case 0x10007:
        PostIfInClient(window, message, a, b);
        break;
    default:
        break;
    }
    return 0;
}

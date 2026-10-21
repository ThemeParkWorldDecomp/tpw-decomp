// InterfaceWindow.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceWindow code, named after it.

#include "AdvisorScoring.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "InterfaceFont.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "std_basic_ifstream.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_menu.h"
#include "ui_Postcard.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>


// The window classes' WhatAmI members, the base window's CalcClientArea and CallDefaultWindowHandler (main app, UI toolkit; unit w36).
// WhatAmI is the interface's run time type: each class answers its own kind number, the one InterfaceWindow::CreateObject
// (src/ui_controls_create.cpp) takes to make it and the window scripts' records test with (src/InterfaceWindow_script.cpp, whose
// records 6 to 16 assert on the answer). The button's and the scroll bar's are in their own files (InterfaceButton.cpp,
// InterfaceScrollBar.cpp); the kinds are the enum in include/ui_controls.h.

// 0x101d987c (36 bytes): the client rectangle is the window's rectangle. The classes whose client area is smaller (the list box, the
// edit box, the list control, the scroll bar) have their own, and the window's constructor reaches this one through the base class.
void InterfaceWindow::CalcClientArea() {
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
}

// InterfaceWindow (main app, UI toolkit 0x101d98d8..0x101dc3e8): the constructor, PointIn and
// MakeRectAbsolute. The rest of the class is untranslated (include/InterfaceWindow.h lists
// its virtual members and data); later units add to this file.

InterfaceWindow* gUILastWindow;   // 0x10395c18, the window the last constructor made


// The window's state is the Mac's field by field. The members that are not set here
// (0x68..0x87, 0xec, 0xf0 and 0x100..0x10f) are left as the allocation had them. The
// constructor reaches the base class's CalcClientArea, as the Mac's does while its vtable
// pointer is still the base's.
InterfaceWindow::InterfaceWindow(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                 int16_t right, int16_t bottom) {
    mDestroying = 0;
    mAttributes = static_cast<uint32_t>(attributes);
    mCallback = UI_DefaultWindowCallback;
    mDefaultCallback = UI_DefaultWindowCallback;
    mParent = parent;
    mNext = nullptr;
    mChild = nullptr;
    mPrevious = nullptr;
    mOwner = nullptr;   // (the word at +0x60: zero on the Mac)
    mF64 = 0;
    mAnim = nullptr;
    mAnimating = 0;
    mAnimFrame = 0;
    mAnimOffset[3] = 0;
    mAnimOffset[2] = 0;
    mAnimOffset[1] = 0;
    mAnimOffset[0] = 0;
    mSlideTimer = nullptr;
    mHelpText = -1;
    mF90 = -1;
    for (int32_t i = 0; i < 4; ++i) mAnimTimers[i] = nullptr;
    mF134 = 0;
    mF130 = 0;
    mCursor = -1;
    mId = id;
    mButtonsHeld = 0;
    mRect.mLeft = left;
    mRect.mTop = top;
    mRect.mRight = right;
    mRect.mBottom = bottom;
    if ((attributes & 0x400) == 0x400) MakeRectAbsolute(&mRect);
    CalcClientArea();
    gUILastWindow = this;
    mFont = nullptr;
    mDrawObject = nullptr;
    mClientDrawObject = nullptr;
    mRegion = &mRect;
    mOriginalRect.mLeft = mRect.mLeft;
    mOriginalRect.mTop = mRect.mTop;
    mOriginalRect.mRight = mRect.mRight;
    mOriginalRect.mBottom = mRect.mBottom;
    if (parent != nullptr) {
        mZ = static_cast<int16_t>(parent->mZ + 2);
        mPivotX = parent->mPivotX;
        mPivotY = parent->mPivotY;
    } else {
        mZ = 1;
        mPivotX = left;
        mPivotY = bottom;
    }
    // (the pivot is then the middle of the rectangle, whichever of the two the branch gave)
    mPivotX = static_cast<int16_t>(left + ((right - left) >> 1));
    mPivotY = static_cast<int16_t>(top + ((bottom - top) >> 1));
    if (mZ > 0x100) mZ = 0x100;
    mZRot = 0;
    std::memset(mExtraParams, 0, sizeof mExtraParams);
    if ((attributes & 1) != 0) UI_UpdatePointerIn();
    UI_PostMessage(this, 0x15, 0, 0);
}

// InterfaceWindow's destructor and the two members it uses (main app, UI toolkit 0x101d9ba4, 0x101d9ee8 and 0x101dcebc; unit w51):
// ~InterfaceWindow, PropagateDestruction and UnCaptureKeys.
//
// The Mac's destructor takes the compiler's deleting flag in r4 (the child windows are destroyed through the vtable with 1, the CoolRect
// members with -1) and calls operator delete when it is positive; on the host the deleting form is the compiler's, so this is the plain
// body. The vtable pointer store at the top (+0x118 on the Mac) is the compiler's too.

namespace {
// A draw object the window lets go of: it loses a user and is told Show(0); true when it is not in the library and has no user left
// (the same test as SetDrawObject's). The caller then deletes it if the window still holds it (the Mac reads the window's member again
// after the Show).
bool GivesUp(InterfaceDrawObject* object) {
    object->mResizable = static_cast<int16_t>(object->mResizable - 1);
    object->Show(0);
    return static_cast<int16_t>(object->mResizable - (1 - object->mInLibrary)) == -1;
}
}  // namespace

// 0x101d9ba4 (736 bytes). In order: tells the window's subtree it is going (unless that has begun), tells the help text handler
// (OnHideOrDelete), lets go of the draw object and then of the client draw object, removes the animation timers that are left (ids
// 0x80080 to 0x80083), deletes a region that is not the window's own rectangle, clears every global that names the window, poisons its
// queued messages in the three queues, unlinks it from its siblings and parent, deletes its children one at a time (each unlinks itself
// from mChild; a child that did not would be deleted for ever), works out the window under the pointer again and, last, the rectangle
// members are destroyed (the compiler's, in the reverse of their order: +0x34, +0x28, +0x1c, +0x10, +4). QUIRK: the window's own
// next and previous links are left as they were (only the parent link is cleared).
InterfaceWindow::~InterfaceWindow() {
    if (mDestroying == 0) PropagateDestruction();
    gUIHelpText->OnHideOrDelete(this);
    if (mDrawObject != nullptr) {
        InterfaceDrawObject* object = mDrawObject;
        if (GivesUp(object) && mDrawObject != nullptr) delete mDrawObject;
    }
    if (mClientDrawObject != nullptr) {
        InterfaceDrawObject* object = mClientDrawObject;
        if (GivesUp(object) && mClientDrawObject != nullptr) delete mClientDrawObject;
    }
    for (int32_t i = 0; i < 4; ++i) {
        if (mAnimTimers[i] != nullptr) UI_RemoveTimer(this, 0x80080 + i);
    }
    if (mRegion != nullptr && mRegion != &mRect) delete mRegion;
    ClearAllPointers(this);
    UI_InvalidateMessages(this);
    UI_InvalidateDrawMessages(this);
    UI_InvalidateDeferredMessages(this);
    if (mParent != nullptr && mPrevious == nullptr) mParent->mChild = mNext;
    if (mPrevious != nullptr) mPrevious->mNext = mNext;
    if (mNext != nullptr) mNext->mPrevious = mPrevious;
    mParent = nullptr;
    while (mChild != nullptr) delete mChild;
    UI_UpdatePointerIn();
}

// 0x101d9eb0: the plain window.
int32_t InterfaceWindow::WhatAmI() { return kUIKindWindow; }

// 0x101d9ee8 (500 bytes). Tells the window message 0x14 (destroy), marks it, does the same for each child in turn along mNext, and
// when a window's subtree is done takes it out of the key capture, the keyboard window and the focus window. The Mac unrolls the first
// four levels and calls itself for the fifth and deeper; the order of the sends and of the clears is the recursion's. A window that is
// marked already is not sent the message (UI_SendMessage refuses it) but is walked all the same.
void InterfaceWindow::PropagateDestruction() {
    UI_SendMessage(this, 0x14, 0, 0);
    mDestroying = 1;
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) child->PropagateDestruction();
    UnCaptureKeys();
    if (gUIKeyboardWindow == this) gUIKeyboardWindow = nullptr;
    if (gUIFocusWindow == this) gUIFocusWindow = nullptr;
}

// InterfaceWindow's font and text setters (main app, UI toolkit): SetFont 0x101da118, SetFontJust 0x101da204, SetFontColour 0x101da3ac,
// setExtraParam 0x101dcfd0, SetClientDrawText 0x101de130 and the virtual SetText 0x101de248. Unit w39.
//
// A window has a draw object (+0x94) and a client draw object (+0x98); the setters tell them what changed through Change, with a flag that
// names the field of the change they should read (the original builds the change on its stack and sets only that field, so the other
// fields are never read). The draw object comes first, and the whole call is skipped for a draw object the window does not have.

namespace {

constexpr uint32_t kChangeText = 0x100;          // the style's text
constexpr uint32_t kChangeJustify = 0xc00;       // the style's two justification bytes
constexpr uint32_t kChangeFont = 0x10000;        // the style's font id
constexpr uint32_t kChangeColour = 0x40000;      // the style's colour

// Tells the window's draw objects (the main one, then the client one) the style, with the flag that says which part is new.
void ChangeBoth(InterfaceDrawObject* draw, InterfaceDrawObject* client, uint32_t flags, DOTextStyle* style) {
    DOChange change{};
    change.mText = style;
    if (draw != nullptr) draw->Change(flags, &change);
    if (client != nullptr) client->Change(flags, &change);
}

}  // namespace

// The font is kept even when the window has no draw object. A window that has the font already does nothing. The id the draw objects get
// is the low byte of the font's id (0xff for no font).
void InterfaceWindow::SetFont(InterfaceFont* font) {
    if (font == mFont) return;
    mFont = font;
    DOTextStyle style{};
    style.mF06[2] = font != nullptr ? static_cast<uint8_t>(font->mID) : 0xff;
    ChangeBoth(mDrawObject, mClientDrawObject, kChangeFont, &style);
}

void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) {
    if (mDrawObject == nullptr && mClientDrawObject == nullptr) return;
    DOTextStyle style{};
    style.mF04 = static_cast<uint8_t>(horizontal);
    style.mF05 = static_cast<uint8_t>(vertical);
    ChangeBoth(mDrawObject, mClientDrawObject, kChangeJustify, &style);
}

// unit w43: main 0x101da2d8, 160 bytes. The wrapping codes occupy style bytes 6 and 7.
void InterfaceWindow::SetFontWrap(int32_t horizontal, int32_t vertical) {
    if (mDrawObject == nullptr && mClientDrawObject == nullptr) return;
    DOTextStyle style{};
    style.mF06[0] = static_cast<uint8_t>(horizontal);
    style.mF06[1] = static_cast<uint8_t>(vertical);
    ChangeBoth(mDrawObject, mClientDrawObject, 0x3000, &style);
}

void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    if (mDrawObject == nullptr && mClientDrawObject == nullptr) return;
    DOTextStyle style{};
    style.mColour[0] = static_cast<uint8_t>(red);
    style.mColour[1] = static_cast<uint8_t>(green);
    style.mColour[2] = static_cast<uint8_t>(blue);
    style.mF13 = static_cast<uint8_t>(alpha);
    ChangeBoth(mDrawObject, mClientDrawObject, kChangeColour, &style);
}

// InterfaceWindow's text offset and size query (main app, unit w93).

// 0x101da484 (168 bytes). The Change record is made on the stack with only its text-style pointer set; the style holds the two
// offsets (flags 0x4000 and 0x8000 read them) and nothing else. The draw object goes first, then the client draw object.
void InterfaceWindow::SetTextOffset(int32_t x, int32_t y) {
    if (mDrawObject == nullptr && mClientDrawObject == nullptr) return;
    DOTextStyle style{};
    const int16_t offsetX = static_cast<int16_t>(x);
    const int16_t offsetY = static_cast<int16_t>(y);
    std::memcpy(&style.mF0A[0], &offsetX, sizeof offsetX);
    std::memcpy(&style.mF0A[2], &offsetY, sizeof offsetY);
    DOChange change{};
    change.mText = &style;
    if (mDrawObject != nullptr) mDrawObject->Change(0xc000, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(0xc000, &change);
}

// InterfaceWindow (main app, UI toolkit 0x101da564..0x101dfe84, ClientDraw 0x1018a2a8 and InterfaceTimer::Stop
// 0x101d8428): the drawing walk, the positions, the z rotation, the frame animation, and UI_DefaultWindowCallback, the
// handler every window starts with. The draw objects a window owns are reached through their virtual members
// (include/ui_DrawObject.h): Draw for the client rectangle, Change for everything else, with the flags below.
//
// Change flags this file uses (DOChange fields): 1 mFrame, 4 mF08 (the z rotation), 0x10 mRect, 0x40 mF0C and mF0E (the
// pivot), 0x80 the first word (a value). The Mac builds the record on the stack with only the fields it sets; the host's
// is zeroed first, which nothing can see.

UIButtonState gUIButtons[3];
int32_t gUIDoubleClickTime = 500;
int32_t gUIAutoRedraw;
int32_t gUIHelpTextResult;

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

// Shifts `rect` by the position of every window above this one (each parent's rectangle
// as it is now, so a parent that is itself absolute is counted with its own parents again).
void InterfaceWindow::MakeRectAbsolute(CoolRect* rect) {
    for (InterfaceWindow* parent = mParent; parent != nullptr; parent = parent->mParent)
        rect->Shift(parent->mRect.mLeft, parent->mRect.mTop);
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

// InterfaceWindow leaves of unit v5119: GetSizeOfClientDrawObject 0x101dbba8, AdjustPosition 0x101db0e8, UnCaptureKb 0x101dcdb0,
// stopAnim 0x101dd1e8, setAnimTimer 0x101dd5e0, SetWindowDrawText 0x101de1bc.

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

// InterfaceWindow (main app, UI toolkit 0x101db1b0..0x101dc2f0): the window's show and position leaves that the drawing unit
// (InterfaceWindow_draw.cpp) calls and does not define: Show, ShowAllDRAWOBJS, GetWindow and two of the three SetPosition overloads.

// The window is moved to (*x, *y) with the size it has (a NULL keeps that coordinate). The right and bottom are 16-bit sums.
int32_t InterfaceWindow::SetPosition(int16_t* x, int16_t* y) {
    const int32_t width = mRect.mRight - mRect.mLeft;
    const int32_t height = mRect.mBottom - mRect.mTop;
    const int32_t left = x != nullptr ? *x : mRect.mLeft;
    const int32_t top = y != nullptr ? *y : mRect.mTop;
    CoolRect rect(static_cast<int16_t>(left), static_cast<int16_t>(top), static_cast<int16_t>(left + width),
                  static_cast<int16_t>(top + height));
    SetPosition(&rect);
    return 1;
}

int32_t InterfaceWindow::SetPosition(int32_t left, int32_t top, int32_t right, int32_t bottom) {
    CoolRect rect(static_cast<int16_t>(left), static_cast<int16_t>(top), static_cast<int16_t>(right), static_cast<int16_t>(bottom));
    SetPosition(&rect);
    return 1;
}

// InterfaceWindow::SetOriginalPosition 0x101db284 (48 bytes; Mac file ui_cityLobbyController.cpp; unit w118).

// The four shorts of the rectangle (read with lha, written with sth) go to the rectangle the window was made with, in the order left, top,
// right, bottom. A NULL rectangle writes nothing. The answer is 1 either way (GetOriginalPosition answers 0 for NULL).
int32_t InterfaceWindow::SetOriginalPosition(CoolRect* rect) {
    if (rect != nullptr) {
        mOriginalRect.mLeft = rect->mLeft;
        mOriginalRect.mTop = rect->mTop;
        mOriginalRect.mRight = rect->mRight;
        mOriginalRect.mBottom = rect->mBottom;
    }
    return 1;
}

// The scroll bar's thumb handler and position leaves (main app, unit w37b).

// 0x101db2f8, 0x101db954. Copy coordinates only, preserving the destination's vtable.
int32_t InterfaceWindow::UpdatePreviousPosition() {
    mPreviousRect.mLeft = mRect.mLeft;
    mPreviousRect.mTop = mRect.mTop;
    mPreviousRect.mRight = mRect.mRight;
    mPreviousRect.mBottom = mRect.mBottom;
    return 1;
}

// InterfaceWindow::setZ (main app, SimTheme Park 2.04b3, 0x101dd8bc, 540 bytes) and GetClientRect (0x101dba4c, 56 bytes); the inventory
// files both under ui_cityLobbyController.cpp. Unit w43b. SetClientRect (0x101db3fc, 56 bytes): unit v5L14.

namespace {

constexpr int16_t kMaxZ = 0x100;   // a window's depth is cut to this above (not below)
constexpr uint32_t kChangeDepth = 2;

// Keeps the depth in the window (cut at kMaxZ) and tells its draw object that depth and its client draw object one more.
void PlaceDepth(InterfaceWindow* window, int16_t z) {
    window->mZ = z;
    if (window->mZ > kMaxZ) window->mZ = kMaxZ;
    DOChange change{};
    change.mDepth = window->mZ;
    if (window->mDrawObject != nullptr) window->mDrawObject->Change(kChangeDepth, &change);
    change.mDepth = static_cast<int16_t>(window->mZ + 1);
    if (window->mClientDrawObject != nullptr) window->mClientDrawObject->Change(kChangeDepth, &change);
}

}  // namespace

int32_t InterfaceWindow::SetClientRect(CoolRect* rect) {
    if (rect == nullptr) return 0;
    mClientRect.mLeft = rect->mLeft;
    mClientRect.mTop = rect->mTop;
    mClientRect.mRight = rect->mRight;
    mClientRect.mBottom = rect->mBottom;
    return 1;
}

// InterfaceWindow's window script (main app, UI toolkit 0x101dec14): the interpreter that builds a panel from a table of 16-bit words
// (Build and ProcessScript), the two draw object setters it calls, and three small members: GetVisibility, SetCallbackFunction and
// CaptureKb. Unit w31a.
//
// THE SCRIPT. A window script is an array of shorts (the Mac's, big-endian; here the host's), read through ONE position that all
// the nested calls share (gUIScriptPosition; Build sets it to 0). A record starts with a word that says what it is; a word that is
// not 0 to 0x12 (a negative one included) ends the level like record 5. A 32-bit value is two words, the LOW half first (read
// unsigned) and then the high half (signed). The records, with the words that follow the type:
//   0  a child window: object kind, attributes (2 words), id (2), left, top, right, bottom (CreateObject). The records after it,
//      up to the 5 that ends them, are about the NEW window (they run in a nested level on it).
//   1  the window's draw object: the hash of its name (2 words; DrawObjectLibrary::FindDrawObject) -> SetDrawObject
//   2  the client draw object (2 words) -> SetClientDrawObject
//   3  the client rectangle: left, top, right, bottom; made absolute when the window's attribute 0x400 is set
//   4  the window's region: a shape number and its numbers: 1 rectangle (4 words), 2 circle (x, y, radius), 3 ellipse (x, y, radii),
//      4 polygon (count, then x and y of each point); any other number changes nothing and takes no more words
//   5  end of this level
//   6, 7, 8  of a scroll bar (kind 3): its up button, its down button and its thumb (four words: the rectangle; a kind 2 child
//      with ids 1, 2 and 3; the thumb gets UI_DefaultThumbtrackCallback); the children's own records follow
//   9  a child of id 1 (kind 3, attributes 0x10: the scroll bar) for a list box (kind 4) or a list control (kind 7) that has no
//      child of id 1 yet (four words); on any other window it takes no words
//   10 the list rectangle of a list box (kind 4) or a list control (kind 7): four words; on any other window no words
//   11 a list control's columns: the count, then left and right of each (SetColumns, SetColumnPos)
//   12 a list control's column header: a column number (the id is 0x10 + it) and a rectangle (a kind 2 child)
//   13 a knob's angles: smallest and largest (SetMinMaxAngles)
//   14, 15  of a vary box (kind 0xc): its plus and minus buttons (kind 2 children, ids 1 and 2): four words, the rectangle
//   16 a light of a light panel (kind 0xd): the light number 0 to 31 (the id is 0x11100000 | it) and a rectangle (a kind 6 child: an image)
//   17 the window's help text number (2 words)
//   18 the word at +0x90 (2 words)
// The records that need a kind of window say so with an assertion (the Mac's strings, below) and go on when it fails.

int32_t gUIScriptPosition;   // 0x10395c5c

namespace {

// The kinds of window the records test for (the Mac source's UIOBJ_CREATE_ names, from its assertion strings) and the kinds of
// child they make.
constexpr int32_t kScrollBar = 3;
constexpr int32_t kListBox = 4;
constexpr int32_t kListControl = 7;
constexpr int32_t kKnob = 0xb;
constexpr int32_t kVaryBox = 0xc;
constexpr int32_t kLightPanel = 0xd;
constexpr int32_t kChildButton = 2;
constexpr int32_t kChildScrollBar = 3;
constexpr int32_t kChildImage = 6;
constexpr int32_t kLightBase = 0x11100000;      // UIID_LIGHTBASE
constexpr uint32_t kButtonlessScroll = 0x40;    // UIATTR_BUTTLESS_SCROLL, a scroll bar's attribute

const char* const kFile = "UI_Window.cpp";

// The words of the script, through the shared position.
int16_t NextWord(const int16_t* script) { return script[gUIScriptPosition++]; }

// A 32-bit value: the low half first (unsigned), then the high half (signed).
int32_t NextLong(const int16_t* script) {
    const uint32_t low = static_cast<uint16_t>(script[gUIScriptPosition++]);
    const int32_t high = script[gUIScriptPosition++];
    return static_cast<int32_t>(low | (static_cast<uint32_t>(high) << 16));
}

struct ScriptRect {
    int16_t left, top, right, bottom;
};
ScriptRect NextRect(const int16_t* script) {
    ScriptRect rect;
    rect.left = NextWord(script);
    rect.top = NextWord(script);
    rect.right = NextWord(script);
    rect.bottom = NextWord(script);
    return rect;
}

// Asks the window for a child (an assertion when it could not make one; the answer is NULL then).
InterfaceWindow* CreateChild(InterfaceWindow* window, int32_t kind, int32_t attributes, int32_t id, const ScriptRect& rect,
                             int32_t line) {
    InterfaceWindow* child = window->CreateObject(kind, attributes, id, rect.left, rect.top, rect.right, rect.bottom);
    if (child == nullptr) msl_assertion_failed("lpNew", kFile, line);
    return child;
}

// The first child with that id, or NULL.
InterfaceWindow* FindChild(InterfaceWindow* window, int32_t id) {
    InterfaceWindow* found = nullptr;
    for (InterfaceWindow* child = window->mChild; child != nullptr && found == nullptr; child = child->mNext)
        if (child->mId == id) found = child;
    return found;
}

// Takes the place of the window's region: the old one goes unless it is the window's own rectangle; the new one is the window's
// own rectangle when it could not be made.
void ReplaceRegion(InterfaceWindow* window, CoolRegion* region) {
    CoolRegion* old = window->mRegion;
    if (old != nullptr && old != &window->mRect) delete old;
    window->mRegion = region != nullptr ? region : &window->mRect;
}

// Record 4. A shape number that is not 1 to 4 changes nothing.
void ReadRegion(InterfaceWindow* window, const int16_t* script) {
    const int16_t shape = NextWord(script);
    if (shape == 1) {
        const ScriptRect rect = NextRect(script);
        ReplaceRegion(window, new (std::nothrow) CoolRect(rect.left, rect.top, rect.right, rect.bottom));
    } else if (shape == 2) {
        const int16_t x = NextWord(script);
        const int16_t y = NextWord(script);
        const int16_t radius = NextWord(script);
        ReplaceRegion(window, new (std::nothrow) CoolCircle(x, y, radius));
    } else if (shape == 3) {
        const int16_t x = NextWord(script);
        const int16_t y = NextWord(script);
        const int16_t radiusX = NextWord(script);
        const int16_t radiusY = NextWord(script);
        ReplaceRegion(window, new (std::nothrow) CoolEllipse(x, y, radiusX, radiusY));
    } else if (shape == 4) {
        // The points are read into a temporary array that the polygon copies and that goes when the region is in place. (A
        // negative count never ends the loop, as in the original.)
        const int16_t count = NextWord(script);
        int16_t* points = new int16_t[static_cast<uint32_t>(count) * 2];
        for (int32_t remaining = count; remaining != 0; --remaining) {
            const int32_t index = count - remaining;
            const int16_t x = NextWord(script);
            const int16_t y = NextWord(script);
            points[2 * index] = x;
            points[2 * index + 1] = y;
        }
        ReplaceRegion(window, new (std::nothrow) CoolPoly(count, points));
        delete[] points;
    }
}

// The interpreter: the records of one level, on `window` (NULL after a CreateObject that failed; the records that use it then
// crash, as the original's do). Answers the last window a record of this level made.
InterfaceWindow* ProcessLevel(InterfaceWindow* window, int16_t* script) {
    InterfaceWindow* last = nullptr;
    bool going = true;
    while (going) {
        const int16_t record = NextWord(script);
        if (static_cast<uint32_t>(static_cast<int32_t>(record)) > 0x12) {   // (unsigned: the negative ones end the level too)
            going = false;
            continue;
        }
        switch (record) {
        case 0: {
            const int32_t kind = NextWord(script);
            const int32_t attributes = NextLong(script);
            const int32_t id = NextLong(script);
            const ScriptRect rect = NextRect(script);
            last = CreateChild(window, kind, attributes, id, rect, 0xa3d);
            ProcessLevel(last, script);
            break;
        }
        case 1:
            window->SetDrawObject(DrawObjectLibrary::FindDrawObject(static_cast<uint32_t>(NextLong(script))));
            break;
        case 2:
            window->SetClientDrawObject(DrawObjectLibrary::FindDrawObject(static_cast<uint32_t>(NextLong(script))));
            break;
        case 3: {
            const ScriptRect rect = NextRect(script);
            window->mClientRect.Set(rect.left, rect.top, rect.right, rect.bottom);
            if ((window->mAttributes & 0x400) != 0) window->MakeRectAbsolute(&window->mClientRect);
            break;
        }
        case 4:
            ReadRegion(window, script);
            break;
        case 5:
            going = false;
            break;
        case 6:
        case 7: {
            // The up and the down button of a scroll bar that has neither it nor the attribute that says it has no buttons.
            const int32_t id = record == 6 ? 1 : 2;
            bool ok = false;
            bool available = false;
            if (window->WhatAmI() == kScrollBar && window->GetWindow(id) == nullptr) available = true;
            if (available && (window->mAttributes & kButtonlessScroll) == 0) ok = true;
            if (!ok)
                msl_assertion_failed(record == 6 ? "WhatAmI() == UIOBJ_CREATE_SCROLLBAR && !GetWindow(UIID_SCROLLUP) && "
                                                   "!(attributes & UIATTR_BUTTLESS_SCROLL)"
                                                 : "WhatAmI() == UIOBJ_CREATE_SCROLLBAR && !GetWindow(UIID_SCROLLDN) && "
                                                   "!(attributes & UIATTR_BUTTLESS_SCROLL)",
                                     kFile, record == 6 ? 0xa90 : 0xa9d);
            const ScriptRect rect = NextRect(script);
            last = CreateChild(window, kChildButton, 0x21, id, rect, record == 6 ? 0xa96 : 0xaa3);
            ProcessLevel(last, script);
            break;
        }
        case 8: {
            // The thumb of a scroll bar that has none: a button with the thumb track's handler.
            bool ok = false;
            if (window->WhatAmI() == kScrollBar && window->GetWindow(3) == nullptr) ok = true;
            if (!ok) msl_assertion_failed("WhatAmI() == UIOBJ_CREATE_SCROLLBAR && !GetWindow(UIID_SCROLLTHUMB)", kFile, 0xaaa);
            const ScriptRect rect = NextRect(script);
            last = CreateChild(window, kChildButton, 1, 3, rect, 0xab0);
            last->SetCallbackFunction(UI_DefaultThumbtrackCallback);
            ProcessLevel(last, script);
            break;
        }
        case 9: {
            // The scroll bar of a list box, or else of a list control; each only when it has no child of id 1 (a list box that
            // has one is asked again what it is).
            bool made = false;
            if (window->WhatAmI() == kListBox && FindChild(window, 1) == nullptr) {
                const ScriptRect rect = NextRect(script);
                last = CreateChild(window, kChildScrollBar, 0x10, 1, rect, 0xaee);
                ProcessLevel(last, script);
                made = true;
            }
            if (!made && window->WhatAmI() == kListControl && FindChild(window, 1) == nullptr) {
                const ScriptRect rect = NextRect(script);
                last = CreateChild(window, kChildScrollBar, 0x10, 1, rect, 0xaf9);
                ProcessLevel(last, script);
            }
            break;
        }
        case 10:
            // (a window that is not a list box is asked again whether it is a list control)
            if (window->WhatAmI() == kListBox) {
                const ScriptRect rect = NextRect(script);
                static_cast<InterfaceListBox*>(window)->SetListRect(rect.left, rect.top, rect.right, rect.bottom);
            } else if (window->WhatAmI() == kListControl) {
                const ScriptRect rect = NextRect(script);
                static_cast<InterfaceListControl2*>(window)->SetListRect(rect.left, rect.top, rect.right, rect.bottom);
            }
            break;
        case 11: {
            if (window->WhatAmI() != kListControl)
                msl_assertion_failed("WhatAmI() == UIOBJ_CREATE_LISTCONTROL", kFile, 0xb1a);
            auto* list = static_cast<InterfaceListControl2*>(window);
            const int16_t count = NextWord(script);
            list->SetColumns(count);
            for (int32_t column = 0; static_cast<int16_t>(column) < count; ++column) {
                const int16_t left = NextWord(script);
                const int16_t right = NextWord(script);
                list->SetColumnPos(static_cast<int16_t>(column), left, right);
            }
            break;
        }
        case 12: {
            if (window->WhatAmI() != kListControl)
                msl_assertion_failed("WhatAmI() == UIOBJ_CREATE_LISTCONTROL", kFile, 0xb2b);
            const int32_t id = NextWord(script) + 0x10;
            const ScriptRect rect = NextRect(script);
            last = CreateChild(window, kChildButton, 1, id, rect, 0xb34);
            ProcessLevel(last, script);
            break;
        }
        case 13: {
            if (window->WhatAmI() != kKnob) msl_assertion_failed("WhatAmI() == UIOBJ_CREATE_KNOB", kFile, 0xb3c);
            int32_t minimum = NextWord(script);
            int32_t maximum = NextWord(script);
            static_cast<InterfaceKnob*>(window)->SetMinMaxAngles(&minimum, &maximum);
            break;
        }
        case 14:
        case 15: {
            // The plus and the minus button of a vary box that has neither.
            const int32_t id = record == 14 ? 1 : 2;
            bool ok = false;
            if (window->WhatAmI() == kVaryBox && window->GetWindow(id) == nullptr) ok = true;
            if (!ok)
                msl_assertion_failed(record == 14 ? "WhatAmI() == UIOBJ_CREATE_VARYBOX && !GetWindow(UIID_VARYBOXPLUSBUT)"
                                                  : "WhatAmI() == UIOBJ_CREATE_VARYBOX && !GetWindow(UIID_VARYBOXMINUSBUT)",
                                     kFile, record == 14 ? 0xab9 : 0xac6);
            const ScriptRect rect = NextRect(script);
            last = CreateChild(window, kChildButton, 0x21, id, rect, record == 14 ? 0xabf : 0xacc);
            ProcessLevel(last, script);
            break;
        }
        case 16: {
            // A light of a light panel: a number below 32 for which the panel has no light yet.
            if (window->WhatAmI() != kLightPanel)
                msl_assertion_failed("WhatAmI() == UIOBJ_CREATE_LIGHTPANEL", kFile, 0xad5);
            const int16_t light = NextWord(script);
            const ScriptRect rect = NextRect(script);
            bool inRange = false;
            bool ok = false;
            if (light >= 0 && light < 0x20) inRange = true;
            if (inRange && window->GetWindow(light | kLightBase) == nullptr) ok = true;
            if (!ok)
                msl_assertion_failed("(lightnum >= 0 && lightnum < 32) && !GetWindow(lightnum | UIID_LIGHTBASE)", kFile, 0xadd);
            last = CreateChild(window, kChildImage, 1, light | kLightBase, rect, 0xae1);
            ProcessLevel(last, script);
            break;
        }
        case 17:
            window->mHelpText = NextLong(script);
            break;
        case 18:
            window->mF90 = NextLong(script);
            break;
        }
    }
    return last;
}

// The text style a draw object is told with a font: the low byte of the font's id (the Mac's byte +8 of the style).
void SetFontStyle(const InterfaceFont* font, DOTextStyle* style) { style->mF06[2] = static_cast<uint8_t>(font->mID); }

}  // namespace

// ---- the draw objects

// The two setters differ in the slot (+0x94 or +0x98), the depth (the window's z or z + 1) and the rectangle (the window's or the
// client's). SetClientDrawObject's release deletes mDrawObject, not the client object (see the header).
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) {
    InterfaceDrawObject* old = mDrawObject;
    if (object == old) return;
    if (old != nullptr) {
        old->mResizable = static_cast<int16_t>(old->mResizable - 1);
        old->Show(0);
        if (static_cast<int16_t>(old->mResizable - (1 - old->mInLibrary)) == -1 && mDrawObject != nullptr) delete mDrawObject;
    }
    mDrawObject = object;
    if (mDrawObject == nullptr) return;
    mDrawObject->mResizable = 1;
    DOChange change{};
    DOTextStyle style{};
    change.mFrame = 0;
    change.mDepth = mZ;
    change.mF08 = mZRot;
    change.mRect = &mRect;
    change.mF0C = mPivotX;
    change.mF0E = mPivotY;
    uint32_t flags = 0x57;
    if (mFont != nullptr) {
        SetFontStyle(mFont, &style);
        change.mText = &style;
        flags |= 0x10000;
    }
    mDrawObject->Change(flags, &change);
    if (GetVisibility() != 0) mDrawObject->Show(1);
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) {
    InterfaceDrawObject* old = mClientDrawObject;
    if (object == old) return;
    if (old != nullptr) {
        old->mResizable = static_cast<int16_t>(old->mResizable - 1);
        old->Show(0);
        if (static_cast<int16_t>(old->mResizable - (1 - old->mInLibrary)) == -1 && mDrawObject != nullptr) delete mDrawObject;
    }
    mClientDrawObject = object;
    if (mClientDrawObject == nullptr) return;
    mClientDrawObject->mResizable = 1;
    DOChange change{};
    DOTextStyle style{};
    change.mFrame = 0;
    change.mDepth = static_cast<int16_t>(mZ + 1);
    change.mF08 = mZRot;
    change.mRect = &mClientRect;
    change.mF0C = mPivotX;
    change.mF0E = mPivotY;
    uint32_t flags = 0x57;
    if (mFont != nullptr) {
        SetFontStyle(mFont, &style);
        change.mText = &style;
        flags |= 0x10000;
    }
    mClientDrawObject->Change(flags, &change);
    if (GetVisibility() != 0) mClientDrawObject->Show(1);
}

// InterfaceWindow::SetFrame (SimTheme Park 2.04b3, 0x101db7dc; the inventory files it under ui_cityLobbyController.cpp): the
// window's picture is chosen by telling its draw objects to show another frame.

// Change's flag 1 carries the frame (a 16-bit number); the window's draw object and its client draw object are each told, when
// the window has them.
void InterfaceWindow::SetFrame(int32_t frame) {
    DOChange change{};
    change.mFrame = static_cast<int16_t>(frame);
    if (mDrawObject != nullptr) mDrawObject->Change(1, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(1, &change);
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

int32_t InterfaceWindow::GetPreviousPosition(CoolRect* rect) {
    if (rect == nullptr) return 0;
    rect->mLeft = mPreviousRect.mLeft;
    rect->mTop = mPreviousRect.mTop;
    rect->mRight = mPreviousRect.mRight;
    rect->mBottom = mPreviousRect.mBottom;
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

int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    if (rect == nullptr) return 0;
    rect->mLeft = mClientRect.mLeft;
    rect->mTop = mClientRect.mTop;
    rect->mRight = mClientRect.mRight;
    rect->mBottom = mClientRect.mBottom;
    return 1;
}

// The interface's message pump (main app): what UIInterface::Process does once a pass. ProcessUI (0x101866c4, ui_main.cpp)
// runs the error display, the timers, the online postcard, the three message queues and the on-frame messages; this file has
// it and everything it reaches that was not translated before: the queues' Post, Peek and GetNumQd and the timed queue's
// Process (UI_Base.cpp's range), the dispatch, onframe and UI_SendMessage, the timers' AddTimer and ServiceAllTimers, the
// window members they use (CallWindowHandler, GetChild, GetNext, GetAttr, AmIGettingDestroyed), UI_GetHighestZ, the postcard
// test and TickUIErrors. The set-up is in UI_init.cpp and InterfaceWindow.cpp; the window handlers, the panel's drawing and
// the dialog are other units' (UI_DialogBox and UI_ActuallyDoPostcard are generated stand-ins in the launcher).

InterfaceWindow* InterfaceWindow::GetChild(int32_t) { return mChild; }

// InterfaceWindow (main app, UI toolkit 0x101dbafc..0x101dd860): the six small members the controls' window handlers call. The
// focus and the pointer capture are two globals (gUIFocusWindow, gUICaptureDevice); the handlers of the button and the knob take
// them on a press and give them back on a release.

InterfaceWindow* InterfaceWindow::GetParent() { return mParent; }

InterfaceWindow* InterfaceWindow::GetNext() { return mNext; }

uint32_t InterfaceWindow::GetAttr() { return mAttributes; }

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

// ui_cityLobbyController.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: InterfaceWindow::FinishWithClientObjectSurface
// (0x101dbd1c, 56 bytes) and InterfaceTimer::SetSlice (0x101d84c8, 8 bytes). Unit v5L16: InterfaceWindow::GetClientObjectSurface
// (0x101dbc98, 64 bytes).

// The client draw object's pixels and size (the virtual at vtable +0x24), 0 without one. The draw objects answer the pixels as a
// 32-bit word: on the host the low half of the address. The surfaces are engine blocks (gMalloc: MacMalloc, GlobalAlloc), which
// come from the arena, and the arena spans less than 4 GB, so the block is the one address from the arena's base up whose low
// half is the word. (Unit v5L28: the word alone, widened, was an address in the unmapped first 4 GB; the ride list's preview
// crashed in its first LbDraw_RectangleFilled.)
uint8_t* InterfaceWindow::GetClientObjectSurface(int32_t* width, int32_t* height) {
    if (mClientDrawObject == nullptr) return nullptr;
    const uint32_t pixels = static_cast<uint32_t>(mClientDrawObject->GetWritableDataArea(width, height));
    if (pixels == 0) return nullptr;
    const uintptr_t base = reinterpret_cast<uintptr_t>(tpw::ArenaBase());
    return reinterpret_cast<uint8_t*>(base + static_cast<uint32_t>(pixels - static_cast<uint32_t>(base)));
}
// Refreshes the client draw object after its pixels were written (the virtual at vtable +0x20).
void InterfaceWindow::FinishWithClientObjectSurface() {
    if (mClientDrawObject != nullptr) mClientDrawObject->Refresh();
}

// ---- the small members

int16_t InterfaceWindow::GetVisibility() {
    int16_t visible = 1;
    for (InterfaceWindow* window = this; window != nullptr && visible != 0; window = window->mParent)
        if ((window->mAttributes & 1) == 0) visible = 0;
    return visible;
}

int32_t InterfaceWindow::GetID() { return mId; }

// The first child with this id, or NULL. (The original takes one more step along the list after finding it; that reads nothing that
// matters.)
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) {
        if (child->mId == id) return child;
    }
    return nullptr;
}

// 0x101dbeb0 (164 bytes).
int32_t InterfaceWindow::GetDims(int16_t* width, int16_t* height, int16_t adjust) {
    mRect.GetDims(width, height);
    if (adjust != 0) {
        if (width != nullptr) *width = static_cast<int16_t>(*width - (mAnimOffset[2] - mAnimOffset[0]));
        if (height != nullptr) *height = static_cast<int16_t>(*height - (mAnimOffset[3] - mAnimOffset[1]));
    }
    return 1;
}

// A window takes part when its attribute bit 1 (visible) is set; a request to hide takes every window, whatever its bit says. A
// part is a visit to both draw objects (Show), the handler (message 0x11 with `shown`, called directly) and then every child, which is
// the same walk. (The Mac unrolls the first three levels of the tree; the order is the same.)
void InterfaceWindow::ShowAllDRAWOBJS(int16_t shown) {
    if (((mAttributes & 1) != 0 && shown != 0) || shown == 0) {
        if (mDrawObject != nullptr) mDrawObject->Show(shown);
        if (mClientDrawObject != nullptr) mClientDrawObject->Show(shown);
        if (mCallback != nullptr) mCallback(this, 0x11, shown, 0);
        for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) child->ShowAllDRAWOBJS(shown);
    }
}

// Any nonzero `on` is 1. A window in the state asked for is left alone. A window that is shown gets its bit set first, and the draw
// objects and the handler hear of it only when this window and all its parents are visible (otherwise it is shown in name only). A
// hidden one is announced to the help text handler after the pointer's window is worked out again.
int32_t InterfaceWindow::Show(int16_t on) {
    const int16_t shown = on != 0 ? 1 : 0;
    const int16_t visible = (mAttributes & 1) == 1 ? 1 : 0;
    if (shown == visible) return 1;
    bool announce = true;
    if (shown == 0) {
        mAttributes &= ~1u;
    } else {
        mAttributes |= 1;
        for (const InterfaceWindow* window = this; window != nullptr && announce; window = window->mParent) {
            if ((window->mAttributes & 1) == 0) announce = false;
        }
    }
    if (announce) ShowAllDRAWOBJS(shown);
    UI_UpdatePointerIn();
    if (shown == 0) gUIHelpText->OnHideOrDelete(this);
    return 1;
}

// The base window's small virtual members (main app, UI toolkit; unit w54): DrawFunction, Enable, operator-, GetValue, GetText and
// GetHelpText. Every window exposes them to the frame through its vtable (0x102a7d94); the button (src/InterfaceButton.cpp) and the
// other controls override some and call these.

// 0x101dc320 (152 bytes). Bit 2 of the attributes is "disabled": Enable(0) sets it, any other value clears it. The answer is 1 when
// the bit changed, else 0 (nothing else is done then). When it changed the window's message function is called DIRECTLY (not through
// UI_SendMessage) with the enable message 0x17 and the short `on` sign-extended, if there is one, and the window under the pointer
// is worked out again.
int16_t InterfaceWindow::Enable(int16_t on) {
    int16_t changed = 0;
    if (on == 0) {
        if ((mAttributes & 2) == 0) {
            changed = 1;
            mAttributes |= 2;
        }
    } else if ((mAttributes & 2) != 0) {
        changed = 1;
        mAttributes &= ~static_cast<uint32_t>(2);
    }
    if (changed != 0) {
        if (mCallback != nullptr) mCallback(this, 0x17, on, 0);
        UI_UpdatePointerIn();
    }
    return changed;
}

// The deepest window under the point: a window that is neither disabled (attribute 2) nor
// being destroyed and is a kind PointIn answers for (attribute 1 or one of 0xf0000000)
// asks its children, in order, and the last that answers wins; then its own region says
// whether the point is in it (asked whether or not a child answered), which makes the
// window itself the answer only when no child did.
InterfaceWindow* InterfaceWindow::PointIn(int16_t x, int16_t y) {
    InterfaceWindow* found = nullptr;
    const uint32_t attributes = mAttributes;
    if ((attributes & 2) == 0 && mDestroying == 0 && ((attributes & 1) != 0 || (attributes & 0xf0000000) != 0)) {
        for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) {
            InterfaceWindow* hit = child->PointIn(x, y);
            if (hit != nullptr) found = hit;
        }
        const bool inside = mRegion->PointIn(x, y);
        if (inside && found == nullptr) found = this;
    }
    return found;
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

// InterfaceWindow::CheckClientMouse, main 0x101dc5d0 (160 bytes), unit w37c.

void InterfaceWindow::CheckClientMouse(int32_t message, int32_t button, int32_t point) {
    if (mClientRect.PointIn(static_cast<int16_t>(static_cast<uint32_t>(point) >> 16), static_cast<int16_t>(point))) {
        const UIButtonState& state = gUIButtons[button];
        UI_PostMessage(this, message | 0x1000, button,
                       static_cast<int32_t>(static_cast<uint32_t>(state.mX) << 16) | state.mY);
    }
}

// The window that has the focus is told it lost it (0x1b) and this one that it got it (0x1a). Nothing happens when this window has it.
// (With no window in focus yet, the first message goes to NULL: UI_PostMessage answers 0 for it.)
void InterfaceWindow::CheckFocus() {
    if (gUIFocusWindow != this) {
        UI_PostMessage(gUIFocusWindow, 0x1b, 0, 0);
        gUIFocusWindow = this;
        UI_PostMessage(gUIFocusWindow, 0x1a, 0, 0);
    }
}

// The control factory and the controls' constructors (main app, UI toolkit): InterfaceWindow::CreateObject (0x101dc750) and
// the constructors of the bar, check buttons, edit box, image, knob, light panel, list box, list control, text box and
// vary box. The button's constructor and members are in src/InterfaceButton.cpp; the scroll bar's belongs to another unit.
//
// Every constructor runs the window's constructor first, stores its own vtable (the compiler's here) and its members, and
// for most of them a window handler. The destructors, the other virtual members and the handlers are untranslated.

// ---- the factory

// Makes a control of the kind `kind` (1 to 13, see the declaration) as a child of this window and links it as the last of
// the children. The kind is tested as an unsigned number, so 0, anything past 13 and every negative number answer NULL and
// make nothing. For a plain window (kind 1) with attribute 0x100 the new window also keeps this one in its word at +0x60.
// (The original skips the constructor and the link steps if operator new answers NULL; the host's throws.)
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                               int16_t right, int16_t bottom) {
    InterfaceWindow* object = nullptr;
    switch (static_cast<uint32_t>(kind)) {
    case 1:
        object = new InterfaceWindow(this, attributes, id, left, top, right, bottom);
        if ((attributes & 0x100) != 0) object->mOwner = this;
        break;
    case 2:
        object = new InterfaceButton(this, attributes, id, left, top, right, bottom);
        break;
    case 3:
        object = new InterfaceScrollBar(this, attributes, id, left, top, right, bottom);
        break;
    case 4:
        object = new InterfaceListBox(this, attributes, id, left, top, right, bottom);
        break;
    case 5:
        object = new InterfaceEditBox(this, attributes, id, left, top, right, bottom);
        break;
    case 6:
        object = new InterfaceImage(this, attributes, id, left, top, right, bottom);
        break;
    case 7:
        object = new InterfaceListControl2(this, attributes, id, left, top, right, bottom);
        break;
    case 8:
        object = new InterfaceCheckButtons(this, attributes, id, left, top, right, bottom);
        break;
    case 9:
        object = new InterfaceBar(this, attributes, id, left, top, right, bottom);
        break;
    case 10:
        object = new InterfaceTextBox(this, attributes, id, left, top, right, bottom);
        break;
    case 11:
        object = new InterfaceKnob(this, attributes, id, left, top, right, bottom);
        break;
    case 12:
        object = new InterfaceVaryBox(this, attributes, id, left, top, right, bottom);
        break;
    case 13:
        object = new InterfaceLightPanel(this, attributes, id, left, top, right, bottom);
        break;
    default:
        return nullptr;
    }
    // the new window goes at the end of the list of children
    if (mChild == nullptr) {
        mChild = object;
        object->mPrevious = nullptr;
    } else {
        InterfaceWindow* last = mChild;
        while (last->mNext != nullptr) last = last->mNext;
        last->mNext = object;
        object->mPrevious = last;
    }
    return object;
}

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    if (callback != nullptr) mCallback = callback;
}

int32_t InterfaceWindow::CaptureDevice() {
    if (gUICaptureDevice == nullptr) {
        gUICaptureDevice = this;
        return 1;
    }
    return 0;
}

// Letting go of the pointer makes the UI work out which window it is over again (UI_UpdatePointerIn).
int32_t InterfaceWindow::UnCaptureDevice() {
    if (gUICaptureDevice == this) {
        gUICaptureDevice = nullptr;
        UI_UpdatePointerIn();
        return 1;
    }
    return 0;
}

void InterfaceWindow::CaptureKb(int32_t on) {
    if (gUIKeyboardWindow == this) return;
    if (gUIKeyboardWindow != nullptr) UI_PostMessage(gUIKeyboardWindow, 0x20, on, 0);
    gUIKeyboardWindow = this;
    UI_PostMessage(gUIKeyboardWindow, 0x1f, on, 0);
}

// 0x101dcdb0 (104 bytes).
void InterfaceWindow::UnCaptureKb(int32_t on) {
    InterfaceWindow* holder = gUIKeyboardWindow;
    if (holder != this) return;
    UI_PostMessage(holder, 0x20, on, 0);
    gUIKeyboardWindow = nullptr;
    UI_UpdatePointerIn();
}

// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

// ---- the window
// 0 when this window holds the keyboard already; else the key list and the window are kept and the answer is 1.
int32_t InterfaceWindow::CaptureKeys(captureKeyList* keys) {
    if (gUIKeyboardWindow == this) return 0;
    gUIKeyCapture.mWindow = this;
    gUIKeyCapture.mKeys = keys;
    return 1;
}

// 0x101dcebc (48 bytes). When this window is the one that captured the keys, both words of the capture are cleared and the answer is
// 1; otherwise nothing changes and the answer is 0. (Nothing reads the answer.)
int32_t InterfaceWindow::UnCaptureKeys() {
    if (gUIKeyCapture.mWindow != this) return 0;
    gUIKeyCapture.mWindow = nullptr;
    gUIKeyCapture.mKeys = nullptr;
    return 1;
}

// 0x101dcf24 (108 bytes). Asks both windows what they are (WhatAmI, vtable slot +0x14, through each one's own vtable: this one first)
// and answers 0. The Mac compares the two answers, but both of its branches answer 0, so the comparison does nothing. (An earlier reading took
// the slot as ClientDraw; the vtable has it at +0x18.)
int32_t InterfaceWindow::operator-(InterfaceWindow& other) {
    WhatAmI();
    other.WhatAmI();
    return 0;
}

// QUIRK: a negative index passes the test (it is compared as a signed number) and writes before the array.
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    if (index >= 16) return 0;
    mExtraParams[index] = value;
    return 1;
}

// unit w43: main 0x101dd030, 36 bytes. Negative indices address the preceding words, as in setExtraParam.
void* InterfaceWindow::getExtraParam(int16_t index) {
    if (index >= 16) return nullptr;
    return mExtraParams[index];
}

// InterfaceWindow::setPivot (main app, SimTheme Park 2.04b3, 0x101ddd34, 440 bytes), startAnim (0x101dd08c, 284 bytes) and
// startAnimTimer (0x101dd6b8, 100 bytes); the inventory files them under ui_cityLobbyController.cpp. Unit w91.

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

// Starts the animation timer with this number: 0 to 3, or its timer id 0x80080 to 0x80083 (the id has the two bits of 0x80080 set and
// loses that value). QUIRK: no bounds check, any other number reads past the array. The answer is whether the timer exists.
int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    if ((which & kAnimTimerId) == kAnimTimerId) which -= kAnimTimerId;
    InterfaceTimer* timer = mAnimTimers[which];
    if (timer != nullptr) timer->Start();
    return timer != nullptr;
}

// The id of a timer is the plain number or the number plus 0x80080 (both bits of the mask set); no range test (as the Mac).
int32_t InterfaceWindow::stopAnimTimer(int32_t id) {
    if ((id & 0x80080) == 0x80080) id -= 0x80080;
    InterfaceTimer* timer = mAnimTimers[id];
    if (timer == nullptr) return 0;
    timer->Stop();
    return 1;
}

// InterfaceWindow::setCursor (main 0x101dd7f0, 60 B).

void InterfaceWindow::setCursor(int16_t cursor) {
    mCursor = cursor;
    if (this == gUIPointerIn) UI_SetCursor(mCursor);
}

void InterfaceWindow::cursorChange() { UI_SetCursor(mCursor); }

// The Mac unrolls three levels (the window, its children with z + 2, theirs with z + 4) and calls setZ for the windows below those with
// z + 6, each of which starts the same walk again. The depths are 16-bit: the additions wrap.
void InterfaceWindow::setZ(int16_t z) {
    PlaceDepth(this, z);
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) {
        PlaceDepth(child, static_cast<int16_t>(z + 2));
        for (InterfaceWindow* grandchild = child->mChild; grandchild != nullptr; grandchild = grandchild->mNext) {
            PlaceDepth(grandchild, static_cast<int16_t>(z + 4));
            for (InterfaceWindow* deeper = grandchild->mChild; deeper != nullptr; deeper = deeper->mNext)
                deeper->setZ(static_cast<int16_t>(z + 6));
        }
    }
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

// Unit v5981: InterfaceWindow::link (0x101ddf20), the island lobby controller's static initialiser (0x101e1e3c), the online login panel's
// message function (0x101e26fc), the outbox address panel's message function (0x101e4b00) and the JPEG loader (0x101e538c).

// 0x101ddf20: makes `child` the LAST child of this window. Nothing happens (0) when `child` is NULL or has a parent or siblings already.
// The flags a and b (the Mac's chars, tested as signed bytes) give the child the parent's depth plus 2 and the parent's pivot. Then the
// pointer is updated, and a window that held the keyboard, when it is this one, loses it (message 0x20) and the pointer is updated again.
void InterfaceWindow::link(InterfaceWindow* child, char a, char b) { linkChecked(child, a, b); }

// InterfaceWindow::unlink (main 0x101de084, 124 B): remove from the sibling list, then update the pointer.

void InterfaceWindow::unlink() {
    if (mParent != nullptr && mPrevious == nullptr) mParent->mChild = mNext;
    if (mPrevious != nullptr) mPrevious->mNext = mNext;
    if (mNext != nullptr) mNext->mPrevious = mPrevious;
    mNext = nullptr;
    mPrevious = nullptr;
    mParent = nullptr;
    UI_UpdatePointerIn();
}

void InterfaceWindow::SetClientDrawText(char16_t* text) {
    if (mClientDrawObject == nullptr) return;
    DOTextStyle style{};
    style.mText = text;
    ChangeBoth(nullptr, mClientDrawObject, kChangeText, &style);
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

// The client draw object is told first, then the main one.
int32_t InterfaceWindow::SetText(char16_t* text) {
    DOChange change{};
    DOTextStyle style{};
    style.mText = text;
    change.mText = &style;
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(kChangeText, &change);
    if (mDrawObject != nullptr) mDrawObject->Change(kChangeText, &change);
    return 1;
}

// 0x101de318 (40 bytes): the handler at +0x114 is called through a function pointer with the window and the three arguments, as
// they came; there is no test for NULL (a window always has one: the constructor sets it).
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) { return mDefaultCallback(this, message, a, b); }

// UI_SendMessage 0x101c9518 (164 bytes) and CallWindowHandler 0x101de384 (56); existing bodies, main, unit w43.

int32_t InterfaceWindow::CallWindowHandler(int32_t message, int32_t a, int32_t b) {
    if (mCallback != nullptr) return mCallback(this, message, a, b);
    return -1;
}

// ---- Build and ProcessScript

// Makes the windows of a script under this one (the callback goes to the first window the script made at its top level).
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    gUIScriptPosition = 0;
    InterfaceWindow* window = ProcessScript(description);
    if (window != nullptr && callback != nullptr) window->SetCallbackFunction(callback);
    return window;
}

InterfaceWindow* InterfaceWindow::ProcessScript(int16_t* script) { return ProcessLevel(this, script); }

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

// 0x101dff2c (8 bytes): 0.
int32_t InterfaceWindow::GetValue() { return 0; }

// UIMenu::Init (main app 0x1018ad20, 260 bytes) and UIMenu::AddItem (0x1018ae78, 572 bytes; the inventory files them under ui_utils.cpp;
// unit v5163): Init makes the menu's full-screen window and hooks the handlers; AddItem adds a text button.
//
// The word at 0 (mF00) holds the item handler (a function pointer on the Mac). The host's pointers are wider than the word, so the
// word holds a handle that UIMenuHandlerHandle makes of the pointer (0 for NULL), see the end of this file.
// The callees that the host cannot call as the Mac does are the menu_callee functions below (weak: the test library defines its own).

// The callees the host cannot call as the Mac does: the program's versions here (weak), the test library's fakes in
// tests/unit_UIMenu_AddItem.cpp replace them.
namespace menu_callee {
// 0x101d7b60: InterfaceStringHeap::GetAndCopy (a stand-in on the Mac side, no translation yet: the program's stand-in answers the text).
__attribute__((weak)) void* GetAndCopy(const char16_t* text) { return const_cast<char16_t*>(text); }
}  // namespace menu_callee

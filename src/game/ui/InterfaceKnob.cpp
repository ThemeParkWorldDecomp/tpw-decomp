// InterfaceKnob.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceKnob code, named after it.

#include "email.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_LoadSave.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "tb_ezsprite.h"
#include "tb_starfield.h"
#include "TbTList.h"
#include "TPMain.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "UI_pump.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

// The control factory and the controls' constructors (main app, UI toolkit): InterfaceWindow::CreateObject (0x101dc750) and
// the constructors of the bar, check buttons, edit box, image, knob, light panel, list box, list control, text box and
// vary box. The button's constructor and members are in src/InterfaceButton.cpp; the scroll bar's belongs to another unit.
//
// Every constructor runs the window's constructor first, stores its own vtable (the compiler's here) and its members, and
// for most of them a window handler. The destructors, the other virtual members and the handlers are untranslated.

InterfaceKnob::InterfaceKnob(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                             int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mDragAngle = 0;
    mAngle = 0;
    mMinAngle = 0;
    mMaxAngle = 0xfff;
    mMinValue = 0;
    mMaxValue = 1;
    mValue = 0;
    mOldValue = 0;
    mAngleRange = mMaxAngle - mMinAngle;
    mValueRange = mMaxValue - mMinValue;
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    mDragging = 0;
    SetCallbackFunction(UI_DefaultKnobCallback);
}

// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

// ---- the knob
// The Mac's destructor stores the vtable and calls the base destructor (then deletes when the hidden flag is above 0: the
// compiler's deleting destructor). Also the key function of the knob's vtable.
InterfaceKnob::~InterfaceKnob() {}

// The window classes' WhatAmI members, the base window's CalcClientArea and CallDefaultWindowHandler (main app, UI toolkit; unit w36).
// WhatAmI is the interface's run time type: each class answers its own kind number, the one InterfaceWindow::CreateObject
// (src/ui_controls_create.cpp) takes to make it and the window scripts' records test with (src/InterfaceWindow_script.cpp, whose
// records 6 to 16 assert on the answer). The button's and the scroll bar's are in their own files (InterfaceButton.cpp,
// InterfaceScrollBar.cpp); the kinds are the enum in include/ui_controls.h.

int32_t InterfaceKnob::WhatAmI() { return kUIKindKnob; }                  // 0x101d0228

// The interface's control classes: the setters the window script calls (main app, unit w31a). InterfaceKnob::SetMinMaxAngles,
// InterfaceListBox::SetListRect, InterfaceListControl2's SetListRect, SetColumns and SetColumnPos, and the constructors of the
// three region shapes a window can be given (CoolCircle, CoolEllipse, CoolPoly; src/ui_cool_shapes.cpp has the rest of them).

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

int32_t InterfaceKnob::SetValue(int32_t value) {
    ChangeValue(value);
    mDragAngle = mAngle;
    return 1;
}

int32_t InterfaceKnob::GetValue() { return mValue; }

// Unit v51047: std::basic_istream<char>::basic_istream 0x1020a4c0, CUIGrapher::Clear 0x101fd9d0 and its constructor 0x101fd6e8,
// CFSLoadSave::GetNextSaveFileDetails 0x101fcd08, CUI2DMapSprite::~CUI2DMapSprite 0x10230ea4, SetVisibility 0x10230f78 and SetScale
// 0x10231094, the program entry 0x102381a4 and InterfaceKnob::operator- 0x101d038c.

// ---- the program entry
void entry() {
    RuntimeInit();
    ThemeParkWorldMain();
    RuntimeExit(0);
}

// ---- InterfaceKnob::operator- 0x101d038c: the difference of the values when both controls are of one kind, else 0.
int32_t InterfaceKnob::operator-(InterfaceWindow& other) {
    int32_t mine = WhatAmI();
    int32_t theirs = other.WhatAmI();
    if (theirs == mine) return mValue - static_cast<InterfaceKnob&>(other).mValue;
    return 0;
}

// Sets the value: cut to the value range (the largest first), nothing more when it is the one the knob has. Otherwise the old value
// is kept, the displayed angle follows from the value (ends to ends, else the smallest angle plus the angle range times the value
// above the smallest over the value range, a 32-bit signed division), the client draw object (if any) is told the angle with change
// flag 4 and the parent hears 0x800 with the knob's id and the value.
void InterfaceKnob::ChangeValue(int32_t value) {
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

// InterfaceKnob (main app, UI toolkit 0x101d0574..0x101d0718): the two members that turn the knob. The handler that drives them
// (UI_DefaultKnobCallback, 0x101d07ac) is in src/ui_handlers_small.cpp.
//
// The knob keeps two angles: mDragAngle, which the pointer moves (8 units for each unit it goes up the screen) between the smallest
// and the largest angle, and mAngle, which is the angle the value maps back to (so the displayed angle snaps to the values). The
// value is the drag angle's share of the value range.

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

// The window handlers of five controls (main app, UI toolkit): the button (0x101cb5f0), the check buttons (0x101cc27c), the knob
// (0x101d07ac), the text box (0x101d8134) and the vary box (0x101d9674). The constructors store them as the window's message
// function (and, for the button and the check buttons, as its default one). Each handles a few messages and passes the rest to the
// window's own handler, UI_DefaultWindowCallback, whose answer it gives back.
//
// The messages the pump sends (FINDINGS 2026-10-11 w21/w29/w30): 0x10001 the pointer came into the window, 0x10002 it left,
// 0x10003 it moved (x in the high half of `a`, y in the low), 0x10004 mouse button `a` went up, 0x10005 went down (`b` is the
// point), 0x1e a frame, 0x15 the window was made. Messages 0x100 (a click), 0x10008 and 0x10009 are the ones a button sends to
// its parent (see InterfaceButton), 0x800 the value of a control changed.

// ---- the knob

// The left button's press takes the focus whatever the button (0x10005) and, for the left one, starts the drag: the pointer is
// captured and its y (the low half of `b`) kept twice. The release (0x10004) of the left button ends the drag and lets go of the
// pointer. Both answer 0 and neither reaches the default handler. A move (0x10003) turns the knob (DoKnobMove), then goes on.
int32_t UI_DefaultKnobCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    InterfaceKnob* knob = static_cast<InterfaceKnob*>(window);
    if (message == 0x10004) {
        if (a == 0) {
            knob->mDragging = 0;
            window->UnCaptureDevice();
        }
        return 0;
    }
    if (message == 0x10005) {
        window->CheckFocus();
        if (a == 0) {
            knob->mDragging = 1;
            knob->mLastY = static_cast<int16_t>(b);
            knob->mPreviousY = knob->mLastY;
            window->CaptureDevice();
        }
        return 0;
    }
    if (message == 0x10003) knob->DoKnobMove(a);
    return UI_DefaultWindowCallback(window, message, a, b);
}

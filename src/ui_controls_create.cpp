// The control factory and the controls' constructors (main app, UI toolkit): InterfaceWindow::CreateObject (0x101dc750) and
// the constructors of the bar, check buttons, edit box, image, knob, light panel, list box, list control, text box and
// vary box. The button's constructor and members are in src/InterfaceButton.cpp; the scroll bar's belongs to another unit.
//
// Every constructor runs the window's constructor first, stores its own vtable (the compiler's here) and its members, and
// for most of them a window handler. The destructors, the other virtual members and the handlers are untranslated.
#include <cstring>

#include "ui_controls.h"

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

// ---- the constructors. The scroll bar that the two lists make for attribute 0x1000 is a child at the right of the client
// rectangle (4 to the right of it, as high as the window and out to its edge), made with CreateObject, switched off, shown
// and given the range 0 to 0; the list box adds 0x80 to the bar's attributes, the list control does not.

InterfaceBar::InterfaceBar(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                           int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mValue = 0;
    mF13C = 0;
    mF138 = 0;
    // the client rectangle is made the window's rectangle, and the bar's rectangle the client rectangle
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    mBarRect.mLeft = mClientRect.mLeft;
    mBarRect.mTop = mClientRect.mTop;
    mBarRect.mRight = mClientRect.mRight;
    mBarRect.mBottom = mClientRect.mBottom;
}

InterfaceCheckButtons::InterfaceCheckButtons(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left,
                                             int16_t top, int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mCurrent = -1;
    mCallback = UI_DefaultCheckButtonsCallback;
    mDefaultCallback = UI_DefaultCheckButtonsCallback;
}

// With attribute 0x40 the box has a text buffer of 256 characters holding the empty text; without it no buffer and a length
// of 0. The cursor's timer is 750 ms with the id 0x75e88. The edit box takes the keyboard capture away (CaptureKb(0)) and
// remembers whether the UI is right to left.
InterfaceEditBox::InterfaceEditBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                   int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mMaxLength = 0;
    if ((attributes & 0x40) != 0) {
        mBuffer = new char16_t[0x100];
        mBuffer[0] = 0;
        mMaxLength = 0xff;
    } else {
        mBuffer = nullptr;
        mMaxLength = 0;
    }
    CalcClientArea();
    mTimer = UI_AddTimer(this, 750, 0x75e88);
    mF14A = -1;
    mF156 = 0;
    mF154 = 0;
    mF152 = 0;
    mF150 = 0;
    mF158 = 0;
    mF14C = -1;
    mF140 = 0;
    mF144 = 0;
    mF148 = 0;
    mF14E = 0;
    mF15C = 0;
    mF161 = 0;
    SetCallbackFunction(UI_DefaultEditBoxCallback);
    mDefaultCallback = UI_DefaultEditBoxCallback;
    CaptureKb(0);
    mF160 = 1;
    mRightToLeft = 0;
    if (gUIHebrew) mRightToLeft = 1;
}

InterfaceImage::InterfaceImage(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                               int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {}

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

InterfaceLightPanel::InterfaceLightPanel(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                         int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mValue = 0;
}

InterfaceListBox::InterfaceListBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                   int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mF148 = -1;
    int32_t barAttributes = 0x10;
    mF144 = 0;
    mF154 = 0;
    mF156 = 0;
    mF150 = 0;
    mF14C = 0;
    CalcClientArea();
    if ((attributes & 0x1000) != 0) {
        if ((attributes & 0x40) != 0) barAttributes |= 0x40;
        InterfaceWindow* bar = CreateObject(3, barAttributes | 0x80, 1, static_cast<int16_t>(mClientRect.mRight + 4),
                                            mRect.mTop, mRect.mRight, mRect.mBottom);
        if (bar != nullptr) {
            bar->Enable(0);
            bar->Show(1);
            static_cast<InterfaceScrollBar*>(bar)->SetMinMax(0, 0);
        }
    }
    SetCallbackFunction(UI_DefaultListBoxCallback);
}

InterfaceListControl2::InterfaceListControl2(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left,
                                             int16_t top, int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mCurrentEntry = -1;
    int32_t barAttributes = 0x10;
    mF144 = 0;
    mF148 = 0;
    mF174 = 0;
    mRowHeight = 0;
    mVisibleRows = 0;
    mTopEntry = 0;
    mItemCount = 0;
    mBlockWords = 0;
    mFirstEntry = -1;
    mColumnCount = 0;
    mF166 = 0;
    mColumns = nullptr;
    mSortColumn = 0;
    mSortDescending = 0;
    CalcClientArea();
    if ((attributes & 0x1000) != 0) {
        if ((attributes & 0x40) != 0) barAttributes |= 0x40;
        InterfaceWindow* bar = CreateObject(3, barAttributes, 1, static_cast<int16_t>(mClientRect.mRight + 4), mRect.mTop,
                                            mRect.mRight, mRect.mBottom);
        if (bar != nullptr) {
            bar->Enable(0);
            bar->Show(1);
            static_cast<InterfaceScrollBar*>(bar)->SetMinMax(0, 0);
        }
    }
    mCallback = UI_DefaultListControl2Callback;
    mDefaultCallback = UI_DefaultListControl2Callback;
}

InterfaceTextBox::InterfaceTextBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                   int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mTextLength = 0;
    mText = nullptr;
    mHorizontal = 0;
    mVertical = 0;
    mF144 = 0;
    mTextX = 0;
    mTextY = 0;
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    SetCallbackFunction(UI_DefaultTextBoxCallback);
}

InterfaceVaryBox::InterfaceVaryBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                   int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mMinValue = 0;
    mMaxValue = 1;
    mValue = 0x9708cd;
    mOldValue = 0;
    mIncrement = 1;
    mForceUpdate = 1;
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    SetCallbackFunction(UI_DefaultVaryBoxCallback);
}

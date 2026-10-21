// InterfaceTextBox.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceTextBox code, named after it.

#include "FS_Online.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "platform/msl.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "UI_pump.h"
#include <cstddef>
#include <cstring>
#include <new>

// The control factory and the controls' constructors (main app, UI toolkit): InterfaceWindow::CreateObject (0x101dc750) and
// the constructors of the bar, check buttons, edit box, image, knob, light panel, list box, list control, text box and
// vary box. The button's constructor and members are in src/InterfaceButton.cpp; the scroll bar's belongs to another unit.
//
// Every constructor runs the window's constructor first, stores its own vtable (the compiler's here) and its members, and
// for most of them a window handler. The destructors, the other virtual members and the handlers are untranslated.

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

// The destructors of the bar and the text box (main app, UI toolkit; unit w54). Both are called by ~InterfaceWindow's
// `while (mChild) delete mChild`. The compiler supplies what the Mac's versions do around the body: the vtable store at +0x118 (0x102a7c78
// for the bar, 0x102afd14 for the text box), the base destructor (~InterfaceWindow, unit w51) and, for the deleting form (the SHORT in r4
// positive), the operator delete of `this`. The Mac skips everything for a NULL `this`, which the host never has.

// 0x101d7cc8 (124 bytes). Frees the text block (+0x138) if there is one, with the plain operator delete (the block is made with operator
// new[] on the Mac by SetText, 0x101d7f50, which is not translated; the host takes the same call so that the heap traffic is the
// Mac's). The pointer is not cleared.
InterfaceTextBox::~InterfaceTextBox() {
    if (mText != nullptr) operator delete(mText);
}

// The window classes' WhatAmI members, the base window's CalcClientArea and CallDefaultWindowHandler (main app, UI toolkit; unit w36).
// WhatAmI is the interface's run time type: each class answers its own kind number, the one InterfaceWindow::CreateObject
// (src/ui_controls_create.cpp) takes to make it and the window scripts' records test with (src/InterfaceWindow_script.cpp, whose
// records 6 to 16 assert on the answer). The button's and the scroll bar's are in their own files (InterfaceButton.cpp,
// InterfaceScrollBar.cpp); the kinds are the enum in include/ui_controls.h.

int32_t InterfaceTextBox::WhatAmI() { return kUIKindTextBox; }            // 0x101d7d74

// Unit v5975: InterfaceTextBox::CalcTextPos 0x101d7dac, InterfaceTextBox::SetText 0x101d7f50, InterfaceStringHeap::GetString 0x101d79ec,
// InterfaceStringHeap::GetAndCopy 0x101d7b60, InterfaceTimer::OperatorDelete 0x101d822c (the class's operator delete).

__attribute__((weak)) void* textbox_callee::NewArray(uint32_t size) { return ::operator new[](static_cast<std::size_t>(size)); }
__attribute__((weak)) void textbox_callee::Delete(void* block) { ::operator delete(block); }

void InterfaceTextBox::CalcTextPos() {
    if (mFont == nullptr || mText == nullptr) return;
    if (mHorizontal == 1) {
        int32_t length = static_cast<int32_t>(wcslen(mText));
        int32_t width = mFont->GetWidth(mText, 0, static_cast<int16_t>(length - 1));
        mTextX = static_cast<int16_t>(mClientRect.mRight - width);
    } else if (mHorizontal == 2) {
        int32_t left = mClientRect.mLeft;
        int32_t length = static_cast<int32_t>(wcslen(mText));
        int32_t width = static_cast<int16_t>(mFont->GetWidth(mText, 0, static_cast<int16_t>(length - 1)));
        mTextX = static_cast<int16_t>(left + ((mClientRect.mRight - left - width) >> 1));
    } else {
        mTextX = mClientRect.mLeft;
    }
    if (mVertical == 1) {
        int32_t height = mFont->GetHeight();
        mTextY = static_cast<int16_t>(mClientRect.mBottom - height);
    } else if (mVertical == 2) {
        int32_t top = mClientRect.mTop;
        int32_t height = static_cast<int16_t>(mFont->GetHeight());
        mTextY = static_cast<int16_t>(top + ((mClientRect.mBottom - top - height) >> 1));
    } else {
        mTextY = mClientRect.mTop;
    }
}

int32_t InterfaceTextBox::SetText(char16_t* text) {
    int32_t length = static_cast<int32_t>(wcslen(text));
    if (length > mTextLength) {
        if (mText != nullptr) textbox_callee::Delete(mText);
        mText = static_cast<char16_t*>(textbox_callee::NewArray(static_cast<uint32_t>(length + 1) << 1));
        mTextLength = length;
    }
    if (mText == nullptr) return 0;
    wcscpy(mText, text);
    CalcTextPos();
    return 1;
}

void InterfaceTimer::OperatorDelete(void* object) {
    InterfaceTimer* slot = static_cast<InterfaceTimer*>(object);
    bool inside = false;
    if (slot != nullptr && gUITimers != nullptr && slot >= gUITimers && slot < gUITimers + gUITimerCount) inside = true;
    if (!inside)
        msl_assertion_failed("p_mem && pUITimerFreeList && (p_mem >= pUITimerFreeList) && (p_mem < pUITimerFreeList+UITimerFreeListSize)",
                             "ui_timers.cpp", 0x48);
    slot->mFree = 1;
}

// InterfaceTextBox::operator- (0x101d8034), main app.

// Compares two windows: 0 when their kinds (WhatAmI, slot +0x14) differ, else wcscmp of the two text blocks (the other one is read
// at +0x138 whatever it is, as the original does).
int32_t InterfaceTextBox::operator-(InterfaceWindow& other) {
    int32_t mine = WhatAmI();
    int32_t theirs = other.WhatAmI();
    if (theirs == mine) return wcscmp(mText, static_cast<InterfaceTextBox&>(other).mText);
    return 0;
}

// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

// ---- the text box (also the key function of its vtable)
void InterfaceTextBox::DrawFunction() {}

// The window handlers of five controls (main app, UI toolkit): the button (0x101cb5f0), the check buttons (0x101cc27c), the knob
// (0x101d07ac), the text box (0x101d8134) and the vary box (0x101d9674). The constructors store them as the window's message
// function (and, for the button and the check buttons, as its default one). Each handles a few messages and passes the rest to the
// window's own handler, UI_DefaultWindowCallback, whose answer it gives back.
//
// The messages the pump sends (FINDINGS 2026-10-11 w21/w29/w30): 0x10001 the pointer came into the window, 0x10002 it left,
// 0x10003 it moved (x in the high half of `a`, y in the low), 0x10004 mouse button `a` went up, 0x10005 went down (`b` is the
// point), 0x1e a frame, 0x15 the window was made. Messages 0x100 (a click), 0x10008 and 0x10009 are the ones a button sends to
// its parent (see InterfaceButton), 0x800 the value of a control changed.

// ---- the text box

int32_t UI_DefaultTextBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    return UI_DefaultWindowCallback(window, message, a, b);
}

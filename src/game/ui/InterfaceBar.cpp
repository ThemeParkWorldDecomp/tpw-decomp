// InterfaceBar.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceBar code, named after it.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "Clock.h"
#include "FS_Online.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "ParkTime.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
#include "TPSound.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include "WorldThings.h"
#include <cstdint>
#include <cstring>
#include <new>

// The control factory and the controls' constructors (main app, UI toolkit): InterfaceWindow::CreateObject (0x101dc750) and
// the constructors of the bar, check buttons, edit box, image, knob, light panel, list box, list control, text box and
// vary box. The button's constructor and members are in src/InterfaceButton.cpp; the scroll bar's belongs to another unit.
//
// Every constructor runs the window's constructor first, stores its own vtable (the compiler's here) and its members, and
// for most of them a window handler. The destructors, the other virtual members and the handlers are untranslated.

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

// The destructors of the bar and the text box (main app, UI toolkit; unit w54). Both are called by ~InterfaceWindow's
// `while (mChild) delete mChild`. The compiler supplies what the Mac's versions do around the body: the vtable store at +0x118 (0x102a7c78
// for the bar, 0x102afd14 for the text box), the base destructor (~InterfaceWindow, unit w51) and, for the deleting form (the SHORT in r4
// positive), the operator delete of `this`. The Mac skips everything for a NULL `this`, which the host never has.

// 0x101c87a4 (120 bytes). The Mac's body is the bar's rectangle member (+0x148) destroyed (CoolRect::~CoolRect, translated, nothing
// to do on the host) before the base class; nothing else.
InterfaceBar::~InterfaceBar() {}

// The window classes' WhatAmI members, the base window's CalcClientArea and CallDefaultWindowHandler (main app, UI toolkit; unit w36).
// WhatAmI is the interface's run time type: each class answers its own kind number, the one InterfaceWindow::CreateObject
// (src/ui_controls_create.cpp) takes to make it and the window scripts' records test with (src/InterfaceWindow_script.cpp, whose
// records 6 to 16 assert on the answer). The button's and the scroll bar's are in their own files (InterfaceButton.cpp,
// InterfaceScrollBar.cpp); the kinds are the enum in include/ui_controls.h.

int32_t InterfaceBar::WhatAmI() { return kUIKindBar; }                    // 0x101c8848

// Unit v51048: eight small functions of the main app (Game.cpp, ActionRecorder.cpp, ui_visitorStatus.cpp's list instance, Peep.cpp's park time,
// UI_Base.cpp's bar and deferred message).

// 0x101c887c (188 bytes): a changed value is clamped to 0 .. 0x400 and told to the draw object and the client draw object (Change, flags 8, a
// block whose first word is the value). 1 when the value changed, else 0.
int32_t InterfaceBar::SetValue(int32_t value) {
    if (value == mValue) return 0;
    mValue = value;
    if (mValue > 0x400) {
        mValue = 0x400;
    } else if (mValue < 0) {
        mValue = 0;
    }
    DOChange change;
    std::memcpy(change.mF00, &mValue, sizeof(int32_t));
    if (mDrawObject != nullptr) mDrawObject->Change(8, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(8, &change);
    return 1;
}

// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

// ---- the bar
int32_t InterfaceBar::GetValue() { return mValue; }

// 0x101c89a0 (128 bytes): the difference of the two values when both windows say they are the same kind (this one is asked first), else 0.
// The other window is read as a bar whatever it is.
int32_t InterfaceBar::operator-(InterfaceWindow& other) {
    const int32_t mine = WhatAmI();
    const int32_t theirs = other.WhatAmI();
    if (theirs != mine) return 0;
    return mValue - static_cast<InterfaceBar&>(other).mValue;
}

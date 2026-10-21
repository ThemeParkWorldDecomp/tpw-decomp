// InterfaceImage.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceImage code, named after it.

#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "Light.h"
#include "platform/arena.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "UI_pump.h"
#include <cstdint>
#include <cstring>
#include <new>

// The control factory and the controls' constructors (main app, UI toolkit): InterfaceWindow::CreateObject (0x101dc750) and
// the constructors of the bar, check buttons, edit box, image, knob, light panel, list box, list control, text box and
// vary box. The button's constructor and members are in src/InterfaceButton.cpp; the scroll bar's belongs to another unit.
//
// Every constructor runs the window's constructor first, stores its own vtable (the compiler's here) and its members, and
// for most of them a window handler. The destructors, the other virtual members and the handlers are untranslated.

InterfaceImage::InterfaceImage(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                               int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {}

// The destructor of the image control (main app, UI toolkit; unit w75; the inventory files it under ui_cityLobbyController.cpp). The
// compiler supplies what the Mac's version does around the body: the vtable store at +0x118 (0x102afaec), the base destructor
// (~InterfaceWindow, unit w51) and, for the deleting form (the SHORT in r4 positive), the operator delete of `this`. The Mac skips
// everything for a NULL `this`, which the host never has. The image has no members to release (mF138 and mValue are plain words).

// 0x101cfebc (104 bytes).
InterfaceImage::~InterfaceImage() {}

// The window classes' WhatAmI members, the base window's CalcClientArea and CallDefaultWindowHandler (main app, UI toolkit; unit w36).
// WhatAmI is the interface's run time type: each class answers its own kind number, the one InterfaceWindow::CreateObject
// (src/ui_controls_create.cpp) takes to make it and the window scripts' records test with (src/InterfaceWindow_script.cpp, whose
// records 6 to 16 assert on the answer). The button's and the scroll bar's are in their own files (InterfaceButton.cpp,
// InterfaceScrollBar.cpp); the kinds are the enum in include/ui_controls.h.

int32_t InterfaceImage::WhatAmI() { return kUIKindImage; }                // 0x101cff50

// 0x101cff88: stores the value, tells both draw objects (Change, flag 1, frame = the low 16 bits), then the window.
int32_t InterfaceImage::SetValue(int32_t value) {
    mValue = value;
    DOChange change;
    change.mFrame = static_cast<int16_t>(value);
    if (mDrawObject != nullptr) mDrawObject->Change(1, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(1, &change);
    InterfaceWindow::SetValue(value);
    return 1;
}

// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

// ---- the image
int32_t InterfaceImage::GetValue() { return mValue; }

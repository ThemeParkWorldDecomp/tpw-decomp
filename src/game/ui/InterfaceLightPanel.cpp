// InterfaceLightPanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceLightPanel code, named after it.

#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "UI_pump.h"
#include <cstring>

// The control factory and the controls' constructors (main app, UI toolkit): InterfaceWindow::CreateObject (0x101dc750) and
// the constructors of the bar, check buttons, edit box, image, knob, light panel, list box, list control, text box and
// vary box. The button's constructor and members are in src/InterfaceButton.cpp; the scroll bar's belongs to another unit.
//
// Every constructor runs the window's constructor first, stores its own vtable (the compiler's here) and its members, and
// for most of them a window handler. The destructors, the other virtual members and the handlers are untranslated.

InterfaceLightPanel::InterfaceLightPanel(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                         int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mValue = 0;
}

// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

// ---- the light panel
InterfaceLightPanel::~InterfaceLightPanel() {}   // key function of the light panel's vtable

// The window classes' WhatAmI members, the base window's CalcClientArea and CallDefaultWindowHandler (main app, UI toolkit; unit w36).
// WhatAmI is the interface's run time type: each class answers its own kind number, the one InterfaceWindow::CreateObject
// (src/ui_controls_create.cpp) takes to make it and the window scripts' records test with (src/InterfaceWindow_script.cpp, whose
// records 6 to 16 assert on the answer). The button's and the scroll bar's are in their own files (InterfaceButton.cpp,
// InterfaceScrollBar.cpp); the kinds are the enum in include/ui_controls.h.

int32_t InterfaceLightPanel::WhatAmI() { return kUIKindLightPanel; }      // 0x101d0a14

// 0x101d0a50: toggles the frame of the child windows 0x11100000 + bit for every bit (0..30) that changed, stores the value.
int32_t InterfaceLightPanel::SetValue(int32_t value) {
    for (int32_t i = 0; i < 31; i++) {
        int32_t bit = 1 << i;
        if (mValue & bit) {
            if (!(value & bit)) {
                InterfaceWindow* w = GetWindow(i | 0x11100000);
                if (w) w->SetFrame(0);
            }
        } else if (value & bit) {
            InterfaceWindow* w = GetWindow(i | 0x11100000);
            if (w) w->SetFrame(1);
        }
    }
    mValue = value;
    InterfaceWindow::SetValue(value);
    return 1;
}

int32_t InterfaceLightPanel::GetValue() { return mValue; }

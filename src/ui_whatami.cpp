// The window classes' WhatAmI members, the base window's CalcClientArea and CallDefaultWindowHandler (main app, UI toolkit; unit w36).
// WhatAmI is the interface's run time type: each class answers its own kind number, the one InterfaceWindow::CreateObject
// (src/ui_controls_create.cpp) takes to make it and the window scripts' records test with (src/InterfaceWindow_script.cpp, whose
// records 6 to 16 assert on the answer). The button's and the scroll bar's are in their own files (InterfaceButton.cpp,
// InterfaceScrollBar.cpp); the kinds are the enum in include/ui_controls.h.
#include "ui_controls.h"

// 0x101d9eb0: the plain window.
int32_t InterfaceWindow::WhatAmI() { return kUIKindWindow; }

// 0x101d987c (36 bytes): the client rectangle is the window's rectangle. The classes whose client area is smaller (the list box, the
// edit box, the list control, the scroll bar) have their own, and the window's constructor reaches this one through the base class.
void InterfaceWindow::CalcClientArea() {
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
}

// 0x101de318 (40 bytes): the handler at +0x114 is called through a function pointer with the window and the three arguments, as
// they came; there is no test for NULL (a window always has one: the constructor sets it).
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) { return mDefaultCallback(this, message, a, b); }

int32_t InterfaceBar::WhatAmI() { return kUIKindBar; }                    // 0x101c8848
int32_t InterfaceCheckButtons::WhatAmI() { return kUIKindCheckButtons; }  // 0x101cc0c8
int32_t InterfaceEditBox::WhatAmI() { return kUIKindEditBox; }            // 0x101cd7a8
int32_t InterfaceImage::WhatAmI() { return kUIKindImage; }                // 0x101cff50
int32_t InterfaceKnob::WhatAmI() { return kUIKindKnob; }                  // 0x101d0228
int32_t InterfaceLightPanel::WhatAmI() { return kUIKindLightPanel; }      // 0x101d0a14
int32_t InterfaceListBox::WhatAmI() { return kUIKindListBox; }            // 0x101d0e78
int32_t InterfaceListControl2::WhatAmI() { return kUIKindListControl; }   // 0x101d1918
int32_t InterfaceTextBox::WhatAmI() { return kUIKindTextBox; }            // 0x101d7d74
int32_t InterfaceVaryBox::WhatAmI() { return kUIKindVaryBox; }            // 0x101d8e0c

// UIMenu::Show 0x1018b0e0, UIMenu::Hide 0x1018b1fc, UIMenu::~UIMenu 0x1018ac8c (main app, ui_utils.cpp).
#include <cstdint>
#include <cstdlib>
#include <new>
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "ui_menu.h"

// The runtime's operator delete 0x10238218 (a weak default; the test library defines a recording fake).
__attribute__((weak)) void menu_callee::OperatorDelete(void* p) { ::operator delete(p); }

void UIMenu::Show(int32_t a) {
    if (mWindow == nullptr || mShown != 0) return;
    mShown = 1;
    mWindow->Show(1);
    int16_t z = static_cast<int16_t>(UI_GetHighestZ(nullptr, nullptr) + 1);
    if (a != 0) {
        gUIRootWindow->link(mWindow, 1, 1);
        mWindow->setZ(z);
    } else {
        UIMain_CheckPanel();
        gUIRootWindow->link(mWindow, 1, 1);
        mWindow->setZ(z);
    }
    mWindow->CaptureKb(0);
}

void UIMenu::Hide() {
    if (mShown == 0) return;
    mShown = 0;
    // The original calls Show on the window without a NULL test, then tests it for unlink.
    mWindow->Show(0);
    InterfaceWindow* window = mWindow;
    asm volatile("" : "+r"(window));   // keep the compiler from deducing non-NULL from the call above
    if (window != nullptr) window->unlink();
    mWindow->UnCaptureKb(0);
    UIMain_DecideKbCapture();
}

// The destructor with its hidden delete flag (r4, a short): the window is told 4, then the storage goes when the flag is above 0.
UIMenu* UIMenuDestruct(UIMenu* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mWindow != nullptr) UI_SendMessage(self->mWindow, 4, 0, 0);
        if (deleteFlag > 0) menu_callee::OperatorDelete(self);
    }
    return self;
}

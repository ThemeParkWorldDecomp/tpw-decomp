// InterfaceWindow (main app, UI toolkit 0x101dbafc..0x101dd860): the six small members the controls' window handlers call. The
// focus and the pointer capture are two globals (gUIFocusWindow, gUICaptureDevice); the handlers of the button and the knob take
// them on a press and give them back on a release.
#include "InterfaceWindow.h"

// The window that has the focus is told it lost it (0x1b) and this one that it got it (0x1a). Nothing happens when this window has it.
// (With no window in focus yet, the first message goes to NULL: UI_PostMessage answers 0 for it.)
void InterfaceWindow::CheckFocus() {
    if (gUIFocusWindow != this) {
        UI_PostMessage(gUIFocusWindow, 0x1b, 0, 0);
        gUIFocusWindow = this;
        UI_PostMessage(gUIFocusWindow, 0x1a, 0, 0);
    }
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

void InterfaceWindow::cursorChange() { UI_SetCursor(mCursor); }

int32_t InterfaceWindow::GetID() { return mId; }

InterfaceWindow* InterfaceWindow::GetParent() { return mParent; }

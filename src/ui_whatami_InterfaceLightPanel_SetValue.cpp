#include "ui_controls.h"

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

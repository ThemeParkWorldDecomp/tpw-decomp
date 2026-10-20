// InterfaceWindow::setCursor (main 0x101dd7f0, 60 B).
#include "InterfaceWindow.h"
void InterfaceWindow::setCursor(int16_t cursor) {
    mCursor = cursor;
    if (this == gUIPointerIn) UI_SetCursor(mCursor);
}

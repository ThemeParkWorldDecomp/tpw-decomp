// Dialog (main app, SimTheme Park 2.04b3, 0x101f12c8, 328 bytes; the inventory files it under ui_ErrorDisplay.cpp): the window maker the
// game's dialogs start from (UI_DialogBox is another function). Unit w43b.
#include "InterfaceWindow.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"

namespace {

char kDimDrawObject[] = "lolight";   // the draw object of the dimmed background of a modal dialog (0x1024f3e5)

}  // namespace

// QUIRKS: the dialog's depth is worked out after the script has been built (and the holder's z is the highest plus one, as a short); a
// modal dialog asks for the highest depth of all windows, not for the windows over the dialog's rectangle.
InterfaceWindow* Dialog(int16_t* description, InterfaceWindowCallback callback, int32_t modal) {
    InterfaceWindow* dialog = nullptr;
    InterfaceWindow* holder = gUIRootWindow->CreateObject(1, 1, 1, 0, 0, 0x7ff, 0x5ff);
    if (holder != nullptr) {
        dialog = holder->Build(description, callback);
        if (dialog != nullptr) {
            int32_t highest;
            if (modal == 0) {
                CoolRect rect;
                dialog->GetPosition(&rect, 1);
                highest = UI_GetHighestZ(nullptr, &rect);
            } else {
                highest = UI_GetHighestZ(nullptr, nullptr);
                holder->SetClientDrawObject(DrawObjectLibrary::FindDrawObject(kDimDrawObject));
            }
            holder->setZ(static_cast<int16_t>(highest + 1));
            if (callback != nullptr) dialog->SetCallbackFunction(callback);
            dialog->CaptureKb(0);
        }
    }
    return dialog;
}

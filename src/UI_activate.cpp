// activateUI (main app, SimTheme Park 2.04b3, UI_Base.cpp; unit w81). UI_Activate and UI_DeActivate (src/UI_focus.cpp) call it.
#include "InterfaceWindow.h"
#include "UI_Base.h"

namespace {

// The message a window gets: 0x21 when the interface becomes active, 0x22 when it does not (any non-zero `active` is "active").
void Notify(InterfaceWindow* window, int32_t active) { UI_PostMessage(window, active != 0 ? 0x21 : 0x22, 0, 0); }

}  // namespace

// 0x101ca30c (672 bytes). Posts the message to the window and then, depth first, to its descendants: the window's children, theirs, and so
// on for FIVE levels below the window, written out as six nested loops as the Mac has them (the sixth level recurses into activateUI for
// its children instead, which then starts a new six levels). Each loop posts to a window, walks that window's children, and only then
// asks the window for its next sibling (the sibling is read AFTER the subtree, so a handler that unlinked it is honoured).
void activateUI(InterfaceWindow* window, int32_t active) {
    Notify(window, active);
    for (InterfaceWindow* level1 = window->GetChild(0); level1 != nullptr; level1 = level1->GetNext()) {
        Notify(level1, active);
        for (InterfaceWindow* level2 = level1->GetChild(0); level2 != nullptr; level2 = level2->GetNext()) {
            Notify(level2, active);
            for (InterfaceWindow* level3 = level2->GetChild(0); level3 != nullptr; level3 = level3->GetNext()) {
                Notify(level3, active);
                for (InterfaceWindow* level4 = level3->GetChild(0); level4 != nullptr; level4 = level4->GetNext()) {
                    Notify(level4, active);
                    for (InterfaceWindow* level5 = level4->GetChild(0); level5 != nullptr; level5 = level5->GetNext()) {
                        Notify(level5, active);
                        for (InterfaceWindow* level6 = level5->GetChild(0); level6 != nullptr; level6 = level6->GetNext()) {
                            activateUI(level6, active);
                        }
                    }
                }
            }
        }
    }
}

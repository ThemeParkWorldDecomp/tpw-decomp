// ui_staffPanel_msg.cpp (main, unit w131): the staff panels' message functions: StaffPanelMsg 0x101bfdd8 (the panels 0x95c and 0x963),
// StaffLocMsg 0x101c00f0 (the location panel 0x146) and ModifiedCheckButtonsCallback 0x101bffb0 (the check button groups 0x95d). Their
// builders are in src/ui_staffPanel_init.cpp.
#include <cstdint>

#include "TPCamera.h"
#include "UI_Base.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_main.h"

CThing* gStaffPanelThing;   // 0x10395a3c

// 0x101bfdd8 (416 bytes): the staff panels. A check button chosen (0x101) while the staff panel is up (gStaffPanelState not 0): the ids 0x95e
// to 0x964 (an unsigned test of a - 0x95e below 7, through a table of the Mac) switch the location panel to 1, 2, 5, 4, 3, nothing (0x963) and 6,
// each only when b is 1. On-frame (0x1e): hackfollowmode, then UpdateStatus. Destroyed (0x14): gStaffPanelA is forgotten. 0x1001d8 (the panel
// is closed): the thing is forgotten, the location panel hidden and unlinked, the state cleared, the first person flag 0x1000 cleared and the
// advisor messages 0x11f and 0xc6 cancelled. Then the default handler's answer.
int32_t StaffPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101) {
        if (gStaffPanelState != 0 && static_cast<uint32_t>(a - 0x95e) <= 6) {
            switch (a - 0x95e) {
                case 0: if (b == 1) StaffLocationSwitch(1); break;
                case 1: if (b == 1) StaffLocationSwitch(2); break;
                case 2: if (b == 1) StaffLocationSwitch(5); break;
                case 3: if (b == 1) StaffLocationSwitch(4); break;
                case 4: if (b == 1) StaffLocationSwitch(3); break;
                case 6: if (b == 1) StaffLocationSwitch(6); break;
                default: break;
            }
        }
    } else if (message == 0x1e) {
        hackfollowmode();
        UpdateStatus();
    } else if (message == 0x14) {
        gStaffPanelA = nullptr;
    } else if (message == 0x1001d8) {
        gStaffPanelThing = nullptr;
        gStaffPanelC->Show(0);
        gStaffPanelC->unlink();
        gStaffPanelState = 0;
        gFirstPersonFlags &= 0xffffefffu;
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x11f));
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xc6));
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101bffb0 (248 bytes): the check button groups. A button (0x100) makes the group's current button that one (SetCurrent) and tells the
// PARENT a check button was chosen (0x101, a, 1); a button that is not pressed (b not 1) is first drawn pressed (state 1, redraw 1). The
// child is looked up first, and may be NULL. Answers 1; any other message goes to the default handler, whose answer this is.
int32_t ModifiedCheckButtonsCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        InterfaceWindow* child = window->GetWindow(a);
        if (b == 1) {
            static_cast<InterfaceCheckButtons*>(window)->SetCurrent(a);
        } else {
            static_cast<InterfaceCheckButtons*>(window)->SetCurrent(a);
            static_cast<InterfaceButton*>(child)->SetState(1, 1);
        }
        UI_PostMessage(window->GetParent(), 0x101, a, 1);
        return 1;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101c00f0 (468 bytes): the staff location panel (window 0x146). 0x10002 hides its child 0x149 and 0x10001 shows it; destroyed (0x14):
// gStaffPanelC is forgotten. 0x10006 with a = 0: the panel shows the staff panel's thing: the status panel of a staff member for the kinds 4 to
// 8, the visitor status panel for every other kind. Then the default handler's answer.
int32_t StaffLocMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10002) {
        window->GetWindow(0x149)->Show(0);
    } else if (message == 0x14) {
        gStaffPanelC = nullptr;
    } else if (message == 0x10001) {
        window->GetWindow(0x149)->Show(1);
    } else if (message == 0x10006 && a == 0) {
        CThing* thing = gStaffPanelThing;
        const uint32_t kind = thing->mKind;
        if (kind - 4 <= 4) gStaffStatusPanel.Show(thing);
        else gVisitorStatusPanel.Show(thing);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

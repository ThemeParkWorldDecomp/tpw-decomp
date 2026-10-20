// ui_entryPricePanel_msg.cpp (main, unit w131): the message functions of the cash panel (CashPanelMsg 0x1019ade8) and of the control panel
// (ControlPanelMsg 0x1019d070). Their builders are in src/ui_entryPricePanel.cpp.
#include <cstdint>

#include "UIInterface.h"
#include "ui_Postcard.h"
#include "ui_main.h"

// 0x1019ade8 (244 bytes): the cash panel (window 0x1b58 and up). A button (0x100): the year end (0x1b59), loans (0x1b5a) and staff costs
// (0x1b5b) buttons make their panel and then close the slide-out panel. The window being destroyed (0x14) forgets gCashPanel. Then the
// default handler's answer.
int32_t CashPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        if (a == 0x1b59) {
            CreateYearEndPanel();
            RemoveSlideoutPanel(1);
        } else if (a == 0x1b5a) {
            CreateLoansPanel();
            RemoveSlideoutPanel(1);
        } else if (a == 0x1b5b) {
            CreateStaffCostsPanel();
            RemoveSlideoutPanel(1);
        }
    } else if (message == 0x14) {
        gCashPanel = nullptr;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x1019d070 (212 bytes): the control panel. A button (0x100): 100 makes the postcard, 0x63 the camcorder mode; whatever the button, the
// advisor message 0x131 is cancelled and the slide-out panel closed. The window being destroyed (0x14) forgets gControlPanel. Then the default
// handler's answer.
int32_t ControlPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        if (a == 100) UI_DoPostcard();
        else if (a == 0x63) gUIInterface.CamcorderMode();
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x131));
        RemoveSlideoutPanel(1);
    } else if (message == 0x14) {
        gControlPanel = nullptr;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

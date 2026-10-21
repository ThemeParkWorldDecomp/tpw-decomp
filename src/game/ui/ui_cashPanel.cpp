// ui_cashPanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ui_main.h"
#include "ui_Postcard.h"
#include "UIInterface.h"
#include <cstdint>

// ui_entryPricePanel_msg.cpp (main, unit w131): the message functions of the cash panel (CashPanelMsg 0x1019ade8) and of the control panel
// (ControlPanelMsg 0x1019d070). Their builders are in src/ui_entryPricePanel.cpp.

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

// ui_entryPricePanel.cpp (main): InitCashPanel 0x1019af14, InitCoasterPanel 0x1019c2dc,
// InitControlPanel 0x1019d180 (116 B each); DeInitCashPanel 0x1019afac,
// DeInitCoasterPanel 0x1019c378, DeInitControlPanel 0x1019d21c (68 B each).

InterfaceWindow* gCashPanel;      // 0x10394824
void InitCashPanel() {
    gCashPanel = gUIRootWindow->Build(gCashPanelScript, CashPanelMsg);
    gCashPanel->unlink();
    gCashPanel->setExtraParam(0, nullptr);
    gCashPanel->Show(0);
}
void DeInitCashPanel() { if (gCashPanel != nullptr) delete gCashPanel; }

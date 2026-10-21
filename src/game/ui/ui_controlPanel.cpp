// ui_controlPanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <new>

// ui_entryPricePanel_msg.cpp (main, unit w131): the message functions of the cash panel (CashPanelMsg 0x1019ade8) and of the control panel
// (ControlPanelMsg 0x1019d070). Their builders are in src/ui_entryPricePanel.cpp.

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

// ui_entryPricePanel.cpp (main): InitCashPanel 0x1019af14, InitCoasterPanel 0x1019c2dc,
// InitControlPanel 0x1019d180 (116 B each); DeInitCashPanel 0x1019afac,
// DeInitCoasterPanel 0x1019c378, DeInitControlPanel 0x1019d21c (68 B each).

InterfaceWindow* gControlPanel;   // 0x10394838

void InitControlPanel() {
    gControlPanel = gUIRootWindow->Build(gControlPanelScript, ControlPanelMsg);
    gControlPanel->unlink();
    gControlPanel->setExtraParam(0, reinterpret_cast<void*>(1));
    gControlPanel->Show(0);
}
void DeInitControlPanel() { if (gControlPanel != nullptr) delete gControlPanel; }

// 0x1019d28c (60 bytes)
void ShowControlPanel() {
    LinkSlideoutPanel(gControlPanel, 0);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x131));
}

// 0x1019d2f0 (52 bytes)
void RemoveControlPanel() {
    RemoveSlideoutPanel(0);
    UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x131));
}

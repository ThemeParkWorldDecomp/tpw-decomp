// ui_entryPricePanel.cpp (main): InitCashPanel 0x1019af14, InitCoasterPanel 0x1019c2dc,
// InitControlPanel 0x1019d180 (116 B each); DeInitCashPanel 0x1019afac,
// DeInitCoasterPanel 0x1019c378, DeInitControlPanel 0x1019d21c (68 B each).
#include "ui_main.h"

InterfaceWindow* gCashPanel;      // 0x10394824
InterfaceWindow* gControlPanel;   // 0x10394838

void InitCashPanel() {
    gCashPanel = gUIRootWindow->Build(gCashPanelScript, CashPanelMsg);
    gCashPanel->unlink();
    gCashPanel->setExtraParam(0, nullptr);
    gCashPanel->Show(0);
}
void InitCoasterPanel() {
    gCoasterPanel = gUIRootWindow->Build(gCoasterPanelScript, CoasterPanelMsg);
    gCoasterPanel->unlink();
    gCoasterPanel->setExtraParam(0, reinterpret_cast<void*>(2));
    gCoasterPanel->Show(0);
}
void InitControlPanel() {
    gControlPanel = gUIRootWindow->Build(gControlPanelScript, ControlPanelMsg);
    gControlPanel->unlink();
    gControlPanel->setExtraParam(0, reinterpret_cast<void*>(1));
    gControlPanel->Show(0);
}
void DeInitCashPanel() { if (gCashPanel != nullptr) delete gCashPanel; }
void DeInitCoasterPanel() { if (gCoasterPanel != nullptr) delete gCoasterPanel; }
void DeInitControlPanel() { if (gControlPanel != nullptr) delete gControlPanel; }

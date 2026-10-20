// ui_staffPanel.cpp (main): DeInitStaffPanel 0x101c05d4 (148 B).
#include "ui_main.h"
InterfaceWindow* gStaffPanelA;   // 0x10395a48
InterfaceWindow* gStaffPanelB;   // 0x10395a44
InterfaceWindow* gStaffPanelC;   // 0x10395a40
void DeInitStaffPanel() {
    if (gStaffPanelA != nullptr) delete gStaffPanelA;
    if (gStaffPanelB != nullptr) delete gStaffPanelB;
    if (gStaffPanelC != nullptr) delete gStaffPanelC;
}

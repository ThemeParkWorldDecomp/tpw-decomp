// ui_entryPricePanel.cpp (main app), unit w49: the two small functions of the coaster editor's panel that the keyboard decision calls,
// CoasterEditPanelUp 0x1019cfdc (12 bytes) and CoasterEditPanelGetKb 0x1019d014 (48 bytes), and the two globals they read.
#include "InterfaceWindow.h"
#include "ui_main.h"

int32_t gCoasterEditMode;
InterfaceWindow* gCoasterPanel;

int32_t CoasterEditPanelUp() { return gCoasterEditMode; }

void CoasterEditPanelGetKb() { gCoasterPanel->CaptureKb(0); }

// DeInitOnlineUI 0x10186620 (124 bytes), Mac file ui_main.cpp: the undo of InitOnlineUI.
#include "InterfaceWindow.h"
#include "Light.h"
#include "MapWho.h"
#include "TbITable.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "platform/msl.h"
#include "ui_main.h"

void RemoveOnlinePanel();   // 0x101e92c4 (56 bytes)

void DeInitOnlineUI() {
    RemoveOnlinePanel();
    UI_SendMessage(gFirstPersonWindow, 4, 0, 0);
    UI_DisableHelpText();
    gei_MapWho->Unlink(static_cast<CMapLinkBase*>(&gUILight), 0x20);
}

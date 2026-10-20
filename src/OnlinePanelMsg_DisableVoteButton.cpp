// OnlinePanelMsg_DisableVoteButton.cpp, main app: two helpers of the online panel (gOnlinePanel).
#include <cstdint>
#include "InterfaceWindow.h"
#include "Mouse.h"
#include "RideDB.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_Postcard.h"
#include "ui_main.h"
#include "ui_tooltip.h"
#include "LLCellSystem.h"
#include "Debug.h"
#include "online.h"
#include "ui_controls.h"

// 0x101e6224: with the online panel, takes its child 0xc3123, that one's 0xc3124, that one's 0xc3129 and calls the virtual
// Enable (slot 0x1c of the table at +0x118) on it with !flag. No NULL check on the children.
namespace online_callee {
void DisableVoteButton(bool flag) {
    if (gOnlinePanel) {
        InterfaceWindow* w = gOnlinePanel->GetWindow(0xc3123);
        w = w->GetWindow(0xc3124);
        w = w->GetWindow(0xc3129);
        w->Enable(!flag);
    }
}
}  // namespace online_callee

// 0x101e6168: the same path ending in child 0xc3128, an InterfaceButton: SetState(down == 1, 1).
void EarmuffsButtonDown(bool down) {
    if (gOnlinePanel) {
        int8_t state = (down == 1) ? 1 : 0;
        InterfaceWindow* w = gOnlinePanel->GetWindow(0xc3123);
        w = w->GetWindow(0xc3124);
        w = w->GetWindow(0xc3128);
        static_cast<InterfaceButton*>(w)->SetState(state, 1);
    }
}

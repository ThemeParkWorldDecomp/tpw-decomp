// ui_main.cpp (main app), unit w49: UIMain_DecideKbCapture 0x1018116c (612 bytes) decides which window has the keyboard after a dialog,
// a panel or the game window has closed. With MainMenuUp 0x101fc1ec (80 bytes; the Mac file ui_mainmenu.cpp) and UIMenu::IsShown
// 0x1018b298 (8 bytes; ui_utils.cpp) and the windows' globals, which the Mac keeps in the data of those files.
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_main.h"
#include "ui_menu.h"

InterfaceWindow* gGameWindow;
InterfaceWindow* gFirstPersonWindow;
InterfaceWindow* gBumholeWindow;
InterfaceWindow* gOnlinePanel;
InterfaceWindow* gOnlineWindow;
InterfaceWindow* gBankruptWindow;
InterfaceWindow* gUIPanelWindow;
UIMenu* gMainMenu;

int32_t UIMenu::IsShown() { return mShown; }

int32_t MainMenuUp() {
    if (gMainMenu != nullptr && gMainMenu->IsShown() != 0) return 1;
    return 0;
}

void UIMain_DecideKbCapture() {
    if (MainMenuUp() != 0) {
        gMainMenu->mWindow->CaptureKb(0);
        return;
    }
    if (gGameWindow == nullptr) {
        if (gFirstPersonWindow != nullptr && (gFirstPersonWindow->GetAttr() & 1) != 0) {
            gKeyTableCamera->Enable();
            gFirstPersonWindow->CaptureKb(0);
            return;
        }
        if (gOnlinePanel != nullptr && (gOnlinePanel->GetAttr() & 1) != 0) {
            gOnlinePanel->CaptureKb(0);
            return;
        }
        if (gOnlineWindow != nullptr && (gOnlineWindow->GetAttr() & 1) != 0) {
            gOnlineWindow->CaptureKb(0);
            return;
        }
        if (gLobby != nullptr && gLobby->mWindow != nullptr) gLobby->mWindow->CaptureKb(0);
        return;
    }
    if (gBankruptWindow != nullptr) {
        gBankruptWindow->CaptureKb(0);
        return;
    }
    // The original does not test the first-person window for NULL here (it reads the attributes at address 0x44 then).
    if ((gFirstPersonWindow->GetAttr() & 1) != 0) {
        gKeyTableCamera->Enable();
        gFirstPersonWindow->CaptureKb(0);
        return;
    }
    if ((gGameWindow->GetAttr() & 1) != 0) {
        if (gUIPanelWindow != nullptr) {
            gUIPanelWindow->CaptureKb(0);
        } else if (CoasterEditPanelUp() == 0) {
            gKeyTableCamera->Enable();
            gKeyTableGame->Enable();
            gKeyTableCheat->Enable();
            gGameWindow->CaptureKb(0);
        } else {
            gKeyTableCamera->Enable();
            CoasterEditPanelGetKb();
        }
        return;
    }
    if (gBumholeWindow != nullptr && (gBumholeWindow->GetAttr() & 1) != 0) {
        gKeyTableCamera->Enable();
        gBumholeWindow->CaptureKb(0);
    }
}

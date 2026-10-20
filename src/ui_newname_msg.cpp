// The "Create new player" bubble's message function and the confirmations' OK callbacks (main app, SimTheme Park 2.04b3; the inventory
// files them under ui_playerLogin.cpp). Unit w47: CreatePlayerMsg 0x101af3fc (468 bytes), NewNameOK 0x101af264 (372),
// GameConfirmQuit_OK 0x101ae1b0 (60) and GameConfirmDelPlayer_OK 0x101adddc (136).
//
// The bubble is the dialog NewName (src/ui_newname.cpp) makes; it keeps the login slot as extra parameter 0. Its buttons send message 0x100
// with the id -1 (OK) or -2 (cancel); the return key in the edit box arrives as message 0x802.
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "UI_Base.h"
#include "lobby.h"
#include "platform/msl.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"

namespace {

constexpr int32_t kMessageClose = 4;
constexpr int32_t kMessageButton = 0x100;
constexpr int32_t kMessageSelected = 0x101;
constexpr int32_t kMessageDestroy = 0x14;
constexpr int32_t kMessageReturnKey = 0x802;
constexpr int32_t kMessageUnused = 0x803;
constexpr int32_t kMessageEscape = 0x804;
constexpr int32_t kButtonOk = -1;
constexpr int32_t kButtonCancel = -2;
constexpr int32_t kPicture = 0x70b;
constexpr int32_t kChoices = 0x70c;
constexpr int32_t kChoiceEasy = 0x70d;

// The login slot a pointer-sized parameter carries (the Mac's 32 bits).
EFSUserInfoGameUser SlotOf(void* slot) { return static_cast<EFSUserInfoGameUser>(static_cast<int32_t>(reinterpret_cast<intptr_t>(slot))); }

// The program's user information, made on first use (the original tests it before every use, CreateNewGameLoginUserInSlot included).
void NeedUserInfo() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo();
}

}  // namespace

// The ladder follows the listing: 0x101 first, then the messages below it (0x14 forgets the dialog, 0x100 is a button), then those above
// it (0x802 is the return key, 0x803 falls through, 0x804 closes; 0x805 and up and everything between 0x102 and 0x801 fall through). The
// buttons: -1 is OK (a name NewNameOK refuses leaves the bubble open, and falls through), -2 is cancel, any other id falls through. A
// fall-through is the dialogs' common handler, whose answer is the answer.
int32_t CreatePlayerMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == kMessageSelected) {
        if (b == 1) {
            InterfaceWindow* picture = window->GetWindow(kPicture);
            picture->Show(a == kChoiceEasy ? 1 : 0);
        }
    } else if (message < kMessageSelected) {
        if (message == kMessageDestroy) {
            gNewNameWindow = nullptr;
        } else if (message == kMessageButton) {
            if (a == kButtonOk) {
                if (NewNameOK(window->getExtraParam(0))) {
                    gLoginPanelFirstPlayer = 1;
                    UI_SendMessage(window, kMessageClose, 0, 0);
                    RemoveLoginPanel(0);
                    return 0;
                }
            } else if (a == kButtonCancel) {
                UI_SendMessage(window, kMessageClose, 0, 0);
                return 0;
            }
        }
    } else if (message != kMessageUnused) {
        if (message == kMessageReturnKey) {
            UI_SendMessage(window, kMessageButton, kButtonOk, 0);
            return 0;
        }
        if (message == kMessageEscape) {
            UI_SendMessage(window, kMessageClose, 0, 0);
            return 0;
        }
    }
    return DefaultDialogCallback(window, message, a, b);
}

// QUIRKS: the name buffer is cut in place even when the name is refused (all spaces: the buffer ends up empty). The user information is made
// again after CreateNewGameLoginUserInSlot (which could have destroyed it). The string is copied from the buffer before the check buttons are
// asked, and the check buttons 0x70c are not tested for NULL.
bool NewNameOK(void* slot) {
    uint32_t length = wcslen(gNewNameText);
    while (length != 0 && gNewNameText[length - 1] == u' ') {
        --length;
        gNewNameText[length] = 0;
    }
    if (length == 0) return false;
    TbDynamicStringTemplate<char16_t> name(gNewNameText);
    InterfaceCheckButtons* choices = static_cast<InterfaceCheckButtons*>(gNewNameWindow->GetWindow(kChoices));
    const int32_t current = choices->GetCurrent();
    NeedUserInfo();
    gUserInfo->CreateNewGameLoginUserInSlot(SlotOf(slot), name, current == kChoiceEasy ? 1 : 0);
    NeedUserInfo();
    gUserInfo->GameLogin(SlotOf(slot));
    return true;
}

// 0x101ae1b0: the quit confirmation. gLobby is not tested for NULL.
int32_t GameConfirmQuit_OK(void*) {
    gLobby->SendQuit();
    RemoveLoginPanel(1);
    return 1;
}

// 0x101adddc: the delete-player confirmation; `slot` is a login slot number carried in a pointer. The names are filled in again.
int32_t GameConfirmDelPlayer_OK(void* slot) {
    NeedUserInfo();
    gUserInfo->DeleteGameLoginUserInSlot(SlotOf(slot));
    FillLoginNames();
    return 1;
}

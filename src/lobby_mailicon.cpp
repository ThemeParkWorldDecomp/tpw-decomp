// The lobby's mail icon (main app 0x100a59ec; the inventory files it in lobby.cpp; unit w40): the window handler of the small envelope
// the lobby window shows when the player has mail waiting (CLBLobby::Init gives it to the window with id 0xbf432, and
// UpdateOutboxMailIcon, src/UI_IslandLobby.cpp, shows and hides it).
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

// 0x100a59ec. Only message 0x10006 (the icon was clicked) is handled here. The online system is made first when there is none (the
// lobby makes it too, but a click can come before). A player who is logged in gets the outbox panel; anyone else a dialog box with
// the interface string 0x1d9, white, a button of kind 2 (the first of three button slots, the other two unused) and no callbacks. Every
// message, the click included, then goes to the window's default handler and its answer is the function's.
int32_t LB_Lobby_MailIconMessagesFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
        if (gOnlineSystem->AreWeLoggedIn() != 0) {
            CreateOnlineOutboxPanel();
        } else {
            dlgboxparams params;
            params.mText = UIMain_GetString(0x1d9);
            params.mRed = 0xff;
            params.mGreen = 0xff;
            params.mBlue = 0xff;
            params.mValue8 = 4;
            params.mValueC = 0;
            UI_DialogBox(params, dlgbutton(2), nullptr, dlgbutton(0), nullptr, dlgbutton(0), nullptr, false);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

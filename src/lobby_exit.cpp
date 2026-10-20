// The way out of the lobby (main app): the key sparkle when the player enters an island (ui_islandLobbyController.cpp
// 0x101e1460), the interface's lobby tear-down (ui_main.cpp 0x10186434, ui_ErrorDisplay.cpp 0x101ef7a4) and the first-person
// ride test (TPCamera.cpp 0x100329cc).
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "Light.h"
#include "MapWho.h"
#include "TPCamera.h"
#include "TPSound.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "lobby.h"
#include "rsse.h"
#include "ui_main.h"

// A burst of sparkles over the middle of the controller's key panel (window 0x1e0ea) and the thunder bank's event 4, unless the
// island screen is not up or the game is the easy one. The emitter's x is taken from the window's TOP and BOTTOM edges (the
// listing reads the rectangle's words at +6 and +0xa, not +4 and +8): (top + (bottom - top) / 2) * 75000 / 0x600, the product
// wrapping in 32 bits and the division truncating. The window is not tested for NULL (GetPosition then runs on NULL). The rectangle
// is made only on the path that uses it.
void KeyParticlePOUFFE() {
    if (gIslandLobbyController == nullptr) return;
    if (TheGameType().mType == kGameTypeEasy) return;
    CoolRect rect;
    InterfaceWindow* window = gIslandLobbyController->GetWindow(0x1e0ea);
    window->GetPosition(&rect, 1);
    const int32_t middle = rect.mTop + ((rect.mBottom - rect.mTop) >> 1);
    const int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(middle) * 75000u);
    PAR_InitEmitter(0x62, 50000, 0, scaled / 0x600);
    SOUND_SendEvent(0, gLobbyThunderBank, 4, 0, 0, 0);
}

// Takes the interface's light out of the map's light list 0x20 (InitLobbyUI linked it) after turning the help text off. The
// Mac tests the address of the light's pointer word against NULL (a constant that is never NULL).
void DeInitLobbyUI() {
    UI_DisableHelpText();
    gei_MapWho->Unlink(static_cast<CMapLinkBase*>(&gUILight), 0x20);
}

void UIInterface::DeInitLobby() { DeInitLobbyUI(); }

// 1 when the ride `instance` is the one the player rides in first person: the first-person ride flag (bit 4 of the flags) is up,
// a ride set is current and the remembered instance, which must not be 0, is the argument.
int32_t IsRideFirstPersonRide(uint32_t instance) {
    if ((gFirstPersonFlags & kFirstPersonRideBit) != 0 && gFirstPersonRideSet != nullptr && gFirstPersonRideInstance != 0 &&
        gFirstPersonRideInstance == instance)
        return 1;
    return 0;
}

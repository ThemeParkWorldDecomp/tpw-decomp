// ui_islandLobbyController.cpp of the main app (SimTheme Park 2.04b3), second part, unit w36: the island lobby controller's message
// function, the two messages the island screen sends it (Remove, Hide) and the key emitter's start and stop. The controller itself,
// its key display and its description (gIslandLobbyWindowData) are src/UI_IslandLobby.cpp's (unit w32).
//
// The controller is the panel InterfaceWindow::Build makes from that description under the lobby's window, with
// IslandLobbyControllerMsg as its message function. The pump (src/UI_pump.cpp) sends it the messages: 0x14 when the panel goes (the
// global is cleared), 0x11 when it is shown or hidden (the key display, or the emitter killed), and 0x100 when a button of it was
// pressed (the button's window id is the first argument). The two buttons of the panel that are not the arrows go to the screens:
// the enter button selects the island the camera is on, the globe button hides the island screen and shows the download globe, the
// arrows move to the previous or the next island.
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "lobby.h"
#include "rsse.h"

namespace {
// The window ids of the controller (src/UI_IslandLobby.cpp has the others; names invented).
constexpr int32_t kEnterButton = 0x1e0e8;
constexpr int32_t kGlobeButton = 0x1e0e9;
constexpr int32_t kPreviousButton = 0x1e0f0;
constexpr int32_t kNextButton = 0x1e0f1;

// The messages the controller answers (the pump's numbers; names invented).
constexpr int32_t kMessageShown = 0x11;
constexpr int32_t kMessageButton = 0x100;
constexpr int32_t kMessageDeleted = 0x14;

// The key emitter: library emitter 0x61 at x 50000 (the same numbers as src/UI_IslandLobby.cpp's, which inlines this function).
constexpr int32_t kKeyEmitterType = 0x61;
}  // namespace

// 0x101e0d98 (396 bytes). Everything but the globe button goes on to the window's default handler, whose answer is the answer; the
// globe button answers 0 itself.
int32_t IslandLobbyControllerMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == kMessageDeleted) {
        gIslandLobbyController = nullptr;
        UpdateKeyParticles(nullptr, false);
    } else if (message == kMessageShown) {
        if (a == 0) UpdateKeyParticles(nullptr, false);
        else UpdateIslandLobbyKeyDisplay();
    } else if (message == kMessageButton) {
        if (a == kPreviousButton) {
            gLobbyScreens->mThemeIslands->GotoPreviousIsland();
        } else if (a == kGlobeButton) {
            gLobbyScreens->mThemeIslands->Hide();
            gLobbyScreens->mDownloadGlobe->Show();
            return 0;
        } else if (a == kEnterButton) {
            gLobbyScreens->mThemeIslands->SelectCurrentIsland();
        } else if (a == kNextButton) {
            gLobbyScreens->mThemeIslands->GotoNextIsland();
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101e11f8 (56 bytes) and 0x101e12d4 (56 bytes): messages 4 and 6 with no arguments to the controller (a NULL window is ignored by
// UI_SendMessage).
void RemoveIslandLobbyController() { UI_SendMessage(gIslandLobbyController, 4, 0, 0); }
void HideIslandLobbyController() { UI_SendMessage(gIslandLobbyController, 6, 0, 0); }

// 0x101e133c (232 bytes): with `on`, the emitter over the window's middle is started unless it is running (the window's rectangle
// less its animation offsets: the middle of top and bottom, times 75000 / 1536 in 32 bits, the product wrapping and the quotient
// truncating toward zero; the window is asked first even when the emitter runs); without, a running emitter is killed and its handle
// cleared. NULL `window` is only for off.
void UpdateKeyParticles(InterfaceWindow* window, bool on) {
    if (on) {
        CoolRect rect;
        window->GetPosition(&rect, 1);
        const int32_t middle = rect.mTop + ((rect.mBottom - rect.mTop) >> 1);
        const int32_t height = static_cast<int32_t>(static_cast<uint32_t>(middle) * 75000u) / 0x600;
        if (gIslandLobbyKeyEmitter == 0) gIslandLobbyKeyEmitter = static_cast<int32_t>(PAR_InitEmitter(kKeyEmitterType, 50000, 0, height));
    } else if (gIslandLobbyKeyEmitter != 0) {
        PAR_KillEmitter(gIslandLobbyKeyEmitter);
        gIslandLobbyKeyEmitter = 0;
    }
}

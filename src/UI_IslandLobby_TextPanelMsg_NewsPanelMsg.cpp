// NewsPanelMsg (0x101e2f84) and RemoveOnlineLoginPanel (0x101e2be4).
#include <cstdint>

#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

extern InterfaceWindow* gOnlineNewsPanel;   // 0x10395dd8
void RemoveOnlineNewsPanel();               // 0x101e35c4

// 0x10395d28: a pointer to a function descriptor (called through __ptr_glue), NULL until set; original name unknown.
void (*gOnlineLoginPanelHook)() = nullptr;

// The news panel's messages: 0x14 and 0x15 pass the lobby window a message 6 (1 / 0), 0x100 with -1 removes the panel.
int32_t NewsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x15) {
        UI_SendMessage(gLobby->mWindow, 6, 0, 0);
    } else if (message < 0x15) {
        if (message >= 0x14) {
            gOnlineNewsPanel = nullptr;
            UI_SendMessage(gLobby->mWindow, 6, 1, 0);
            UIMain_DecideKbCapture();
        }
    } else if (message == 0x100 && a == -1) {
        RemoveOnlineNewsPanel();
        return 0;
    }
    return DefaultDialogCallback(window, message, a, b);
}

void RemoveOnlineLoginPanel(int32_t arg) {
    UI_SendMessage(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(static_cast<uint32_t>(gOnlineLoginPanelFlag))), 4, 0, 0);
    if (arg != 0) {
        if (gOnlineLoginPanelHook != nullptr) gOnlineLoginPanelHook();
    }
}

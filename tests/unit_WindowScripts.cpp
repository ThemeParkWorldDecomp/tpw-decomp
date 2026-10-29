// Test library for the window scripts of src/game/ui/ui_allStaffPanel.cpp, src/game/ui/ui_cityLobbyController.cpp, src/game/ui/ui_onlineCityInfoOverlay.cpp, src/game/ui/ui_onlineOutbox.cpp, src/game/ui/ui_onlineParkInfoPanel.cpp, src/game/ui/ui_parkStatus.cpp (unit v5L42; libtpw_unit_WindowScripts.dylib,
// tests/test_WindowScripts.py), main app: the arrays and nothing else; tpw_ws_addr names them for the test. Also the online world's scripts
// (src/game/ui/ui_cityLobbyController.cpp, ui_onlineLogin.cpp, ui_onlineNewsPanel.cpp, ui_onlinePanel.cpp, ui_onlineParkFindPanel.cpp,
// ui_onlineParkListPanel.cpp, ui_publishParkPanel.cpp, ui_worldLobbyController.cpp).
#include <cstdint>
#include <cstring>

#include "FS_Online.h"
#include "FS_OnlineLoginPanel.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "lobby.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"

extern int16_t gOnlineNewsScript[];   // 0x102b0440 (src/game/ui/ui_onlineNewsPanel.cpp; no header declares it)

extern "C" {

uint64_t tpw_ws_addr(const char* name) {
    struct Entry {
        const char* name;
        const int16_t* address;
    };
    const Entry entries[] = {
        {"gAllShowsScript", gAllShowsScript},
        {"gOptionsPanelScript", gOptionsPanelScript},
        {"gProgBarScript", gProgBarScript},
        {"gCityInfoOverlayScript", gCityInfoOverlayScript},
        {"gOutboxPanelData", gOutboxPanelData},
        {"gOnlineParkInfoScript", gOnlineParkInfoScript},
        {"gCityLobbyScript", gCityLobbyScript},
        {"gOnlineLoginPanelScript", gOnlineLoginPanelScript},
        {"gOnlineNewsScript", gOnlineNewsScript},
        {"gOnlinePanelScript", gOnlinePanelScript},
        {"gOnlineUnderlayScriptA", gOnlineUnderlayScriptA},
        {"gOnlineUnderlayScriptB", gOnlineUnderlayScriptB},
        {"gOnlineParkFindScript", gOnlineParkFindScript},
        {"gOnlineParkListScript", gOnlineParkListScript},
        {"gPublishParkPanelScript", gPublishParkPanelScript},
        {"gWorldLobbyScript", gWorldLobbyScript},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

}  // extern "C"

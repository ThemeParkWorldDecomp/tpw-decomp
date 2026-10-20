// Unit v5948: the world lobby window's creation and the satellite number windows' message function (main app).
//   CreateWorldLobbyController 0x1022b038 (1600), SatelliteMsg 0x1022ae30 (464)
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "email.h"
#include "lobby.h"
#include "online.h"
#include "platform/samsutils.h"
#include "std_basic_filebuf.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

int16_t gWorldLobbyScript[2] = {1, 0};   // 0x102b20f8

// Weak; the test library fakes it (the constructor is another file's).
namespace worldlobby_callee {
__attribute__((weak)) InterfaceDrawObject* NewNumberText() {
    void* memory = ::operator new(std::max<size_t>(0x19c, sizeof(InterfaceNumberTextObject)));   // the host object is larger than the Mac's
    lender_callee::NumberCtor(memory);   // 0x1018d930 with NULL
    return static_cast<InterfaceDrawObject*>(memory);
}
}  // namespace worldlobby_callee

namespace {

// The lazy creation the Mac repeats before every use of the online system.
CFSOnlineSystem* Sys() {
    if (gOnlineSystem == nullptr) gOnlineSystem = findpanel_callee::NewOnlineSystem();
    return gOnlineSystem;
}

// One of the satellite panel's number windows: a number text object, the small font, value 0 twice, SatelliteMsg, its number, shown.
void MakeNumberWindow(InterfaceWindow* window, int32_t number) {
    window->SetClientDrawObject(worldlobby_callee::NewNumberText());
    window->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(window, 0);
    window->SetFontJust(1, 1);
    lender_callee::WindowSetValue(window, 0);
    window->SetCallbackFunction(SatelliteMsg);
    window->setExtraParam(0, reinterpret_cast<void*>(static_cast<intptr_t>(number)));
    entry_callee::WindowEnable(window, 1);
}

}  // namespace

// 0x1022b038
void CreateWorldLobbyController(bool publish) {
    if (gUISatelliteParent != nullptr) return;
    if (gLobby->mWindow == nullptr) return;
    gUISatelliteParent = gLobby->mWindow->Build(gWorldLobbyScript, WorldLobbyControllerMsg);
    gUISatelliteParent->setExtraParam(0, reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint8_t>(publish))));
    if (publish) {
        gUISatelliteParent->GetWindow(0x16007)->Show(0);
        entry_callee::WindowEnable(gUISatelliteParent->GetWindow(0x16003), 0);
        entry_callee::WindowEnable(gUISatelliteParent->GetWindow(0x16000), 0);
    } else {
        InterfaceWindow* panel = gUISatelliteParent->GetWindow(0x16007);
        entry_callee::WindowEnable(panel, 1);
        for (int32_t i = 0; i < 5; ++i) MakeNumberWindow(panel->GetWindow(0x16008 + i), i + 1);
        gChatSatelliteWanted = 1;
    }
    static const int32_t kButtons[4] = {0x16002, 0x16004, 0x16005, 0x16001};
    for (int32_t id : kButtons) static_cast<InterfaceButton*>(gUISatelliteParent->GetWindow(id))->SetAutoRepeat(0, 10);
}

// 0x1022ae30
int32_t SatelliteMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        if (Sys()->mCityList->mF28 != 0) {
            if (Sys()->mThemeList->mF28 != 0) {
                if (Sys()->mServerList->mF28 != 0) {
                    if (Sys()->mParksInCity2->mF28 != 0) {
                        int32_t number = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(0)));
                        Sys()->EnterChatSatellite(number - 1);
                    }
                }
            }
        }
    } else if (message == 0x1e) {
        ProcessGetChatSatellitePlayers();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

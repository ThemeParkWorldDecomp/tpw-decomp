// Unit v5060: the online lobby's interface set-up (InitOnlineUI 0x101864b0) and the bankrupt window's message function
// (BankruptWindowMsg 0x101868c0), Mac file ui_main.cpp.
#include <cstdint>

#include "InterfaceWindow.h"
#include "Light.h"
#include "MapWho.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "tb_resource_system.h"
#include "ui_ErrorDisplay.h"
#include "ui_Postcard.h"
#include "ui_main.h"

void CreateOnlinePanel();   // 0x101e8f08

// Gives the interface's light its colour (white, colour word all ones) and direction, links it into the map's light list 0x20,
// builds the online panel and the first person window (the full-screen window over the root, the message function
// Engine_GameFirstPersonMsg, cursor 0x15, script gFirstPersonWindowScript), then sets the help text by the tooltips setting.
void InitOnlineUI() {
    UI_FlushAllMessages();
    gei_System.UpdateLights();
    gUILight.mColorWord = 0xffffffffu;
    gUILight.mColor[0] = 1.0f;
    gUILight.mColor[1] = 1.0f;
    gUILight.mColor[2] = 1.0f;
    gUILight.mColor[3] = 1.0f;
    gei_System.UpdateLights();
    gUILight.mNormal.x = -0.3f;
    gUILight.mNormal.y = -0.5f;
    gUILight.mNormal.z = 0.8f;
    gei_MapWho->Link(static_cast<CMapLinkBase*>(&gUILight), 0x20);
    CreateOnlinePanel();
    InterfaceWindow* window = gUIRootWindow->CreateObject(1, 0, 1, 0, 0, 0x7ff, 0x5ff);
    if (window != nullptr) {
        window->SetCallbackFunction(Engine_GameFirstPersonMsg);
        gFirstPersonWindow = window;
        window->setCursor(0x15);
        gFirstPersonWindow->Build(gFirstPersonWindowScript, nullptr);
    }
    if (gPlayerSettings.mCurrent.mTooltipsOn)
        UI_EnableHelpText();
    else
        UI_DisableHelpText();
}

namespace {

// Closes the bankrupt window's way: puts the bumhole window back to the bankrupt script and animates it from its middle.
InterfaceWindow* BankruptRebuild() {
    InterfaceWindow* window = gBumholeWindow->Build(gBankruptScript, BankruptWindowMsg);
    return window;
}

void BankruptAnimate(InterfaceWindow* window) {
    CoolRect rect;
    window->GetPosition(&rect, 1);
    window->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
    window->startAnim(gBankruptAnim, 0x23);
}

}  // namespace

// Message 0x15 keeps the UI clock; 0x14 forgets the window; the right button (0x10006, button 1, RMB cancel on) more than 2000 ticks
// later, or the escape key (0x1000b, 0x1b, no modifier), closes the window and animates the bumhole window; the rest goes to the
// default handler (the 0x1000b case answers 0 in every path).
int32_t BankruptWindowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        if (a == 1 && gPlayerSettings.mCurrent.mRMBCancelOn &&
            static_cast<int32_t>(IveGotTicks() - static_cast<uint32_t>(gCongratsTicks)) > 2000 && gBankruptWindow != nullptr) {
            UI_SendMessage(window, 4, 0, 0);
            StopRidingRideIn1stPerson();
            ToggleGameWindow();
            InterfaceWindow* built = BankruptRebuild();
            EnableAllKeytables();
            gUIMainKeyTablesDisabled = 0;
            BankruptAnimate(built);
            return 0;
        }
    } else if (message < 0x10006) {
        if (message == 0x15) {
            gCongratsTicks = static_cast<int32_t>(IveGotTicks());
        } else if (message < 0x15 && message > 0x13) {
            gBankruptWindow = nullptr;
        }
    } else if (message == 0x1000b) {
        if (gBankruptWindow != nullptr && a == 0x1b && ((static_cast<uint32_t>(b) >> 16) & 0xff) == 0) {
            UI_SendMessage(window, 4, 0, 0);
            StopRidingRideIn1stPerson();
            ToggleGameWindow();
            InterfaceWindow* built = BankruptRebuild();
            CoolRect rect;
            built->GetPosition(&rect, 1);
            built->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
            built->startAnim(gBankruptAnim, 0x23);
            EnableAllKeytables();
            gUIMainKeyTablesDisabled = 0;
        }
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

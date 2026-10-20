// ui_main.cpp (main): InitGameUI 0x10185e58 (944 B), DeInitGameUI 0x1018622c (212 B).
// UIInterface wrappers: InitGame 0x101ef6a4 (44 B), DeInitGame 0x101ef6fc (36 B).
#include <new>
#include "ui_main.h"
#include "Mouse.h"
#include "Light.h"
#include "MapWho.h"
#include "Text.h"
#include "TPSettings.h"
#include "UIInterface.h"
#include "tb_resource_system.h"

float gGameWindowCentreX;              // 0x102b2b5c
float gGameWindowCentreY;              // 0x102b2b58
float gGameWindowHalfWidth = 1.0f;     // 0x10278a28
float gGameWindowHalfHeight = 1.0f;    // 0x10278a2c

void InitGameUI() {
    UI_FlushAllMessages();
    gText.InitThemeSpecific();
    CreateStaticTags();
    if (gMouse == nullptr) {
        CMouse* mouse = new (std::nothrow) CMouse;
        if (mouse != nullptr) {
            CMMDefault* mode = new (std::nothrow) CMMDefault;
            mouse->AssignMouseMode(mode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    if (gMouseMode != nullptr) gMouseMode->GetMode();
    CLight* light = &gUILight;
    gei_System.UpdateLights();
    light->mColorWord = 0xffffffffu;
    light->mColor[0] = 1.0f;
    light->mColor[1] = 1.0f;
    light->mColor[2] = 1.0f;
    light->mColor[3] = 1.0f;
    light = &gUILight;
    gei_System.UpdateLights();
    light->mNormal.x = -0.3f;
    light->mNormal.y = -0.5f;
    light->mNormal.z = 0.8f;
    gei_MapWho->Link(static_cast<CMapLinkBase*>(light), 0x20);

    InterfaceWindow* window = gUIRootWindow->CreateObject(1, 0, 0, 0, 0, 0x7ff, 0x5ff);
    if (window != nullptr) {
        window->Show(1);
        window->SetCallbackFunction(Engine_GameWinMsg);
        gGameWindow = window;
        CoolRect rect;
        window->GetPosition(&rect, 1);
        const int32_t halfWidth = (static_cast<int32_t>(rect.mRight) - rect.mLeft) / 2;
        const int32_t width = gUIRealScreenWidth;
        const int32_t centreX = rect.mLeft + halfWidth;
        const int32_t scaledHalfWidth = width * halfWidth;
        gGameWindowCentreX = static_cast<float>((width * centreX) / 2048);
        const int32_t halfHeight = (static_cast<int32_t>(rect.mBottom) - rect.mTop) / 2;
        const int32_t height = gUIRealScreenHeight;
        const int32_t centreY = rect.mTop + halfHeight;
        // lfd / fsubs converts each signed quotient, with one rounding to single precision.
        gGameWindowCentreY = static_cast<float>((height * centreY) / 1536);
        gGameWindowHalfWidth = static_cast<float>(scaledHalfWidth / 2048);
        gGameWindowHalfHeight = static_cast<float>((height * halfHeight) / 1536);
    }
    window = gUIRootWindow->CreateObject(1, 0, 1, 0, 0, 0x7ff, 0x5ff);
    if (window != nullptr) {
        window->SetCallbackFunction(Engine_GameFirstPersonMsg);
        gFirstPersonWindow = window;
        window->setCursor(0x15);
        gFirstPersonWindow->Build(gFirstPersonWindowScript, nullptr);
    }
    CreateMainPanel();
    InitControlPanel();
    InitStaffPanel();
    InitCashPanel();
    InitTagPanel();
    InitCoasterPanel();
    gUIPanelWindow = nullptr;
    UIMain_DecideKbCapture();
    if (gPlayerSettings.mCurrent.mTooltipsOn) UI_EnableHelpText();
    else UI_DisableHelpText();
}

void DeInitGameUI() {
    InstantRemoveAllTags();
    gei_MapWho->Unlink(static_cast<CMapLinkBase*>(&gUILight), 0x20);
    UI_DisableHelpText();
    DeInitControlPanel();
    DeInitStaffPanel();
    DeInitCashPanel();
    DeInitTagPanel();
    DeInitCoasterPanel();
    UI_SendMessage(gGameWindow, 4, 0, 0);
    UI_SendMessage(gFirstPersonWindow, 4, 0, 0);
    DestroyStaticTags();
    gText.DeInitThemeSpecific();
}

void UIInterface::InitGame() {
    InitGameUI();
    UI_ClearSelected();
}
void UIInterface::DeInitGame() { DeInitGameUI(); }

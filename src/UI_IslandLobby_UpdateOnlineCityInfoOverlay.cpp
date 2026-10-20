// The online city info overlay of the island lobby (0x101e1f64, 0x101e20f0) and the online login panel's message function (0x101e24c8).
#include <new>

#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "lobby.h"
#include "online.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

InterfaceWindow* gOnlineCityOverlay = nullptr;            // 0x10395d04
const CFSOnlineCity* gOnlineCityOverlayShown = nullptr;   // 0x10395d00
int32_t gOnlineLoginPanelFlag = 0;                        // 0x10395d2c
char16_t gOnlineLoginPassword[33] = {};                   // 0x10395d30
char16_t gOnlineLoginName[33] = {};                       // 0x10395d72
extern const char16_t kOnlineCityNoText[1] = {0};         // 0x1024ee5e

// 0x101e1f64: shows the city's name and its formatted number in the overlay's two text windows (0x16c11, 0x16c12); no city empties both.
// Does nothing without an overlay or when the city is the one shown already.
void UpdateOnlineCityInfoOverlay(const CFSOnlineCity* city) {
    if (gOnlineCityOverlay == nullptr) return;
    if (city == gOnlineCityOverlayShown) return;
    gOnlineCityOverlayShown = city;
    if (city != nullptr) {
        char16_t* name = city->mName;
        gOnlineCityOverlay->GetWindow(0x16c11)->SetText(name);
        TbIntegerVTag tag(0x1b, city->mValue);
        // gText + 0x564: the sixth of the resource pointers (the table of the interface strings in use here)
        const TbITable* table = static_cast<const TbITable*>(gText.mResourcePointers[6]);
        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1ce, &tag);
        gOnlineCityOverlay->GetWindow(0x16c12)->SetText(gUIMainConvertBuffer);
    } else {
        char16_t* none = const_cast<char16_t*>(kOnlineCityNoText);
        gOnlineCityOverlay->GetWindow(0x16c11)->SetText(none);
        gOnlineCityOverlay->GetWindow(0x16c12)->SetText(none);
    }
}

namespace {
// The same set-up for both text windows: a text sprite as the client draw object, centred, wrapped, white, in font `font`, no text.
void MakeOverlayText(InterfaceWindow* window, uint32_t font) {
    void* memory = online_callee::OperatorNew(0x19c);
    InterfaceTextObjectSprite* sprite = nullptr;
    if (memory != nullptr) sprite = new (memory) InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetFontJust(1, 1);
    window->SetFontWrap(1, 1);
    window->SetFont(GetFontTableEntry(font));
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    window->SetClientDrawText(nullptr);
}
}  // namespace

// 0x101e20f0: builds the overlay window from the script as a child of the lobby's window, once, and sets its two text windows up.
void CreateOnlineCityInfoOverlay() {
    if (gOnlineCityOverlay != nullptr) return;
    gOnlineCityOverlay = gLobby->mWindow->Build(gCityInfoOverlayScript, CityInfoOverlayMsg);
    MakeOverlayText(gOnlineCityOverlay->GetWindow(0x16c11), 5);
    MakeOverlayText(gOnlineCityOverlay->GetWindow(0x16c12), 8);
    gOnlineCityOverlayShown = nullptr;
}

// 0x101e24c8: the login panel's message function.
int32_t OnlineLoginPanelDlgMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    int32_t result = 0;
    (void)b;
    if (message == 0x100) {
        if (a == -1) {
            if (gOnlineSystem == nullptr) {
                void* memory = online_callee::OperatorNew(0x404);
                CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(memory);
                if (memory != nullptr) new (memory) CFSOnlineSystem();
                gOnlineSystem = system;
            }
            gOnlineSystem->DoLogin(gOnlineLoginName, gOnlineLoginPassword);
        } else if (a == -2) {
            RemoveOnlineLoginPanel(1);
            result = 1;
        }
    } else if (message < 0x100) {
        if (message == 0x15) {
            UI_SendMessage(gLobby->mWindow, 6, 0, 0);
            UIMain_DisableAllKeyTables();
        } else if (message == 0x14) {
            gOnlineLoginPanelFlag = 0;
            UI_SendMessage(gLobby->mWindow, 6, 1, 0);
            UIMain_DecideKbCapture();
            UIMain_EnableAllKeyTables();
        }
    } else if (message == 0x803) {
        if (a == 0x49)
            window->GetWindow(0x4b)->CaptureKb(0);
        else
            window->GetWindow(0x49)->CaptureKb(0);
    } else if (message < 0x803) {
        if (message >= 0x802) {
            if (a == 0x49)
                window->GetWindow(0x4b)->CaptureKb(0);
            else
                UI_PostMessage(window, 0x100, -1, 0);
        }
    } else if (message < 0x805) {
        RemoveOnlineLoginPanel(1);
        result = 1;
    }
    return result;
}

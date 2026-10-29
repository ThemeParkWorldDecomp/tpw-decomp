// ui_onlineCityInfoOverlay.cpp: name proven by __sinit_ui_onlineCityInfoOverlay_cpp in the main application.

#include "AdvisorScoring.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "online.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_Postcard.h"
#include "ui_publishParkPanel.h"
#include "WindowScripts.inc"
#include <new>

// Unit v51067: the small functions of the online UI between 0x101e0388 and 0x101ec444 (callbacks, remove / show / hide wrappers of the
// online panels, the contacts row functions, the string initialisers of three online UI files). Mac addresses in the comments.

uint8_t gOnlineCityInfoStringA[sizeof(TbDynamicStringTemplate<char16_t>)] __attribute__((aligned(8)));   // 0x10395d08
uint8_t gOnlineCityInfoNodeA[16];                                                                         // 0x10395d1c

// 0x101e1ee8, 60 bytes: message 0x14 forgets the overlay.
int32_t CityInfoOverlayMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gOnlineCityOverlay = nullptr;
    return window->CallDefaultWindowHandler(message, a, b);
}

// The online city info overlay of the island lobby (0x101e1f64, 0x101e20f0) and the online login panel's message function (0x101e24c8).

InterfaceWindow* gOnlineCityOverlay = nullptr;            // 0x10395d04
const CFSOnlineCity* gOnlineCityOverlayShown = nullptr;   // 0x10395d00
extern const char16_t kOnlineCityNoText[8] = u"\0upload";   // 0x1024ee5e, the image's words: the sinit's text at +1 is u"upload"

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

// 0x101e2314, 56 bytes.
void RemoveOnlineCityInfoOverlay() { UI_SendMessage(gOnlineCityOverlay, 4, 0, 0); }

// 0x101e2380, 56 bytes.
void ShowOnlineCityInfoOverlay() { UI_SendMessage(gOnlineCityOverlay, 6, 1, 0); }

// 0x101e23e8, 56 bytes.
void HideOnlineCityInfoOverlay() { UI_SendMessage(gOnlineCityOverlay, 6, 0, 0); }

// 0x101e2450, 64 bytes.
void __sinit_ui_onlineCityInfoOverlay_cpp() {
    void* a = CityLobbyStringConstruct(gOnlineCityInfoStringA, kOnlineCityNoText + 1);
    __register_global_object(a, reinterpret_cast<void*>(&CityLobbyStringDestructor), gOnlineCityInfoNodeA);
}

// Window scripts that InterfaceWindow::Build reads and that no translated file defined (unit v5L42): the launcher had all-zero
// stand-ins for them, so opening the panel failed the lpNew assertion in ProcessLevel. A file of their own because each building
// function's unit test defines its array with its fakes; the words are the image's, generated at configure time
// (devtools/gen_game_tables.py, WindowScripts.inc).

int16_t gCityInfoOverlayScript[] = {TPW_WINDOW_SCRIPT_102B0288};   // 0x102b0288, CreateOnlineCityInfoOverlay 0x101e20f0

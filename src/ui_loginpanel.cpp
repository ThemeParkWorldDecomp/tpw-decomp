// The player login panel (main app, ui_playerLogin.cpp, 0x101ae39c and 0x101ae9e0) and two small helpers of the interface (UIMain_GetString,
// ui_main.cpp, and UI_TITLE_COLOUR, ui_utils.cpp). Unit w39.
//
// The panel is the window script gLoginPanelWindowData (src/ui_loginpanel_data.cpp) built under the interface's root window. It has four
// rows, one for each saved player (the row windows 0x7a14, 0x7a19, 0x7a1a and 0x7a1b), each with a name area (0x7a15), a button (0x7a16)
// and a picture (0x7a17, shown for a player in the easy game); a fifth row (0x7a1c) carries the text of the quit button. CreateLoginPanel
// dresses the name areas with title text objects and FillLoginNames puts the players' names in them.
//
// Quirks kept: FillLoginNames and CreateLoginPanel call GetWindow on the answer of the one before without a test (a panel without the
// rows crashes them); the easy-mode answer is cut to a short before it is passed to Show; UI_TITLE_COLOUR takes no theme number (the
// colour is fixed).
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "Text.h"
#include "TbITable.h"
#include "TbResource.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

// The names' colour, passed to SetFontColour as it is: signed bytes, so the Mac's arguments are -20, -94 and 4.
int8_t gLoginNameRed = static_cast<int8_t>(0xec);     // 0x102ac004
int8_t gLoginNameGreen = static_cast<int8_t>(0xa2);   // 0x102ac005
int8_t gLoginNameBlue = 4;                            // 0x102ac006

InterfaceWindow* gLoginPanelWindow;   // 0x10394abc
int32_t gLoginPanelFirstPlayer;       // 0x10394ab8

namespace {

// The windows of the script (the names are invented; the numbers are the script's ids).
constexpr int32_t kRowWindow[4] = {0x7a14, 0x7a19, 0x7a1a, 0x7a1b};   // the four players' rows
constexpr int32_t kQuitRow = 0x7a1c;                                  // the fifth row
constexpr int32_t kNameArea = 0x7a15;                                 // in a row: the name
constexpr int32_t kRowButton = 0x7a16;                                // in a row: the button, shown for a player
constexpr int32_t kRowPicture = 0x7a17;                               // in a row: the easy game's picture

constexpr int32_t kTitleFont = 5;           // GetFontTableEntry's number for the names
constexpr int32_t kQuitStringId = 8;        // the quit row's text
constexpr int32_t kEmptyRowStringId = 0x1a5;   // what a row without a player says
constexpr int32_t kAdvisorFirstPlayer = 0x186;    // the advisor's lines when no player is saved
constexpr int32_t kAdvisorFirstPlayerMore = 0x187;
constexpr int32_t kAdvisorPlayers = 0x18e;        // the line when players are saved

// The user information, made on first use (the Mac's inline accessor).
CFSUserInfo* UserInfo() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    return gUserInfo;
}

// A row's name area dressed as a title: the title text object, font 5, centred, no text yet and the names' colour.
void DressNameArea(InterfaceWindow* area, char16_t* text, int32_t extraParam, InterfaceWindowCallback callback) {
    area->SetClientDrawObject(new InterfaceTitleTextObject(nullptr));
    area->SetFont(GetFontTableEntry(kTitleFont));
    area->SetFontJust(1, 1);
    area->SetText(text);
    UI_TITLE_COLOUR(area);
    area->SetFontColour(gLoginNameRed, gLoginNameGreen, gLoginNameBlue, 0xff);
    area->setExtraParam(0, reinterpret_cast<void*>(static_cast<intptr_t>(extraParam)));
    area->SetCallbackFunction(callback);
}

}  // namespace

// ---- the two helpers

const char16_t* UIMain_GetString(int32_t id) {
    const unsigned char* text = static_cast<const TbITable*>(gText.mResourcePointers[6])->GetConstString(static_cast<uint32_t>(id));
    if (text == nullptr) return nullptr;
    // (converted only when the Unicode-to-multibyte converter is loaded too, as UIMain_ConvertMulti does)
    if (gText.mResourcePointers[1] != nullptr)
        static_cast<TbIMBToUnicode*>(gText.mResourcePointers[0])->ConvertString(gUIMainConvertBuffer, text, 0x200);
    return gUIMainConvertBuffer;
}

void UI_TITLE_COLOUR(InterfaceWindow* window) { window->SetFontColour(0xea, 0xef, 0x66, 0xff); }

// ---- the panel

// One row at a time: the player's name and button, and the easy game's picture. A slot without a player says the empty row's string and
// hides the button and the picture. The row is looked up from the panel each time, the panel being read again for every call.
void FillLoginNames() {
    if (gLoginPanelWindow == nullptr) return;
    for (int32_t slot = 0; slot < 4; ++slot) {
        const int32_t row = kRowWindow[slot];
        InterfaceWindow* area = gLoginPanelWindow->GetWindow(row)->GetWindow(kNameArea);
        const TbStringBase<char16_t>* name = UserInfo()->GetGameLoginSlotUserName(static_cast<EFSUserInfoGameUser>(slot));
        if (name == nullptr) {
            area->SetText(const_cast<char16_t*>(UIMain_GetString(kEmptyRowStringId)));
            area->GetWindow(kRowButton)->Show(0);
            gLoginPanelWindow->GetWindow(row)->GetWindow(kRowPicture)->Show(0);
        } else {
            area->SetText(name->mData);
            area->GetWindow(kRowButton)->Show(1);
            const int16_t easy = static_cast<int16_t>(UserInfo()->IsGameLoginSlotUserEasyMode(static_cast<EFSUserInfoGameUser>(slot)));
            gLoginPanelWindow->GetWindow(row)->GetWindow(kRowPicture)->Show(easy);
        }
    }
}

void CreateLoginPanel() {
    DisableAllKeytables();
    gLobby->mWindow->Show(0);
    gLoginPanelWindow = gUIRootWindow->Build(gLoginPanelWindowData, LoginPanelMsg);
    if (gLoginPanelWindow != nullptr) {
        gLoginPanelWindow->CaptureKb(0);
        for (int32_t slot = 0; slot < 4; ++slot) {
            InterfaceWindow* area = gLoginPanelWindow->GetWindow(kRowWindow[slot])->GetWindow(kNameArea);
            DressNameArea(area, nullptr, slot, EntryMsg);
        }
        InterfaceWindow* quit = gLoginPanelWindow->GetWindow(kQuitRow);
        // (the quit row's text is read after its font is set, and its callback is the quit message function)
        quit->SetClientDrawObject(new InterfaceTitleTextObject(nullptr));
        quit->SetFont(GetFontTableEntry(kTitleFont));
        quit->SetFontJust(1, 1);
        quit->SetText(const_cast<char16_t*>(UIMain_GetString(kQuitStringId)));
        UI_TITLE_COLOUR(quit);
        quit->SetFontColour(gLoginNameRed, gLoginNameGreen, gLoginNameBlue, 0xff);
        quit->setExtraParam(0, reinterpret_cast<void*>(static_cast<intptr_t>(3)));
        quit->SetCallbackFunction(QuitMsg);
        FillLoginNames();
    }
    if (UserInfo()->GetNumValidGameLoginSlots() == 0) {
        gLobbyScreens->mAdvisor->Add(AdvisorResponse{kAdvisorFirstPlayer}, 1);
        gLobbyScreens->mAdvisor->Add(AdvisorResponse{kAdvisorFirstPlayerMore}, 0);
        gLoginPanelFirstPlayer = 1;
    } else {
        gLobbyScreens->mAdvisor->Add(AdvisorResponse{kAdvisorPlayers}, 1);
        gLoginPanelFirstPlayer = 0;
    }
    gLobbyScreens->mThemeIslands->ResetIslandLoopingSounds();
}

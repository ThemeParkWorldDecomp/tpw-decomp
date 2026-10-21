// ui_playerLogin.cpp: name proven by __sinit_ui_playerLogin_cpp in the main application.

#include "Advisor.h"
#include "AdvisorScoring.h"
#include "email.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "lobby.h"
#include "platform/msl.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbResource.h"
#include "Text.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include <cstdint>
#include <cstring>
#include <new>

// ui_playerLogin.cpp (main app), unit w43: LoginPanelMsg 0x101add48 (92 bytes), EntryMsg 0x101ade94 (744),
// QuitMsg 0x101ae218 (336), RemoveLoginPanel 0x101af02c (528).

int8_t gLoginHoverRed = -1;   // 0x102ac007
int8_t gLoginHoverGreen = -1; // 0x102ac008
int8_t gLoginHoverBlue;       // 0x10394ac0

namespace {
CFSUserInfo* UserInfo() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    return gUserInfo;
}
}  // namespace

int32_t LoginPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gLoginPanelWindow = nullptr;
    else if (message == 5) {
        RemoveLoginPanel(0);
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// The "Create new player" bubble's message function and the confirmations' OK callbacks (main app, SimTheme Park 2.04b3; the inventory
// files them under ui_playerLogin.cpp). Unit w47: CreatePlayerMsg 0x101af3fc (468 bytes), NewNameOK 0x101af264 (372),
// GameConfirmQuit_OK 0x101ae1b0 (60) and GameConfirmDelPlayer_OK 0x101adddc (136).
//
// The bubble is the dialog NewName (src/ui_newname.cpp) makes; it keeps the login slot as extra parameter 0. Its buttons send message 0x100
// with the id -1 (OK) or -2 (cancel); the return key in the edit box arrives as message 0x802.

namespace {

constexpr int32_t kMessageClose = 4;
constexpr int32_t kMessageButton = 0x100;
constexpr int32_t kMessageSelected = 0x101;
constexpr int32_t kMessageDestroy = 0x14;
constexpr int32_t kMessageReturnKey = 0x802;
constexpr int32_t kMessageUnused = 0x803;
constexpr int32_t kMessageEscape = 0x804;
constexpr int32_t kButtonOk = -1;
constexpr int32_t kButtonCancel = -2;
constexpr int32_t kPicture = 0x70b;
constexpr int32_t kChoices = 0x70c;
constexpr int32_t kChoiceEasy = 0x70d;

// The login slot a pointer-sized parameter carries (the Mac's 32 bits).
EFSUserInfoGameUser SlotOf(void* slot) { return static_cast<EFSUserInfoGameUser>(static_cast<int32_t>(reinterpret_cast<intptr_t>(slot))); }

// The program's user information, made on first use (the original tests it before every use, CreateNewGameLoginUserInSlot included).
void NeedUserInfo() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo();
}

}  // namespace

// 0x101adddc: the delete-player confirmation; `slot` is a login slot number carried in a pointer. The names are filled in again.
int32_t GameConfirmDelPlayer_OK(void* slot) {
    NeedUserInfo();
    gUserInfo->DeleteGameLoginUserInSlot(SlotOf(slot));
    FillLoginNames();
    return 1;
}

int32_t EntryMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10002) {
        window->SetFontColour(gLoginNameRed, gLoginNameGreen, gLoginNameBlue, 0xff);
    } else if (message == 0x10001) {
        window->SetFontColour(gLoginHoverRed, gLoginHoverGreen, gLoginHoverBlue, 0xff);
    } else if (message == 0x10006) {
        const auto slot = static_cast<EFSUserInfoGameUser>(reinterpret_cast<intptr_t>(window->getExtraParam(0)));
        if (UserInfo()->mUsers[slot].mName.Length() == 0) {
            NewName(slot);
        } else {
            UserInfo()->GameLogin(slot);
            RemoveLoginPanel(0);
            if (gPlayerSettings.mCurrent.mTooltipsOn) UI_EnableHelpText();
            else UI_DisableHelpText();
        }
        return 0;
    } else if (message == 0x100) {
        CFSUserInfo* user = UserInfo();
        const auto slot = static_cast<EFSUserInfoGameUser>(reinterpret_cast<intptr_t>(window->getExtraParam(0)));
        const TbStringBase<char16_t>* name = user->GetGameLoginSlotUserName(slot);
        TbUniStringVTag tag(4, name->mData);
        static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gUIMainConvertBuffer, 0x3fff, 0x18f, &tag);
        dlgboxparams params{gUIMainConvertBuffer, 0xff, 0xff, 0xff, 4,
                            static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(0)))};
        UI_DialogBox(params, dlgbutton(2), nullptr, dlgbutton(1), GameConfirmDelPlayer_OK, dlgbutton(0), nullptr, false);
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101ae1b0: the quit confirmation. gLobby is not tested for NULL.
int32_t GameConfirmQuit_OK(void*) {
    gLobby->SendQuit();
    RemoveLoginPanel(1);
    return 1;
}

int32_t QuitMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10002) {
        window->SetFontColour(gLoginNameRed, gLoginNameGreen, gLoginNameBlue, 0xff);
    } else if (message == 0x10001) {
        window->SetFontColour(0xff, 0, 0, 0xff);
    } else if (message == 0x10006) {
        dlgboxparams params{UIMain_GetString(9), 0xff, 0xff, 0xff, 4, 0};
        UI_DialogBox(params, dlgbutton(2), nullptr, dlgbutton(1), GameConfirmQuit_OK, dlgbutton(0), nullptr, false);
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

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

const char16_t* UIMain_GetString(int32_t id);

void UI_TITLE_COLOUR(InterfaceWindow* window);

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

void RemoveLoginPanel(int32_t cancel) {
    gLoginPanelWindow->GetParent()->CaptureKb(0);
    UI_SendMessage(gLoginPanelWindow, 4, 0, 0);
    gLobby->mWindow->Show(1);
    gLobby->mWindow->CaptureKb(0);
    EnableAllKeytables();
    gLobbyScreens->mAdvisor->ClearAll();
    if (cancel != 0) return;
    if (UserInfo()->mGameUser != -1 && gLoginPanelFirstPlayer != 0) {
        if (TheGameType().mType == kGameTypeEasy) {
            gLobbyScreens->mAdvisor->Add(AdvisorResponse{0x18a}, 1);
        } else {
            if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
            gMasterSave->GiveExtraKey();
            UserInfo()->SaveGlobalMasterSave();
            gLobbyScreens->mAdvisor->Add(AdvisorResponse{0x189}, 1);
        }
    }
    gLobbyScreens->mThemeIslands->ResetCurrentIsland(1);
    gLobbyScreens->mThemeIslands->UpdateSigns();
    UpdateIslandLobbyUI();
}

// QUIRKS: the name buffer is cut in place even when the name is refused (all spaces: the buffer ends up empty). The user information is made
// again after CreateNewGameLoginUserInSlot (which could have destroyed it). The string is copied from the buffer before the check buttons are
// asked, and the check buttons 0x70c are not tested for NULL.
bool NewNameOK(void* slot) {
    uint32_t length = wcslen(gNewNameText);
    while (length != 0 && gNewNameText[length - 1] == u' ') {
        --length;
        gNewNameText[length] = 0;
    }
    if (length == 0) return false;
    TbDynamicStringTemplate<char16_t> name(gNewNameText);
    InterfaceCheckButtons* choices = static_cast<InterfaceCheckButtons*>(gNewNameWindow->GetWindow(kChoices));
    const int32_t current = choices->GetCurrent();
    NeedUserInfo();
    gUserInfo->CreateNewGameLoginUserInSlot(SlotOf(slot), name, current == kChoiceEasy ? 1 : 0);
    NeedUserInfo();
    gUserInfo->GameLogin(SlotOf(slot));
    return true;
}

// The ladder follows the listing: 0x101 first, then the messages below it (0x14 forgets the dialog, 0x100 is a button), then those above
// it (0x802 is the return key, 0x803 falls through, 0x804 closes; 0x805 and up and everything between 0x102 and 0x801 fall through). The
// buttons: -1 is OK (a name NewNameOK refuses leaves the bubble open, and falls through), -2 is cancel, any other id falls through. A
// fall-through is the dialogs' common handler, whose answer is the answer.
int32_t CreatePlayerMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == kMessageSelected) {
        if (b == 1) {
            InterfaceWindow* picture = window->GetWindow(kPicture);
            picture->Show(a == kChoiceEasy ? 1 : 0);
        }
    } else if (message < kMessageSelected) {
        if (message == kMessageDestroy) {
            gNewNameWindow = nullptr;
        } else if (message == kMessageButton) {
            if (a == kButtonOk) {
                if (NewNameOK(window->getExtraParam(0))) {
                    gLoginPanelFirstPlayer = 1;
                    UI_SendMessage(window, kMessageClose, 0, 0);
                    RemoveLoginPanel(0);
                    return 0;
                }
            } else if (a == kButtonCancel) {
                UI_SendMessage(window, kMessageClose, 0, 0);
                return 0;
            }
        }
    } else if (message != kMessageUnused) {
        if (message == kMessageReturnKey) {
            UI_SendMessage(window, kMessageButton, kButtonOk, 0);
            return 0;
        }
        if (message == kMessageEscape) {
            UI_SendMessage(window, kMessageClose, 0, 0);
            return 0;
        }
    }
    return DefaultDialogCallback(window, message, a, b);
}

// NewName (main app, SimTheme Park 2.04b3, 0x101af60c, 1,124 bytes; the inventory files it under ui_playerLogin.cpp): the name entry that
// the login panel's "Create new player" button opens. Unit w43b.
//
// The dialog is a window script (gNewNameWindowData, src/ui_newname_data.cpp) built by Dialog (src/ui_dialog.cpp); this function then dresses
// its windows the way CreateLoginPanel dresses the panel's: each gets the text draw object its job needs, a font, a colour and a text.

InterfaceWindow* gNewNameWindow;   // 0x10394a90
char16_t gNewNameText[18];         // 0x10394a94

namespace {

// The windows of the dialog (the numbers are the script's ids; the names are invented).
constexpr int32_t kTitle = 0x709;
constexpr int32_t kNameEntry = 0x70a;
constexpr int32_t kChoice = 0x70c;          // the check buttons
constexpr int32_t kChoiceFirst = 0x70d;
constexpr int32_t kChoiceSecond = 0x70e;
constexpr int32_t kMissingChoice = 0x7a17;   // what the original asks the check buttons to select (see NewName)

constexpr int32_t kEntryFont = 6;
constexpr int32_t kChoiceFont = 6;
constexpr int32_t kEntryCapacity = 0x10;          // the characters the name entry takes
constexpr int32_t kAdvisorLine = 0x23a;
constexpr int32_t kEntryTextId = 0xf3;
constexpr int32_t kFirstChoiceTextId = 0xf1;
constexpr int32_t kSecondChoiceTextId = 0xf2;
constexpr int32_t kTitleTextId = 0xef;

// The names of the draw objects in the library and the characters the name entry refuses (a file name's: \ / * ? : | < > ").
char kCursorDrawObject[] = "hilight";
char16_t kRefusedChars[] = {0x5c, 0x2f, 0x2a, 0x3f, 0x3a, 0x7c, 0x3c, 0x3e, 0x22, 0};

// One of the check buttons' buttons dressed as black centred text (font 6) that says a string of the interface.
void DressChoice(InterfaceWindow* button, int32_t textId) {
    button->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    button->SetFontJust(1, 1);
    button->SetFont(GetFontTableEntry(kChoiceFont));
    button->SetFontColour(0, 0, 0, 0xff);
    button->SetClientDrawText(nullptr);
    button->SetText(const_cast<char16_t*>(UIMain_GetString(textId)));
}

}  // namespace

// The rectangle the dialog's client area gives (GetClientRect) is never read. The check buttons' children are looked up from the dialog's
// answer with no test for NULL (a dialog without them crashes the original, and this).
void NewName(int32_t slot) {
    CoolRect clientRect;
    if (gLoginPanelWindow == nullptr) return;
    gLobbyScreens->mAdvisor->Add(AdvisorResponse{kAdvisorLine}, 1);
    InterfaceWindow* dialog = Dialog(gNewNameWindowData, CreatePlayerMsg, 1);
    gNewNameWindow = dialog;
    dialog->GetClientRect(&clientRect);

    // the name entry
    InterfaceEditBox* entry = static_cast<InterfaceEditBox*>(dialog->GetWindow(kNameEntry));
    entry->SetStaticTextArea(gNewNameText, kEntryCapacity);
    entry->SetClientDrawObject(new InterfaceCursorPosTextObjectSprite(nullptr));
    entry->SetFont(GetFontTableEntry(kEntryFont));
    entry->SetFontColour(0xff, 0xff, 0xff, 0xff);
    entry->SetFontWrap(0, 0);
    entry->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(kCursorDrawObject));
    entry->SetUnAcceptedChars(kRefusedChars);
    entry->SetText(const_cast<char16_t*>(UIMain_GetString(kEntryTextId)));
    entry->CaptureKb(0);
    entry->SelectAll();
    dialog->setExtraParam(0, reinterpret_cast<void*>(static_cast<intptr_t>(slot)));

    // the two choices
    InterfaceWindow* choice = gNewNameWindow->GetWindow(kChoice);
    DressChoice(choice->GetWindow(kChoiceFirst), kFirstChoiceTextId);
    DressChoice(choice->GetWindow(kChoiceSecond), kSecondChoiceTextId);
    // (asks for a button the check buttons do not have, so none becomes the current one)
    static_cast<InterfaceCheckButtons*>(choice)->SetCurrent(kMissingChoice);

    // the title
    InterfaceTitleTextObject* titleObject = new InterfaceTitleTextObject(nullptr);
    dialog->GetWindow(kTitle)->SetClientDrawObject(titleObject);
    InterfaceFont* titleFont = GetFontTableEntry(kTitleFont);
    dialog->GetWindow(kTitle)->SetFont(titleFont);
    dialog->GetWindow(kTitle)->SetFontJust(1, 1);
    const char16_t* titleText = UIMain_GetString(kTitleTextId);
    dialog->GetWindow(kTitle)->SetText(const_cast<char16_t*>(titleText));
    UI_TITLE_COLOUR(dialog->GetWindow(kTitle));
}

// ui_playerLogin.cpp's static initialiser (0x101afa90): two wide strings built from the data at 0x1024e19e and registered for
// destruction at exit.

void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// The data at 0x1024e19e starts "/\" (0x002f 0x005c); the two strings sit at +0x14 and +0x22. Their text is not known yet.
const char16_t gPlayerLoginStrings[0x30] = {u'/', u'\\'};
alignas(16) uint8_t gPlayerLoginStringA[64];   // 0x10394ae4
alignas(16) uint8_t gPlayerLoginStringB[64];   // 0x10394ac4
uint8_t gPlayerLoginNodeA[16];                 // 0x10394af8
uint8_t gPlayerLoginNodeB[16];                 // 0x10394ad8

void __sinit_ui_playerLogin_cpp() {
    void* destroy = reinterpret_cast<void*>(&UIMain_DestroyWString);
    auto* a = new (gPlayerLoginStringA) TbDynamicStringTemplate<char16_t>(gPlayerLoginStrings + 0x14 / 2);
    __register_global_object(a, destroy, gPlayerLoginNodeA);
    auto* b = new (gPlayerLoginStringB) TbDynamicStringTemplate<char16_t>(gPlayerLoginStrings + 0x22 / 2);
    __register_global_object(b, destroy, gPlayerLoginNodeB);
}

// The login panel's window description (main app, SimTheme Park 2.04b3, data at 0x102abbb0, 371 words), for InterfaceWindow::Build.
// CreateLoginPanel (0x101ae9e0, a later unit) builds the panel from it when the lobby starts without a player. Unit w36 carries the
// words over so that the three lobby scripts live together (the others are gLobbyWindowData in src/lobby.cpp and
// gIslandLobbyWindowData in src/UI_IslandLobby.cpp); the format is described in src/InterfaceWindow_script.cpp. Each line is one
// record of the script; the window names in the comments are invented. The panel is the whole 2048 x 1536 screen with four list
// rows (0x7a14, 0x7a19, 0x7a1a, 0x7a1b: a name area with a button, a picture, a draw object), a panel at the lower left and a fifth
// row that starts to the right of the screen (its left edge is 1264 and its right 2428; it has a client rectangle but no children).
// The test compares all 371 words with the program image.

int16_t gLoginPanelWindowData[] = {
    0, 1, 1, 0, 31251, 0, 0, 0, 2048, 1536,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a13 (the login panel, the whole screen), rect (0, 0, 2048, 1536)
      17, -1, -1,   // 17: help text number -1
      18, -1, -1,   // 18: word at +0x90 is -1
      0, 1, 1, 0, 31252, 0, 884, 46, 2048, 193,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a14 (list row 1), rect (884, 46, 2048, 193)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 1, 1, 0, 31253, 0, 941, 75, 1716, 164,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a15 (a row's name area), rect (941, 75, 1716, 164)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          0, 2, 1, 0, 31254, 0, 1919, 68, 2021, 171,   // 0: child, kind 2 (button), attributes 0x1, id 0x7a16 (a row's button), rect (1919, 68, 2021, 171)
            17, -1, -1,   // 17: help text number -1
            18, -1, -1,   // 18: word at +0x90 is -1
            1, 658, 352,   // 1: draw object 0x01600292
            5,   // 5: end of this level
          5,   // 5: end of this level
        0, 6, 1, 0, 31255, 0, 1762, 68, 1865, 171,   // 0: child, kind 6 (image), attributes 0x1, id 0x7a17 (a row's picture), rect (1762, 68, 1865, 171)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          1, 4062, 31049,   // 1: draw object 0x79490fde
          5,   // 5: end of this level
        1, -16748, -4558,   // 1: draw object 0xee32be94
        5,   // 5: end of this level
      0, 1, 1, 0, 31256, 0, 32, 1321, 442, 1497,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a18 (lower left panel), rect (32, 1321, 442, 1497)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        1, -630, -4961,   // 1: draw object 0xec9ffd8a
        5,   // 5: end of this level
      0, 1, 1, 0, 31257, 0, 884, 245, 2048, 392,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a19 (list row 2), rect (884, 245, 2048, 392)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 1, 1, 0, 31253, 0, 941, 274, 1716, 363,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a15 (a row's name area), rect (941, 274, 1716, 363)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          0, 2, 1, 0, 31254, 0, 1919, 267, 2021, 370,   // 0: child, kind 2 (button), attributes 0x1, id 0x7a16 (a row's button), rect (1919, 267, 2021, 370)
            17, -1, -1,   // 17: help text number -1
            18, -1, -1,   // 18: word at +0x90 is -1
            1, 658, 352,   // 1: draw object 0x01600292
            5,   // 5: end of this level
          5,   // 5: end of this level
        0, 6, 1, 0, 31255, 0, 1762, 267, 1865, 370,   // 0: child, kind 6 (image), attributes 0x1, id 0x7a17 (a row's picture), rect (1762, 267, 1865, 370)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          1, 4062, 31049,   // 1: draw object 0x79490fde
          5,   // 5: end of this level
        1, -16748, -4558,   // 1: draw object 0xee32be94
        5,   // 5: end of this level
      0, 1, 1, 0, 31258, 0, 884, 444, 2048, 591,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a1a (list row 3), rect (884, 444, 2048, 591)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 1, 1, 0, 31253, 0, 941, 473, 1716, 562,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a15 (a row's name area), rect (941, 473, 1716, 562)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          0, 2, 1, 0, 31254, 0, 1919, 466, 2021, 569,   // 0: child, kind 2 (button), attributes 0x1, id 0x7a16 (a row's button), rect (1919, 466, 2021, 569)
            17, -1, -1,   // 17: help text number -1
            18, -1, -1,   // 18: word at +0x90 is -1
            1, 658, 352,   // 1: draw object 0x01600292
            5,   // 5: end of this level
          5,   // 5: end of this level
        0, 6, 1, 0, 31255, 0, 1762, 466, 1865, 569,   // 0: child, kind 6 (image), attributes 0x1, id 0x7a17 (a row's picture), rect (1762, 466, 1865, 569)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          1, 4062, 31049,   // 1: draw object 0x79490fde
          5,   // 5: end of this level
        1, -16748, -4558,   // 1: draw object 0xee32be94
        5,   // 5: end of this level
      0, 1, 1, 0, 31259, 0, 884, 643, 2048, 790,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a1b (list row 4), rect (884, 643, 2048, 790)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 1, 1, 0, 31253, 0, 941, 672, 1716, 761,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a15 (a row's name area), rect (941, 672, 1716, 761)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          0, 2, 1, 0, 31254, 0, 1919, 665, 2021, 768,   // 0: child, kind 2 (button), attributes 0x1, id 0x7a16 (a row's button), rect (1919, 665, 2021, 768)
            17, -1, -1,   // 17: help text number -1
            18, -1, -1,   // 18: word at +0x90 is -1
            1, 658, 352,   // 1: draw object 0x01600292
            5,   // 5: end of this level
          5,   // 5: end of this level
        0, 6, 1, 0, 31255, 0, 1762, 665, 1865, 768,   // 0: child, kind 6 (image), attributes 0x1, id 0x7a17 (a row's picture), rect (1762, 665, 1865, 768)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          1, 4062, 31049,   // 1: draw object 0x79490fde
          5,   // 5: end of this level
        1, -16748, -4558,   // 1: draw object 0xee32be94
        5,   // 5: end of this level
      0, 1, 1, 0, 31260, 0, 1264, 895, 2428, 1042,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a1c (row sliding in from the right), rect (1264, 895, 2428, 1042)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        3, 1339, 924, 2039, 1013,   // 3: client rectangle (1339, 924, 2039, 1013)
        1, -16748, -4558,   // 1: draw object 0xee32be94
        5,   // 5: end of this level
      5,   // 5: end of this level
    5,   // 5: end of this level
};
static_assert(sizeof(gLoginPanelWindowData) == 371 * sizeof(int16_t), "gLoginPanelWindowData has 371 words");

// The name entry dialog's window description (main app, SimTheme Park 2.04b3, data at 0x102abe96, 183 words), for InterfaceWindow::Build.
// NewName (0x101af60c, src/ui_newname.cpp) builds the dialog from it. It follows the login panel's description in the image (the
// panel's 371 words end at 0x102abe96; see src/ui_loginpanel_data.cpp). Each line is one record of the script (the format is described in
// src/InterfaceWindow_script.cpp); the window names in the comments are invented from what NewName does with the ids. The dialog 0x708 holds
// the title 0x709, the name entry 0x70a, a picture 0x70b, the check buttons 0x70c with their two buttons 0x70d and 0x70e, and the two
// buttons with the ids -2 and -1 that come first. The test compares all 183 words with the program image.

int16_t gNewNameWindowData[] = {
    0, 1, 1, 0, 1800, 0, 500, 233, 1516, 830,   // 0: child, kind 1 (window), attributes 0x1, id 0x708 (the dialog), rect (500, 233, 1516, 830)
      17, -1, -1,   // 17: help text number -1
      18, -1, -1,   // 18: word at +0x90 is -1
      1, 4894, -12909,   // 1: draw object 0xcd93131e
      0, 2, 1, 0, -2, -1, 1387, 613, 1470, 696,   // 0: child, kind 2 (button), attributes 0x1, id -2 (a button), rect (1387, 613, 1470, 696)
        17, 2, 0,   // 17: help text number 2
        18, -1, -1,   // 18: word at +0x90 is -1
        1, 16207, 5804,   // 1: draw object 0x16ac3f4f
        5,   // 5: end of this level
      0, 2, 1, 0, -1, -1, 1376, 524, 1460, 608,   // 0: child, kind 2 (button), attributes 0x1, id -1 (a button), rect (1376, 524, 1460, 608)
        17, 360, 0,   // 17: help text number 360
        18, -1, -1,   // 18: word at +0x90 is -1
        1, -7029, 5287,   // 1: draw object 0x14a7e48b
        5,   // 5: end of this level
      0, 1, 1, 0, 1801, 0, 655, 285, 1315, 365,   // 0: child, kind 1 (window), attributes 0x1, id 0x709 (the title), rect (655, 285, 1315, 365)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        5,   // 5: end of this level
      0, 5, 1, 0, 1802, 0, 593, 616, 1216, 698,   // 0: child, kind 5 (edit box), attributes 0x1, id 0x70a (the name entry), rect (593, 616, 1216, 698)
        17, 356, 0,   // 17: help text number 356
        18, -1, -1,   // 18: word at +0x90 is -1
        1, 15424, -25732,   // 1: draw object 0x9b7c3c40
        3, 623, 632, 1186, 677,   // 3: client rectangle (623, 632, 1186, 677)
        5,   // 5: end of this level
      0, 6, 1, 0, 1803, 0, 1241, 606, 1343, 708,   // 0: child, kind 6 (image), attributes 0x1, id 0x70b (a picture), rect (1241, 606, 1343, 708)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        1, 4062, 31049,   // 1: draw object 0x79490fde
        5,   // 5: end of this level
      0, 8, 17, 0, 1804, 0, 735, 377, 1235, 597,   // 0: child, kind 8 (check buttons), attributes 0x11, id 0x70c (the check buttons), rect (735, 377, 1235, 597)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 2, 17, 0, 1805, 0, 764, 405, 1206, 476,   // 0: child, kind 2 (button), attributes 0x11, id 0x70d (the first check button), rect (764, 405, 1206, 476)
          17, 358, 0,   // 17: help text number 358
          18, -1, -1,   // 18: word at +0x90 is -1
          1, -11071, -30933,   // 1: draw object 0x872bd4c1
          5,   // 5: end of this level
        0, 2, 17, 0, 1806, 0, 764, 498, 1206, 569,   // 0: child, kind 2 (button), attributes 0x11, id 0x70e (the second check button), rect (764, 498, 1206, 569)
          17, 359, 0,   // 17: help text number 359
          18, -1, -1,   // 18: word at +0x90 is -1
          1, -11071, -30933,   // 1: draw object 0x872bd4c1
          5,   // 5: end of this level
        1, -12040, -17669,   // 1: draw object 0xbafbd0f8
        5,   // 5: end of this level
      5,   // 5: end of this level
    5,   // 5: end of this level
};
static_assert(sizeof(gNewNameWindowData) == 183 * sizeof(int16_t), "gNewNameWindowData has 183 words");

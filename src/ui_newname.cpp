// NewName (main app, SimTheme Park 2.04b3, 0x101af60c, 1,124 bytes; the inventory files it under ui_playerLogin.cpp): the name entry that
// the login panel's "Create new player" button opens. Unit w43b.
//
// The dialog is a window script (gNewNameWindowData, src/ui_newname_data.cpp) built by Dialog (src/ui_dialog.cpp); this function then dresses
// its windows the way CreateLoginPanel dresses the panel's: each gets the text draw object its job needs, a font, a colour and a text.
#include "Advisor.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

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
constexpr int32_t kTitleFont = 5;
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

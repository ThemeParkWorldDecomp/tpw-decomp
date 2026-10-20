// Unit v5064: the publish park panel's message function and its creation, and the online park list panel's creation
// (0x101eca2c, 0x101eccc8, 0x101ec4e0).
#include "ui_publishParkPanel.h"

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

// ---- the globals of the panels (no other file of the program that is translated defines them; names invented)
OnlineParkSelection* gOnlineParkSelection = nullptr;   // 0x102e4864
OnlineUploadFile gOnlineUploadFile = {};               // 0x102e4868
InterfaceWindow* gPublishParkPanel = nullptr;          // 0x10396278
char16_t gPublishParkDescription[0x80] = {};           // 0x1039627c
char16_t gPublishParkName[0x10] = {};                  // 0x1039637c
int16_t gPublishParkPanelScript[2] = {1, 0};           // 0x102b11e6
int16_t gOnlineParkListScript[2] = {1, 0};             // 0x102b1086
InterfaceWindow* gOnlineParkListPanel = nullptr;       // 0x10396234
void (*gOnlineParkListClose)() = nullptr;              // 0x10396230

// 0x101eca2c, 608 bytes. The message function of the publish panel: 0x15 and 0x14 (the panel's show and hide, with the key tables and the
// lobby window's message 6), 0x100 (a button: -1 uploads the park, -2 closes), the edit boxes' 0x802 to 0x804 (a key in one box moves the
// keyboard to the other), everything else (and all of them but the two buttons) to the dialog's default handler. The game type object is
// built on first use here (the guard byte at 0x102b3aa4).
int32_t PublishPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        if (a == -1) {
            if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
            CFSOnlineSystem* system = gOnlineSystem;
            CGameType& type = TheGameType();
            system->RequestUpload(gOnlineUploadFile.mPath, gOnlineParkSelection->mID, gPublishParkName, gPublishParkDescription,
                                  type.mType == kGameTypeEasy, PostRequestUploadCallbackFunction, nullptr);
            RemovePublishParkPanel();
            return 0;
        }
        if (a == -2) {
            RemovePublishParkPanel();
            return 0;
        }
    } else if (message < 0x100) {
        if (message == 0x15) {
            UI_SendMessage(gLobby->mWindow, 6, 0, 0);
            UIMain_DisableAllKeyTables();
        } else if (message == 0x14) {
            gPublishParkPanel = nullptr;
            UIMain_EnableAllKeyTables();
            UI_SendMessage(gLobby->mWindow, 6, 1, 0);
            UIMain_DecideKbCapture();
        }
    } else if (message == 0x803) {
        if (a == 0x11e4)
            window->GetWindow(0x11e3)->CaptureKb(0);
        else
            window->GetWindow(0x11e4)->CaptureKb(0);
    } else if (message < 0x803) {
        if (message >= 0x802 && a == 0x11e4) window->GetWindow(0x11e3)->CaptureKb(0);
    } else if (message < 0x805) {
        UI_PostMessage(window, 4, 0, 0);
    }
    return DefaultDialogCallback(window, message, a, b);
}

// 0x101eccc8, 628 bytes. Makes the publish panel: the dialog, one level above its parent, the name box (a cursor text object, the font 7,
// the park name's buffer of 0xf characters, the hilight cursor) with the localised theme name as text, the description box (the same with
// wrap, left justification and 0x7e characters) with the interface string 0xdc, the title (string 0xda) and the advisor's line 0x250.
// The text goes in through the window's virtual SetText (slot 0x2c of the vtable at 0x118).
void CreatePublishParkPanel() {
    static char kHilight[] = "hilight";
    UIMain_CheckPanel();
    gPublishParkPanel = Dialog(gPublishParkPanelScript, PublishPanelMsg, 0);
    gPublishParkPanel->setZ(static_cast<int16_t>(gPublishParkPanel->mZ + 4));
    InterfaceWindow* box = gPublishParkPanel->GetWindow(0x11e4);
    InterfaceCursorPosTextObjectSprite* object = new InterfaceCursorPosTextObjectSprite(nullptr);
    box->SetClientDrawObject(object);
    box->SetFont(GetFontTableEntry(7));
    static_cast<InterfaceEditBox*>(box)->SetStaticTextArea(gPublishParkName, 0xf);
    static_cast<InterfaceEditBox*>(box)->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(kHilight));
    TbDynamicStringTemplate<char16_t> themeName;
    gGame.GetLocalisedThemeName(gGame.GetCurrentThemeName(), themeName);
    box->SetText(themeName.mData);
    box = gPublishParkPanel->GetWindow(0x11e3);
    object = new InterfaceCursorPosTextObjectSprite(nullptr);
    box->SetClientDrawObject(object);
    box->SetFont(GetFontTableEntry(7));
    box->SetFontWrap(1, 0);
    box->SetFontJust(0, 0);
    static_cast<InterfaceEditBox*>(box)->SetStaticTextArea(gPublishParkDescription, 0x7e);
    static_cast<InterfaceEditBox*>(box)->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(kHilight));
    box->SetText(const_cast<char16_t*>(UIMain_GetString(0xdc)));
    UIMain_SetTitleText(gPublishParkPanel, 0x11e5, const_cast<char16_t*>(UIMain_GetString(0xda)));
    gLobbyScreens->mAdvisor->Add(AdvisorResponse{0x250}, 1);
}

// 0x101ec4e0, 812 bytes. Makes the online park list panel from the lobby window: the list control with its five columns (two texts, three
// numbers), the selection bar, the five column buttons (strings 0x126 to 0x12a, texts 0x177 to 0x17b), then asks the online system (made on
// first use) for the parks in the city the player picked, with the title formatted from string 0x1bf of the interface table and the city's
// name, and the advisor's line 0x253.
void CreateOnlineParkListPanel(void (*onClose)()) {
    static char kHilight[] = "hilight";
    gOnlineParkListPanel = gLobby->mWindow->Build(gOnlineParkListScript, OnlineParkListPanelMsg);
    gOnlineParkListClose = onClose;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOnlineParkListPanel->GetWindow(0x14beb));
    list->SetColumnFormatsAll(OnlineParkListLCFormat, OnlineParkListLCPostFormat);
    list->SetColumnType(0, kLCText);
    list->SetColumnType(1, kLCText);
    list->SetColumnType(2, kLCNumber);
    list->SetColumnType(3, kLCNumber);
    list->SetColumnType(4, kLCNumber);
    list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(kHilight));
    static const int32_t kStrings[5] = {0x126, 0x127, 0x128, 0x129, 0x12a};
    for (int32_t i = 0; i < 5; ++i) {
        const char16_t* text = UIMain_GetString(kStrings[i]);
        InterfaceWindow* column = list->GetWindow(0x10 + i);
        MAKECOLUMNBUTTON(column, const_cast<char16_t*>(text), 8, 0xff, 0xef, 0, 0x177 + i);
    }
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
    gOnlineSystem->mParksInCity->GetList(gOnlineParkSelection->mID, OnlineParkListPostGetAllParksInCityCallbackFn, nullptr);
    TbUniStringVTag tag(0xf, gOnlineParkSelection->mName);
    static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1bf, &tag);
    UIMain_SetTitleText(gOnlineParkListPanel, 0x14bec, gUIMainConvertBuffer);
    gOnlineParkListPanel->CaptureKb(0);
    gLobbyScreens->mAdvisor->Add(AdvisorResponse{0x253}, 1);
}
